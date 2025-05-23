/*********
  Rui Santos
  Complete instructions at https://RandomNerdTutorials.com/esp32-ble-server-client/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#include "BLEDevice.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// Default Temperature is in Celsius
// Comment the next line for Temperature in Fahrenheit
#define temperatureCelsius

// BLE Server name (the other ESP32 name running the server sketch)
#define bleServerName "11_server"

/* UUID's of the service, characteristic that we want to read*/
// BLE Service
static BLEUUID bmeServiceUUID("91bad492-b950-4226-aa2b-4ede9fa42f59");

// BLE Characteristics
#ifdef temperatureCelsius
  // Temperature Celsius Characteristic
  static BLEUUID temperatureCharacteristicUUID("cba1d466-344c-4be3-ab3f-189f80dd7518");
#else
  // Temperature Fahrenheit Characteristic
  static BLEUUID temperatureCharacteristicUUID("f78ebbff-c8b7-4107-93de-889a6a06d408");
#endif

// Humidity Characteristic
static BLEUUID humidityCharacteristicUUID("ca73b3ba-39f6-4ab3-91ae-186dc9577d99");

// Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;

// Address of the peripheral device. Address will be found during scanning...
static BLEAddress *pServerAddress;
 
// Characteristics that we want to read
static BLERemoteCharacteristic* temperatureCharacteristic;
static BLERemoteCharacteristic* humidityCharacteristic;

// Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Variables to store temperature and humidity as proper strings
char tempString[10]; 
char humString[10];

// Variables to store temperature and humidity as floating-point values
float temperature = 0.0;
float humidity = 0.0;

// Flags to check whether new temperature and humidity readings are available
boolean newTemperature = false;
boolean newHumidity = false;

// Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress) {
   BLEClient* pClient = BLEDevice::createClient();
 
  // Connect to the remote BLE Server.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(bmeServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bmeServiceUUID.toString().c_str());
    return (false);
  }
 
  // Obtain a reference to the characteristics in the service of the remote BLE server.
  temperatureCharacteristic = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);
  humidityCharacteristic = pRemoteService->getCharacteristic(humidityCharacteristicUUID);

  if (temperatureCharacteristic == nullptr || humidityCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  Serial.println(" - Found our characteristics");
 
  // Assign callback functions for the Characteristics
  temperatureCharacteristic->registerForNotify(temperatureNotifyCallback);
  humidityCharacteristic->registerForNotify(humidityNotifyCallback);
  return true;
}

// Callback function that gets called when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) { // Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop(); // Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); // Address of advertiser is the one we need
      doConnect = true; // Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
    }
  }
};
 
// When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                     uint8_t* pData, size_t length, bool isNotify) {
  // Safe handling of received data
  if (length < sizeof(tempString)) {
    // Copy data to our buffer
    memcpy(tempString, pData, length);
    tempString[length] = '\0'; // Ensure null termination
    
    // Clean up the string - only keep digits, decimal point and sign
    int j = 0;
    for (int i = 0; i < length; i++) {
      if (isdigit(tempString[i]) || tempString[i] == '.' || tempString[i] == '-' || tempString[i] == '+') {
        tempString[j++] = tempString[i];
      }
    }
    tempString[j] = '\0'; // Re-terminate after filtering
    
    // Convert to float for potential calculations
    temperature = atof(tempString);
    
    // Debug output
    Serial.print("Raw temperature data received. Length: ");
    Serial.print(length);
    Serial.print(", Value: ");
    Serial.println(tempString);
    
    newTemperature = true;
  } else {
    Serial.println("Temperature data too long for buffer");
  }
}

// When the BLE Server sends a new humidity reading with the notify property
static void humidityNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                                  uint8_t* pData, size_t length, bool isNotify) {
  // Safe handling of received data
  if (length < sizeof(humString)) {
    // Copy data to our buffer
    memcpy(humString, pData, length);
    humString[length] = '\0'; // Ensure null termination
    
    // Clean up the string - only keep digits, decimal point and sign
    int j = 0;
    for (int i = 0; i < length; i++) {
      if (isdigit(humString[i]) || humString[i] == '.' || humString[i] == '-' || humString[i] == '+') {
        humString[j++] = humString[i];
      }
    }
    humString[j] = '\0'; // Re-terminate after filtering
    
    // Convert to float for potential calculations
    humidity = atof(humString);
    
    // Debug output
    Serial.print("Raw humidity data received. Length: ");
    Serial.print(length);
    Serial.print(", Value: ");
    Serial.println(humString);
    
    newHumidity = true;
  } else {
    Serial.println("Humidity data too long for buffer");
  }
}

// Function that prints the latest sensor readings in the OLED display
void printReadings() {
  display.clearDisplay();  
  
  // Display temperature
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(temperature, 2); // Display as float with 2 decimal places
  display.setTextSize(1);
  display.cp437(true);
  display.write(167); // Degree symbol
  display.setTextSize(2);
  
  Serial.print("Temperature: ");
  Serial.print(temperature, 2);
  
  #ifdef temperatureCelsius
    // Temperature Celsius
    display.print("C");
    Serial.print("C");
  #else
    // Temperature Fahrenheit
    display.print("F");
    Serial.print("F");
  #endif

  // Display humidity 
  display.setTextSize(1);
  display.setCursor(0, 35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(humidity, 2); // Display as float with 2 decimal places
  display.print("%");
  display.display();
  
  Serial.print(" Humidity: ");
  Serial.print(humidity, 2); 
  Serial.println("%");
}

void setup() {
  // OLED display setup
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Initial display content
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE,0);
  display.setCursor(0, 10);
  display.print("Team 11");
  display.setCursor(0, 35);
  display.print("Client");
  display.display();
  
  // Start serial communication
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

  // Init BLE device
  BLEDevice::init("");
 
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device. Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect. Now we connect to it. Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      // Activate the Notify property of each Characteristic
      temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      humidityCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
    }
    doConnect = false;
  }
  
  // If new temperature readings are available, print in the OLED
  if (newTemperature && newHumidity){
    newTemperature = false;
    newHumidity = false;
    printReadings();
  }
  
  delay(1000); // Delay a second between loops.
}