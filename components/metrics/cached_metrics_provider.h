// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CACHED_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CACHED_METRICS_PROVIDER_H_

#include "components/metrics/early_safe_metrics_provider.h"

namespace metrics {

class SystemProfileProto;

// Base class for providers that fetch late-initialized hardware or system data.
// During early startup (when synchronous IPCs and blocking operations are
// forbidden), it populates the fallback system profile by reading from the
// previous session's system profile persisted by EnvironmentRecorder
// (prefs::kStabilitySavedSystemProfile) via EarlyMetricsGuard.
class CachedMetricsProvider : public EarlySafeMetricsProvider {
 public:
  CachedMetricsProvider();

  CachedMetricsProvider(const CachedMetricsProvider&) = delete;
  CachedMetricsProvider& operator=(const CachedMetricsProvider&) = delete;

  ~CachedMetricsProvider() override;

  // EarlySafeMetricsProvider:
  void ProvideSystemProfileMetrics(SystemProfileProto* system_profile) override;

 protected:
  // Called during early startup to populate `system_profile` from the previous
  // session's cached system profile (`cached_system_profile`). Subclasses MUST
  // NOT perform any blocking operations or IPCs here.
  virtual void ProvideSystemProfileMetricsFromCache(
      const SystemProfileProto& cached_system_profile,
      SystemProfileProto* system_profile) = 0;

  // Called during normal startup/operation (outside EarlyMetricsGuard) to
  // populate `system_profile` with live data. If the live data is not available
  // yet, implementations must return without writing anything (no placeholder
  // or default values): this profile is saved to Local State and becomes the
  // next session's early-startup cache.
  virtual void ProvideSystemProfileMetricsNormal(
      SystemProfileProto* system_profile) = 0;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CACHED_METRICS_PROVIDER_H_
