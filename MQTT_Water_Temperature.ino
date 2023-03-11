#include <OneWire.h> 
#include <DallasTemperature.h>
#include <WiFi.h>
#include "arduino_secrets.h"
#include "PubSubClient.h"

//Må settes til rett GPIO
#define ONE_WIRE_BUS 15 // PIN 15 is GPIO 34, and digital input?
//#define ONE_WIRE 36
#define CLIENT_ID "Seawater"
#define PUBLISH_DELAY 3000 // that is 3 seconds interval
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds 
#define TIME_TO_SLEEP  300        // Time ESP32 will go to sleep (in seconds) 
#define NUMBER_OF_ITERATIONS_BEFORE_SLEEPMODE 1 //Number of readings to be done before starting to set the ESP in deep sleep mode, to reduce number of readings over time to conserve battery power

OneWire onewire(ONE_WIRE_BUS); 
//OneWire onewire(ONE_WIRE); 
DallasTemperature sensorTemperature(&onewire);

float temperatureReading = 0; 
char ip[16];
//char tempStringValue[10] = {' '};;
int signalStrength = 0;
RTC_DATA_ATTR bool sleepModeActive = false;
RTC_DATA_ATTR long iterations = 0;
int radio_Status = WL_IDLE_STATUS;  // WiFi radio's status
WiFiClient wifiClient;
PubSubClient mqttClient;
IPAddress ipaddress;

 void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  sensorTemperature.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("ESP32s_Seawater");

  // attempt to connect to WiFi network:
  while (radio_Status != WL_CONNECTED) {
    // Connect to WPA/WPA2 network:
    radio_Status = WiFi.begin(WIFI_SSID, WIFI_PWD);

    // wait 5 seconds for connection:
    delay(5000);
  }

  ipaddress = WiFi.localIP();
  //itoa (WiFi.localIP(), ipAddress, 10);

  // setup mqtt client
  mqttClient.setClient(wifiClient);
  mqttClient.setServer( "192.168.1.2", 1883);
}

void loop() {
  // put your main code here, to run repeatedly:
  onewire.reset();                
  temperatureReading = readTemperature();

  signalStrength = WiFi.RSSI();
  ipaddress = WiFi.localIP();
  sprintf(ip, "%d.%d.%d.%d", ipaddress[0], ipaddress[1], ipaddress[2], ipaddress[3]);
  
  transmitData_MQTT();
  mqttClient.loop();  
  signalStrength = 0; 
  
  if(iterations >= NUMBER_OF_ITERATIONS_BEFORE_SLEEPMODE){
    sleepModeActive = true; 
  }
  if (sleepModeActive){
    sleep();
    sleepModeActive = false; 
  }
}

void sleep() {
  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Going to sleep now");
  delay(1000);
  Serial.flush(); 
  esp_deep_sleep_start();
}

float readTemperature() {
  float average = 0;

  for (int i = 0; i < 10; i++){
    // make 10 readings to enable average calculation
    sensorTemperature.requestTemperatures();
    average += sensorTemperature.getTempCByIndex(0);
    delay(1000); //at 12 bit resolution the reading and conversion takes about 750ms
  }

  iterations++;
  return average / 10;
}

void transmitData_MQTT() {
  if (mqttClient.connect(CLIENT_ID, "preben", "passord")) {
    char temperature[7] = {' '} ;
        
    //itoa (temperatureReading, temperature, 10);   
    //dtostrf(temperatureReading, 7, 2, temperature);
    int tempvalue_int = (int) temperatureReading;
    float tempvalue_float = (abs(temperatureReading) - abs(tempvalue_int)) * 100000;
    int tempvalue_fra = (int)tempvalue_float;
    sprintf (temperature, "%d.%d", tempvalue_int, tempvalue_fra); //
    mqttClient.publish("Seawater/temperature", temperature);

    char strength[10] = {' '} ;
    itoa (signalStrength, strength, 10); 
    mqttClient.publish("Seawater/signalstrength", strength);
    mqttClient.publish("Seawater/ipaddress", ip);
    
    //mqttClient.publish("Seawater/temperature", temperature);
    //mqttClient.publish("Seawater/temperature", String(temperatureReading).c_str());
    mqttClient.disconnect();
  }
  else {
    Serial.println("mqtt could not be sent. No connection available.");
  }
}
