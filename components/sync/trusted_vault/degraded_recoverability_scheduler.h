// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"

namespace syncer {

// Schedules refresh of degraded recoverability state based on the current
// state, heuristics and last refresh time.
class DegradedRecoverabilityScheduler {
 public:
  DegradedRecoverabilityScheduler();
  DegradedRecoverabilityScheduler(const DegradedRecoverabilityScheduler&) =
      delete;
  DegradedRecoverabilityScheduler& operator=(
      const DegradedRecoverabilityScheduler&) = delete;
  ~DegradedRecoverabilityScheduler() = default;

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
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_
