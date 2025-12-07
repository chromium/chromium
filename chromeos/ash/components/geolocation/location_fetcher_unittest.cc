// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/location_fetcher.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_request_test_monitor.h"
#include "chromeos/ash/components/geolocation/test_utils.h"
#include "chromeos/ash/components/network/network_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace utils = geolocation::test_utils;

namespace {

constexpr base::TimeDelta kRequestTimeout = base::Seconds(1);
constexpr base::TimeDelta kRetryDelay = base::Milliseconds(10);

WifiAccessPoint GetTestWifiAp() {
  WifiAccessPoint wifi_ap;
  wifi_ap.ssid = "ssid_0";
  wifi_ap.mac_address = "01:00:00:00:00:00";
  wifi_ap.timestamp = base::Time();
  wifi_ap.signal_strength = 10;
  wifi_ap.signal_to_noise = 0;
  wifi_ap.channel = 1;

  return wifi_ap;
}

CellTower GetTestCellTower() {
  CellTower cell_tower;
  cell_tower.mcc = base::NumberToString(100);
  cell_tower.mnc = base::NumberToString(101);
  cell_tower.lac = base::NumberToString(3);
  cell_tower.ci = base::NumberToString(1);
  cell_tower.timestamp = base::Time();

  return cell_tower;
}

}  // namespace

class LocationFetcherTest : public testing::Test {
 public:
  LocationFetcherTest() : interceptor(&url_factory_) {}

  void SetUp() override {
    url_factory_.SetInterceptor(
        base::BindRepeating(&utils::GeolocationApiInterceptor::Intercept,
                            base::Unretained(&interceptor)));
    location_fetcher = std::make_unique<LocationFetcher>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_factory_),
        GURL(utils::kTestGeolocationProviderUrl), &network_delegate);
  }

  void TearDown() override { location_fetcher.reset(); }

  void OnRequestDone(const Geoposition& position,
                     bool server_error,
                     base::TimeDelta elapsed) {
    position_ = position;
    server_error_ = server_error;
    message_loop_runner_.Quit();
  }

  void WaitUntilRequestDone() { message_loop_runner_.Run(); }

  void SetRequestRetryInterval(const base::TimeDelta retry_interval) {
    ASSERT_NE(nullptr, location_fetcher);
    ASSERT_EQ(1u, location_fetcher->requests_.size());
    location_fetcher->requests_[0]->set_retry_sleep_on_server_error_for_testing(
        retry_interval);
    location_fetcher->requests_[0]->set_retry_sleep_on_bad_response_for_testing(
        retry_interval);
  }

  void AddWifiAP() { network_delegate.AddWifiAP(GetTestWifiAp()); }
  void AddCellTower() { network_delegate.AddCellTower(GetTestCellTower()); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop message_loop_runner_;

 protected:
  Geoposition position_;
  bool server_error_;
  utils::FakeNetworkDelegate network_delegate;
  network::TestURLLoaderFactory url_factory_;
  utils::GeolocationApiInterceptor interceptor;
  std::unique_ptr<LocationFetcher> location_fetcher;
};

TEST_F(LocationFetcherTest, ResponseOK) {
  location_fetcher->RequestGeolocation(
      kRequestTimeout, false, false,
      base::BindOnce(&LocationFetcherTest::OnRequestDone,
                     base::Unretained(this)));
  WaitUntilRequestDone();

  EXPECT_EQ(utils::kExpectedPosition, position_.ToString());
  EXPECT_FALSE(server_error_);
  EXPECT_EQ(1U, interceptor.attempts());
}

TEST_F(LocationFetcherTest, ResponseOKWithRetries) {
  interceptor.Configure(net::HTTP_OK, utils::kSimpleResponseBody,
                        3 /* require_retries */);

  location_fetcher->RequestGeolocation(
      kRequestTimeout, false, false,
      base::BindOnce(&LocationFetcherTest::OnRequestDone,
                     base::Unretained(this)));
  SetRequestRetryInterval(kRetryDelay);
  WaitUntilRequestDone();

  EXPECT_EQ(utils::kExpectedPosition, position_.ToString());
  EXPECT_FALSE(server_error_);
  EXPECT_EQ(4U, interceptor.attempts());
}

TEST_F(LocationFetcherTest, ResponseWithErrorTooManyRequestsIsNotRetried) {
  interceptor.Configure(net::HTTP_TOO_MANY_REQUESTS, utils::kSimpleResponseBody,
                        0 /* require_retries */);

  location_fetcher->RequestGeolocation(
      kRequestTimeout, false, false,
      base::BindOnce(&LocationFetcherTest::OnRequestDone,
                     base::Unretained(this)));
  SetRequestRetryInterval(kRetryDelay);
  WaitUntilRequestDone();

  // Check that Geoposition is not populated.
  EXPECT_FALSE(position_.Valid());
  EXPECT_EQ(Geoposition::Status::STATUS_SERVER_ERROR, position_.status);
  // Check that the request was not retried.
  EXPECT_EQ(1U, interceptor.attempts());
}

TEST_F(LocationFetcherTest, InvalidResponse) {
  interceptor.Configure(net::HTTP_OK, "invalid JSON string",
                        0 /* require_retries */);

  constexpr auto timeout = base::Seconds(1);
  constexpr auto retry_interval = base::Milliseconds(300);

  location_fetcher->RequestGeolocation(
      timeout, false, false,
      base::BindOnce(&LocationFetcherTest::OnRequestDone,
                     base::Unretained(this)));
  SetRequestRetryInterval(retry_interval);
  WaitUntilRequestDone();

  EXPECT_TRUE(base::Contains(
      position_.ToString(),
      "latitude=200.000000, longitude=200.000000, accuracy=-1.000000, "
      "error_code=0, error_message='SystemLocationProvider at "
      "'https://localhost/' : JSONReader failed:"));
  EXPECT_TRUE(base::Contains(position_.ToString(), "status=4 (TIMEOUT)"));
  EXPECT_TRUE(server_error_);

  // Number of retries should be within [-1,+1] range of expected_retries.
  size_t expected_retries = timeout / retry_interval;
  EXPECT_NEAR(interceptor.attempts(), expected_retries, 1);
}

// get<0>(GetParam()) - `use_wifi_scan` argument for RequestGeolocation().
// get<1>(GetParam()) - `use_cellular_scan` argument for RequestGeolocation().
class LocationFetcherPreciseLocationTest
    : public LocationFetcherTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  void SetUp() override {
    SimpleGeolocationRequest::SetTestMonitor(&requests_monitor);
    LocationFetcherTest::SetUp();

    // Populate WiFi and Cellular scan data.
    AddWifiAP();
    AddCellTower();
  }

  void TearDown() override {
    LocationFetcherTest::TearDown();
    SimpleGeolocationRequest::SetTestMonitor(nullptr);
  }

 protected:
  utils::RequestTestMonitor requests_monitor;
};

TEST_P(LocationFetcherPreciseLocationTest, OnlyIntendedNetworkSignalsAreUsed) {
  bool use_wifi_scan = std::get<0>(GetParam());
  bool use_cellular_scan = std::get<1>(GetParam());

  location_fetcher->RequestGeolocation(
      kRequestTimeout, use_wifi_scan, use_cellular_scan,
      base::BindOnce(&LocationFetcherTest::OnRequestDone,
                     base::Unretained(this)));
  WaitUntilRequestDone();

  // In all cases a successful response should be received.
  EXPECT_FALSE(server_error_);
  EXPECT_EQ(1U, interceptor.attempts());
  EXPECT_EQ(utils::kExpectedPosition, position_.ToString());

  // Check that the request body contained the right network signals.
  std::string expected_request_body;
  if (use_wifi_scan && use_cellular_scan) {
    expected_request_body = utils::kCellTowerAndWifiAPRequestBody;
  } else if (use_wifi_scan && !use_cellular_scan) {
    expected_request_body = utils::kOneWiFiAPRequestBody;
  } else if (!use_wifi_scan && use_cellular_scan) {
    expected_request_body = utils::kOneCellTowerRequestBody;
  } else {
    expected_request_body = utils::kIPOnlyRequestBody;
  }
  EXPECT_EQ(expected_request_body, requests_monitor.last_request_body());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    LocationFetcherPreciseLocationTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf("wifi%d_cellular%d", std::get<0>(info.param),
                                std::get<1>(info.param));
    });

}  // namespace ash
