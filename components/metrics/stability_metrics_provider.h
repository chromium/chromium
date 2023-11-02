// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/metrics_provider.h"

class PrefService;
class PrefRegistrySimple;

namespace metrics {

class SystemProfileProto;

// Stores and loads system information to prefs for stability logs.
class StabilityMetricsProvider : public MetricsProvider {
 public:
  explicit StabilityMetricsProvider(PrefService* local_state);

  StabilityMetricsProvider(const StabilityMetricsProvider&) = delete;
  StabilityMetricsProvider& operator=(const StabilityMetricsProvider&) = delete;

  ~StabilityMetricsProvider() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void LogCrash(base::Time last_live_timestamp);
  void LogLaunch();

 private:
#if BUILDFLAG(IS_WIN)
  // This function is virtual for testing. The |last_live_timestamp| is a
  // time point where the previous browser was known to be alive, and is used
  // to determine whether the system session embedding that timestamp terminated
  // uncleanly.
  virtual bool IsUncleanSystemSession(base::Time last_live_timestamp);
  void MaybeLogSystemCrash(base::Time last_live_timestamp);
#endif
  // Increments an Integer pref value specified by |path|.
  void IncrementPrefValue(const char* path);

  // Gets pref value specified by |path| and resets it to 0 after retrieving.
  int GetAndClearPrefValue(const char* path, int* value);

  // MetricsProvider:
  void Init() override;
  void ClearSavedStabilityMetrics() override;
  void ProvideStabilityMetrics(
      SystemProfileProto* system_profile_proto) override;

  raw_ptr<PrefService> local_state_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_
