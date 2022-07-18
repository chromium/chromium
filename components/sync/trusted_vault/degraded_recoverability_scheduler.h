// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_

#include "base/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace syncer {
// Exposed only for testing.
constexpr base::TimeDelta kLongDegradedRecoverabilityRefreshPeriod =
    base::Days(7);
constexpr base::TimeDelta kShortDegradedRecoverabilityRefreshPeriod =
    base::Hours(1);

// Schedules refresh of degraded recoverability state based on the current
// state, heuristics and last refresh time.
class DegradedRecoverabilityScheduler {
 public:
  explicit DegradedRecoverabilityScheduler(
      base::RepeatingClosure refresh_callback);
  DegradedRecoverabilityScheduler(const DegradedRecoverabilityScheduler&) =
      delete;
  DegradedRecoverabilityScheduler& operator=(
      const DegradedRecoverabilityScheduler&) = delete;
  ~DegradedRecoverabilityScheduler();

  void StartLongIntervalRefreshing();
  void StartShortIntervalRefreshing();
  void RefreshImmediately();

 private:
  void Start();
  void Refresh();

  // A "timer" takes care of invoking Refresh() in the future, once after a
  // `current_refresh_period_` delay has elapsed.
  base::OneShotTimer next_refresh_timer_;
  base::TimeDelta current_refresh_period_;
  // The last time Refresh has executed, it's initially null until the first
  // Refresh() execution.
  base::TimeTicks last_refreshed_time_;
  base::RepeatingClosure refresh_callback_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_
