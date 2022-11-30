// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_MODE_ARBITER_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_MODE_ARBITER_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/power_scheduler/traced_power_mode.h"

namespace power_scheduler {

// Decides the active PowerMode for a process. To do this, it collects votes
// from various instrumentation points in Chromium.
//
// Each instrumentation point can instantiate a PowerModeVoter and submit votes:
//   auto voter = PowerModeArbiter::GetInstance()->NewVoter("MyVoter");
//   voter->VoteFor(PowerMode::kCharging);
//
// The active PowerMode is decided via a prioritization mechanism, see
// ComputeActiveModeLocked().
class COMPONENT_EXPORT(POWER_SCHEDULER) PowerModeArbiter
    : public base::trace_event::TraceLog::EnabledStateObserver,
      public base::trace_event::TraceLog::IncrementalStateObserver,
      public PowerModeVoter::Delegate {
 public:
  class COMPONENT_EXPORT(POWER_SCHEDULER) Observer {
   public:
    virtual ~Observer();
    virtual void OnPowerModeChanged(PowerMode old_mode, PowerMode new_mode) = 0;
  };

  // Limits the frequency at which we can run the UpdatePendingResets() task.
  // All pending resets are aligned to this time resolution. Public for testing.
  static constexpr base::TimeDelta kResetVoteTimeResolution =
      base::Milliseconds(100);

  static PowerModeArbiter* GetInstance();

  // Public for testing.
  PowerModeArbiter();
  ~PowerModeArbiter() override;

  PowerModeArbiter(const PowerModeArbiter&) = delete;
  PowerModeArbiter& operator=(const PowerModeArbiter&) = delete;

  // Instantiates a new Voter for an instrumentation site. |name| must be a
  // static string and is used for tracing instrumentation.
  std::unique_ptr<PowerModeVoter> NewVoter(const char* name);

  // Adds/removes an observer that is notified for every change of the
  // process-wide mode. The observer is notified on the task sequence it
  // registered on.
  void AddObserver(Observer*);
  void RemoveObserver(Observer*);

  // Should be called by the embedder during process startup, once the thread
  // pool is available.
  void OnThreadPoolAvailable();

  // Enables or disables the kCharging PowerMode. Defaults to enabled, i.e.
  // kCharging will preempt all other PowerModes while the device is on a
  // charger.
  void SetChargingModeEnabled(bool enabled);

  // Provide a custom task runner for unit tests. Replaces a call to
  // OnThreadPoolAvailable().
  void SetTaskRunnerForTesting(scoped_refptr<base::SequencedTaskRunner>);

  PowerMode GetActiveModeForTesting();
  // TODO(eseckler): Replace this with SetChargingModeEnabled() in tests.
  void SetOnBatteryPowerForTesting(bool on_battery_power);

 private:
  FRIEND_TEST_ALL_PREFIXES(PowerModeArbiterTest, ObserverEnablesResetTasks);

  class ChargingPowerModeVoter;

  // PowerModeVoter::Delegate implementation:
  void OnVoterDestroyed(PowerModeVoter*) override;
  void SetVote(PowerModeVoter*, PowerMode) override;
  void ResetVoteAfterTimeout(PowerModeVoter*, base::TimeDelta timeout) override;

  void OnTaskRunnerAvailable(scoped_refptr<base::SequencedTaskRunner>,
                             int sequence_number);

  void UpdatePendingResets(int sequence_number);
  void OnVotesUpdated();

  void ServicePendingResetsLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  PowerMode ComputeActiveModeLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  void UpdateTraceObserver() LOCKS_EXCLUDED(lock_, trace_observer_lock_);

  // trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  // trace_event::TraceLog::IncrementalStateObserver implementation:
  void OnIncrementalStateCleared() override;

  // Protects trace_observer_{,added_}. Should only be acquired when |lock_| is
  // not held.
  base::Lock trace_observer_lock_;
  std::unique_ptr<Observer> trace_observer_ GUARDED_BY(trace_observer_lock_);
  bool trace_observer_added_ GUARDED_BY(trace_observer_lock_) = false;

  base::Lock lock_;  // Protects subsequent members.
  scoped_refptr<base::SequencedTaskRunner> task_runner_ GUARDED_BY(lock_);
  std::map<PowerModeVoter*, TracedPowerMode> votes_ GUARDED_BY(lock_);
  std::map<PowerModeVoter*, base::TimeTicks /*effective_time*/> pending_resets_
      GUARDED_BY(lock_);
  base::TimeTicks next_pending_vote_update_time_ GUARDED_BY(lock_);
  TracedPowerMode active_mode_ GUARDED_BY(lock_);
  base::TimeTicks active_mode_changed_timestamp_ GUARDED_BY(lock_);
  int update_task_sequence_number_ GUARDED_BY(lock_) = 0;
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_
      GUARDED_BY(lock_);
  bool has_observers_ GUARDED_BY(lock_) = false;
  bool charging_mode_enabled_ = true;

  // Owned by the arbiter but otherwise behaves like a regular voter.
  std::unique_ptr<ChargingPowerModeVoter> charging_voter_;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_MODE_ARBITER_H_
