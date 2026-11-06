// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
  // `local_state` must outlive this provider.
  explicit StabilityMetricsProvider(PrefService* local_state);

  StabilityMetricsProvider(const StabilityMetricsProvider&) = delete;
  StabilityMetricsProvider& operator=(const StabilityMetricsProvider&) = delete;

  ~StabilityMetricsProvider() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  void LogCrash(base::Time last_live_timestamp);
  void LogLaunch();

#if BUILDFLAG(IS_WIN)
  // Determines whether the system session that contained `last_live_timestamp`
  // ended uncleanly. Invoked on a blocking thread-pool sequence.
  using IsUncleanSystemSessionCallback =
      base::RepeatingCallback<bool(base::Time last_live_timestamp)>;

  // Overrides the unclean-session check with `callback` in tests so they do not
  // traverse the real Windows System event log. Must be called before
  // LogCrash().
  void SetIsUncleanSystemSessionCallbackForTesting(
      IsUncleanSystemSessionCallback callback);
#endif

 private:
#if BUILDFLAG(IS_WIN)
  // Posts the unclean-session check to a blocking thread-pool sequence and, if
  // the previous system session ended uncleanly, increments the persisted
  // system-crash counter back on the calling sequence.
  void MaybeLogSystemCrash(base::Time last_live_timestamp);
  void OnSystemSessionChecked(bool unclean);
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

#if BUILDFLAG(IS_WIN)
  // Computes whether the previous system session ended uncleanly. Defaults to
  // the SystemSessionAnalyzer-based check; tests may override it via
  // SetIsUncleanSystemSessionCallbackForTesting().
  IsUncleanSystemSessionCallback is_unclean_system_session_callback_;

  base::WeakPtrFactory<StabilityMetricsProvider> weak_factory_{this};
#endif
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_STABILITY_METRICS_PROVIDER_H_
