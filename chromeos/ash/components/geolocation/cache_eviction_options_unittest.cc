// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/cache_eviction_options.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/components/network/network_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
int constexpr kNumScans = 20;

WifiAccessPoint GetTestWifiAP(size_t i = 0) {
  WifiAccessPoint wifi_ap;
  wifi_ap.ssid = "ssid";
  wifi_ap.mac_address = "00:00:00:00:00:00:" + base::NumberToString(i);
  wifi_ap.timestamp = base::Time();
  wifi_ap.signal_strength = 0;
  wifi_ap.signal_to_noise = 0;
  wifi_ap.channel = 0;

  return wifi_ap;
}

CellTower GetTestCellTower(size_t i = 0) {
  CellTower cell_tower;
  cell_tower.mcc = base::NumberToString(0);
  cell_tower.mnc = base::NumberToString(0);
  cell_tower.lac = base::NumberToString(0);
  cell_tower.ci = base::NumberToString(i);
  cell_tower.timestamp = base::Time();

  return cell_tower;
}

void PopulateWifiAPs(geolocation::GeopositionContext& context,
                     size_t n,
                     bool shuffle_ssid,
                     bool shuffle_signal_strength) {
  for (size_t i = 0; i < n; i++) {
    auto wifi_ap = GetTestWifiAP();
    wifi_ap.mac_address.append(":" + base::NumberToString(i));
    if (shuffle_ssid) {
      wifi_ap.ssid.append("_" + base::NumberToString(i));
    }
    if (shuffle_signal_strength) {
      wifi_ap.signal_strength = i;
    }
    context.wifi_context.push_back(wifi_ap);
  }
}

void PopulateCellTowers(geolocation::GeopositionContext& context,
                        size_t n,
                        bool shuffle_lac) {
  for (size_t i = 0; i < n; i++) {
    auto cell_tower = GetTestCellTower();
    cell_tower.ci = base::NumberToString(i);
    if (shuffle_lac) {
      cell_tower.lac = base::NumberToString(i);
    }
    context.cell_tower_context.push_back(cell_tower);
  }
}

// MUST BE in line with the thresholds used in `cache_eviction_options.cc`.
constexpr double GetSimilarityThreshold(
    geolocation::SimilarityDegree similarity_degree) {
  switch (similarity_degree) {
    case geolocation::SimilarityDegree::kLoose:
      return 0.5;  // 50% overlap required
    case geolocation::SimilarityDegree::kModerate:
      return 0.7;  // 70% overlap required
    case geolocation::SimilarityDegree::kStrict:
      return 0.9;  // 90% overlap required
  }
  NOTREACHED();
}

}  // namespace

using CacheEvictionOptions_WifiEquivalenceWithToleranceTestBaseCase =
    testing::TestWithParam<geolocation::SimilarityDegree>;
TEST_P(CacheEvictionOptions_WifiEquivalenceWithToleranceTestBaseCase,
       CheckEmpty) {
  auto similarity_degree = GetParam();
  geolocation::WifiEquivalenceWithTolerance eviction_strategy(
      similarity_degree);

  geolocation::GeopositionContext old_context;
  // Combination of empty contexts don't flag displacement.
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  auto wifi_ap_0 = GetTestWifiAP();
  auto cell_tower_0 = GetTestCellTower();
  old_context.wifi_context.push_back(wifi_ap_0);
  old_context.cell_tower_context.push_back(cell_tower_0);
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  geolocation::GeopositionContext new_context;
  // If only one context is empty, significant displacement is implied.
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CacheEvictionOptions_WifiEquivalenceWithToleranceTestBaseCase,
    testing::Values(geolocation::SimilarityDegree::kLoose,
                    geolocation::SimilarityDegree::kModerate,
                    geolocation::SimilarityDegree::kStrict));

using CacheEvictionOptions_WifiEquivalenceWithToleranceTest =
    testing::TestWithParam<
        std::tuple<geolocation::SimilarityDegree, bool, bool>>;
TEST_P(CacheEvictionOptions_WifiEquivalenceWithToleranceTest,
       CheckToleranceLimits) {
  auto similarity_degree = std::get<0>(GetParam());
  auto shuffle_ssid = std::get<1>(GetParam());
  auto shuffle_signal_strength = std::get<2>(GetParam());
  geolocation::WifiEquivalenceWithTolerance eviction_strategy(
      similarity_degree);

  geolocation::GeopositionContext old_context;
  geolocation::GeopositionContext new_context;

  // Populate old context with `kNumScans` WiFi APs.
  // New context will be the subset of first `kNumScans*tolerance`
  // elements to check the boundary condition.
  PopulateWifiAPs(old_context, kNumScans, shuffle_ssid,
                  shuffle_signal_strength);
  PopulateWifiAPs(new_context,
                  kNumScans * GetSimilarityThreshold(similarity_degree),
                  shuffle_ssid, shuffle_signal_strength);

  // Scans should be similar:
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));

  // Discard one of the WiFi APs from the new scan.
  new_context.wifi_context.pop_back();

  // Now the interseciton should be below threshold, expect significant
  // displacement:
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CacheEvictionOptions_WifiEquivalenceWithToleranceTest,
    testing::Combine(testing::Values(geolocation::SimilarityDegree::kLoose,
                                     geolocation::SimilarityDegree::kModerate,
                                     geolocation::SimilarityDegree::kStrict),
                     testing::Bool(),
                     testing::Bool()));

using CacheEvictionOptions_HasCommonWifiAP = testing::Test;

TEST_F(CacheEvictionOptions_HasCommonWifiAP, CheckEmpty) {
  geolocation::HasCommonWifiAP eviction_strategy;

  // Combination of empty contexts don't flag displacement.
  geolocation::GeopositionContext old_context;
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  // Identical contexts don't flag displacement.
  old_context.wifi_context.push_back(GetTestWifiAP());
  old_context.cell_tower_context.push_back(GetTestCellTower());
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  // If only one context is empty, significant displacement is implied.
  geolocation::GeopositionContext new_context;
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

TEST_F(CacheEvictionOptions_HasCommonWifiAP, CheckOnlyMacAddressesMatter) {
  geolocation::HasCommonWifiAP cache_eviction;

  WifiAccessPoint wifi_ap_0;
  wifi_ap_0.ssid = "ssid_0";
  wifi_ap_0.mac_address = "00:00:00:00:00:00";
  wifi_ap_0.timestamp = base::Time();
  wifi_ap_0.signal_strength = 0;
  wifi_ap_0.signal_to_noise = 0;
  wifi_ap_0.channel = 0;

  WifiAccessPoint wifi_ap_1;
  wifi_ap_1.ssid = "ssid_1";
  wifi_ap_1.mac_address = "00:00:00:00:00:00";
  wifi_ap_1.timestamp = base::Time();
  wifi_ap_1.signal_strength = 1;
  wifi_ap_1.signal_to_noise = 1;
  wifi_ap_1.channel = 1;

  geolocation::GeopositionContext old_context;
  geolocation::GeopositionContext new_context;

  old_context.wifi_context.push_back(wifi_ap_0);
  new_context.wifi_context.push_back(wifi_ap_1);
  EXPECT_FALSE(cache_eviction.IsSignificantDisplacementIndicated(old_context,
                                                                 new_context));

  WifiAccessPoint wifi_ap_2 = wifi_ap_0;
  wifi_ap_2.mac_address = "00:00:00:00:00:01";
  new_context.wifi_context.clear();
  new_context.wifi_context.push_back(wifi_ap_2);
  EXPECT_TRUE(cache_eviction.IsSignificantDisplacementIndicated(old_context,
                                                                new_context));
}

using CacheEvictionOptions_CellularEquivalenceWithToleranceBaseCaseTest =
    testing::TestWithParam<geolocation::SimilarityDegree>;
TEST_P(CacheEvictionOptions_CellularEquivalenceWithToleranceBaseCaseTest,
       CheckEmpty) {
  auto similarity_degree = GetParam();
  geolocation::CellularEquivalenceWithTolerance eviction_strategy(
      similarity_degree);

  geolocation::GeopositionContext old_context;
  // Combination of empty contexts don't flag displacement.
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  auto wifi_ap_0 = GetTestWifiAP();
  auto cell_tower_0 = GetTestCellTower();
  old_context.wifi_context.push_back(wifi_ap_0);
  old_context.cell_tower_context.push_back(cell_tower_0);
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  geolocation::GeopositionContext new_context;
  // If only one context is empty, significant displacement is implied.
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CacheEvictionOptions_CellularEquivalenceWithToleranceBaseCaseTest,
    testing::Values(geolocation::SimilarityDegree::kLoose,
                    geolocation::SimilarityDegree::kModerate,
                    geolocation::SimilarityDegree::kStrict));

using CacheEvictionOptions_CellularEquivalenceWithToleranceTest =
    testing::TestWithParam<std::tuple<geolocation::SimilarityDegree, bool>>;
TEST_P(CacheEvictionOptions_CellularEquivalenceWithToleranceTest,
       CheckToleranceLimits) {
  auto similarity_degree = std::get<0>(GetParam());
  auto shuffle_lac = std::get<1>(GetParam());
  geolocation::CellularEquivalenceWithTolerance eviction_strategy(
      similarity_degree);

  geolocation::GeopositionContext old_context;
  geolocation::GeopositionContext new_context;

  // Populate old context with `kNumScans` Cell Towers.
  // New context will be the subset of first `kNumScans*tolerance`
  // elements to check the boundary condition.
  PopulateCellTowers(old_context, kNumScans, shuffle_lac);
  PopulateCellTowers(new_context,
                     kNumScans * GetSimilarityThreshold(similarity_degree),
                     shuffle_lac);

  // Scans should be similar:
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));

  // Discard one of the WiFi APs from the new scan.
  new_context.cell_tower_context.pop_back();

  // Now the interseciton should be below threshold, expect significant
  // displacement:
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CacheEvictionOptions_CellularEquivalenceWithToleranceTest,
    testing::Combine(testing::Values(geolocation::SimilarityDegree::kLoose,
                                     geolocation::SimilarityDegree::kModerate,
                                     geolocation::SimilarityDegree::kStrict),
                     testing::Bool()));

using CacheEvictionOptions_HasCommonCellTowerTest = testing::Test;

TEST_F(CacheEvictionOptions_HasCommonCellTowerTest, CheckEmpty) {
  geolocation::HasCommonCellTower eviction_strategy;

  // Combination of empty contexts don't flag displacement.
  geolocation::GeopositionContext old_context;
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  // Identical contexts don't flag displacement.
  old_context.wifi_context.push_back(GetTestWifiAP());
  old_context.cell_tower_context.push_back(GetTestCellTower());
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  // If only one context is empty, significant displacement is implied.
  geolocation::GeopositionContext new_context;
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

TEST_F(CacheEvictionOptions_HasCommonCellTowerTest, CheckOnlyCellIdMatters) {
  geolocation::HasCommonCellTower cache_eviction;

  CellTower cell_tower_0;
  cell_tower_0.mcc = base::NumberToString(0);
  cell_tower_0.mnc = base::NumberToString(0);
  cell_tower_0.lac = base::NumberToString(0);
  cell_tower_0.ci = base::NumberToString(0);
  cell_tower_0.timestamp = base::Time();

  CellTower cell_tower_1;
  cell_tower_1.mcc = base::NumberToString(1);
  cell_tower_1.mnc = base::NumberToString(1);
  cell_tower_1.lac = base::NumberToString(1);
  cell_tower_1.ci = base::NumberToString(0);
  cell_tower_1.timestamp = base::Time();

  geolocation::GeopositionContext old_context;
  geolocation::GeopositionContext new_context;

  old_context.cell_tower_context.push_back(cell_tower_0);
  new_context.cell_tower_context.push_back(cell_tower_1);
  EXPECT_FALSE(cache_eviction.IsSignificantDisplacementIndicated(old_context,
                                                                 new_context));

  CellTower cell_tower_2 = cell_tower_0;
  cell_tower_2.ci = "1";
  new_context.cell_tower_context.clear();
  new_context.cell_tower_context.push_back(cell_tower_2);
  EXPECT_TRUE(cache_eviction.IsSignificantDisplacementIndicated(old_context,
                                                                new_context));
}

using CacheEvictionOptions_HasCommonWifiApAndCellTower = testing::Test;

TEST_F(CacheEvictionOptions_HasCommonWifiApAndCellTower, CheckEmpty) {
  geolocation::HasCommonWifiApAndCellTower eviction_strategy;

  // Combination of empty contexts don't flag displacement.
  geolocation::GeopositionContext old_context;
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  // Identical contexts don't flag displacement.
  old_context.wifi_context.push_back(GetTestWifiAP());
  old_context.cell_tower_context.push_back(GetTestCellTower());
  EXPECT_FALSE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, old_context));

  // If only one context is empty, significant displacement is implied.
  geolocation::GeopositionContext new_context;
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      old_context, new_context));
  EXPECT_TRUE(eviction_strategy.IsSignificantDisplacementIndicated(
      new_context, old_context));
}

TEST_F(CacheEvictionOptions_HasCommonWifiApAndCellTower,
       CheckOnlyCellIdMatters) {
  geolocation::HasCommonWifiApAndCellTower cache_eviction;

  geolocation::GeopositionContext old_context;
  old_context.wifi_context = {GetTestWifiAP(1)};
  old_context.cell_tower_context = {GetTestCellTower(1)};

  geolocation::GeopositionContext new_context;

  for (auto wifi_ap : {GetTestWifiAP(1), GetTestWifiAP(2)}) {
    for (auto cell_tower : {GetTestCellTower(1), GetTestCellTower(2)}) {
      new_context.wifi_context = {wifi_ap};
      new_context.cell_tower_context = {cell_tower};

      if (new_context.wifi_context == old_context.wifi_context &&
          new_context.cell_tower_context == old_context.cell_tower_context) {
        EXPECT_FALSE(cache_eviction.IsSignificantDisplacementIndicated(
            old_context, new_context));
      } else {
        EXPECT_TRUE(cache_eviction.IsSignificantDisplacementIndicated(
            old_context, new_context));
      }
    }
  }
}

}  // namespace ash
