// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_EARLY_METRICS_GUARD_H_
#define COMPONENTS_METRICS_EARLY_METRICS_GUARD_H_

namespace metrics {

// RAII guard that indicates early startup metrics recording mode is active
// while in scope. Providers inheriting from EarlySafeMetricsProvider must
// check this guard to avoid emitting cycle-dependent lifecycle/status
// histograms that would otherwise be double-counted.
class EarlyMetricsGuard {
 public:
  EarlyMetricsGuard();
  ~EarlyMetricsGuard();

  EarlyMetricsGuard(const EarlyMetricsGuard&) = delete;
  EarlyMetricsGuard& operator=(const EarlyMetricsGuard&) = delete;

  // Returns true if the guard is currently active.
  static bool IsEarlyMetricsRecordingActive();
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_EARLY_METRICS_GUARD_H_
