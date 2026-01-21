/*
 * Proje: ESP32 Hareket Takip Sistemi
 * Yazar: [Enver Barış Bektaş]
 * Okul: Kocaeli Üniversitesi
 * Tarih: Ocak 2026
 */

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <WebServer.h>

//wifi
const char* ssid = "WIFI_ADINIZI_GIRIN";
const char* password = "WIFI_SIFRENIZI_GIRIN";

//web sunucu
WebServer server(80);

Adafruit_MPU6050 mpu;

//sensör ayarları
const int numSamples = 40;      
const int sampleDelay = 15;     
float vSamples[numSamples]; 

//karar kısmı
String anlikKarar = "";          
String kesinlesmisKarar = "BEKLENIYOR"; 

//sayac
int degisimSayaci = 0;           
const int KARAR_LIMITI = 3;      

// inis sayacı zıplamadan sonra hemen yürüme atmaması için araya
int inisSayaci = 0; 

//web sayfası
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Hareket Takibi</title>
  <style>
    body { font-family: Arial; text-align: center; background-color: #f2f2f2; margin-top: 50px;}
    h1 { color: #333; }
    #durum { 
      font-size: 80px; 
      font-weight: bold; 
      color: #0066cc; 
      margin-top: 20px;
      padding: 20px;
      border: 5px solid #333;
      display: inline-block;
      background-color: #fff;
      border-radius: 10px;
    }
  </style>
</head>
<body>
  <h1>Anlik Hareket Durumu</h1>
  <div id="durum">YUKLENIYOR...</div>
  
  <script>
    setInterval(function() {
      var xhttp = new XMLHttpRequest();
      xhttp.onreadystatechange = function() {
        if (this.readyState == 4 && this.status == 200) {
          document.getElementById("durum").innerHTML = this.responseText;
        }
      };
      xhttp.open("GET", "/durum", true);
      xhttp.send();
    }, 1000); 
  </script>
</body>
</html>)rawliteral";

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleDurum() {
  server.send(200, "text/plain", kesinlesmisKarar);
}

void setup(void) {
  Serial.begin(115200);
  while (!Serial) delay(10); 

  if (!mpu.begin()) {
    Serial.println("MPU6050 bulunamadi!");
    while (1) { delay(10); }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.print("WiFi baglaniyor: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi Baglandi! IP Adresi: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);      
  server.on("/durum", handleDurum); 
  server.begin();
  Serial.println("Web Sunucusu Baslatildi.");
}

void loop() {
  server.handleClient();

  //veri toplama
  float toplam = 0;
  for (int i = 0; i < numSamples; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float magnitude = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
    vSamples[i] = magnitude;
    toplam += magnitude;
    delay(sampleDelay); 
  }

  float ortalama = toplam / numSamples;

  float varyansToplami = 0;
  for (int i = 0; i < numSamples; i++) {
    varyansToplami += pow(vSamples[i] - ortalama, 2);
  }
  float stdSapma = sqrt(varyansToplami / numSamples);

  //karar ağacı
  if (stdSapma < 1.75) {
      if (ortalama < 9.35) {
          anlikKarar = "AYAKTA";
      } else {
          anlikKarar = "OTURMA";
      }
  } 
  else {
      
      // Zıplama Tespiti
      if (stdSapma > 2.0 && ortalama < 10.5 && ortalama > 10.0) {
          anlikKarar = "ZIPLAMA";
      }
      else if (ortalama < 12.60) {
          anlikKarar = "YURUME";
      } else {
          anlikKarar = "KOSMA";
      }
  }
  
  //zıplama sonrası ayarlama
  if (anlikKarar == "ZIPLAMA") {
      inisSayaci = 3; 
  }

  if (inisSayaci > 0) {
      inisSayaci--; 
      
      //zıplama sonrası araya yanlıslıkla yürüme atarsa ayaktaya çevir
      if (anlikKarar == "YURUME") {
          anlikKarar = "AYAKTA";
      }
  }


  //filtre 
  
  if (anlikKarar == "ZIPLAMA") {
      kesinlesmisKarar = anlikKarar;
      degisimSayaci = 0; 
  }
  else if (kesinlesmisKarar == "ZIPLAMA") {
      kesinlesmisKarar = anlikKarar; 
      degisimSayaci = 0;
  }
  else if (anlikKarar == kesinlesmisKarar) {
      degisimSayaci = 0;
  } 
  else {
      degisimSayaci++;
      if (degisimSayaci >= KARAR_LIMITI) {
          kesinlesmisKarar = anlikKarar; 
          degisimSayaci = 0;            
      }
  }

  Serial.print("Ort: "); Serial.print(ortalama);
  Serial.print(" | Std: "); Serial.print(stdSapma);
  Serial.print(" -> DURUM: ");
  Serial.println(kesinlesmisKarar); 

  Serial.println("-------------------------");
}