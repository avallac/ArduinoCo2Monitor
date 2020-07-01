#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <Adafruit_SSD1351.h>
#include <SoftwareSerial.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <WiFiUDP.h>

#define SENSOR_MHZ19 1
#define SENSOR_S8 2

#define SENSOR SENSOR_MHZ19

// Экран
#define VERSION "010520"
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128
#define SCLK_PIN D7
#define MOSI_PIN D3
#define DC_PIN   D0
#define CS_PIN   D5
#define RST_PIN  D6
#define BLACK  0x0000
#define BLUE   0x001F
#define GREEN  0x07E0
#define YELLOW 0xFFE0
#define RED    0xF800
#define WHITE  0xFFFF
#define GRAY   0x4208
Adafruit_SSD1351 tft = Adafruit_SSD1351(SCREEN_WIDTH, SCREEN_HEIGHT, CS_PIN, DC_PIN, MOSI_PIN, SCLK_PIN, RST_PIN);  

// CO2
int oldCO2;
int currentPpm = 0, currentTemp = 0, currentHum = 0;
SoftwareSerial mySerial(D4, D8);
unsigned char response[9];
String oldTemp, oldHum;

ESP8266WebServer server(80);

#ifndef STASSID
#define STASSID "AvNeT"
#define STAPSK  ""
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiUDP Udp;

Adafruit_BME280 bme;

void setup() {
  tft.begin();
  uint16_t time = millis();
  tft.fillRect(0, 0, 128, 128, BLACK);
  tft.println("Init");  
  delay(1000);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    tft.println("WiFiProblem");  
    delay(5000);
    ESP.restart();
  }
  tft.println("Ready");
  tft.println("IP address: ");
  tft.println(WiFi.localIP());
  bme.begin();
  delay(5000);
  
  server.on("/", WebServerRoot);
  server.on("/reboot", WebServerReboot);
  server.on("/calibrate", WebServerCalibrate);
  server.begin();

  tft.fillScreen(BLACK);
  tempIco(41, 25);
  humIco(39, 90);
  
  drowTemp("  ");
  tft.setTextSize(2);
  tft.print("C");

  drowHum("  ");
  tft.setTextSize(2);
  tft.print("%");

  // CO2 шкала
  tft.setTextSize(0);
  tft.setTextColor(RED);
  tft.setCursor(34, 0);
  tft.print("1200");
  tft.setTextColor(YELLOW);
  tft.setCursor(34, 60);
  tft.print("800");
  tft.setTextColor(GREEN);
  tft.setCursor(34, 120);
  tft.print("400");

  tft.setTextSize(0);
  tft.setTextColor(GRAY);
  tft.setCursor(90, 0);
  tft.print(VERSION);

  tft.setCursor(65, 60);
  tft.print(WiFi.localIP());
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

#if SENSOR == SENSOR_MHZ19  
  byte abc[9] = {0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  mySerial.write(abc, 9);
#endif
}

void drawCo2(int value)
{
  int mv;
  if (value > 400) {
    if (value > 600) {
      if (value > 800) {
        if (value > 1000) {
          if (value > 1200) {
            tft.fillRect(0, 0, 30, 32, RED); // RED Полностью 
          } else {
            // RED 800 - 1000
            mv = ceil((1200 - (float)value) * 32 / 200);
            tft.fillRect(0, 0, 30, mv - 1,  BLACK);
            tft.fillRect(0, mv, 30, 32 - mv,  RED);
          }
          tft.fillRect(0, 32, 30, 32, YELLOW); // YELLOW0 Полностью 
        } else {
        // YELLOW0 800 - 1000
        mv = ceil((1000 - (float)value) * 32 / 200);
        tft.fillRect(0, 0, 30, 32 + mv - 1,  BLACK);
        tft.fillRect(0, 32 + mv, 30, 32 - mv,  YELLOW);
        }
         tft.fillRect(0, 64, 30, 32, YELLOW); // YELLOW1 Полностью 
      } else {
        // YELLOW1 600 - 800
        mv = ceil((800 - (float)value) * 32 / 200);
        tft.fillRect(0, 0, 30, 64 + mv - 1,  BLACK);
        tft.fillRect(0, 64 + mv, 30, 32 - mv,  YELLOW);
      }
      tft.fillRect(0, 96, 30, 32, GREEN); // GREEN Полностью
    } else {
      // GREEN 400 - 600
      mv = ceil((600 - (float)value) * 32 / 200);
      tft.fillRect(0, 0, 30, 96 + mv - 1,  BLACK);
      tft.fillRect(0, 96 + mv, 30, 32 - mv,  GREEN);
    }
  } else {
    tft.fillRect(0, 0, 30, 128,  BLACK);
  }
  tft.setTextColor(BLACK);
  tft.setTextSize(1);
  tft.setCursor(100, 120);
  tft.print(oldCO2);
  tft.setTextColor(BLUE);
  tft.setCursor(100, 120);
  tft.print(value);
  oldCO2 = value;
}

#if SENSOR == SENSOR_MHZ19
byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
int getCO2Data() {
  int ppm;
  mySerial.write(cmd, 9);
  memset(response, 0, 9);
  mySerial.readBytes(response, 9);
  int i;
  byte crc = 0;
  for (i = 1; i < 8; i++) crc += response[i];
  crc = 255 - crc;
  crc++;
  if ( !(response[0] == 0xFF && response[1] == 0x86 && response[8] == crc) ) {
    tft.setCursor(0, 0);
    tft.setTextColor(WHITE);
    tft.setTextSize(1);
    for(i = 0; i < 60; i++) {
      tft.fillRect(0, 0, 128, 128, BLACK);
      tft.setCursor(0, 20);
      tft.println("CRC errorre");
      tft.print("reboot:");
      tft.print(60 - i);
      delay(1000);
    }
    ESP.restart();

    ppm = -1;
  } else {
    unsigned int responseHigh = (unsigned int) response[2];
    unsigned int responseLow = (unsigned int) response[3];
    ppm = (256 * responseHigh) + responseLow;
    if (ppm <= 200 || ppm > 4900) {
      tft.println("CO2: no valid data");
    } else {
      return ppm;
    }
  }
}
#elif SENSOR == SENSOR_S8
byte addArray[] = {0xFE, 0X44, 0X00, 0X08, 0X02, 0X9F, 0X25};
int getCO2Data() {
  while(!mySerial.available())  //keep sending request until we start to get a response
  {
    mySerial.write(addArray,7);
    delay(50);
  }

  int timeout=0;  //set a timeoute counter
  while(mySerial.available() < 7 ) //Wait to get a 7 byte response
  {
    timeout++;
    if(timeout > 10)    //if it takes to long there was probably an error
      {
        while(mySerial.available())  //flush whatever we have
          mySerial.read();

          break;                        //exit and try again
      }
      delay(50);
  }

  for (int i=0; i < 7; i++)
  {
    response[i] = mySerial.read();
  }

  int resHigh = (int) response[3];
  int resLow  = (int) response[4];
  return (256*resHigh) + resLow;
}
#endif

int getTemp()
{
  return bme.readTemperature() - 5;
}

int getHumid()
{
  return bme.readHumidity();
}

void tempIco(int x, int y)
{
  tft.drawLine(x + 3, y, x + 5, y, WHITE);
  tft.drawLine(x + 2, y + 1, x + 2, y + 14, WHITE);
  tft.drawLine(x + 6, y + 1, x + 6, y + 14, WHITE);
  tft.drawPixel(x + 1, y + 15, WHITE);
  tft.drawPixel(x + 7, y + 15, WHITE);
  tft.drawLine(x, y + 16, x, y + 20, WHITE);
  tft.drawLine(x + 8, y + 16, x + 8, y + 20, WHITE);
  tft.drawPixel(x + 1, y + 21, WHITE);
  tft.drawPixel(x + 7, y + 21, WHITE);
  tft.drawLine(x + 2, y + 22, x + 6, y + 22, WHITE);
  tft.drawLine(x + 4, y + 7, x + 4, y + 15, RED);
  tft.drawLine(x + 3, y + 15, x + 5, y + 15, RED);
  tft.fillRect(x + 2, y + 16, 5, 5, RED);
}


void humIco(int x, int y)
{
  tft.drawLine(x + 6, y, x + 6, y + 1, BLUE);
  tft.drawLine(x + 5, y + 2, x + 5, y + 3, BLUE);
  tft.drawLine(x + 7, y + 2, x + 7, y + 3, BLUE);
  tft.drawPixel(x + 4, y + 4, BLUE);
  tft.drawPixel(x + 8, y + 4, BLUE);
  tft.drawLine(x + 3, y + 5, x + 3, y + 6, BLUE);
  tft.drawLine(x + 9, y + 5, x + 9, y + 6, BLUE);
  tft.drawLine(x + 7, y + 5, x + 7, y + 6, BLUE);
  tft.drawPixel(x + 2, y + 7, BLUE);
  tft.drawPixel(x + 10, y + 7, BLUE);
  tft.drawPixel(x + 8, y + 7, BLUE);
  tft.drawLine(x + 1, y + 8, x + 1, y + 9, BLUE);
  tft.drawLine(x + 11, y + 8, x + 11, y + 9, BLUE);
  tft.drawPixel(x + 9, y + 9, BLUE);
  tft.drawLine(x + 0, y + 10, x + 0, y + 12, BLUE);
  tft.drawLine(x + 12, y + 10, x + 12, y + 12, BLUE);
  tft.drawLine(x + 1, y + 13, x + 1, y + 14, BLUE);
  tft.drawLine(x + 11, y + 13, x + 11, y + 14, BLUE);
  tft.drawPixel(x + 3, y + 14, BLUE); 
  tft.drawPixel(x + 2, y + 15, BLUE);
  tft.drawPixel(x + 10, y + 15, BLUE);
  tft.drawPixel(x + 3, y + 16, BLUE);
  tft.drawPixel(x + 9, y + 16, BLUE);
  tft.drawLine(x + 4, y + 17, x + 8, y + 17, BLUE);
}

void drowTemp(String value)
{
  tft.setTextColor(BLACK);
  tft.setTextSize(4);
  tft.setCursor(60, 20);
  tft.print(oldTemp);
  tft.setTextColor(WHITE);
  tft.setCursor(60, 20);
  tft.print(value);
  oldTemp = value;
}

void drowHum(String value)
{
  if (oldHum != value) {
    tft.setTextColor(BLACK);
    tft.setTextSize(4);
    tft.setCursor(60, 80);
    tft.print(oldHum);
    tft.setTextColor(WHITE);
    tft.setCursor(60, 80);
    tft.print(value);
    oldHum = value;
  }
}

unsigned long CO2Millis = 10000;
unsigned long MillisLast = 0;

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  unsigned long CurrentMillis = millis();
  if (CurrentMillis - MillisLast > CO2Millis || MillisLast == 0) {
    MillisLast = CurrentMillis;
    currentPpm = getCO2Data();
    currentTemp = getTemp();
    currentHum = getHumid();
    drawCo2(currentPpm);
    drowTemp(String(currentTemp));
    drowHum(String(currentHum));
    Send();
  }
}

void WebServerRoot() {
   String responce = "{";
   responce += "\"ppm\":" + String(currentPpm) + ",";
   responce += "\"temperature\":" + String(currentTemp) + ",\"humidity\":" + String(currentHum) + "}"; 
   server.send (200, "text/html", responce);
}

void WebServerReboot() {
 server.send (200, "text/html", String("ok"));
 delay(1000);
 ESP.restart();
}

void WebServerCalibrate()
{
  byte calibrate[9] = {0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78};
  mySerial.write(calibrate, 9);
}

void Send()
{
  String str = "{";
   str += "\"id\":\"" + String(WiFi.macAddress()) + "\",";
   str += "\"version\":\"" + String(VERSION) + "\",";
   str += "\"ppm\":" + String(currentPpm) + ",";
   str += "\"temperature\":" + String(currentTemp) + ",\"humidity\":" + String(currentHum) + "}"; 
  Udp.beginPacket("255.255.255.255", 1366);
  Udp.print(str);
  Udp.endPacket();
}
