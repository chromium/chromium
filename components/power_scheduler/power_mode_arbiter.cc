// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_arbiter.h"

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/power_scheduler/traced_power_mode.h"

namespace power_scheduler {

// Created and owned by the arbiter on thread pool initialization because there
// has to be exactly one per process, and //base can't depend on the
// power_scheduler component.
class PowerModeArbiter::ChargingPowerModeVoter : base::PowerStateObserver {
 public:
  ChargingPowerModeVoter()
      : charging_voter_(PowerModeArbiter::GetInstance()->NewVoter(
            "PowerModeVoter.Charging")) {
    const bool on_battery =
        base::PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(this);
    if (base::PowerMonitor::IsInitialized())
      OnPowerStateChange(on_battery);
  }

  ~ChargingPowerModeVoter() override {
    base::PowerMonitor::RemovePowerStateObserver(this);
  }

  void OnPowerStateChange(bool on_battery_power) override {
    charging_voter_->VoteFor(on_battery_power ? PowerMode::kIdle
                                              : PowerMode::kCharging);
  }

 private:
  std::unique_ptr<PowerModeVoter> charging_voter_;
};

PowerModeArbiter::Observer::~Observer() = default;

constexpr base::TimeDelta PowerModeArbiter::kResetVoteTimeResolution;

// static
PowerModeArbiter* PowerModeArbiter::GetInstance() {
  static base::NoDestructor<PowerModeArbiter> arbiter;
  return arbiter.get();
}

PowerModeArbiter::PowerModeArbiter()
    : observers_(new base::ObserverListThreadSafe<Observer>()),
      active_mode_("PowerModeArbiter", this) {
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

PowerModeArbiter::~PowerModeArbiter() {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
}

void PowerModeArbiter::OnThreadPoolAvailable() {
  // May be called multiple times in single-process mode.
  if (task_runner_)
    return;

  // Currently only used for the delayed votes.
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Create the charging voter on the task runner sequence, so that charging
  // state notifications are received there.
  task_runner_->PostTask(FROM_HERE, base::BindOnce([] {
                           PowerModeArbiter::GetInstance()->charging_voter_ =
                               std::make_unique<ChargingPowerModeVoter>();
                         }));

  // Check if there are any actionable resets and post another task to handle
  // future ones if necessary. |update_task_sequence_number_| is initialized to
  // 0 and incremented in UpdatePendingResets for the first time.
  UpdatePendingResets(/*sequence_number=*/0);
}

std::unique_ptr<PowerModeVoter> PowerModeArbiter::NewVoter(const char* name) {
  std::unique_ptr<PowerModeVoter> voter(new PowerModeVoter(this));
  {
    base::AutoLock lock(lock_);
    votes_.emplace(voter.get(), TracedPowerMode(name, voter.get()));
  }
  return voter;
}

void PowerModeArbiter::AddObserver(Observer* observer) {
  base::AutoLock lock(lock_);
  observer->OnPowerModeChanged(PowerMode::kIdle, active_mode_.mode());
  observers_->AddObserver(observer);
}

void PowerModeArbiter::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void PowerModeArbiter::OnVoterDestroyed(PowerModeVoter* voter) {
  {
    base::AutoLock lock(lock_);
    votes_.erase(voter);
    pending_resets_.erase(voter);
  }
  OnVotesUpdated();
}

void PowerModeArbiter::SetVote(PowerModeVoter* voter, PowerMode mode) {
  bool did_change = false;
  {
    base::AutoLock lock(lock_);
    auto it = votes_.find(voter);
    DCHECK(it != votes_.end());
    TracedPowerMode& voter_mode = it->second;
    did_change |= voter_mode.mode() != mode;
    voter_mode.SetMode(mode);
    pending_resets_.erase(voter);
  }
  if (did_change)
    OnVotesUpdated();
}

void PowerModeArbiter::ResetVoteAfterTimeout(PowerModeVoter* voter,
                                             base::TimeDelta timeout) {
  bool should_post_update_task = false;
  int sequence_number = 0;
  {
    base::AutoLock lock(lock_);
    base::TimeTicks scheduled_time = base::TimeTicks::Now() + timeout;
    // Align to the reset task's resolution.
    scheduled_time = scheduled_time.SnappedToNextTick(base::TimeTicks(),
                                                      kResetVoteTimeResolution);
    pending_resets_[voter] = scheduled_time;
    // Only post a new task if there isn't one scheduled to run earlier yet.
    // This reduces the number of posted callbacks in situations where the
    // pending vote is cleared soon after UpdateVoteAfterTimeout() by SetVote().
    if (task_runner_ && (next_pending_vote_update_time_.is_null() ||
                         scheduled_time < next_pending_vote_update_time_)) {
      next_pending_vote_update_time_ = scheduled_time;
      should_post_update_task = true;
      ++update_task_sequence_number_;
      sequence_number = update_task_sequence_number_;
    }
  }

  if (should_post_update_task) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerModeArbiter::UpdatePendingResets,
                       base::Unretained(this), sequence_number),
        timeout);
  }
}

void PowerModeArbiter::UpdatePendingResets(int sequence_number) {
  // Note: This method may run at any point and on any thread. Do not assume
  // that there are any resets that have expired, or that any other
  // UpdatePendingResets() task is scheduled.
  bool did_change = false;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks next_task_time;
  int next_sequence_number = 0;
  {
    base::AutoLock lock(lock_);

    // Check if this task was cancelled and replaced by another one.
    if (update_task_sequence_number_ != sequence_number)
      return;

    now = base::TimeTicks::Now();
    for (auto it = pending_resets_.begin(); it != pending_resets_.end();) {
      base::TimeTicks task_time = it->second;
      if (task_time <= now) {
        PowerModeVoter* voter = it->first;
        auto votes_it = votes_.find(voter);
        DCHECK(votes_it != votes_.end());
        TracedPowerMode& voter_mode = votes_it->second;
        did_change |= voter_mode.mode() != PowerMode::kIdle;
        voter_mode.SetMode(PowerMode::kIdle);
        it = pending_resets_.erase(it);
      } else {
        if (next_task_time.is_null() || task_time < next_task_time)
          next_task_time = task_time;
        ++it;
      }
    }

    next_pending_vote_update_time_ = next_task_time;
    if (!next_task_time.is_null()) {
      ++update_task_sequence_number_;
      next_sequence_number = update_task_sequence_number_;
    }
  }
  if (!next_task_time.is_null()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerModeArbiter::UpdatePendingResets,
                       base::Unretained(this), next_sequence_number),
        next_task_time - now);
  }
  if (did_change)
    OnVotesUpdated();
}

void PowerModeArbiter::OnVotesUpdated() {
  base::AutoLock lock(lock_);
  PowerMode old_mode = active_mode_.mode();
  PowerMode new_mode = ComputeActiveModeLocked();
  active_mode_.SetMode(new_mode);

  if (old_mode == new_mode)
    return;

  // Notify while holding |lock| to avoid out-of-order observer updates.
  observers_->Notify(FROM_HERE, &Observer::OnPowerModeChanged, old_mode,
                     new_mode);
}

PowerMode PowerModeArbiter::ComputeActiveModeLocked() {
  PowerMode mode = PowerMode::kIdle;
  bool is_audible = false;

  for (const auto& voter_and_vote : votes_) {
    PowerMode vote = voter_and_vote.second.mode();
    if (vote > mode)
      mode = vote;
    if (vote == PowerMode::kAudible)
      is_audible = true;
  }

  // In background, audible overrides.
  if (mode == PowerMode::kBackground && is_audible)
    return PowerMode::kAudible;

  return mode;
}

PowerMode PowerModeArbiter::GetActiveModeForTesting() {
  base::AutoLock lock(lock_);
  return active_mode_.mode();
}

void PowerModeArbiter::OnTraceLogEnabled() {
  base::AutoLock lock(lock_);
  for (const auto& voter_and_vote : votes_)
    voter_and_vote.second.OnTraceLogEnabled();
  active_mode_.OnTraceLogEnabled();
}

void PowerModeArbiter::OnTraceLogDisabled() {}

}  // namespace power_scheduler
