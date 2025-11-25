// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/geolocation/cached_location_provider.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chromeos/ash/components/geolocation/location_fetcher.h"
#include "chromeos/ash/components/network/network_util.h"

namespace ash {

// Evict the cache if the set of surrounding unique Wi-Fi Access Points or
// Cellular towers has changed. A change in signal strength is ignored.
class ScanEqualityEviction : public CachedLocationProvider::CacheEviction {
 public:
  bool IsSignificantDisplacementIndicated(
      const CachedLocationProvider::GeopositionCache::Context& context_a,
      const CachedLocationProvider::GeopositionCache::Context& context_b)
      const override {
    bool is_wifi_scan_same =
        std::equal(context_a.wifi_context.begin(), context_a.wifi_context.end(),
                   context_b.wifi_context.begin(), context_b.wifi_context.end(),
                   [](const WifiAccessPoint& a, const WifiAccessPoint& b) {
                     return a.mac_address == b.mac_address;
                   });
    bool is_cellular_scan_same = std::equal(
        context_a.cell_tower_context.begin(),
        context_a.cell_tower_context.end(),
        context_b.cell_tower_context.begin(),
        context_b.cell_tower_context.end(),
        [](const CellTower& a, const CellTower& b) { return a.ci == b.ci; });

    return !is_wifi_scan_same || !is_cellular_scan_same;
  }
};

CachedLocationProvider::GeopositionCache::Context::Context() = default;

CachedLocationProvider::GeopositionCache::Context::Context(Context&& other) =
    default;

CachedLocationProvider::GeopositionCache::Context&
CachedLocationProvider::GeopositionCache::Context::operator=(Context&& other) =
    default;

CachedLocationProvider::GeopositionCache::Context::~Context() = default;

CachedLocationProvider::GeopositionCache::GeopositionCache() = default;

CachedLocationProvider::GeopositionCache::GeopositionCache(
    Geoposition position,
    base::TimeTicks fetch_time,
    std::optional<Context> context)
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
      cache_eviction_method_(std::make_unique<ScanEqualityEviction>()) {}
CachedLocationProvider::~CachedLocationProvider() = default;

void CachedLocationProvider::RequestLocation(base::TimeDelta timeout,
                                             bool send_wifi_access_points,
                                             bool send_cell_towers,
                                             ResponseCallback callback) {
  std::optional<GeopositionCache>* cache_to_use;
  std::optional<GeopositionCache::Context> context_to_use;
  std::optional<CacheEviction*> eviction_to_use;

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
    std::move(callback).Run(cache_to_use->value().position, false,
                            base::Seconds(0));
    return;
  }

  // Initiate a new network request. We bind `cache_to_use` (pointer to member)
  // so the correct cache is updated when the request completes.
  location_fetcher_->RequestGeolocation(
      timeout, send_wifi_access_points, send_cell_towers,
      base::BindOnce(&CachedLocationProvider::OnLocationResolved,
                     weak_ptr_factory_.GetWeakPtr(), cache_to_use,
                     std::move(context_to_use), std::move(callback)));
}

bool CachedLocationProvider::IsCacheUsable(
    const std::optional<GeopositionCache>& location_cache,
    const std::optional<GeopositionCache::Context>& new_context,
    const std::optional<CacheEviction*> eviction_strategy) {
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
    std::optional<GeopositionCache::Context> request_context,
    ResponseCallback callback,
    const Geoposition& position,
    bool server_error,
    base::TimeDelta elapsed) {
  // If the request succeeded, update the specific cache member.
  if (!server_error && position.Valid()) {
    cache_to_use->emplace(GeopositionCache(position, base::TimeTicks::Now(),
                                           std::move(request_context)));
  }

  std::move(callback).Run(position, server_error, elapsed);
}

CachedLocationProvider::GeopositionCache::Context
CachedLocationProvider::GetPreciseLocationContext() {
  GeopositionCache::Context current_context;
  location_fetcher_->GetNetworkInformation(&current_context.wifi_context,
                                           &current_context.cell_tower_context);

  return current_context;
}

}  // namespace ash
