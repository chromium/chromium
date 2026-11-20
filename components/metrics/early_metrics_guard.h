// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EARLY_METRICS_GUARD_H_
#define COMPONENTS_METRICS_EARLY_METRICS_GUARD_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "third_party/metrics_proto/system_profile.pb.h"

class PrefService;

namespace metrics {

// RAII guard that indicates early startup metrics recording mode is active
// while in scope. Providers inheriting from EarlySafeMetricsProvider must
// check this guard to avoid emitting cycle-dependent lifecycle/status
// histograms that would otherwise be double-counted.
//
// When constructed with `local_state`, it also lazily loads and caches the
// previous session's system profile (persisted by EnvironmentRecorder) on
// demand for the duration of the guard's scope.
class EarlyMetricsGuard {
 public:
  explicit EarlyMetricsGuard(PrefService* local_state = nullptr);
  ~EarlyMetricsGuard();

  EarlyMetricsGuard(const EarlyMetricsGuard&) = delete;
  EarlyMetricsGuard& operator=(const EarlyMetricsGuard&) = delete;

  // Returns true if the guard is currently active.
  static bool IsEarlyMetricsRecordingActive();

  // Returns the cached system profile from the previous session, lazily decoded
  // once per guard lifetime. Returns nullptr if no guard is active,
  // `local_state` is null, or cache loading fails.
  static const SystemProfileProto* GetCachedSystemProfile();

 private:
  const SystemProfileProto* GetOrLoadCachedSystemProfile();

  raw_ptr<PrefService> local_state_;
  bool cache_loaded_ = false;
  std::optional<SystemProfileProto> cached_system_profile_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EARLY_METRICS_GUARD_H_
