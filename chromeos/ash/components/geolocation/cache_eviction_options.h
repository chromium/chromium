// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_GEOLOCATION_CACHE_EVICTION_OPTIONS_H_
#define CHROMEOS_ASH_COMPONENTS_GEOLOCATION_CACHE_EVICTION_OPTIONS_H_

#include "chromeos/ash/components/geolocation/geoposition_context.h"

namespace ash::geolocation {

// The full list of strategies considered for the `CachedLocationProvided`.
enum class CacheEvictionStrategy {
  kWifiTolerance = 0,
  kCommonWifi,
  kCellularTolerance,
  kCommonCell,
  kCommonWifiAndCell,
  kMaxValue = kCommonWifiAndCell,
};

// Similarity degree needed for two network scans (e.g. Wifi AP scans) to be
// considered equal. Used by the `CacheEviction` classes with the tolerance
// parameter.
enum class SimilarityDegree {
  kLoose = 0,
  kModerate,
  kStrict,
  kMaxValue = kStrict,
};

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) CacheEviction {
 public:
  explicit CacheEviction(CacheEvictionStrategy strategy)
      : strategy_(strategy) {}
  virtual ~CacheEviction() = default;

  // Determines if the significant physical displacement is implied by the
  // difference of the two context points. This would trigger a cache eviction
  // action.
  //
  // The definition of "significant" is implementation-dependent, but all
  // concrete implementations must provide a policy relevant for the ChromeOS
  // system services (all `SystemLocationProvider` clients).
  virtual bool IsSignificantDisplacementIndicated(
      const GeopositionContext& context_a,
      const GeopositionContext& context_b) const = 0;

  CacheEvictionStrategy strategy() const { return strategy_; }

 private:
  CacheEvictionStrategy strategy_;
};

// Eviction strategy based on the overlap of visible WiFi Access Points (APs).
// Calculates the intersection of APs between two scans. If the intersection
// is large enough (determined by `similarity_degree`), the device is assumed to
// be stationary (no eviction).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    WifiEquivalenceWithTolerance : public CacheEviction {
 public:
  // `similarity_degree` represents the required overlap ratio (0.0 to 1.0) to
  // assume stillness. A higher `similarity_degree` requires a larger overlap to
  // keep the cache valid, meaning the cache expires more easily.
  explicit WifiEquivalenceWithTolerance(SimilarityDegree similarity_degree)
      : CacheEviction(CacheEvictionStrategy::kWifiTolerance),
        similarity_degree_(similarity_degree) {}

  // CacheEviction:
  bool IsSignificantDisplacementIndicated(
      const GeopositionContext& context_a,
      const GeopositionContext& context_b) const override;

 private:
  SimilarityDegree similarity_degree_;
};

// Minimalist strategy: Considers the cached location valid as long as there is
// at least one common WiFi Access Point between the two scans. Only evicts if
// the sets are completely disjoint (no intersection).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) HasCommonWifiAP
    : public CacheEviction {
 public:
  HasCommonWifiAP() : CacheEviction(CacheEvictionStrategy::kCommonWifi) {}
  // CacheEviction:
  bool IsSignificantDisplacementIndicated(
      const GeopositionContext& context_a,
      const GeopositionContext& context_b) const override;
};

// Eviction strategy based on the overlap of visible Cell Towers.
// Calculates the intersection of Cell Towers between two scans. If the
// intersection is large enough (determined by `similarity_degree`), the device
// is assumed to be stationary (no eviction).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    CellularEquivalenceWithTolerance : public CacheEviction {
 public:
  // `similarity_degree` represents the required overlap ratio (0.0 to 1.0) to
  // assume stillness. A higher `similarity_degree` requires a larger overlap to
  // keep the cache valid, meaning the cache expires more easily.
  explicit CellularEquivalenceWithTolerance(SimilarityDegree similarity_degree)
      : CacheEviction(CacheEvictionStrategy::kCellularTolerance),
        similarity_degree_(similarity_degree) {}

  // CacheEviction:
  bool IsSignificantDisplacementIndicated(
      const GeopositionContext& context_a,
      const GeopositionContext& context_b) const override;

 private:
  SimilarityDegree similarity_degree_;
};

// Minimalist strategy: Considers the cached location valid as long as there is
// at least one common Cell Tower between the two scans. Only evicts if
// the sets are completely disjoint (no intersection).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION) HasCommonCellTower
    : public CacheEviction {
 public:
  HasCommonCellTower() : CacheEviction(CacheEvictionStrategy::kCommonCell) {}
  // CacheEviction:
  bool IsSignificantDisplacementIndicated(
      const GeopositionContext& context_a,
      const GeopositionContext& context_b) const override;
};

// Hybrid strategy that monitors both WiFi and Cellular signals to detect
// movement.
// It indicates significant displacement if *either* the WiFi scan *or* the
// Cellular scan fails to find common elements with the previous state.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_GEOLOCATION)
    HasCommonWifiApAndCellTower : public CacheEviction {
 public:
  HasCommonWifiApAndCellTower()
      : CacheEviction(CacheEvictionStrategy::kCommonWifiAndCell) {}
  // CacheEviction:
  bool IsSignificantDisplacementIndicated(
      const GeopositionContext& context_a,
      const GeopositionContext& context_b) const override;
};

}  // namespace ash::geolocation

#endif  // CHROMEOS_ASH_COMPONENTS_GEOLOCATION_CACHE_EVICTION_OPTIONS_H_
