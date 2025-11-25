// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/cached_location_provider.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/geolocation/test_utils.h"
#include "chromeos/ash/components/network/network_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr base::TimeDelta kRequestTimeout = base::Milliseconds(100);

WifiAccessPoint GetTestWifiAP() {
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

namespace utils = geolocation::test_utils;

class CachedLocationProviderTestBase
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  CachedLocationProviderTestBase() : interceptor(&url_factory_) {
    use_wifi_scan = std::get<0>(GetParam());
    use_cellular_scan = std::get<1>(GetParam());
  }

  void SetUp() override {
    network_delegate.AddWifiAP(GetTestWifiAP());
    network_delegate.AddCellTower(GetTestCellTower());

    url_factory_.SetInterceptor(
        base::BindRepeating(&utils::GeolocationApiInterceptor::Intercept,
                            base::Unretained(&interceptor)));
    cached_location_provider = std::make_unique<CachedLocationProvider>(
        std::make_unique<LocationFetcher>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_factory_),
            GURL(utils::kTestGeolocationProviderUrl), &network_delegate));
  }

  void ConfigureLocationResponse(const Geoposition& position) {
    auto access_points = std::make_unique<WifiAccessPointVector>();
    auto cell_towers = std::make_unique<CellTowerVector>();
    network_delegate.GetNetworkInformation(access_points.get(),
                                           cell_towers.get());

    if (!use_wifi_scan) {
      access_points.reset();
    }

    if (!use_cellular_scan) {
      cell_towers.reset();
    }

    interceptor.RegisterResponseForQuery(
        GetRequestBodyFor(std::move(access_points), std::move(cell_towers)),
        position);
  }

  base::TimeDelta GetRateLimit() {
    return cached_location_provider->GetRateLimitForTesting();
  }

  bool IsForPreciseResolution(bool use_wifi_signals,
                              bool use_cellular_signals) {
    return use_wifi_signals || use_cellular_signals;
  }

 protected:
  bool use_wifi_scan;
  bool use_cellular_scan;

  utils::GeolocationApiInterceptor interceptor;
  std::unique_ptr<ash::CachedLocationProvider> cached_location_provider;
  utils::FakeNetworkDelegate network_delegate;
  network::TestURLLoaderFactory url_factory_;

 private:
  std::string GetRequestBodyFor(
      std::unique_ptr<WifiAccessPointVector> access_points,
      std::unique_ptr<CellTowerVector> cell_towers) {
    // `LocationFetcher::RequestGeolocation` drops empty scans.
    if (!access_points || access_points->empty()) {
      access_points.reset(nullptr);
    }
    if (!cell_towers || cell_towers->empty()) {
      cell_towers.reset(nullptr);
    }

    auto request = SimpleGeolocationRequest(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_factory_),
        GURL(utils::kTestGeolocationProviderUrl), base::TimeDelta(),
        std::move(access_points), std::move(cell_towers));
    return request.FormatRequestBodyForTesting();
  }
};

class CachedLocationProviderTest : public CachedLocationProviderTestBase {
 private:
  base::test::SingleThreadTaskEnvironment task_environment;
};

TEST_P(CachedLocationProviderTest, FirstRequestFetchesLocation) {
  base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
  cached_location_provider->RequestLocation(
      kRequestTimeout, use_wifi_scan, use_cellular_scan, future.GetCallback());
  // Wait for the response.
  EXPECT_TRUE(future.Wait());

  // Check that the API has been called.
  EXPECT_EQ(1U, interceptor.attempts());
}

TEST_P(CachedLocationProviderTest, SequentialRequestReturnsCache) {
  Geoposition first_position;
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    first_position = future.Get<0>();
    EXPECT_EQ(1U, interceptor.attempts());
  }

  // Second request.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    auto second_position = future.Get<0>();

    // Check that new API call was not and the position is the same.
    EXPECT_EQ(1U, interceptor.attempts());
    EXPECT_EQ(first_position, second_position);
  }
}

TEST_P(CachedLocationProviderTest, CoarseAndPreciseCachesAreSeparate) {
  Geoposition first_position;
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    first_position = future.Get<0>();
    EXPECT_EQ(1U, interceptor.attempts());
  }

  // Arguments to trigger an opposite granularity requests.
  bool opposite_use_wifi_scan;
  bool opposite_use_cellular_scan;
  if (IsForPreciseResolution(use_wifi_scan, use_cellular_scan)) {
    opposite_use_wifi_scan = false;
    opposite_use_cellular_scan = false;
  } else {
    opposite_use_wifi_scan = true;
    opposite_use_cellular_scan = true;
  }

  // Request location for the opposite granularity.
  Geoposition second_position;
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(
        kRequestTimeout, opposite_use_wifi_scan, opposite_use_cellular_scan,
        future.GetCallback());
    second_position = future.Get<0>();
    EXPECT_EQ(2U, interceptor.attempts());
  }

  // Check that resolved positions are different.
  EXPECT_NE(first_position, second_position);

  // Check that subsequent requests for both granularity will return respective
  // cached values:
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    auto position = future.Get<0>();
    EXPECT_EQ(2U, interceptor.attempts());
    EXPECT_EQ(first_position, position);
  }
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(
        kRequestTimeout, opposite_use_wifi_scan, opposite_use_cellular_scan,
        future.GetCallback());
    auto position = future.Get<0>();
    EXPECT_EQ(2U, interceptor.attempts());
    EXPECT_EQ(second_position, position);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CachedLocationProviderTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf("wifi%d_cellular%d", std::get<0>(info.param),
                                std::get<1>(info.param));
    });

class CachedLocationProviderMockTimeTest
    : public CachedLocationProviderTestBase {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_P(CachedLocationProviderMockTimeTest, CheckRateLimit) {
  Geoposition first_position;

  // First request always fetches.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    task_environment.FastForwardBy(kRequestTimeout);
    first_position = future.Get<0>();
    EXPECT_EQ(1U, interceptor.attempts());
  }

  // Check that request at the end of the rate limit period will return cache.
  task_environment.FastForwardBy(GetRateLimit() - kRequestTimeout);
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    task_environment.FastForwardBy(kRequestTimeout);
    auto position = future.Get<0>();
    EXPECT_EQ(1U, interceptor.attempts());
    EXPECT_EQ(first_position, position);
  }

  // Escape from the rate limited interval.
  task_environment.FastForwardBy(base::Milliseconds(1));

  // Clear network signals to avoid cache-hit.
  if (IsForPreciseResolution(use_wifi_scan, use_cellular_scan)) {
    network_delegate.RemoveWifiAP(GetTestWifiAP());
    network_delegate.RemoveCellTower(GetTestCellTower());
  }

  // Configure Remote API to return different location:
  auto new_expected_position = Geoposition();
  new_expected_position.latitude = 10;
  new_expected_position.longitude = 10;
  new_expected_position.accuracy = 100;
  new_expected_position.status = Geoposition::STATUS_OK;
  new_expected_position.timestamp = base::Time::Now();
  ConfigureLocationResponse(new_expected_position);

  // Check that the request fetches fresh location.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    task_environment.FastForwardBy(kRequestTimeout);
    auto position = future.Get<0>();
    EXPECT_EQ(2U, interceptor.attempts());
    EXPECT_EQ(new_expected_position, position);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CachedLocationProviderMockTimeTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf("wifi%d_cellular%d", std::get<0>(info.param),
                                std::get<1>(info.param));
    });

}  // namespace ash
