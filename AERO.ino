#define CUSTOM_SETTINGS
#define INCLUDE_GAMEPAD_MODULE
#include <Dabble.h>
#include <Servo.h> // Usamos la librería estándar para Arduino UNO

// ===================== ESQUEMA DE PINES (UNO) =====================
// Puente H / Driver de motores
constexpr uint8_t PIN_IN1 = 13;
constexpr uint8_t PIN_IN2 = 12;
constexpr uint8_t PIN_IN3 = 11;
constexpr uint8_t PIN_IN4 = 10;
constexpr uint8_t PIN_ENA = 5;  // PWM
constexpr uint8_t PIN_ENB = 6;  // PWM

// Pines para radar y alerta
constexpr uint8_t PIN_TRIG = A0;
constexpr uint8_t PIN_ECHO = A1;
constexpr uint8_t PIN_SERVO = 9;
constexpr uint8_t PIN_BUZZER = 7; // Añadido para emitir alertas

// ===================== VARIABLES Y ESTADO =====================
Servo sensorServo;
int velocidadBase = 180;  // Rango: 0 a 255
bool modoAutomatico = false;

// Ángulos de visión del Radar
constexpr int ANGULO_IZQ = 150;
constexpr int ANGULO_CEN = 90;
constexpr int ANGULO_DER = 30;

// Umbrales de decisión (Centímetros)
constexpr float DISTANCIA_PRECOLISION_CM = 40.0;
constexpr float DISTANCIA_DECISION_ALTA = 30.0;
constexpr float DISTANCIA_FAILSAFE_CM = 8.0;

// ===================== FUNCIONES MOTORES =====================
void setVelocidadMotores(int izq, int der) {
  analogWrite(PIN_ENA, constrain(izq, 0, 255));
  analogWrite(PIN_ENB, constrain(der, 0, 255));
}

void detenerMotores() {
  digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, LOW);
  setVelocidadMotores(0, 0);
}

void avanzar(int pwm) {
  digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, HIGH);
  setVelocidadMotores(pwm, pwm);
}

void retroceder(int pwm) {
  digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  setVelocidadMotores(pwm, pwm);
}

void girarIzquierda(int pwm) {
  digitalWrite(PIN_IN1, HIGH); digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW); digitalWrite(PIN_IN4, HIGH);
  setVelocidadMotores(pwm, pwm);
}

void girarDerecha(int pwm) {
  digitalWrite(PIN_IN1, LOW); digitalWrite(PIN_IN2, HIGH);
  digitalWrite(PIN_IN3, HIGH); digitalWrite(PIN_IN4, LOW);
  setVelocidadMotores(pwm, pwm);
}

// ===================== FUNCIONES DEL RADAR =====================
float medirDistanciaCM() {
  // Limpiar el pin TRIG
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  
  // Mandar un pulso sónico de 10 microsegundos
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Leer el tiempo que tarda en volver el eco
  unsigned long duracion = pulseIn(PIN_ECHO, HIGH, 25000UL);
  
  // Si no recibe nada, asume que está libre (999 cm)
  if (duracion == 0) return 999.0;
  
  // Cálculo matemático con velocidad de sonido de 343 m/s
  float cm = (duracion * 0.0343) / 2.0;
  
  // Alerta sonora si hay algo cerca
  if (cm < DISTANCIA_DECISION_ALTA) {
    tone(PIN_BUZZER, 1000, 30); 
  }
  return cm;
}

float medirEnAngulo(int angulo) {
  sensorServo.write(angulo);
  delay(250); // Dar tiempo físico al servo para girar
  return medirDistanciaCM();
}

char escogerDireccionLibre() {
  float dIzq = medirEnAngulo(ANGULO_IZQ);
  float dDer = medirEnAngulo(ANGULO_DER);
  sensorServo.write(ANGULO_CEN); // Regresar cabeza al centro
  return (dIzq >= dDer) ? 'L' : 'R'; // Elige el lado con más espacio
}

// ===================== NAVEGACIÓN AUTÓNOMA =====================
void navegarAutomatico() {
  float frente = medirEnAngulo(ANGULO_CEN);
  
  if (frente > DISTANCIA_PRECOLISION_CM) {
    avanzar(velocidadBase);
  } 
  else if (frente <= DISTANCIA_FAILSAFE_CM) {
    // Si está demasiado cerca, frenar de emergencia
    detenerMotores();
    tone(PIN_BUZZER, 500, 500); 
  } 
  else {
    // Maniobra de evasión
    detenerMotores();
    delay(200);
    retroceder(150);
    delay(400);
    detenerMotores();
    
    char decision = escogerDireccionLibre();
    if (decision == 'L') {
      girarIzquierda(170);
    } else {
      girarDerecha(170);
    }
    delay(500);
    detenerMotores();
  }
}

// ===================== CONTROL BLUETOOTH (DABBLE) =====================
void procesarGamePadDabble() {
  // Botón "Select" del control para alternar entre Manual y Automático
  if (GamePad.isSelectPressed()) {
    modoAutomatico = !modoAutomatico;
    detenerMotores();
    delay(500); // Pequeña pausa para no leer el botón múltiples veces
    return;
  }

  // Si estamos en modo autónomo, ignorar el joystick manual
  if (modoAutomatico) return;

  // Ajuste de velocidad con los botones
  if (GamePad.isTrianglePressed()) {
    velocidadBase = min(255, velocidadBase + 5);
  }
  if (GamePad.isCrossPressed()) {
    velocidadBase = max(90, velocidadBase - 5);
  }

  // Movimiento manual
  if (GamePad.isUpPressed()) {
    avanzar(velocidadBase);
  } else if (GamePad.isDownPressed()) {
    retroceder(velocidadBase);
  } else if (GamePad.isLeftPressed()) {
    girarIzquierda(velocidadBase);
  } else if (GamePad.isRightPressed()) {
    girarDerecha(velocidadBase);
  } else {
    detenerMotores();
  }
}

// ===================== CONFIGURACIÓN INICIAL =====================
void setup() {
  Serial.begin(9600);
  Dabble.begin(9600); // Inicia la comunicación con la App

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);
  
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  sensorServo.attach(PIN_SERVO);
  sensorServo.write(ANGULO_CEN); // Iniciar mirando al frente

  detenerMotores();
}

// ===================== BUCLE PRINCIPAL =====================
void loop() {
  Dabble.processInput(); // Actualizar datos recibidos de la App
  procesarGamePadDabble(); // Evaluar comandos de la App
  
  if (modoAutomatico) {
    navegarAutomatico(); // Cederle el control al Radar
  }
  
  delay(20);
}