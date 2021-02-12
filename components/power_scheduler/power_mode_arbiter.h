// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_SCHEDULER_POWER_MODE_ARBITER_H_
#define COMPONENTS_POWER_SCHEDULER_POWER_MODE_ARBITER_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
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
      public PowerModeVoter::Delegate {
 public:
  class COMPONENT_EXPORT(POWER_SCHEDULER) Observer {
   public:
    virtual ~Observer();
    virtual void OnPowerModeChanged(PowerMode old_mode, PowerMode new_mode) = 0;
  };

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

  // Returns the currently active PowerMode. Public for testing.
  PowerMode GetActiveModeForTesting();

 private:
  class ChargingPowerModeVoter;

  // PowerModeVoter::Delegate implementation:
  void OnVoterDestroyed(PowerModeVoter*) override;
  void SetVote(PowerModeVoter*, PowerMode) override;
  void ResetVoteAfterTimeout(PowerModeVoter*, base::TimeDelta timeout) override;

  void UpdatePendingResets();
  void OnVotesUpdated();

  PowerMode ComputeActiveModeLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // trace_event::TraceLog::EnabledStateObserver implementation:
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  scoped_refptr<base::TaskRunner> task_runner_;
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  base::Lock lock_;
  std::map<PowerModeVoter*, TracedPowerMode> votes_ GUARDED_BY(lock_);
  std::map<PowerModeVoter*, base::TimeTicks /*effective_time*/> pending_resets_
      GUARDED_BY(lock_);
  base::TimeTicks next_pending_vote_update_time_ GUARDED_BY(lock_);
  TracedPowerMode active_mode_ GUARDED_BY(lock_);

  // Owned by the arbiter but otherwise behaves like a regular voter.
  std::unique_ptr<ChargingPowerModeVoter> charging_voter_;
};

}  // namespace power_scheduler

#endif  // COMPONENTS_POWER_SCHEDULER_POWER_MODE_ARBITER_H_
