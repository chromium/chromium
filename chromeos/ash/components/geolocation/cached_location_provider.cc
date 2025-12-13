// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/cached_location_provider.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/cache_eviction_options.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/network/network_util.h"
#include "chromeos/constants/chromeos_features.h"

namespace ash {
namespace {

constexpr char kCacheEvictionHistogramPrefix[] =
    "ChromeOS.Geolocation.CacheEviction";

// Converts degrees to radians.
inline double ToRadians(double degrees) {
  return degrees * std::numbers::pi / 180.0;
}

// Calculate the travel distance using the Haversine Formula.
// See https://en.wikipedia.org/wiki/Haversine_formula.
double CalculateDistance(double lat1, double lon1, double lat2, double lon2) {
  constexpr double kEarthRadiusMeters = 6371000.0;

  const double lat1_rad = ToRadians(lat1);
  const double lat2_rad = ToRadians(lat2);
  const double delta_lat_rad = ToRadians(lat2 - lat1);
  const double delta_lon_rad = ToRadians(lon2 - lon1);

  // hav(theta) = hav(delta_lat) + cos(lat1)*cos(lat2)*hav(delta_lon)
  double hav_theta = std::sin(delta_lat_rad / 2) * std::sin(delta_lat_rad / 2) +
                     std::cos(lat1_rad) * std::cos(lat2_rad) *
                         std::sin(delta_lon_rad / 2) *
                         std::sin(delta_lon_rad / 2);

  // Clamp hav(theta) to [0,1], otherwise floating point error could breach this
  // segment and result in NaN below.
  hav_theta = std::clamp(hav_theta, 0.0, 1.0);

  // Calculate theta from hav_theta.
  const double theta = 2 * std::asin(std::sqrt(hav_theta));

  // d = r * theta
  return kEarthRadiusMeters * theta;
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

// Constructs the full UMA histogram name used to record location displacement.
// e.g. ChromeOS.Geolocation.CacheEviction.CommonWifi.PredictedYes
std::string GetHistogramName(
    geolocation::CacheEvictionStrategy strategy,
    std::optional<geolocation::SimilarityDegree> similarity_degree,
    bool prediction) {
  switch (strategy) {
    case geolocation::CacheEvictionStrategy::kWifiTolerance:
    case geolocation::CacheEvictionStrategy::kCellularTolerance:
      CHECK(similarity_degree);
      break;
    case geolocation::CacheEvictionStrategy::kCommonWifi:
    case geolocation::CacheEvictionStrategy::kCommonCell:
    case geolocation::CacheEvictionStrategy::kCommonWifiAndCell:
      CHECK(!similarity_degree);
      break;
    default:
      NOTREACHED();
  }

  std::string_view eviction_name = EvictionStrategyToString(strategy);
  std::string_view eviction_similarity_degree =
      similarity_degree ? SimilarityDegreeToString(*similarity_degree) : "";
  std::string_view prediction_token =
      prediction ? "PredictedYes" : "PredictedNo";

  return base::StrCat({kCacheEvictionHistogramPrefix, ".", eviction_name,
                       eviction_similarity_degree, ".", prediction_token});
}

constexpr base::FeatureParam<geolocation::CacheEvictionStrategy>::Option
    kEvictionStrategyOptions[] = {
        {geolocation::CacheEvictionStrategy::kWifiTolerance, "wifi_tolerance"},
        {geolocation::CacheEvictionStrategy::kCommonWifi, "common_wifi"},
        {geolocation::CacheEvictionStrategy::kCellularTolerance,
         "cellular_tolerance"},
        {geolocation::CacheEvictionStrategy::kCommonCell, "common_cell"},
        {geolocation::CacheEvictionStrategy::kCommonWifiAndCell,
         "common_wifi_and_cell"},
};

const base::FeatureParam<bool> kCacheEvictionFieldTrialModeParam{
    &chromeos::features::kCachedLocationProvider, "field_trial_phase",
    false  // Disabled by default
};

const base::FeatureParam<geolocation::CacheEvictionStrategy>
    kCacheEvictionStrategyParam{
        &chromeos::features::kCachedLocationProvider, "strategy",
        geolocation::CacheEvictionStrategy::kWifiTolerance,
        &kEvictionStrategyOptions};

constexpr base::FeatureParam<geolocation::SimilarityDegree>::Option
    kEvictionStrategyToleranceOptions[] = {
        {geolocation::SimilarityDegree::kLoose, "loose_similarity"},
        {geolocation::SimilarityDegree::kModerate, "moderate_similarity"},
        {geolocation::SimilarityDegree::kStrict, "strict_similarity"},
};

const base::FeatureParam<geolocation::SimilarityDegree>
    kCacheEvictionToleranceParam{&chromeos::features::kCachedLocationProvider,
                                 "tolerance",
                                 geolocation::SimilarityDegree::kLoose,
                                 &kEvictionStrategyToleranceOptions};

// Returns the `CacheEviction` strategy configured by the feature parameter -
// `kCacheEvictionStrategyParam`.
std::unique_ptr<geolocation::CacheEviction> CreateEvictionStrategy() {
  geolocation::SimilarityDegree tolerance = kCacheEvictionToleranceParam.Get();

  switch (kCacheEvictionStrategyParam.Get()) {
    case geolocation::CacheEvictionStrategy::kCommonWifi:
      return std::make_unique<geolocation::HasCommonWifiAP>();
    case geolocation::CacheEvictionStrategy::kCellularTolerance:
      return std::make_unique<geolocation::CellularEquivalenceWithTolerance>(
          tolerance);
    case geolocation::CacheEvictionStrategy::kCommonCell:
      return std::make_unique<geolocation::HasCommonCellTower>();
    case geolocation::CacheEvictionStrategy::kCommonWifiAndCell:
      return std::make_unique<geolocation::HasCommonWifiApAndCellTower>();
    case geolocation::CacheEvictionStrategy::kWifiTolerance:
    default:
      return std::make_unique<geolocation::WifiEquivalenceWithTolerance>(
          tolerance);
  }
}

}  // namespace

CachedLocationProvider::GeopositionCache::GeopositionCache() = default;

CachedLocationProvider::GeopositionCache::GeopositionCache(
    Geoposition position,
    base::TimeTicks fetch_time,
    std::optional<geolocation::GeopositionContext> context)
    : position(position), fetch_time(fetch_time), context(std::move(context)) {}

CachedLocationProvider::GeopositionCache::GeopositionCache(
    CachedLocationProvider::GeopositionCache&&) = default;

CachedLocationProvider::GeopositionCache&
CachedLocationProvider::GeopositionCache::operator=(
    CachedLocationProvider::GeopositionCache&&) = default;

CachedLocationProvider::GeopositionCache::~GeopositionCache() = default;

CachedLocationProvider::CachedLocationProvider(
    std::unique_ptr<LocationFetcher> location_fetcher)
    : LocationProvider(std::move(location_fetcher)),
      cache_eviction_method_(CreateEvictionStrategy()) {}

CachedLocationProvider::~CachedLocationProvider() = default;

void CachedLocationProvider::RequestLocation(base::TimeDelta timeout,
                                             bool send_wifi_access_points,
                                             bool send_cell_towers,
                                             ResponseCallback callback) {
  std::optional<GeopositionCache>* cache_to_use;
  std::optional<geolocation::GeopositionContext> context_to_use;
  std::optional<geolocation::CacheEviction*> eviction_to_use;

  // Determine if this is a Precise or Coarse request.
  if (send_wifi_access_points || send_cell_towers) {
    cache_to_use = &precise_location_cache_;
    context_to_use.emplace(GetPreciseLocationContext());
    eviction_to_use = cache_eviction_method_.get();
  } else {
    cache_to_use = &coarse_location_cache_;
    context_to_use = std::nullopt;
    eviction_to_use = std::nullopt;
  }

  // Return cached position whenever possible.
  if (IsCacheUsable(*cache_to_use, context_to_use, eviction_to_use)) {
    VLOG(1) << "Cache hit";
    std::move(callback).Run(cache_to_use->value().position, false,
                            base::Seconds(0));
    return;
  }
  VLOG(1) << "Cache miss!";

  // Initiate a new network request. We bind `cache_to_use` (pointer to member)
  // so the correct cache is updated when the request completes.
  location_fetcher_->RequestGeolocation(
      timeout, send_wifi_access_points, send_cell_towers,
      base::BindOnce(&CachedLocationProvider::OnLocationResolved,
                     weak_ptr_factory_.GetWeakPtr(), cache_to_use,
                     std::move(context_to_use), std::move(callback)));
}

const std::vector<CachedLocationProvider::EvictionStrategyPair>*
CachedLocationProvider::GetEvictionStrategiesUnderTest() {
  static const base::NoDestructor<std::vector<EvictionStrategyPair>>
      kEvictionList([] {
        std::vector<EvictionStrategyPair> eviction_strategies;

        // Pre-allocate to avoid resize overhead
        eviction_strategies.reserve(9);

        for (auto similarity_degree :
             {geolocation::SimilarityDegree::kLoose,
              geolocation::SimilarityDegree::kModerate,
              geolocation::SimilarityDegree::kStrict}) {
          eviction_strategies.emplace_back(
              std::make_unique<geolocation::WifiEquivalenceWithTolerance>(
                  similarity_degree),
              std::make_optional(similarity_degree));
          eviction_strategies.emplace_back(
              std::make_unique<geolocation::CellularEquivalenceWithTolerance>(
                  similarity_degree),
              std::make_optional(similarity_degree));
        }

        eviction_strategies.emplace_back(
            std::make_unique<geolocation::HasCommonWifiAP>(), std::nullopt);

        eviction_strategies.emplace_back(
            std::make_unique<geolocation::HasCommonCellTower>(), std::nullopt);

        eviction_strategies.emplace_back(
            std::make_unique<geolocation::HasCommonWifiApAndCellTower>(),
            std::nullopt);

        return eviction_strategies;
      }());

  return kEvictionList.get();
}

bool CachedLocationProvider::IsCacheUsable(
    const std::optional<GeopositionCache>& location_cache,
    const std::optional<geolocation::GeopositionContext>& new_context,
    const std::optional<geolocation::CacheEviction*> eviction_strategy) {
  if (IsFieldTrialPhase()) {
    return false;
  }

  if (!location_cache) {
    return false;
  }

  // 1. Time-based validity:
  // If the cache is fresh (fetched within the rate limit window), reuse it
  // immediately regardless of potential movement.
  if (base::TimeTicks::Now() - location_cache->fetch_time <= rate_limit_) {
    return true;
  }

  // 2. Spatial validity (Precise Location only):
  // If the cache is stale by time, use selected `CacheEviction` heuristics.
  if (eviction_strategy) {
    CHECK(new_context);
    return !eviction_strategy.value()->IsSignificantDisplacementIndicated(
        location_cache->context.value(), new_context.value());
  }

  return false;
}

void CachedLocationProvider::OnLocationResolved(
    std::optional<GeopositionCache>* cache_to_use,
    std::optional<geolocation::GeopositionContext> request_context,
    ResponseCallback callback,
    const Geoposition& position,
    bool server_error,
    base::TimeDelta elapsed) {
  // If the request succeeded, update the specific cache member.
  if (!server_error && position.Valid()) {
    VLOG(1) << (request_context ? "Precise " : "Coarse ") << "cache updated!";
    GeopositionCache new_cache{position, base::TimeTicks::Now(),
                               std::move(request_context)};

    if (IsFieldTrialPhase() && *cache_to_use) {
      ReportFieldTrialMetrics(cache_to_use->value(), new_cache);
    }

    *cache_to_use = std::move(new_cache);
  }

  std::move(callback).Run(position, server_error, elapsed);
}

geolocation::GeopositionContext
CachedLocationProvider::GetPreciseLocationContext() {
  geolocation::GeopositionContext current_context;
  location_fetcher_->GetNetworkInformation(&current_context.wifi_context,
                                           &current_context.cell_tower_context);

  return current_context;
}

bool CachedLocationProvider::IsFieldTrialPhase() {
  return kCacheEvictionFieldTrialModeParam.Get();
}

void CachedLocationProvider::ReportFieldTrialMetrics(
    const GeopositionCache& old_cache,
    const GeopositionCache& new_cache) {
  if (!old_cache.context || !new_cache.context) {
    return;
  }

  // Distance travelled between the requests.
  auto distance_meters = CalculateDistance(
      old_cache.position.latitude, old_cache.position.longitude,
      new_cache.position.latitude, new_cache.position.longitude);

  // Now that we know the distance travelled, collect fitness metrics on all
  // eviction strategies under test.
  for (const auto& strategy_pair : *GetEvictionStrategiesUnderTest()) {
    const auto& strategy = strategy_pair.first;
    const auto similarity_degree = strategy_pair.second;

    bool prediction = strategy->IsSignificantDisplacementIndicated(
        old_cache.context.value(), new_cache.context.value());

    std::string histogram_name =
        GetHistogramName(strategy->strategy(), similarity_degree, prediction);

    // Use CustomHistogram for exact boundary control.
    static const base::NoDestructor<std::vector<int>> kCustomRanges(
        {1, 10, 100, 1000, 10000, 100000, 1000000});

    base::HistogramBase* histogram = base::CustomHistogram::FactoryGet(
        histogram_name, *kCustomRanges,
        base::HistogramBase::kUmaTargetedHistogramFlag);

    histogram->Add(static_cast<int>(distance_meters));
  }
}

}  // namespace ash
