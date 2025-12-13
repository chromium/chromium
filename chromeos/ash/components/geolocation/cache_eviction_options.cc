// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/cache_eviction_options.h"

#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/geolocation/cached_location_provider.h"
#include "chromeos/ash/components/network/network_util.h"

namespace ash::geolocation {
namespace {

// Returns the percentage of overlap (0.0 to 1.0) required to consider
// two scans "similar" enough to prevent eviction.
constexpr double GetSimilarityThreshold(
    const SimilarityDegree similarity_degree) {
  switch (similarity_degree) {
    case SimilarityDegree::kLoose:
      return 0.5;  // 50% overlap required
    case SimilarityDegree::kModerate:
      return 0.7;  // 70% overlap required
    case SimilarityDegree::kStrict:
      return 0.9;  // 90% overlap required
  }
  NOTREACHED();
}

template <typename T, typename TComparator>
  requires std::strict_weak_order<TComparator, const T&, const T&>
std::vector<T> GetIntersection(const std::vector<T>& scan_a,
                               const std::vector<T>& scan_b,
                               TComparator comparator) {
  // 1. Create sortable copies (Required by std::set_intersection).
  std::vector<T> sorted_a = scan_a;
  std::vector<T> sorted_b = scan_b;

  // 2. Sort the copies using the provided comparator.
  std::stable_sort(sorted_a.begin(), sorted_a.end(), comparator);
  std::stable_sort(sorted_b.begin(), sorted_b.end(), comparator);

  std::vector<T> result_intersection;

  // Reserve space for optimization.
  // The result can't be larger than the smallest input.
  result_intersection.reserve(std::min(scan_a.size(), scan_b.size()));

  // 3. Compute the intersection.
  std::set_intersection(sorted_a.begin(), sorted_a.end(), sorted_b.begin(),
                        sorted_b.end(), std::back_inserter(result_intersection),
                        comparator);

  return result_intersection;
}

}  // namespace

bool WifiEquivalenceWithTolerance::IsSignificantDisplacementIndicated(
    const GeopositionContext& context_a,
    const GeopositionContext& context_b) const {
  auto& wifi_scan_a = context_a.wifi_context;
  auto& wifi_scan_b = context_b.wifi_context;

  if (wifi_scan_a.empty() && wifi_scan_b.empty()) {
    return false;
  }

  auto wifi_scan_intersection =
      GetIntersection(wifi_scan_a, wifi_scan_b,
                      [](const WifiAccessPoint& a, const WifiAccessPoint& b) {
                        return a.mac_address < b.mac_address;
                      });

  return wifi_scan_intersection.size() <
         std::max(wifi_scan_a.size(), wifi_scan_b.size()) *
             GetSimilarityThreshold(similarity_degree_);
}

bool HasCommonWifiAP::IsSignificantDisplacementIndicated(
    const GeopositionContext& context_a,
    const GeopositionContext& context_b) const {
  auto& wifi_scan_a = context_a.wifi_context;
  auto& wifi_scan_b = context_b.wifi_context;

  if (wifi_scan_a.empty() && wifi_scan_b.empty()) {
    return false;
  }

  auto wifi_scan_intersection =
      GetIntersection(wifi_scan_a, wifi_scan_b,
                      [](const WifiAccessPoint& a, const WifiAccessPoint& b) {
                        return a.mac_address < b.mac_address;
                      });

  return wifi_scan_intersection.empty();
}

bool CellularEquivalenceWithTolerance::IsSignificantDisplacementIndicated(
    const GeopositionContext& context_a,
    const GeopositionContext& context_b) const {
  auto& cellular_scan_a = context_a.cell_tower_context;
  auto& cellular_scan_b = context_b.cell_tower_context;

  if (cellular_scan_a.empty() && cellular_scan_b.empty()) {
    return false;
  }

  CellTowerVector cellular_scan_intersection = GetIntersection(
      cellular_scan_a, cellular_scan_b,
      [](const CellTower& a, const CellTower& b) { return a.ci < b.ci; });

  return cellular_scan_intersection.size() <
         std::max(cellular_scan_a.size(), cellular_scan_b.size()) *
             GetSimilarityThreshold(similarity_degree_);
}

bool HasCommonCellTower::IsSignificantDisplacementIndicated(
    const GeopositionContext& context_a,
    const GeopositionContext& context_b) const {
  auto& cellular_scan_a = context_a.cell_tower_context;
  auto& cellular_scan_b = context_b.cell_tower_context;

  if (cellular_scan_a.empty() && cellular_scan_b.empty()) {
    return false;
  }

  CellTowerVector cellular_scan_intersection = GetIntersection(
      cellular_scan_a, cellular_scan_b,
      [](const CellTower& a, const CellTower& b) { return a.ci < b.ci; });

  return cellular_scan_intersection.empty();
}

bool HasCommonWifiApAndCellTower::IsSignificantDisplacementIndicated(
    const GeopositionContext& context_a,
    const GeopositionContext& context_b) const {
  const static HasCommonWifiAP has_common_wifi_ap;
  const static HasCommonCellTower has_common_cell_tower;

  return has_common_wifi_ap.IsSignificantDisplacementIndicated(context_a,
                                                               context_b) ||
         has_common_cell_tower.IsSignificantDisplacementIndicated(context_a,
                                                                  context_b);
}

}  // namespace ash::geolocation
