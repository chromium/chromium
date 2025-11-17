// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_TEST_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_TEST_UTILS_H_

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request_test_monitor.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_util.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

namespace ash::geolocation::test_utils {

inline constexpr base::TimeDelta kRequestRetryIntervalMilliSeconds =
    base::Milliseconds(200);

// This should be different from default to prevent SimpleGeolocationRequest
// from modifying it.
inline constexpr char kTestGeolocationProviderUrl[] =
    "https://localhost/geolocation/v1/geolocate?";

inline constexpr char kSimpleResponseBody[] =
    "{\n"
    "  \"location\": {\n"
    "    \"lat\": 51.0,\n"
    "    \"lng\": -0.1\n"
    "  },\n"
    "  \"accuracy\": 1200.4\n"
    "}";

inline constexpr char kIPOnlyRequestBody[] = "{\"considerIp\": \"true\"}";
inline constexpr char kOneWiFiAPRequestBody[] =
    "{"
    "\"considerIp\":true,"
    "\"wifiAccessPoints\":["
    "{"
    "\"channel\":1,"
    "\"macAddress\":\"01:00:00:00:00:00\","
    "\"signalStrength\":10,"
    "\"signalToNoiseRatio\":0"
    "}"
    "]"
    "}";
inline constexpr char kOneCellTowerRequestBody[] =
    "{"
    "\"cellTowers\":["
    "{"
    "\"cellId\":\"1\","
    "\"locationAreaCode\":\"3\","
    "\"mobileCountryCode\":\"100\","
    "\"mobileNetworkCode\":\"101\""
    "}"
    "],"
    "\"considerIp\":true"
    "}";
inline constexpr char kCellTowerAndWifiAPRequestBody[] =
    "{"
    "\"cellTowers\":["
    "{"
    "\"cellId\":\"1\","
    "\"locationAreaCode\":\"3\","
    "\"mobileCountryCode\":\"100\","
    "\"mobileNetworkCode\":\"101\""
    "}"
    "],"
    "\"considerIp\":true,"
    "\"wifiAccessPoints\":["
    "{"
    "\"channel\":1,"
    "\"macAddress\":\"01:00:00:00:00:00\","
    "\"signalStrength\":10,"
    "\"signalToNoiseRatio\":0"
    "}"
    "]"
    "}";
inline constexpr char kExpectedPosition[] =
    "latitude=51.000000, longitude=-0.100000, accuracy=1200.400000, "
    "error_code=0, error_message='', status=1 (OK)";

inline constexpr char kWiFiAP1MacAddress[] = "01:00:00:00:00:00";
inline constexpr char kCellTower1MNC[] = "101";

// This implements fake Google MAPS Geolocation API remote endpoint.
class TestGeolocationAPILoaderFactory : public network::TestURLLoaderFactory {
 public:
  TestGeolocationAPILoaderFactory(const GURL& url,
                                  const net::HttpStatusCode http_status,
                                  const std::string& response,
                                  const size_t require_retries);

  ~TestGeolocationAPILoaderFactory() override;

  TestGeolocationAPILoaderFactory(const TestGeolocationAPILoaderFactory&) =
      delete;
  TestGeolocationAPILoaderFactory& operator=(
      const TestGeolocationAPILoaderFactory&) = delete;

  void Configure(const GURL& url,
                 const net::HttpStatusCode http_status,
                 const std::string& response,
                 const size_t require_retries);

  void Intercept(const network::ResourceRequest& request);

  size_t attempts() const { return attempts_; }

 private:
  GURL url_;
  net::HttpStatusCode http_status_;
  std::string response_;
  size_t require_retries_;
  size_t attempts_ = 0;
  WifiAccessPointVector wifi_scan_vector_;
  CellTowerVector cell_scan_vector;
};

// This class is single-use and can only process one request per instance.
// A new instance must be created for every subsequent request.
class GeolocationReceiver {
 public:
  GeolocationReceiver();
  ~GeolocationReceiver();

  void OnRequestDone(const Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed);

  void WaitUntilRequestDone();

  const Geoposition& position() const { return position_; }
  bool server_error() const { return server_error_; }
  base::TimeDelta elapsed() const { return elapsed_; }

 private:
  Geoposition position_;
  bool server_error_;
  base::TimeDelta elapsed_;
  base::RunLoop message_loop_runner_;
};

class RequestTestMonitor : public SimpleGeolocationRequestTestMonitor {
 public:
  RequestTestMonitor() = default;

  RequestTestMonitor(const RequestTestMonitor&) = delete;
  RequestTestMonitor& operator=(const RequestTestMonitor&) = delete;

  void OnRequestCreated(SimpleGeolocationRequest* request) override;
  void OnStart(SimpleGeolocationRequest* request) override;

  const std::string& last_request_body() const { return last_request_body_; }
  GURL last_request_url() const { return last_request_url_; }
  unsigned int requests_count() const { return requests_count_; }

 private:
  std::string last_request_body_;
  GURL last_request_url_;
  unsigned int requests_count_ = 0;
};

// Fake implementation of `GeolocationHandler`.
// Returns `wifi_aps_` and `cell_towers_` members as available network signals.
class FakeNetworkDelegate : public GeolocationHandler {
 public:
  FakeNetworkDelegate();
  ~FakeNetworkDelegate() override;

  // GeolocationHandler:
  bool GetNetworkInformation(WifiAccessPointVector* access_points,
                             CellTowerVector* cell_towers) override;
  bool wifi_enabled() const override;
  bool GetWifiAccessPoints(WifiAccessPointVector* access_points,
                           int64_t* age_ms) override;

  // Add single Wifi scan.
  void AddWifiAP();
  // Add single Cell Tower scan.
  void AddCellTower();

 private:
  WifiAccessPointVector wifi_aps_;
  CellTowerVector cell_towers_;
};

}  // namespace ash::geolocation::test_utils

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_TEST_UTILS_H_
