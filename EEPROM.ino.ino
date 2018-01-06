#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <time.h>
#include <SPI.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <SimpleTimer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define UpperThreshold 550
#define LowerThreshold 500
#define OLED_RESET LED_BUILTIN 
Adafruit_SSD1306 display(OLED_RESET);
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client (espClient);
long lastMsg = 0;
char msg [50];
int ledPin = 13;
int x=0;
int LastTime=0;
bool BPMTiming=false;
bool BeatComplete=false;
int BPM=0;
int value55 =0;
int timezone = 7 * 3600;
int dst = 0;
int stoptime=0;
// Variables for accelerometer
const int MPU_addr=0x68;
int16_t AcX, AcY, AcZ, TmP,GyX,GyY,GyZ;
float AcX_calc, AcY_calc, AcZ_calc;
uint32_t lastTime;
// pin setup
const uint8_t scl = D6;
const uint8_t sda = D7;
// variables to publish to mqtt
char * condition;

const char * mqtt_server = "192.168.43.8";
String ssid = "";
String passphrase = "";

String content;
int statusCode;
void writeData(String s, String p) {
  Serial.println("Writing to EEPROM...");
  String ssid = "";
  String wifipass = "";

  for (int i = 0; i < 20; ++i) {
    EEPROM.write(i, ssid[i]);
  }
  for (int i = 20; i < 40; ++i) {
    EEPROM.write(i, wifipass[i - 20]);
  }
  EEPROM.commit();
  Serial.println("Write successful...");
}

void readData() {
  for (int i = 0; i < 20; ++i) {
    ssid += char(EEPROM.read(i));
  }
  for (int i = 20; i < 40; ++i) {
    passphrase += char(EEPROM.read(i));
  }
  Serial.println("Reading EEPROM");
  Serial.println("wifi ssid: " + ssid);
  Serial.println("wifi password: " + passphrase);
}

void reconnectMqtt(){
  // Loop until we're reconencted
  while(!client.connected()){
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(("ESP8266Client"))){
      Serial.println("connected");
    } else {
      Serial.print("Failed, rc = ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds...");
      delay(3000);
    }
  }
}

void check_imu(){
  readIMU();
  Serial.print("AcX: "); Serial.print(AcX); Serial.print("g | AcY: "); Serial.print(AcY); Serial.print("g | AcZ: "); Serial.print(AcZ);
  Serial.println("g");
  if(abs(AcX_calc)> 22000 || abs(AcY)> 22000|| abs(AcZ) > 27000){
    Serial.println("Fall detected");
    condition = "1"; // fallen
    lastTime = millis();
  } else {
    condition = "0"; // not fallen
  }
  delay(500);
}


void setup() {
  Serial.begin(115200);
  delay(10);
  EEPROM.begin(512);
  readData();
  WiFi.begin();
  
  if (testWifi()) {
    launchWeb(0);//OK
    timedisplay();
 client.setServer(mqtt_server, 1883);
//begin accelerometer
  Wire.begin(sda, scl);
    return;
  } else {
    const char *ssidap = "NodeMCU-AP"; const char* passap = "";
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssidap, passap);
    Serial.print("AP mode-http://");
    Serial.println(WiFi.softAPIP());
    launchWeb(1);
  }
  
}

void readIMU(){
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr,14,true);  // request a total of 14 registers
  AcX=Wire.read()<<8|Wire.read();  // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)    
  AcY=Wire.read()<<8|Wire.read();  // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  TmP=Wire.read()<<8|Wire.read();  // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)
}


bool testWifi() {
  WiFi.begin(ssid.c_str(),passphrase.c_str());
  int c = 0;
  Serial.println("Waiting for Wifi to connect");
  WiFi.mode(WIFI_STA);
  while ( c < 20 ) {
    if (WiFi.status()== WL_CONNECTED) {
      Serial.print(WiFi.localIP());
      return true;
    }
    delay(500);
    Serial.print(WiFi.status());
    c++;
  }
  Serial.println("AP Mode");
  return false;
}

void launchWeb(int webtype) {
  createWebServer(webtype);
  //server.begin();
}

void timedisplay(){
  display.begin(SSD1306_SWITCHCAPVCC, 0x78>>1);

  // Clear the buffer.  
  display.clearDisplay();
  pinMode(ledPin,OUTPUT);
  digitalWrite(ledPin,LOW);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  display.setCursor(0,0);
  // Clear the buffer.
  display.clearDisplay();
  display.display();
  display.setCursor(0,0);
  configTime(timezone, dst, "pool.ntp.org","time.nist.gov");
  while(!time(nullptr)){
     Serial.print("*");     
     delay(1000);
  }
  display.display(); 
  display.println("");
  display.print(WiFi.localIP() );
  display.display(); 
  delay(1000);
}


void createWebServer(int webtype)
{

  if ( webtype == 0 ) {
    server.on("/", []() {
      String stip;
      for (int i = 0; i < 13; ++i)
      {
        stip += char(EEPROM.read(i));
      }
     content = "<!DOCTYPE HTML>\r\n<html>WIFI Mode<br>";
      content += "</p><form method ='get' action = 'setting'><label>SSID: ";
      content += "</label><br><input name='ssid' length=32><br><br>";
      content += "<label>Passphrase: </label><br><input name='passphrase'";
      content += "length=32><br><br><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);
    });
    server.on("/setting", []() {
      String stip = server.arg("ip");
      if (stip.length() > 0 ) {
        for (int i = 0; i < 13; ++i) {
          EEPROM.write(i, 0); //clearing
        }
        EEPROM.commit();

        for (int i = 0; i < 13 ; ++i)
        {
          EEPROM.write(i, stip[i]);
        }
        EEPROM.commit();

        content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
        statusCode = 200;
      } else {
        content = "{\"Error\":\"404 not found\"}";
        statusCode = 404;
        Serial.println("Sending 404");
      }
      server.send(statusCode, "application/json", content);

    });

    server.on("/clear", []() { //x.x.x.x/setting?clear
      String qsid = server.arg("ssid");
      for (int i = 0; i < 13; ++i) { //192
        EEPROM.write(i, 0);
      }
      EEPROM.commit();
      content = "<!DOCTYPE HTML>\r\n<html>";
      content += "<p>EEPROM ERASED. RESET DEVICE</p></html>";
      server.send(200, "text/html", content);
    });
  }
  if (webtype == 1) {
    server.begin();
    server.on("/", []() {
      content = "<!DOCTYPE HTML>\r\n<html>AP Mode<br>";
      content += "</p><form method ='get' action = 'setting'><label>SSID: ";
      content += "</label><br><input name='ssid' length=32><br><br>";
      content += "<label>Passphrase: </label><br><input name='passphrase'";
      content += "length=32><br><br><input type='submit'></form>";
      content += "</html>";
      server.send(200, "text/html", content);

    });
    server.on("/setting", []() {
      ssid = server.arg("ssid");
      passphrase = server.arg("passphrase");
      writeData(ssid, passphrase);
      content = "Success.Please reboot to take effect";
      server.send(200, "text/html", content);

    });
  }
}

void showtime(){
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  
  
  Serial.print(p_tm->tm_mday);
  Serial.print("/");
  Serial.print(p_tm->tm_mon + 1);
  Serial.print("/");
  Serial.print(p_tm->tm_year + 1900);
  
 // Serial.print(" ");
  
  Serial.print(p_tm->tm_hour);
  Serial.print(":");
  Serial.print(p_tm->tm_min);
  Serial.print(":");
  Serial.println(p_tm->tm_sec);
  
  // Clear the buffer.
  display.clearDisplay();
 
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  display.setCursor(0,0);
  display.print(p_tm->tm_hour);
  display.print(":");
  if( p_tm->tm_min <10)
    display.print("0"); 
  display.print(p_tm->tm_min);
  display.setCursor(0,10);
  display.println(WiFi.localIP());
  display.display();
}

void loop() {
  if (WiFi.status()== WL_CONNECTED) {
  
  int heartbeatvalue=analogRead(A0);
  display.setTextColor(WHITE);

  // calc bpm
 
  if(heartbeatvalue>UpperThreshold)
  {
    if(BeatComplete)
    {
      BPM=millis()-LastTime;
      BPM=int(60/(float(BPM)/1000));
      BPMTiming=false;
      BeatComplete=false;
    }
    if(BPMTiming==false)
    {
      LastTime=millis();
      BPMTiming=true;
    }
  }
  if((heartbeatvalue<LowerThreshold)&(BPMTiming))
    BeatComplete=true;
    // display bpm
  display.writeFillRect(0,50,128,16,BLACK);
  display.setCursor(60,0);
  display.print(BPM);
  display.print(" BPM");
  Serial.println(BPM);
  display.display();
  showtime();
  
 //  if (!client.connected()){
 //   reconnectMqtt();
 // }
 // client.loop();

  // operations for accelerometer
  check_imu();
  if(millis()>(pow(2,32)-5000)){
   ESP.reset();
  }

  //client.publish ("pubTopic/sensor/data", condition); 
  // publish accelerometer consition
}
    
    else{
        server.handleClient();
    }  
}
