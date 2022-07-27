// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace sync_pb {
class LocalTrustedVaultDegradedRecoverabilityState;
}  // namespace sync_pb

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
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    virtual ~Delegate() = default;

    virtual void WriteDegradedRecoverabilityState(
        const sync_pb::LocalTrustedVaultDegradedRecoverabilityState&
            degraded_recoverability_state) = 0;
    virtual void OnDegradedRecoverabilityChanged(bool value) = 0;
  };

  // TODO(crbug.com/1247990): `refresh_callback` is currently used for testing,
  // should be replaced with the connection calls once it passed to the
  // scheduler.
  DegradedRecoverabilityScheduler(Delegate* delegate,
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
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
  GetDegradedRecoverabilityState() const;

  const raw_ptr<Delegate> delegate_;
  base::RepeatingClosure refresh_callback_;
  // A "timer" takes care of invoking Refresh() in the future, once after a
  // `current_refresh_period_` delay has elapsed.
  base::OneShotTimer next_refresh_timer_;
  base::TimeDelta current_refresh_period_;
  // The last time Refresh has executed, it's initially null until the first
  // Refresh() execution.
  base::TimeTicks last_refresh_time_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_DEGRADED_RECOVERABILITY_SCHEDULER_H_
