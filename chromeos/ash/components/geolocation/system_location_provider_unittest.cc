// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/system_location_provider.h"

#include <stddef.h>

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/geolocation_access_level.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/geolocation/live_location_provider.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request_test_monitor.h"
#include "chromeos/ash/components/geolocation/test_utils.h"
#include "chromeos/ash/components/network/geolocation_handler_impl.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "google_apis/api_key_cache.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace utils = geolocation::test_utils;

class WirelessTestMonitor : public SimpleGeolocationRequestTestMonitor {
 public:
  WirelessTestMonitor() = default;

  WirelessTestMonitor(const WirelessTestMonitor&) = delete;
  WirelessTestMonitor& operator=(const WirelessTestMonitor&) = delete;

  void OnRequestCreated(SimpleGeolocationRequest* request) override {}
  void OnStart(SimpleGeolocationRequest* request) override {
    last_request_body_ = request->FormatRequestBodyForTesting();
    last_request_url_ = request->GetServiceURLForTesting();
    ++requests_count_;
  }

  const std::string& last_request_body() const { return last_request_body_; }
  GURL last_request_url() const { return last_request_url_; }
  unsigned int requests_count() const { return requests_count_; }

 private:
  std::string last_request_body_;
  GURL last_request_url_;
  unsigned int requests_count_ = 0;
};

class SystemLocationProviderTestBase {
 public:
  SystemLocationProviderTestBase() : interceptor(&url_factory_) {}

  ~SystemLocationProviderTestBase() = default;

  void EnableGeolocationUsage() {
    SystemLocationProvider::GetInstance()->SetGeolocationAccessLevel(
        GeolocationAccessLevel::kAllowed);
  }

  void DisableGeolocatioUsage() {
    SystemLocationProvider::GetInstance()->SetGeolocationAccessLevel(
        GeolocationAccessLevel::kDisallowed);
  }

  LocationFetcher* GetLocationFetcher() {
    return SystemLocationProvider::GetInstance()
        ->GetLocationProviderForTesting()
        ->GetLocationFetcherForTesting();
  }

  network::TestURLLoaderFactory url_factory_;
  utils::GeolocationApiInterceptor interceptor;
};

class SystemLocationProviderTest : public SystemLocationProviderTestBase,
                                   public testing::Test {
 protected:
  void SetUp() override {
    url_factory_.SetInterceptor(
        base::BindRepeating(&utils::GeolocationApiInterceptor::Intercept,
                            base::Unretained(&interceptor)));
    SystemLocationProvider::Initialize(std::make_unique<LiveLocationProvider>(
        std::make_unique<LocationFetcher>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_factory_),
            GURL(utils::kTestGeolocationProviderUrl), nullptr)));
  }

  void TearDown() override { SystemLocationProvider::DestroyForTesting(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Test SystemLocationProvider when the system geolocation permission is
// denied. System shall not send out any geolocation request.
TEST_F(SystemLocationProviderTest, SystemGeolocationPermissionDenied) {
  NetworkHandlerTestHelper network_handler_test_helper;
  utils::GeolocationReceiver receiver;
  WirelessTestMonitor requests_monitor;

  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);
  interceptor.Configure(net::HTTP_OK, utils::kSimpleResponseBody, 0);

  // Set system geolocation permission to disabled.
  DisableGeolocatioUsage();

  // Test for every request type.
  for (bool send_wifi : {false, true}) {
    for (bool send_cell : {false, true}) {
      SystemLocationProvider::GetInstance()->RequestGeolocation(
          base::Seconds(1), send_wifi, send_cell,
          base::BindOnce(&utils::GeolocationReceiver::OnRequestDone,
                         base::Unretained(&receiver)),
          SystemLocationProvider::ClientId::kForTesting);

      // Waiting is not needed, requests are dropped, thus nothing is pending.
      EXPECT_EQ(0U, requests_monitor.requests_count());
      EXPECT_EQ(std::string(), requests_monitor.last_request_body());
      EXPECT_EQ(0U, interceptor.attempts());
    }
  }
}

namespace override_geo_api_keys {

// We start every test by creating a clean environment for the
// preprocessor defines used in define_baked_in_api_keys-inc.cc
#undef GOOGLE_API_KEY
#undef GOOGLE_API_KEY_CROS_SYSTEM_GEO
#undef GOOGLE_API_KEY_CROS_CHROME_GEO

// Set Geolocation-specific keys.
#define GOOGLE_API_KEY "bogus_api_key"
#define GOOGLE_API_KEY_CROS_SYSTEM_GEO "bogus_cros_system_geo_api_key"
#define GOOGLE_API_KEY_CROS_CHROME_GEO "bogus_cros_chrome_geo_api_key"

// This file must be included after the internal files defining official keys.
#include "google_apis/default_api_keys-inc.cc"

}  // namespace override_geo_api_keys

class SystemLocationProviderAPIKeyTest : public SystemLocationProviderTestBase,
                                         public testing::TestWithParam<bool> {
 public:
  SystemLocationProviderAPIKeyTest()
      : is_separate_api_keys_enabled_(GetParam()) {
    if (is_separate_api_keys_enabled_) {
      feature_list_.InitAndEnableFeature(features::kCrosSeparateGeoApiKey);
    } else {
      feature_list_.InitAndDisableFeature(features::kCrosSeparateGeoApiKey);
    }
  }

  void SetUp() override {
    url_factory_.SetInterceptor(
        base::BindRepeating(&utils::GeolocationApiInterceptor::Intercept,
                            base::Unretained(&interceptor)));
    // Set URL to the production value to let the `SimpleGeolocationRequest`
    // extend it with the API keys.
    SystemLocationProvider::Initialize(std::make_unique<LiveLocationProvider>(
        std::make_unique<LocationFetcher>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_factory_),
            GURL(LocationFetcher::kDefaultGeolocationProviderUrl), nullptr)));
  }

  void TearDown() override { SystemLocationProvider::DestroyForTesting(); }

  const bool is_separate_api_keys_enabled_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(SystemLocationProviderAPIKeyTest, TestCorrectAPIKeysAreUsed) {
  utils::GeolocationReceiver receiver;
  WirelessTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  // Override the `ApiKeyCache` with the bogus values from above.
  auto default_key_values =
      override_geo_api_keys::GetDefaultApiKeysFromDefinedValues();
  default_key_values.allow_unset_values = true;
  google_apis::ApiKeyCache api_key_cache(default_key_values);
  auto scoped_key_cache_override =
      google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

  // Request geolocation and wait for the response.
  EnableGeolocationUsage();
  SystemLocationProvider::GetInstance()->RequestGeolocation(
      base::Seconds(1), false, false,
      base::BindOnce(&utils::GeolocationReceiver::OnRequestDone,
                     base::Unretained(&receiver)),
      SystemLocationProvider::ClientId::kForTesting);
  receiver.WaitUntilRequestDone();

  // Check that the appropriate API key was used depending on the
  // `CrosSeparateGeoApiKey` feature status.
  const GURL request_url = requests_monitor.last_request_url();
  ASSERT_TRUE(request_url.has_query());
  EXPECT_EQ(is_separate_api_keys_enabled_,
            request_url.GetQuery().find(GOOGLE_API_KEY_CROS_SYSTEM_GEO) !=
                std::string::npos);
  EXPECT_EQ(is_separate_api_keys_enabled_,
            request_url.GetQuery().find(GOOGLE_API_KEY) == std::string::npos);
  EXPECT_EQ(1U, interceptor.attempts());
}

// GetParam() - `ash::features::kCrosSeparateGeoApiKey` feature state.
INSTANTIATE_TEST_SUITE_P(All,
                         SystemLocationProviderAPIKeyTest,
                         testing::Bool());

// Test sending of WiFi Access points and Cell Towers.
// (This is mostly derived from GeolocationHandlerTest.)
class SystemLocationProviderWirelessTest
    : public SystemLocationProviderTestBase,
      public ::testing::TestWithParam<bool> {
 public:
  SystemLocationProviderWirelessTest() : manager_test_(nullptr) {}

  SystemLocationProviderWirelessTest(
      const SystemLocationProviderWirelessTest&) = delete;
  SystemLocationProviderWirelessTest& operator=(
      const SystemLocationProviderWirelessTest&) = delete;

  ~SystemLocationProviderWirelessTest() override = default;

  void SetUp() override {
    // Get the test interface for manager / device.
    manager_test_ = ShillManagerClient::Get()->GetTestInterface();
    ASSERT_TRUE(manager_test_);

    url_factory_.SetInterceptor(
        base::BindRepeating(&utils::GeolocationApiInterceptor::Intercept,
                            base::Unretained(&interceptor)));
    geolocation_handler_.reset(new GeolocationHandlerImpl());
    geolocation_handler_->Init();
    SystemLocationProvider::Initialize(std::make_unique<LiveLocationProvider>(
        std::make_unique<LocationFetcher>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_factory_),
            GURL(utils::kTestGeolocationProviderUrl),
            geolocation_handler_.get())));

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    SystemLocationProvider::DestroyForTesting();
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
  std::unique_ptr<GeolocationHandlerImpl> geolocation_handler_;
  raw_ptr<ShillManagerClient::TestInterface> manager_test_;
  WifiAccessPointVector wifi_access_points_;
  CellTowerVector cell_towers_;
};

// Parameter - (bool) enable/disable sending of WiFi data.
TEST_P(SystemLocationProviderWirelessTest, WiFiExists) {
  bool send_wifi_access_points = GetParam();

  WirelessTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  interceptor.Configure(net::HTTP_OK, utils::kSimpleResponseBody,
                        0 /* require_retries */);
  // Set system geolocation permission to allowed. This permission is tested
  // separately.
  EnableGeolocationUsage();

  {
    utils::GeolocationReceiver receiver;
    SystemLocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), send_wifi_access_points, false,
        base::BindOnce(&utils::GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SystemLocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    EXPECT_EQ(utils::kIPOnlyRequestBody, requests_monitor.last_request_body());

    EXPECT_EQ(utils::kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    EXPECT_EQ(1U, interceptor.attempts());
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
  EXPECT_EQ(utils::kWiFiAP1MacAddress, wifi_access_points_[0].mac_address);
  EXPECT_EQ(1, wifi_access_points_[0].channel);

  {
    utils::GeolocationReceiver receiver;
    SystemLocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), send_wifi_access_points, false,
        base::BindOnce(&utils::GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SystemLocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    if (send_wifi_access_points) {
      // Sending WiFi data is enabled.
      EXPECT_EQ(utils::kOneWiFiAPRequestBody,
                requests_monitor.last_request_body());
    } else {
      // Sending WiFi data is disabled.
      EXPECT_EQ(utils::kIPOnlyRequestBody,
                requests_monitor.last_request_body());
    }

    EXPECT_EQ(utils::kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    // This is total.
    EXPECT_EQ(2U, interceptor.attempts());
  }
}

// Parameter - (bool) enable/disable sending of WiFi data.
TEST_P(SystemLocationProviderWirelessTest, CellularExists) {
  bool send_cell_towers = GetParam();

  WirelessTestMonitor requests_monitor;
  SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);

  // Enable system permission for geolocation usage.
  EnableGeolocationUsage();

  {
    utils::GeolocationReceiver receiver;
    SystemLocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), false, send_cell_towers,
        base::BindOnce(&utils::GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SystemLocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    EXPECT_EQ(utils::kIPOnlyRequestBody, requests_monitor.last_request_body());

    EXPECT_EQ(utils::kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    EXPECT_EQ(1U, interceptor.attempts());
  }

  AddCellTower(1);
  base::RunLoop().RunUntilIdle();
  // Initial call should return false and request cell towers.
  EXPECT_FALSE(GetCellTowers());
  base::RunLoop().RunUntilIdle();
  // Second call should return true since we have a tower.
  EXPECT_TRUE(GetCellTowers());
  ASSERT_EQ(1u, cell_towers_.size());
  EXPECT_EQ(utils::kCellTower1MNC, cell_towers_[0].mnc);
  EXPECT_EQ(base::NumberToString(1), cell_towers_[0].ci);

  {
    utils::GeolocationReceiver receiver;
    SystemLocationProvider::GetInstance()->RequestGeolocation(
        base::Seconds(1), false, send_cell_towers,
        base::BindOnce(&utils::GeolocationReceiver::OnRequestDone,
                       base::Unretained(&receiver)),
        SystemLocationProvider::ClientId::kForTesting);
    receiver.WaitUntilRequestDone();
    if (send_cell_towers) {
      // Sending Cellular data is enabled.
      EXPECT_EQ(utils::kOneCellTowerRequestBody,
                requests_monitor.last_request_body());
    } else {
      // Sending Cellular data is disabled.
      EXPECT_EQ(utils::kIPOnlyRequestBody,
                requests_monitor.last_request_body());
    }

    EXPECT_EQ(utils::kExpectedPosition, receiver.position().ToString());
    EXPECT_FALSE(receiver.server_error());
    // This is total.
    EXPECT_EQ(2U, interceptor.attempts());
  }
}

// This test verifies that WiFi and Cell tower  data is sent only if sending was
// requested. System geolocation permission is enabled.
INSTANTIATE_TEST_SUITE_P(EnableDisableSendingWifiData,
                         SystemLocationProviderWirelessTest,
                         testing::Bool());

}  // namespace ash
