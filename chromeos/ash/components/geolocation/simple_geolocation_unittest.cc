// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>

#include "ash/constants/geolocation_access_level.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request_test_monitor.h"
#include "chromeos/ash/components/network/geolocation_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

constexpr int kRequestRetryIntervalMilliSeconds = 200;

// This should be different from default to prevent SimpleGeolocationRequest
// from modifying it.
constexpr char kTestGeolocationProviderUrl[] =
    "https://localhost/geolocation/v1/geolocate?";

constexpr char kSimpleResponseBody[] =
    "{\n"
    "  \"location\": {\n"
    "    \"lat\": 51.0,\n"
    "    \"lng\": -0.1\n"
    "  },\n"
    "  \"accuracy\": 1200.4\n"
    "}";
constexpr char kIPOnlyRequestBody[] = "{\"considerIp\": \"true\"}";
constexpr char kOneWiFiAPRequestBody[] =
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
constexpr char kOneCellTowerRequestBody[] =
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
constexpr char kExpectedPosition[] =
    "latitude=51.000000, longitude=-0.100000, accuracy=1200.400000, "
    "error_code=0, error_message='', status=1 (OK)";

constexpr char kWiFiAP1MacAddress[] = "01:00:00:00:00:00";
constexpr char kCellTower1MNC[] = "101";
}  // anonymous namespace

namespace ash {

// This implements fake Google MAPS Geolocation API remote endpoint.
class TestGeolocationAPILoaderFactory : public network::TestURLLoaderFactory {
 public:
  TestGeolocationAPILoaderFactory(const GURL& url,
                                  const net::HttpStatusCode http_status,
                                  const std::string& response,
                                  const size_t require_retries)
      : url_(url),
        http_status_(http_status),
        response_(response),
        require_retries_(require_retries) {
    SetInterceptor(base::BindRepeating(
        &TestGeolocationAPILoaderFactory::Intercept, base::Unretained(this)));
    // Default response. In most tests it is overridden by the subsequent calls
    // of `AddResponse()`.
    AddResponse(url_.spec(), std::string(), net::HTTP_INTERNAL_SERVER_ERROR);
  }

  TestGeolocationAPILoaderFactory(const TestGeolocationAPILoaderFactory&) =
      delete;
  TestGeolocationAPILoaderFactory& operator=(
      const TestGeolocationAPILoaderFactory&) = delete;

  void Configure(const GURL& url,
                 const net::HttpStatusCode http_status,
                 const std::string& response,
                 const size_t require_retries) {
    url_ = url;
    http_status_ = http_status;
    response_ = response;
    require_retries_ = require_retries;
  }

  void Intercept(const network::ResourceRequest& request) {
    EXPECT_EQ(url_, request.url);

    SimpleGeolocationProvider* provider =
        SimpleGeolocationProvider::GetInstance();
    EXPECT_NE(nullptr, provider);
    EXPECT_EQ(provider->requests_.size(), 1U);

    SimpleGeolocationRequest* geolocation_request =
        provider->requests_[0].get();

    const base::TimeDelta base_retry_interval =
        base::Milliseconds(kRequestRetryIntervalMilliSeconds);
    geolocation_request->set_retry_sleep_on_server_error_for_testing(
        base_retry_interval);
    geolocation_request->set_retry_sleep_on_bad_response_for_testing(
        base_retry_interval);

    if (++attempts_ > require_retries_)
      AddResponse(url_.spec(), response_, http_status_);
  }

  size_t attempts() const { return attempts_; }

 private:
  GURL url_;
  net::HttpStatusCode http_status_;
  std::string response_;
  size_t require_retries_;
  size_t attempts_ = 0;
};

class GeolocationReceiver {
 public:
  GeolocationReceiver() : server_error_(false) {}

  void OnRequestDone(const Geoposition& position,
                     bool server_error,
                     const base::TimeDelta elapsed) {
    position_ = position;
    server_error_ = server_error;
    elapsed_ = elapsed;

    message_loop_runner_->Quit();
  }

  void WaitUntilRequestDone() {
    message_loop_runner_ = std::make_unique<base::RunLoop>();
    message_loop_runner_->Run();
  }

  const Geoposition& position() const { return position_; }
  bool server_error() const { return server_error_; }
  base::TimeDelta elapsed() const { return elapsed_; }

 private:
  Geoposition position_;
  bool server_error_;
  base::TimeDelta elapsed_;
  std::unique_ptr<base::RunLoop> message_loop_runner_;
};

class WirelessTestMonitor : public SimpleGeolocationRequestTestMonitor {
 public:
  WirelessTestMonitor() = default;

  WirelessTestMonitor(const WirelessTestMonitor&) = delete;
  WirelessTestMonitor& operator=(const WirelessTestMonitor&) = delete;

  void OnRequestCreated(SimpleGeolocationRequest* request) override {}
  void OnStart(SimpleGeolocationRequest* request) override {
    last_request_body_ = request->FormatRequestBodyForTesting();
    ++requests_count_;
  }

  const std::string& last_request_body() const { return last_request_body_; }
  unsigned int requests_count() const { return requests_count_; }

 private:
  std::string last_request_body_;
  unsigned int requests_count_ = 0;
};

class SimpleGeolocationTestBase {
 public:
  SimpleGeolocationTestBase()
      : url_factory_(GURL(kTestGeolocationProviderUrl),
                     net::HTTP_OK,
                     kSimpleResponseBody,
                     0 /* require_retries */) {}

  ~SimpleGeolocationTestBase() = default;

  void EnableGeolocationUsage() {
    SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
        GeolocationAccessLevel::kAllowed);
  }

  void DisableGeolocatioUsage() {
    SimpleGeolocationProvider::GetInstance()->SetGeolocationAccessLevel(
        GeolocationAccessLevel::kDisallowed);
  }

 protected:
  void SetUp() {
    SimpleGeolocationProvider::Initialize(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_factory_));
    SimpleGeolocationProvider::GetInstance()
        ->SetGeolocationProviderUrlForTesting(kTestGeolocationProviderUrl);
  }

  void TearDown() { SimpleGeolocationProvider::DestroyForTesting(); }
  TestGeolocationAPILoaderFactory url_factory_;
};

class SimpleGeolocationTest : public SimpleGeolocationTestBase,
                              public testing::Test {
 protected:
  void SetUp() override { SimpleGeolocationTestBase::SetUp(); }

  void TearDown() override { SimpleGeolocationTestBase::TearDown(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(SimpleGeolocationTest, ResponseOK) {
  // Set user permission to granted.
  EnableGeolocationUsage();

  GeolocationReceiver receiver;
  SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(1), false, false,
      base::BindOnce(&GeolocationReceiver::OnRequestDone,
                     base::Unretained(&receiver)),
      SimpleGeolocationProvider::ClientId::kForTesting);
  receiver.WaitUntilRequestDone();

  EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(1U, url_factory_.attempts());
}

TEST_F(SimpleGeolocationTest, ResponseOKWithRetries) {
  url_factory_.Configure(GURL(kTestGeolocationProviderUrl), net::HTTP_OK,
                         kSimpleResponseBody, 3 /* require_retries */);

  // Set user permission to granted.
  EnableGeolocationUsage();

  GeolocationReceiver receiver;
  SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(1), false, false,
      base::BindOnce(&GeolocationReceiver::OnRequestDone,
                     base::Unretained(&receiver)),
      SimpleGeolocationProvider::ClientId::kForTesting);
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(4U, url_factory_.attempts());
}

TEST_F(SimpleGeolocationTest, ResponseWithErrorTooManyRequestsIsNotRetried) {
  url_factory_.Configure(GURL(kTestGeolocationProviderUrl),
                         net::HTTP_TOO_MANY_REQUESTS, kSimpleResponseBody,
                         0 /* require_retries */);

  // Set user permission to granted.
  EnableGeolocationUsage();

  GeolocationReceiver receiver;
  SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(1), false, false,
      base::BindOnce(&GeolocationReceiver::OnRequestDone,
                     base::Unretained(&receiver)),
      SimpleGeolocationProvider::ClientId::kForTesting);
  receiver.WaitUntilRequestDone();

  // Check that Geoposition is not populated.
  EXPECT_FALSE(receiver.position().Valid());
  EXPECT_EQ(Geoposition::Status::STATUS_SERVER_ERROR,
            receiver.position().status);
  // Check that the request was not retried.
  EXPECT_EQ(1U, url_factory_.attempts());
}

TEST_F(SimpleGeolocationTest, InvalidResponse) {
  url_factory_.Configure(GURL(kTestGeolocationProviderUrl), net::HTTP_OK,
                         "invalid JSON string", 0 /* require_retries */);
  EnableGeolocationUsage();

  const int timeout_seconds = 1;
  size_t expected_retries = static_cast<size_t>(
      timeout_seconds * 1000 / kRequestRetryIntervalMilliSeconds);
  ASSERT_GE(expected_retries, 2U);

  GeolocationReceiver receiver;
  SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(timeout_seconds), false, false,
      base::BindOnce(&GeolocationReceiver::OnRequestDone,
                     base::Unretained(&receiver)),
      SimpleGeolocationProvider::ClientId::kForTesting);
  receiver.WaitUntilRequestDone();

  std::string receiver_position = receiver.position().ToString();
  EXPECT_TRUE(base::Contains(
      receiver_position,
      "latitude=200.000000, longitude=200.000000, accuracy=-1.000000, "
      "error_code=0, error_message='SimpleGeolocation provider at "
      "'https://localhost/' : JSONReader failed:"));
  EXPECT_TRUE(base::Contains(receiver_position, "status=4 (TIMEOUT)"));
  EXPECT_TRUE(receiver.server_error());
  EXPECT_GE(url_factory_.attempts(), 2U);
  if (url_factory_.attempts() > expected_retries + 1) {
    LOG(WARNING)
        << "SimpleGeolocationTest::InvalidResponse: Too many attempts ("
        << url_factory_.attempts() << "), no more than " << expected_retries + 1
        << " expected.";
  }
  if (url_factory_.attempts() < expected_retries - 1) {
    LOG(WARNING)
        << "SimpleGeolocationTest::InvalidResponse: Too little attempts ("
        << url_factory_.attempts() << "), greater than " << expected_retries - 1
        << " expected.";
  }
}

TEST_F(SimpleGeolocationTest, NoWiFi) {
  NetworkHandlerTestHelper network_handler_test_helper;

  WirelessTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  url_factory_.Configure(GURL(kTestGeolocationProviderUrl), net::HTTP_OK,
                         kSimpleResponseBody, 0 /* require_retries */);
  EnableGeolocationUsage();

  GeolocationReceiver receiver;
  SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(1), true, false,
      base::BindOnce(&GeolocationReceiver::OnRequestDone,
                     base::Unretained(&receiver)),
      SimpleGeolocationProvider::ClientId::kForTesting);
  receiver.WaitUntilRequestDone();
  EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());

  EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
  EXPECT_FALSE(receiver.server_error());
  EXPECT_EQ(1U, url_factory_.attempts());
}

// Test SimpleGeolocationProvider when the system geolocation permission is
// denied. System shall not send out any geolocation request.
TEST_F(SimpleGeolocationTest, SystemGeolocationPermissionDenied) {
  NetworkHandlerTestHelper network_handler_test_helper;
  GeolocationReceiver receiver;
  WirelessTestMonitor requests_monitor;

  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);
  url_factory_.Configure(GURL(kTestGeolocationProviderUrl), net::HTTP_OK,
                         kSimpleResponseBody, 0);

  // Set system geolocation permission to disabled.
  DisableGeolocatioUsage();

  // Test for every request type.
  for (bool send_wifi : {false, true}) {
    for (bool send_cell : {false, true}) {
      SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
          base::Seconds(1), send_wifi, send_cell,
          base::BindOnce(&GeolocationReceiver::OnRequestDone,
                         base::Unretained(&receiver)),
          SimpleGeolocationProvider::ClientId::kForTesting);

      // Waiting is not needed, requests are dropped, thus nothing is pending.
      EXPECT_EQ(0U, requests_monitor.requests_count());
      EXPECT_EQ(std::string(), requests_monitor.last_request_body());
      EXPECT_EQ(0U, url_factory_.attempts());
    }
  }
}

// Test sending of WiFi Access points and Cell Towers.
// (This is mostly derived from GeolocationHandlerTest.)
class SimpleGeolocationWirelessTest : public SimpleGeolocationTestBase,
                                      public ::testing::TestWithParam<bool> {
 public:
  SimpleGeolocationWirelessTest() : manager_test_(nullptr) {}

  SimpleGeolocationWirelessTest(const SimpleGeolocationWirelessTest&) = delete;
  SimpleGeolocationWirelessTest& operator=(
      const SimpleGeolocationWirelessTest&) = delete;

  ~SimpleGeolocationWirelessTest() override = default;

  void SetUp() override {
    // Get the test interface for manager / device.
    manager_test_ = ShillManagerClient::Get()->GetTestInterface();
    ASSERT_TRUE(manager_test_);
    geolocation_handler_.reset(new GeolocationHandler());
    geolocation_handler_->Init();
    SimpleGeolocationTestBase::SetUp();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    SimpleGeolocationTestBase::TearDown();
    geolocation_handler_.reset();
  }

  bool GetWifiAccessPoints() {
    return geolocation_handler_->GetWifiAccessPoints(&wifi_access_points_,
                                                     nullptr);
  }

  bool GetCellTowers() {
    return geolocation_handler_->GetNetworkInformation(nullptr, &cell_towers_);
  }

  // This should remain in sync with the format of shill (chromeos) dict entries
  void AddAccessPoint(int idx) {
    base::Value::Dict properties;
    std::string mac_address =
        base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X", idx, 0, 0, 0, 0, 0);
    std::string channel = base::NumberToString(idx);
    std::string strength = base::NumberToString(idx * 10);
    properties.Set(shill::kGeoMacAddressProperty, mac_address);
    properties.Set(shill::kGeoChannelProperty, channel);
    properties.Set(shill::kGeoSignalStrengthProperty, strength);
    manager_test_->AddGeoNetwork(shill::kGeoWifiAccessPointsProperty,
                                 std::move(properties));
    base::RunLoop().RunUntilIdle();
  }

  // This should remain in sync with the format of shill (chromeos) dict entries
  void AddCellTower(int idx) {
    base::Value::Dict properties;
    std::string ci = base::NumberToString(idx);
    std::string lac = base::NumberToString(idx * 3);
    std::string mcc = base::NumberToString(idx * 100);
    std::string mnc = base::NumberToString(idx * 100 + 1);

    properties.Set(shill::kGeoCellIdProperty, ci);
    properties.Set(shill::kGeoLocationAreaCodeProperty, lac);
    properties.Set(shill::kGeoMobileCountryCodeProperty, mcc);
    properties.Set(shill::kGeoMobileNetworkCodeProperty, mnc);

    manager_test_->AddGeoNetwork(shill::kGeoCellTowersProperty,
                                 std::move(properties));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  NetworkHandlerTestHelper network_handler_test_helper_;
  std::unique_ptr<GeolocationHandler> geolocation_handler_;
  raw_ptr<ShillManagerClient::TestInterface> manager_test_;
  WifiAccessPointVector wifi_access_points_;
  CellTowerVector cell_towers_;
};

// Parameter - (bool) enable/disable sending of WiFi data.
TEST_P(SimpleGeolocationWirelessTest, WiFiExists) {
  bool send_wifi_access_points = GetParam();

  WirelessTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  url_factory_.Configure(GURL(kTestGeolocationProviderUrl), net::HTTP_OK,
                         kSimpleResponseBody, 0 /* require_retries */);
  // Set system geolocation permission to allowed. This permission is tested
  // separately.
  EnableGeolocationUsage();

  SimpleGeolocationProvider::GetInstance()->set_geolocation_handler(
      geolocation_handler_.get());
  {
    GeolocationReceiver receiver;
    SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), send_wifi_access_points, false,
        base::BindOnce(&GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SimpleGeolocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());

    EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    EXPECT_EQ(1U, url_factory_.attempts());
  }

  // Add cell and wifi to ensure only wifi is sent when cellular disabled.
  AddAccessPoint(1);
  AddCellTower(1);
  base::RunLoop().RunUntilIdle();
  // Initial call should return false and request access points.
  EXPECT_FALSE(GetWifiAccessPoints());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have an access point.
  EXPECT_TRUE(GetWifiAccessPoints());
  ASSERT_EQ(1u, wifi_access_points_.size());
  EXPECT_EQ(kWiFiAP1MacAddress, wifi_access_points_[0].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);

  {
    GeolocationReceiver receiver;
    SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), send_wifi_access_points, false,
        base::BindOnce(&GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SimpleGeolocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    if (send_wifi_access_points) {
      // Sending WiFi data is enabled.
      EXPECT_EQ(kOneWiFiAPRequestBody, requests_monitor.last_request_body());
    } else {
      // Sending WiFi data is disabled.
      EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());
    }

    EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    // This is total.
    EXPECT_EQ(2U, url_factory_.attempts());
  }
}

// Parameter - (bool) enable/disable sending of WiFi data.
TEST_P(SimpleGeolocationWirelessTest, CellularExists) {
  bool send_cell_towers = GetParam();

  WirelessTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  url_factory_.Configure(GURL(kTestGeolocationProviderUrl), net::HTTP_OK,
                         kSimpleResponseBody, 0 /* require_retries */);
  // Enable system permission for geolocation usage.
  EnableGeolocationUsage();

  SimpleGeolocationProvider::GetInstance()->set_geolocation_handler(
      geolocation_handler_.get());
  {
    GeolocationReceiver receiver;
    SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), false, send_cell_towers,
        base::BindOnce(&GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SimpleGeolocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());

    EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    EXPECT_EQ(1U, url_factory_.attempts());
  }

  AddCellTower(1);
  base::RunLoop().RunUntilIdle();
  // Initial call should return false and request cell towers.
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a tower.
  EXPECT_TRUE(GetCellTowers());
  ASSERT_EQ(1u, cell_towers_.size());
  EXPECT_EQ(kCellTower1MNC, cell_towers_[0].mnc);
  EXPECT_EQ(base::NumberToString(1), cell_towers_[0].ci);

  {
    GeolocationReceiver receiver;
    SimpleGeolocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), false, send_cell_towers,
        base::BindOnce(&GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SimpleGeolocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    if (send_cell_towers) {
      // Sending Cellular data is enabled.
      EXPECT_EQ(kOneCellTowerRequestBody, requests_monitor.last_request_body());
    } else {
      // Sending Cellular data is disabled.
      EXPECT_EQ(kIPOnlyRequestBody, requests_monitor.last_request_body());
    }

    EXPECT_EQ(kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    // This is total.
    EXPECT_EQ(2U, url_factory_.attempts());
  }
}

// This test verifies that WiFi and Cell tower  data is sent only if sending was
// requested. System geolocation permission is enabled.
INSTANTIATE_TEST_SUITE_P(EnableDisableSendingWifiData,
                         SimpleGeolocationWirelessTest,
                         testing::Bool());

}  // namespace ash
