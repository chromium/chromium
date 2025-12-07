// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_CACHED_LOCATION_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_CACHED_LOCATION_PROVIDER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/cache_eviction_options.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/geoposition_context.h"
#include "chromeos/ash/components/geolocation/location_provider.h"
#include "chromeos/ash/components/network/network_util.h"

namespace ash {
namespace geolocation {
class CacheEviction;
}  // namespace geolocation

// Manages caching for resolved geolocation data to optimize performance and
// control remote API usage.
//
// The class maintains separate caches for coarse (IP-based) and precise
// (wireless signal-based) locations to adhere to strict privacy requirements.
//
// 1. Rate Limiting: Implements a throttling mechanism to govern the frequency
// of outbound requests and prevent resource depletion of the Geolocation API
// Web Service.
// 2. Cache Validity: Requests are not always served with real-time data. For
// precise location calls, cached data is returned if the underlying wireless
// signals have not changed significantly, based on defined displacement
// criteria.
//
// NOTE: While `IsFieldTrialPhase()`, this class will behave just like
// `LiveLocationProvider`, applying NO optimizations and serving all requests
// with a real-time (live) location through the remote API calls.
// During this phase fitness metrics on the selected `CacheEviction` methods
// will be collected.
//
// TODO(crbug.com/463591748): Refactor to avoid conditional handling of
// the coarse/precise flows.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    CachedLocationProvider : public LocationProvider {
 public:
  using EvictionStrategyPair =
      std::pair<std::unique_ptr<geolocation::CacheEviction>,
                std::optional<geolocation::SimilarityDegree>>;

  explicit CachedLocationProvider(
      std::unique_ptr<LocationFetcher> location_fetcher);
  ~CachedLocationProvider() override;

  // LocationProvider:
  void RequestLocation(base::TimeDelta timeout,
                       bool use_wifi,
                       bool use_cell_towers,
                       ResponseCallback callback) override;

  // Exposes the list of eviction strategies used in the field trial.
  static const std::vector<EvictionStrategyPair>*
  GetEvictionStrategiesUnderTest();

  base::TimeDelta GetRateLimitForTesting() { return rate_limit_; }
  geolocation::CacheEviction* GetEvictionStrategyForTesting() {
    return cache_eviction_method_.get();
  }

 private:
  // Represents a single location cache entry. It combines the computed
  // position with the context used to generate it.
  struct GeopositionCache {
    // The actual latitude, longitude, and accuracy of the position.
    Geoposition position;

    // The time the Geoposition was successfully fetched from the server, it's
    // the time when the location was received, not when it was resolved on
    // remote end. Used for determining cache freshness/expiration.
    base::TimeTicks fetch_time;

    // The Wi-Fi and cell tower information used as input to generate the
    // `position`. This is optional because coarse location requests don't use
    // this field.
    std::optional<geolocation::GeopositionContext> context;

    GeopositionCache();
    GeopositionCache(Geoposition,
                     base::TimeTicks,
                     std::optional<geolocation::GeopositionContext>);

    // Move-only.
    GeopositionCache(const GeopositionCache&) = delete;
    GeopositionCache& operator=(const GeopositionCache&) = delete;
    GeopositionCache(GeopositionCache&&);
    GeopositionCache& operator=(GeopositionCache&&);

    ~GeopositionCache();
  };

  // Checks if the specified `location_cache` is valid and fresh enough to
  // serve the current request.
  //
  // Returns true if the cache is within the time limit and, for precise
  // requests, if the displacement since the last fetch is not expected to be
  // significant according to `eviction_strategy`.
  //
  // `new_context` and `eviction_strategy` must be null for coarse location
  // checks.
  bool IsCacheUsable(
      const std::optional<GeopositionCache>& location_cache,
      const std::optional<geolocation::GeopositionContext>& new_context,
      const std::optional<geolocation::CacheEviction*> eviction_strategy);

  // Callback for the remote Geolocation API request.
  //
  // Upon a successful resolution, updates the specific cache pointed to by
  // `cache_to_use` (either the coarse or precise member cache) and passes the
  // result to the original `callback`.
  void OnLocationResolved(
      std::optional<GeopositionCache>* cache_to_use,
      std::optional<geolocation::GeopositionContext> request_context,
      ResponseCallback callback,
      const Geoposition& position,
      bool server_error,
      base::TimeDelta elapsed);

  // Retrieves the current Wi-Fi and cell tower data for precise location.
  geolocation::GeopositionContext GetPreciseLocationContext();

  // Returns true if the provider is currently running a field trial
  // experiment to measure fitness for each eviction strategy under test.
  // During this phase, the caching optimizations are disabled and this class
  // effectively mimics the `LiveLocationProvider` to collect the ground truth
  // data.
  // TODO(crbug.com/465074906): Rename to InMetricsCollectionMode().
  bool IsFieldTrialPhase();

  // Collects metrics for the selected cache eviction methods.
  // For each `CacheEviction` strategy under test, populates 2 different UMA
  // histograms:
  // "<EvictionName>.PredictedYes" - emits actual displacement (in meters) if
  //        the strategy predicts "Significant Displacement" on the
  //        {`old_cache`,`new_cache`} pair.
  // "<EvictionName>.PredictedNo" - emits actual displacement (in meters) if
  //        the strategy DOES NOT predict "Significant Displacement" on the
  //        {`old_cache`, `new_cache`} pair.
  //
  //  `CacheEvictionStrategy` enum lists all eviction strategies being tested.
  void ReportFieldTrialMetrics(const GeopositionCache& old_cache,
                               const GeopositionCache& new_cache);

  // The minimum time that must elapse between successful outbound requests.
  // Applies to both Precise and Coarse requests.
  // If a request is made before this duration has passed since the last
  // successful fetch, the cached position will be returned instead of
  // initiating a new network request.
  base::TimeDelta rate_limit_ = base::Hours(1);

  // Stores the most recently resolved, respectively coarse and precise,
  // locations. These caches are separate for privacy reasons.
  std::optional<GeopositionCache> coarse_location_cache_;
  std::optional<GeopositionCache> precise_location_cache_;

  // The strategy object used to determine if two contexts represent a
  // significant displacement to clear the cache.
  std::unique_ptr<geolocation::CacheEviction> cache_eviction_method_;

  base::WeakPtrFactory<CachedLocationProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_CACHED_LOCATION_PROVIDER_H_
