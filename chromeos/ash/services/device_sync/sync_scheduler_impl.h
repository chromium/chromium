// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNC_SCHEDULER_IMPL_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNC_SCHEDULER_IMPL_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/device_sync/sync_scheduler.h"

namespace ash {

namespace device_sync {

// Implementation of SyncScheduler.
class SyncSchedulerImpl : public SyncScheduler {
 public:
  // Creates the scheduler:
  // |delegate|: Handles sync requests and must outlive the scheduler.
  // |refresh_period|: The time to wait for the PERIODIC_REFRESH strategy.
  // |base_recovery_period|: The initial time to wait for the
  //    AGGRESSIVE_RECOVERY strategy. The time delta is increased for each
  //    subsequent failure.
  // |max_jitter_ratio|: The maximum ratio that the time to next sync can be
  //    jittered (both positively and negatively).
  // |scheduler_name|: The name of the scheduler for debugging purposes.
  SyncSchedulerImpl(Delegate* delegate,
                    base::TimeDelta refresh_period,
                    base::TimeDelta base_recovery_period,
                    double max_jitter_ratio,
                    const std::string& scheduler_name);

  SyncSchedulerImpl(const SyncSchedulerImpl&) = delete;
  SyncSchedulerImpl& operator=(const SyncSchedulerImpl&) = delete;

  ~SyncSchedulerImpl() override;

  // SyncScheduler:
  void Start(const base::TimeDelta& elapsed_time_since_last_sync,
             Strategy strategy) override;
  void ForceSync() override;
  base::TimeDelta GetTimeToNextSync() const override;
  Strategy GetStrategy() const override;
  SyncState GetSyncState() const override;

 protected:
  // Creates and returns a base::OneShotTimer object. Exposed for testing.
  virtual std::unique_ptr<base::OneShotTimer> CreateTimer();

 private:
  // SyncScheduler:
  void OnSyncCompleted(bool success) override;

  // Called when |timer_| is fired.
  void OnTimerFired();

  // Schedules |timer_| for the next sync request.
  void ScheduleNextSync(const base::TimeDelta& sync_delta);

  // Adds a random jitter to the value of GetPeriod(). The returned
  // TimeDelta will be clamped to be non-negative.
  base::TimeDelta GetJitteredPeriod();

  // Returns the time to wait for the current strategy.
  base::TimeDelta GetPeriod();

  // The delegate handling sync requests when they are fired.
  const raw_ptr<Delegate> delegate_;

  // The time to wait until the next refresh when the last sync attempt was
  // successful.
  const base::TimeDelta refresh_period_;

  // The base recovery period for the AGGRESSIVE_RECOVERY strategy before
  // backoffs are applied.
  const base::TimeDelta base_recovery_period_;

  // The maximum percentage (both positively and negatively) that the time to
  // wait between each sync request is jittered. The jitter is randomly applied
  // to each period so we can avoid synchronous calls to the server.
  const double max_jitter_ratio_;

  // The name of the scheduler, used for debugging purposes.
  const std::string scheduler_name_;

  // The current strategy of the scheduler.
  Strategy strategy_;

  // The current state of the scheduler.
  SyncState sync_state_;

  // The number of failed syncs made in a row. Once a sync request succeeds,
  // this counter is reset.
  size_t failure_count_;

  // Timer firing for the next sync request.
  std::unique_ptr<base::OneShotTimer> timer_;

  base::WeakPtrFactory<SyncSchedulerImpl> weak_ptr_factory_{this};
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_SYNC_SCHEDULER_IMPL_H_
