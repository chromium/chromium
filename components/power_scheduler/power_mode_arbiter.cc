// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_arbiter.h"

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_log.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_tracer.h"
#include "components/power_scheduler/power_mode_voter.h"

namespace power_scheduler {

PowerModeArbiter::Observer::~Observer() = default;

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
  task_runner_ = base::ThreadPool::CreateTaskRunner(
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // Check if we need to post a task to update pending votes.
  base::TimeTicks next_effective_time;
  {
    base::AutoLock lock(lock_);
    for (const auto& entry : pending_resets_) {
      base::TimeTicks effective_time = entry.second;
      if (next_effective_time.is_null() ||
          effective_time < next_effective_time) {
        next_effective_time = effective_time;
      }
    }
    next_pending_vote_update_time_ = next_effective_time;
  }

  if (!next_effective_time.is_null()) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (next_effective_time < now)
      next_effective_time = now;
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerModeArbiter::UpdatePendingResets,
                       base::Unretained(this)),
        next_effective_time - now);
  }
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
  {
    base::AutoLock lock(lock_);
    base::TimeTicks scheduled_time = base::TimeTicks::Now() + timeout;
    pending_resets_[voter] = scheduled_time;
    // Only post a new task if there isn't one scheduled to run earlier yet.
    // This reduces the number of posted callbacks in situations where the
    // pending vote is cleared soon after UpdateVoteAfterTimeout() by SetVote().
    if (task_runner_ && (next_pending_vote_update_time_.is_null() ||
                         scheduled_time < next_pending_vote_update_time_)) {
      next_pending_vote_update_time_ = scheduled_time;
      should_post_update_task = true;
    }
  }

  if (should_post_update_task) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerModeArbiter::UpdatePendingResets,
                       base::Unretained(this)),
        timeout);
  }
}

void PowerModeArbiter::UpdatePendingResets() {
  // Note: This method may run at any point. Do not assume that there are any
  // resets that have expired, or that any other UpdatePendingResets() task is
  // scheduled.
  bool did_change = false;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks next_task_time;
  {
    base::AutoLock lock(lock_);
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
  }
  if (!next_task_time.is_null()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerModeArbiter::UpdatePendingResets,
                       base::Unretained(this)),
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
