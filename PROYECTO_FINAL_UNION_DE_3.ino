#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>
#include <SPI.h>
#include <SD.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define LED_PIN 2
#define BUZZER_PIN 25
#define SD_CS 5  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MAX30105 particleSensor;
MPU6050 mpu;

//  Variables BPM
long lastBeat = 0;
float beatsPerMinute = 0;
int beatNow = 0;

// Variables SD
File dataFile;
unsigned long lastSave = 0;
unsigned long tiempoSegundos = 0;
bool sdDisponible = false;

// tiempos de lectura
unsigned long lastOledUpdate = 0;
unsigned long lastBpmRead = 0;
const unsigned long oledInterval = 200; // actualizar OLED cada 200ms
const unsigned long bpmInterval = 20;   // leer BPM cada 20ms

// Variables MPU 
float angulo = 0;
bool cabezaInclinada = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

   if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED no encontrada");
    while(1);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  //  MAX30102 
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 no encontrado");
    while (1);
  }
  particleSensor.setup(100, 4, 2, 400, 411, 4096);
  particleSensor.setPulseAmplitudeRed(0x10);
  particleSensor.setPulseAmplitudeIR(0x15);

  //MPU6050 
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 no conectado");
    while(true);
  }

  // lector sd
  if (!SD.begin(SD_CS)) {
    Serial.println("Error inicializando SD");
    sdDisponible = false;
  } else {
    sdDisponible = true;
    // Crear archivo CSV 
    dataFile = SD.open("/datos.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Tiempo(s);BPM;Angulo;Estado");
      dataFile.close();
    }
  }
}

void loop() {
  unsigned long now = millis();

  // Lectura BPM 
  
  if(now - lastBpmRead >= bpmInterval){
    lastBpmRead = now;

    long irValue = particleSensor.getIR();
    if(irValue < 5000) beatNow = 0;
    else if(checkForBeat(irValue)){
      long delta = now - lastBeat;
      lastBeat = now;
      beatsPerMinute = 60 / (delta / 1000.0);
      if(beatsPerMinute < 200 && beatsPerMinute > 40) beatNow = (int)beatsPerMinute;
    }
  }

  // Lectura MPU
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  angulo = atan2((float)ay, (float)az) * 180.0 / PI;
  if (angulo < 0) angulo = -angulo;
  if (angulo > 90) angulo = 90;
  angulo = 90 - angulo;

  cabezaInclinada = (angulo < 45);
  digitalWrite(LED_PIN, cabezaInclinada ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, cabezaInclinada ? HIGH : LOW);

  //  Guardar SD
  if (sdDisponible && now - lastSave >= 5000){
    lastSave = now;
    tiempoSegundos += 5;

    String estado = cabezaInclinada ? "INCLINADA" : "NORMAL";

    dataFile = SD.open("/datos.csv", FILE_APPEND);
    if(dataFile){
      dataFile.print(tiempoSegundos); dataFile.print(";");
      dataFile.print(beatNow);       dataFile.print(";");
      dataFile.print(angulo,1);      dataFile.print(";");
      dataFile.println(estado);
      dataFile.close();
    }
  }

  //  Actualizar OLED daros
  if(now - lastOledUpdate >= oledInterval){
    lastOledUpdate = now;

    long irValue = particleSensor.getIR();

    display.clearDisplay();

    // fecha + SD OK o NO si no deecta
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("20-02-26   ");
    if(sdDisponible) display.println("REC");
    else display.println("NO");

    // Datos de pulso y ángulo
    display.setCursor(0,15);
    if(irValue < 5000)
      display.println("SIN PULSO");
    else
      display.println("PULSO DETECTADO");

    display.print("IR: "); display.println(irValue);
    display.print("BPM: "); display.println(beatNow);
    display.print("Ang: "); display.print(angulo,1); display.println(" grados");

    if(cabezaInclinada){
      display.setCursor(0, 54);
      display.println("ALERTA");
    }

    display.display();
  }
}
