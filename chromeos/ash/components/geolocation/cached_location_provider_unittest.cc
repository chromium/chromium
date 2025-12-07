// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/cached_location_provider.h"

#include "base/containers/fixed_flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/cache_eviction_options.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/geolocation/test_utils.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/constants/chromeos_features.h"
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

// MUST BE in line with the `kEvictionStrategyOptions` in
// `cached_location_provider.cc`.
constexpr std::string_view EvictionStrategyToParamString(
    const geolocation::CacheEvictionStrategy strategy) {
  switch (strategy) {
    case geolocation::CacheEvictionStrategy::kWifiTolerance:
      return "wifi_tolerance";
    case geolocation::CacheEvictionStrategy::kCommonWifi:
      return "common_wifi";
    case geolocation::CacheEvictionStrategy::kCellularTolerance:
      return "cellular_tolerance";
    case geolocation::CacheEvictionStrategy::kCommonCell:
      return "common_cell";
    case geolocation::CacheEvictionStrategy::kCommonWifiAndCell:
      return "common_wifi_and_cell";
  }
  NOTREACHED();
}

constexpr std::string_view EvictionStrategyToString(
    const geolocation::CacheEvictionStrategy strategy) {
  switch (strategy) {
    case geolocation::CacheEvictionStrategy::kWifiTolerance:
      return "WifiTolerance";
    case geolocation::CacheEvictionStrategy::kCommonWifi:
      return "CommonWifi";
    case geolocation::CacheEvictionStrategy::kCellularTolerance:
      return "CellularTolerance";
    case geolocation::CacheEvictionStrategy::kCommonCell:
      return "CommonCell";
    case geolocation::CacheEvictionStrategy::kCommonWifiAndCell:
      return "CommonWifiAndCell";
  }
  NOTREACHED();
}

constexpr std::string_view SimilarityDegreeToString(
    const geolocation::SimilarityDegree similarity_degree) {
  switch (similarity_degree) {
    case geolocation::SimilarityDegree::kLoose:
      return "Loose";
    case geolocation::SimilarityDegree::kModerate:
      return "Moderate";
    case geolocation::SimilarityDegree::kStrict:
      return "Strict";
  }
  NOTREACHED();
}

// Must be in line with the `kEvictionStrategyToleranceOptions` in
// `cached_location_provider.cc`.
constexpr std::string_view SimilarityDegreeToParamString(
    const geolocation::SimilarityDegree similarity_degree) {
  switch (similarity_degree) {
    case geolocation::SimilarityDegree::kLoose:
      return "loose_similarity";
    case geolocation::SimilarityDegree::kModerate:
      return "moderate_similarity";
    case geolocation::SimilarityDegree::kStrict:
      return "strict_similarity";
  }
  NOTREACHED();
}

}  // namespace

namespace utils = geolocation::test_utils;

class CachedLocationProviderTestBase : public testing::Test {
 public:
  CachedLocationProviderTestBase(bool use_wifi_scan, bool use_cellular_scan)
      : use_wifi_scan(use_wifi_scan),
        use_cellular_scan(use_cellular_scan),
        interceptor(&url_factory_) {}

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

class CachedLocationProviderTest
    : public CachedLocationProviderTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CachedLocationProviderTest()
      : CachedLocationProviderTestBase(std::get<0>(GetParam()),
                                       std::get<1>(GetParam())) {}

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

class CachedLocationProviderFieldTrialTestBase
    : public CachedLocationProviderTestBase {
 public:
  CachedLocationProviderFieldTrialTestBase(
      bool use_wifi_scan,
      bool use_cellular_scan,
      bool is_field_trial_mode,
      geolocation::CacheEvictionStrategy eviction_strategy =
          geolocation::CacheEvictionStrategy::kWifiTolerance,
      geolocation::SimilarityDegree similarity_degree =
          geolocation::SimilarityDegree::kLoose)
      : CachedLocationProviderTestBase(use_wifi_scan, use_cellular_scan),
        is_field_trial_mode(is_field_trial_mode),
        eviction_strategy(eviction_strategy),
        similarity_degree(similarity_degree) {
    feature_list_.InitAndEnableFeatureWithParameters(
        chromeos::features::kCachedLocationProvider,
        {
            {"field_trial_phase", is_field_trial_mode ? "true" : "false"},
            {"strategy",
             std::string(EvictionStrategyToParamString(eviction_strategy))},
            {"tolerance",
             std::string(SimilarityDegreeToParamString(similarity_degree))},
        });
  }

 protected:
  const bool is_field_trial_mode;
  const geolocation::CacheEvictionStrategy eviction_strategy;
  const geolocation::SimilarityDegree similarity_degree;

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::ScopedFeatureList feature_list_;
};

class CachedLocationProviderFieldTrialHistogramsTester
    : public CachedLocationProviderFieldTrialTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CachedLocationProviderFieldTrialHistogramsTester()
      : CachedLocationProviderFieldTrialTestBase(std::get<0>(GetParam()),
                                                 std::get<1>(GetParam()),
                                                 /*is_field_trial_mode=*/true) {
  }

 protected:
  base::HistogramTester histogram_tester;

  static constexpr char kCacheEvictionHistogramPrefix[] =
      "ChromeOS.Geolocation.CacheEviction";

  std::vector<std::string> GetFieldTrialHistogramNames() {
    std::vector<std::string> histogram_names;
    for (auto& strategy_pair :
         *cached_location_provider->GetEvictionStrategiesUnderTest()) {
      histogram_names.emplace_back(base::StrCat(
          {kCacheEvictionHistogramPrefix, ".",
           EvictionStrategyToString(strategy_pair.first->strategy()),
           (strategy_pair.second
                ? SimilarityDegreeToString(*strategy_pair.second)
                : "")}));
    }

    return histogram_names;
  }

  std::string GetPredictedYesHistogramName(std::string_view histogram) {
    return base::StrCat({histogram, ".", "PredictedYes"});
  }
  std::string GetPredictedNoHistogramName(std::string_view histogram) {
    return base::StrCat({histogram, ".", "PredictedNo"});
  }
};

TEST_P(CachedLocationProviderFieldTrialHistogramsTester,
       CheckFieldTrialMetrics_SameLocation) {
  Geoposition first_position;
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    first_position = future.Get<0>();
    EXPECT_EQ(1U, interceptor.attempts());
  }

  // First location fetch doesn't populate histograms.
  EXPECT_TRUE(
      histogram_tester.GetTotalCountsForPrefix(kCacheEvictionHistogramPrefix)
          .empty());

  // Second request.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    auto second_position = future.Get<0>();

    // Check that new API call was made but expect the same position.
    EXPECT_EQ(2U, interceptor.attempts());
    EXPECT_EQ(first_position.latitude, second_position.latitude);
    EXPECT_EQ(first_position.longitude, second_position.longitude);
    EXPECT_EQ(first_position.accuracy, second_position.accuracy);
  }

  if (!IsForPreciseResolution(use_wifi_scan, use_cellular_scan)) {
    // Coarse requests don't populate the eviction histograms.
    EXPECT_TRUE(
        histogram_tester.GetTotalCountsForPrefix(kCacheEvictionHistogramPrefix)
            .empty());
  } else {
    for (auto histogram : GetFieldTrialHistogramNames()) {
      auto predicted_no = GetPredictedNoHistogramName(histogram);
      auto predicted_yes = GetPredictedYesHistogramName(histogram);

      histogram_tester.ExpectTotalCount(predicted_no, 1);
      histogram_tester.ExpectBucketCount(predicted_no, 0, 1);

      histogram_tester.ExpectTotalCount(predicted_yes, 0);
    }
  }
}

TEST_P(CachedLocationProviderFieldTrialHistogramsTester,
       CheckFieldTrialMetrics_DifferentLocation) {
  // Configure Remote API to return the following location:
  auto expected_position = Geoposition();
  expected_position.latitude = 10;
  expected_position.longitude = 10;
  expected_position.accuracy = 100;
  expected_position.status = Geoposition::STATUS_OK;
  expected_position.timestamp = base::Time::Now();
  ConfigureLocationResponse(expected_position);

  Geoposition first_position;
  // First request.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    first_position = future.Get<0>();
    EXPECT_EQ(1U, interceptor.attempts());
  }

  // First location fetch doesn't populate histograms (the cache is empty).
  EXPECT_TRUE(
      histogram_tester.GetTotalCountsForPrefix(kCacheEvictionHistogramPrefix)
          .empty());

  // Configure Remote API to return diametrical position.
  expected_position.latitude *= -1;
  expected_position.longitude *= -1;
  ConfigureLocationResponse(expected_position);

  // Second request.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    auto second_position = future.Get<0>();

    // Check that new API call was made.
    EXPECT_EQ(2U, interceptor.attempts());
    EXPECT_EQ(expected_position.latitude, second_position.latitude);
    EXPECT_EQ(expected_position.longitude, second_position.longitude);
  }

  if (!IsForPreciseResolution(use_wifi_scan, use_cellular_scan)) {
    // Coarse requests don't populate the eviction histograms.
    EXPECT_TRUE(
        histogram_tester.GetTotalCountsForPrefix(kCacheEvictionHistogramPrefix)
            .empty());
  } else {
    for (auto histogram : GetFieldTrialHistogramNames()) {
      auto predicted_no = GetPredictedNoHistogramName(histogram);
      auto predicted_yes = GetPredictedYesHistogramName(histogram);

      histogram_tester.ExpectTotalCount(predicted_no, 1);
      histogram_tester.ExpectBucketCount(predicted_no, 1000000, 1);

      histogram_tester.ExpectTotalCount(predicted_yes, 0);
    }
  }

  // Empty out network context, this would lead to all eviction methods
  // predicting significant displacement (emitting to *PredictedYes histograms).
  network_delegate.RemoveWifiAP(GetTestWifiAP());
  network_delegate.RemoveCellTower(GetTestCellTower());

  // Third request.
  {
    base::test::TestFuture<const Geoposition&, bool, base::TimeDelta> future;
    cached_location_provider->RequestLocation(kRequestTimeout, use_wifi_scan,
                                              use_cellular_scan,
                                              future.GetCallback());
    auto third_position = future.Get<0>();

    EXPECT_EQ(3U, interceptor.attempts());
    if (!IsForPreciseResolution(use_wifi_scan, use_cellular_scan)) {
      // Coarse requests don't populate the eviction histograms.
      EXPECT_TRUE(histogram_tester
                      .GetTotalCountsForPrefix(kCacheEvictionHistogramPrefix)
                      .empty());
    } else {
      for (auto histogram : GetFieldTrialHistogramNames()) {
        auto predicted_no = GetPredictedNoHistogramName(histogram);
        auto predicted_yes = GetPredictedYesHistogramName(histogram);
        // Check *PredictedNo stayed the same.
        histogram_tester.ExpectTotalCount(predicted_no, 1);
        histogram_tester.ExpectBucketCount(predicted_no, 1000000, 1);

        // Check *PredictedYes was populated by this request.
        histogram_tester.ExpectTotalCount(predicted_yes, 1);
        histogram_tester.ExpectBucketCount(predicted_yes, 1000000, 1);
      }
    }
  }
}
INSTANTIATE_TEST_SUITE_P(All,
                         CachedLocationProviderFieldTrialHistogramsTester,
                         testing::Combine(testing::Bool(), testing::Bool()));

class CachedLocationProviderFieldTrialTest
    : public CachedLocationProviderFieldTrialTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CachedLocationProviderFieldTrialTest()
      : CachedLocationProviderFieldTrialTestBase(std::get<0>(GetParam()),
                                                 std::get<1>(GetParam()),
                                                 /*is_field_trial_mode=*/true) {
  }
};

TEST_P(CachedLocationProviderFieldTrialTest, OptimizationsAreDisabled) {
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

    // Check that new API call was made but expect the same position, only
    // timestamp could differ.
    EXPECT_EQ(2U, interceptor.attempts());
    EXPECT_EQ(first_position.latitude, second_position.latitude);
    EXPECT_EQ(first_position.longitude, second_position.longitude);
    EXPECT_EQ(first_position.accuracy, second_position.accuracy);
    EXPECT_EQ(first_position.status, second_position.status);
    EXPECT_EQ(first_position.error_code, second_position.error_code);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CachedLocationProviderFieldTrialTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class CachedLocationProviderFieldTrialDisabledTest
    : public CachedLocationProviderFieldTrialTestBase,
      public testing::WithParamInterface<geolocation::CacheEvictionStrategy> {
 public:
  CachedLocationProviderFieldTrialDisabledTest()
      : CachedLocationProviderFieldTrialTestBase(
            /*use_wifi_scan=*/true,
            /*use_cellular_scan=*/true,
            /*is_field_trial_mode=*/false,
            GetParam()) {}
};

TEST_P(CachedLocationProviderFieldTrialDisabledTest, CorrectEvictionUsed) {
  CHECK_EQ(
      eviction_strategy,
      cached_location_provider->GetEvictionStrategyForTesting()->strategy());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CachedLocationProviderFieldTrialDisabledTest,
    testing::Values(geolocation::CacheEvictionStrategy::kWifiTolerance,
                    geolocation::CacheEvictionStrategy::kCommonWifi,
                    geolocation::CacheEvictionStrategy::kCellularTolerance,
                    geolocation::CacheEvictionStrategy::kCommonCell,
                    geolocation::CacheEvictionStrategy::kCommonWifiAndCell));

class CachedLocationProviderMockTimeTest
    : public CachedLocationProviderTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CachedLocationProviderMockTimeTest()
      : CachedLocationProviderTestBase(std::get<0>(GetParam()),
                                       std::get<1>(GetParam())) {}

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
