#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ================= WiFi Config =================
const char* ssid = "naim's phone";      // <-- replace with your WiFi SSID
const char* password = "12341234";  // <-- replace with your WiFi password

WebServer server(80);

// ================= Motor Control Pins =================
#define ENA 18
#define IN1 19
#define IN2 21
#define IN3 22
#define IN4 23
#define ENB 5

// ================= Flame Sensor Pins =================
#define FLAME_FRONT 35
#define FLAME_LEFT  34
#define FLAME_RIGHT 33

// ================= Water Pump Pin =================
#define WATER_PUMP 26

//===================
// ================= Servo Pin =================
#define SERVO_PIN 12
Servo waterServo;

// ================= Mode and Control Vars =================
bool autoMode = true;
bool pumpOn = false;
int motorSpeed = 60; // default manual speed (range 80–255)
int manualServoAngle = 90;

// ================= Thresholds =================
#define FLAME_THRESHOLD 3200
#define FLAME_PUMP_THRESHOLD 117
#define FLAME_SAFE_DISTANCE 100

// ================= Servo Sweep (Auto Mode) =================
const int SERVO_MIN = 10;
const int SERVO_MAX = 170;
int servoAngle = SERVO_MIN;
int step = 10;
unsigned long lastServoMove = 0;
const int SERVO_INTERVAL = 8;

// ================= Motor Functions =================
void moveForward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void moveBackward() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void turnLeft() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
}

void turnRight() {
  analogWrite(ENA, motorSpeed);
  analogWrite(ENB, motorSpeed);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
}

void stopMotors() {
  analogWrite(ENA, 0); analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// ================= Servo Sweep =================
void updateServo() {
  if (!pumpOn) return;
  unsigned long now = millis();
  if (now - lastServoMove >= SERVO_INTERVAL) {
    lastServoMove = now;
    waterServo.write(servoAngle);
    servoAngle += step;
    if (servoAngle >= SERVO_MAX || servoAngle <= SERVO_MIN) step = -step;
  }
}

// ================= HTML Page =================
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FF Robot</title>
<style>
:root{--bg:#0f1116;--card:#1b1f2a;--face:#2a3142;--text:#e9eefb;--mut:#b9c2d8;--accent:#5aa2ff;--ok:#26d07c;--warn:#ff7a59;--red:#ff4d4d}
*{box-sizing:border-box}
body{font-family:system-ui,-apple-system,"Segoe UI",Roboto;background:var(--bg);color:var(--text);margin:18px}
.card{background:var(--card);border-radius:16px;padding:14px;margin-bottom:14px;box-shadow:0 10px 28px rgba(0,0,0,.25)}
.title{font-weight:800;font-size:20px;margin-bottom:8px}
.pill{display:inline-block;background:#2a3142;padding:8px 12px;border-radius:999px;margin-right:8px}
.mode-on{background:var(--ok);color:#032b14;border:none;padding:8px 12px;border-radius:10px}
.mode-off{background:#6c757d;color:#fff;border:none;padding:8px 12px;border-radius:10px}
.grid{display:grid;grid-template-columns:repeat(3,110px);gap:10px;justify-content:center}
.btn{background:#6d768c;border:none;border-radius:14px;height:60px;font-size:20px;color:#fff}
.stop{background:#c44}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}
label{color:var(--mut)}
input[type=range]{width:100%}
.status{font-family:ui-monospace,Consolas,monospace;color:var(--mut)}
.fire-no{color:#e9eefb}
.fire-yes{color:var(--red);font-weight:800}
@media(max-width:520px){.grid{grid-template-columns:repeat(3,90px)} .btn{height:56px}}
</style></head>
<body>
<div class="title">Firefighting Robot — Web Control</div>

<div class="card">
  <div class="row">
    <button id="modeBtn" class="mode-on" onclick="toggleMode()">Mode: Auto</button>
    <span class="pill" id="ip">IP: -</span>
  </div>
</div>

<div class="card">
  <div class="title">Manual Drive</div>
  <div class="grid">
    <span></span>
    <button class="btn" ontouchstart="cmd('F')" onmousedown="cmd('F')" onmouseup="cmd('S')" ontouchend="cmd('S')">↑</button>
    <span></span>
    <button class="btn" ontouchstart="cmd('L')" onmousedown="cmd('L')" onmouseup="cmd('S')" ontouchend="cmd('S')">←</button>
    <button class="btn stop" onclick="cmd('S')">■</button>
    <button class="btn" ontouchstart="cmd('R')" onmousedown="cmd('R')" onmouseup="cmd('S')" ontouchend="cmd('S')">→</button>
    <span></span>
    <button class="btn" ontouchstart="cmd('B')" onmousedown="cmd('B')" onmouseup="cmd('S')" ontouchend="cmd('S')">↓</button>
    <span></span>
  </div>

  <div style="margin-top:10px">
    <label>Speed: <b id="spv">80</b></label>
    <input type="range" id="speed" min="70" max="255" value="80" oninput="setSpeed(this.value)">
  </div>

  <div class="row" style="margin-top:10px">
    <button class="pill" onclick="pump(1)">Pump ON</button>
    <button class="pill" onclick="pump(0)">Pump OFF</button>
  </div>
  
  <div style="margin-top:10px">
    <label>Servo Angle: <b id="angv">90</b>°</label>
    <input type="range" id="angle" min="45" max="135" value="90" oninput="setServo(this.value)">
  </div>
</div>

<div class="card status">
  <div class="title">Live Status</div>
  <div>Mode: <b id="am">true</b></div>
  <div>Flame L/M/R: <b id="fl">- / - / -</b></div>
  <div>🔥 Fire Detected: <b id="fire" class="fire-no">NO</b></div>
</div>

<script>
function cmd(c){ fetch('/cmd?act='+c); }
function setSpeed(v){ document.getElementById('spv').innerText=v; fetch('/speed?val='+v); }
function pump(on){ fetch('/pump?on='+on); }
function setServo(v){ document.getElementById('angv').innerText=v; fetch('/servo?angle='+v); }
function toggleMode(){ fetch('/mode').then(r=>r.json()).then(j=>setModeBtn(j.auto)); }
function setModeBtn(isAuto){
  const btn=document.getElementById('modeBtn');
  btn.textContent='Mode: '+(isAuto?'Auto':'Manual');
  btn.className=isAuto?'mode-on':'mode-off';
}
function applyFireText(isFire){
  const el=document.getElementById('fire');
  el.textContent = isFire ? 'YES' : 'NO';
  el.className = isFire ? 'fire-yes' : 'fire-no';
}
setInterval(()=>{
  fetch('/status').then(r=>r.json()).then(j=>{
    document.getElementById('am').innerText=j.auto;
    document.getElementById('fl').innerText=j.flL+' / '+j.flM+' / '+j.flR;
    document.getElementById('ip').innerText='IP: '+j.ip;
    setModeBtn(j.auto);
    applyFireText(j.fire);
  });
},400);
</script>
</body></html>
)HTML";

// ================= Web Handlers =================
void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleCmd() {
  if (autoMode) { server.send(200, "text/plain", "Auto mode active"); return; }
  String act = server.arg("act");
  if (act == "F") moveForward();
  else if (act == "B") moveBackward();
  else if (act == "L") turnLeft();
  else if (act == "R") turnRight();
  else if (act == "S") stopMotors();
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  if (autoMode) { server.send(200, "text/plain", "Auto mode active"); return; }
  motorSpeed = server.arg("val").toInt();
  server.send(200, "text/plain", "Speed set");
}

void handlePump() {
  if (autoMode) { server.send(200, "text/plain", "Auto mode active"); return; }
  int on = server.arg("on").toInt();
  pumpOn = (on == 1);
  digitalWrite(WATER_PUMP, pumpOn ? HIGH : LOW);
  server.send(200, "text/plain", pumpOn ? "Pump ON" : "Pump OFF");
}

void handleServo() {
  if (autoMode) { server.send(200, "text/plain", "Auto mode active"); return; }
  manualServoAngle = server.arg("angle").toInt();
  waterServo.write(manualServoAngle);
  server.send(200, "text/plain", "Servo set");
}

void handleMode() {
  autoMode = !autoMode;
  server.send(200, "application/json", "{\"auto\":" + String(autoMode ? "true" : "false") + "}");
}

void handleStatus() {
  int fL = analogRead(FLAME_LEFT);
  int fM = analogRead(FLAME_FRONT);
  int fR = analogRead(FLAME_RIGHT);
  bool fire = (fL < FLAME_THRESHOLD || fM < FLAME_THRESHOLD || fR < FLAME_THRESHOLD);

  String json = "{";
  json += "\"auto\":" + String(autoMode ? "true" : "false") + ",";
  json += "\"flL\":" + String(fL) + ",";
  json += "\"flM\":" + String(fM) + ",";
  json += "\"flR\":" + String(fR) + ",";
  json += "\"fire\":" + String(fire ? "true" : "false") + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ================= WiFi Setup =================
void setupWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // Flame sensors
  pinMode(FLAME_FRONT, INPUT);
  pinMode(FLAME_LEFT, INPUT);
  pinMode(FLAME_RIGHT, INPUT);

  // Water pump
  pinMode(WATER_PUMP, OUTPUT);
  digitalWrite(WATER_PUMP, LOW);

  // Servo
  waterServo.setPeriodHertz(5);
  waterServo.attach(SERVO_PIN, 500, 2400);

  // Connect WiFi
  setupWiFi();

  // Routes
  server.on("/", handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/speed", handleSpeed);
  server.on("/pump", handlePump);
  server.on("/servo", handleServo);
  server.on("/mode", handleMode);
  server.on("/status", handleStatus);
  server.begin();
}

// ================= Loop =================
void loop() {
  server.handleClient();

  if (autoMode) {
    int flameFront = analogRead(FLAME_FRONT);
    int flameLeft  = analogRead(FLAME_LEFT);
    int flameRight = analogRead(FLAME_RIGHT);

    bool fireDetected = (flameFront < FLAME_THRESHOLD || 
                         flameLeft  < FLAME_THRESHOLD || 
                         flameRight < FLAME_THRESHOLD);

    if (fireDetected) {
      // --- Safe distance braking system ---
      if (flameFront < FLAME_SAFE_DISTANCE || 
          flameLeft  < FLAME_SAFE_DISTANCE || 
          flameRight < FLAME_SAFE_DISTANCE) {
        // 🚨 Close enough, stop and extinguish
        stopMotors();
        digitalWrite(WATER_PUMP, HIGH);
        pumpOn = true;
        updateServo();  // spray left-right
      } else {
        // 🚗 Move towards fire (not too close yet)
        if (flameFront < FLAME_THRESHOLD) moveForward();
        else if (flameLeft < FLAME_THRESHOLD) turnLeft();
        else if (flameRight < FLAME_THRESHOLD) turnRight();
        else stopMotors();

        // Pump off until we reach safe distance
        digitalWrite(WATER_PUMP, LOW);
        pumpOn = false;
      }
    } else {
      // No fire detected
      stopMotors();
      digitalWrite(WATER_PUMP, LOW);
      pumpOn = false;
    }
  }
}
