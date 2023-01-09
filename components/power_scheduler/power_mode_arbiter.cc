// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_scheduler/power_mode_arbiter.h"

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_log.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/power_scheduler/traced_power_mode.h"

namespace power_scheduler {

using ObserverList = base::ObserverListThreadSafe<PowerModeArbiter::Observer>;

namespace {
class TraceObserver : public PowerModeArbiter::Observer {
 public:
  ~TraceObserver() override = default;
  void OnPowerModeChanged(PowerMode old_mode, PowerMode new_mode) override {}
};
}  // namespace

// Created and owned by the arbiter on thread pool initialization because there
// has to be exactly one per process, and //base can't depend on the
// power_scheduler component.
class PowerModeArbiter::ChargingPowerModeVoter : base::PowerStateObserver {
 public:
  explicit ChargingPowerModeVoter(PowerModeArbiter* arbiter)
      : charging_voter_(arbiter->NewVoter("PowerModeVoter.Charging")) {
    // Start out in charging mode until we can register the observer with
    // PowerMonitor, which itself also starts out in charging mode.
    charging_voter_->VoteFor(PowerMode::kCharging);
  }

  ~ChargingPowerModeVoter() override {
    if (was_setup_)
      base::PowerMonitor::RemovePowerStateObserver(this);
  }

  void Setup() {
    if (was_setup_)
      return;
    was_setup_ = true;

    const bool on_battery =
        base::PowerMonitor::AddPowerStateObserverAndReturnOnBatteryState(this);
    OnPowerStateChange(on_battery);
  }

  void SetOnBatteryPowerForTesting(bool on_battery_power) {
    OnPowerStateChange(on_battery_power);
    was_setup_ = true;  // Prevent real setup in the test.
  }

  void OnPowerStateChange(bool on_battery_power) override {
    charging_voter_->VoteFor(on_battery_power ? PowerMode::kIdle
                                              : PowerMode::kCharging);
  }

 private:
  std::unique_ptr<PowerModeVoter> charging_voter_;
  bool was_setup_ = false;
};

PowerModeArbiter::Observer::~Observer() = default;

constexpr base::TimeDelta PowerModeArbiter::kResetVoteTimeResolution;

// static
PowerModeArbiter* PowerModeArbiter::GetInstance() {
  static base::NoDestructor<PowerModeArbiter> arbiter;
  return arbiter.get();
}

PowerModeArbiter::PowerModeArbiter()
    : trace_observer_(std::make_unique<TraceObserver>()),
      active_mode_("PowerModeArbiter", this),
      observers_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()),
      charging_voter_(std::make_unique<ChargingPowerModeVoter>(this)) {
  base::trace_event::TraceLog::GetInstance()->AddEnabledStateObserver(this);
  base::trace_event::TraceLog::GetInstance()->AddIncrementalStateObserver(this);
}

PowerModeArbiter::~PowerModeArbiter() {
  base::trace_event::TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  base::trace_event::TraceLog::GetInstance()->RemoveIncrementalStateObserver(
      this);
}

void PowerModeArbiter::OnThreadPoolAvailable() {
  int sequence_number = 0;
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  {
    base::AutoLock lock(lock_);

    // May be called multiple times in single-process mode.
    if (task_runner_)
      return;

    // Set task_runner_ under lock to avoid a race with AddObserver().
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    task_runner = task_runner_;

    // Acquire the current sequence number in case it was previously incremented
    // by RemoveObserver(). It will be incremented by UpdatePendingResets()
    // in OnTaskRunnerAvailable().
    sequence_number = update_task_sequence_number_;
  }

  OnTaskRunnerAvailable(task_runner, sequence_number);
}

void PowerModeArbiter::SetChargingModeEnabled(bool enabled) {
  {
    base::AutoLock lock(lock_);
    if (charging_mode_enabled_ == enabled)
      return;
    charging_mode_enabled_ = enabled;
  }
  OnVotesUpdated();
}

void PowerModeArbiter::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  int sequence_number = 0;
  {
    base::AutoLock lock(lock_);

    DCHECK(!task_runner_);
    task_runner_ = task_runner;
    sequence_number = update_task_sequence_number_;
  }

  OnTaskRunnerAvailable(task_runner, sequence_number);
}

void PowerModeArbiter::OnTaskRunnerAvailable(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    int sequence_number) {
  UpdateTraceObserver();

  // Check if there are any actionable resets and post another task to handle
  // future ones if necessary. If sequence_number is changed concurrently by
  // RemoveObserver() or ResetVoteAfterTimeout(), this has call has no effect,
  // but a future call to UpdatePendingResets() will take its place.
  UpdatePendingResets(sequence_number);

  // Create the charging voter on the task runner sequence, so that charging
  // state notifications are received there.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce([] {
        PowerModeArbiter::GetInstance()->charging_voter_->Setup();
      }));
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
  DCHECK(observer);
  bool should_update_pending_resets = false;
  int sequence_number = 0;

  {
    base::AutoLock lock(lock_);
    observer->OnPowerModeChanged(PowerMode::kIdle, active_mode_.mode());
    should_update_pending_resets = task_runner_ && !has_observers_;
    // Acquire the current sequence number in case it was previously incremented
    // by RemoveObserver(). If necessary, it will be incremented by
    // UpdatePendingResets() below.
    sequence_number = update_task_sequence_number_;
    observers_->AddObserver(observer);
    has_observers_ = true;
  }

  // Reset tasks are disabled until the first observer is registered. If
  // sequence_number is changed concurrently by RemoveObserver() or
  // ResetVoteAfterTimeout(), this has call has no effect, but a future call to
  // UpdatePendingResets() will take its place.
  if (should_update_pending_resets)
    UpdatePendingResets(sequence_number);
}

void PowerModeArbiter::RemoveObserver(Observer* observer) {
  base::AutoLock lock(lock_);
  ObserverList::RemoveObserverResult result =
      observers_->RemoveObserver(observer);
  has_observers_ =
      result == ObserverList::RemoveObserverResult::kRemainsNonEmpty;

  // Increment update_task_sequence_number_ so that any scheduled update tasks
  // are skipped and only restarted if another observer registers.
  if (!has_observers_)
    ++update_task_sequence_number_;
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
  scoped_refptr<base::TaskRunner> task_runner;
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks scheduled_time = now + timeout;
  // Align to the reset task's resolution.
  scheduled_time = scheduled_time.SnappedToNextTick(base::TimeTicks(),
                                                    kResetVoteTimeResolution);

  {
    base::AutoLock lock(lock_);
    pending_resets_[voter] = scheduled_time;
    // Only post a new task if there isn't one scheduled to run earlier yet.
    // This reduces the number of posted callbacks in situations where the
    // pending vote is cleared soon after UpdateVoteAfterTimeout() by SetVote().
    if (task_runner_ && has_observers_ &&
        (next_pending_vote_update_time_.is_null() ||
         scheduled_time < next_pending_vote_update_time_)) {
      next_pending_vote_update_time_ = scheduled_time;
      should_post_update_task = true;
      ++update_task_sequence_number_;
      sequence_number = update_task_sequence_number_;
      task_runner = task_runner_;
    }
  }

  if (should_post_update_task) {
    task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PowerModeArbiter::UpdatePendingResets,
                       base::Unretained(this), sequence_number),
        scheduled_time - now);
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
  scoped_refptr<base::TaskRunner> task_runner;
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
      task_runner = task_runner_;
      ++update_task_sequence_number_;
      next_sequence_number = update_task_sequence_number_;
    }
  }
  if (!next_task_time.is_null()) {
    task_runner->PostDelayedTask(
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
  PowerMode max_animation_mode = PowerMode::kIdle;
  bool is_audible = false;
  bool is_loading = false;

  for (const auto& voter_and_vote : votes_) {
    PowerMode vote = voter_and_vote.second.mode();

    if (!charging_mode_enabled_ && vote == PowerMode::kCharging)
      continue;

    if (vote > mode)
      mode = vote;
    if (vote == PowerMode::kAudible)
      is_audible = true;
    if (vote == PowerMode::kLoading)
      is_loading = true;

    if ((vote == PowerMode::kAnimation || vote == PowerMode::kNopAnimation ||
         vote == PowerMode::kSmallAnimation ||
         vote == PowerMode::kMediumAnimation) &&
        vote > max_animation_mode) {
      max_animation_mode = vote;
    }
  }

  // In background, audible overrides.
  if (mode == PowerMode::kBackground && is_audible)
    return PowerMode::kAudible;

  // Break out loading while concurrently animating into a separate mode.
  if ((mode == PowerMode::kAnimation && is_loading) ||
      (mode == PowerMode::kLoading &&
       max_animation_mode > PowerMode::kNopAnimation)) {
    return PowerMode::kLoadingAnimation;
  }

  // Break out specific animation modes for main thread animations, too.
  if (mode == PowerMode::kMainThreadAnimation) {
    if (max_animation_mode == PowerMode::kNopAnimation) {
      // The main thread seems to be taking a long time to produce a frame. Fold
      // this into no-op animation mode. Note that this depends on kLoading mode
      // overriding kMainThreadAnimation mode - otherwise we may incorrectly
      // classify loading work as no-op animations.
      static_assert(PowerMode::kLoading > PowerMode::kMainThreadAnimation,
                    "Can't fold kMainThreadAnimation into kNopAnimation if the "
                    "former preempts kLoading");
      mode = PowerMode::kNopAnimation;
    } else if (max_animation_mode == PowerMode::kSmallAnimation) {
      mode = PowerMode::kSmallMainThreadAnimation;
    } else if (max_animation_mode == PowerMode::kMediumAnimation) {
      mode = PowerMode::kMediumMainThreadAnimation;
    }
  }

  return mode;
}

PowerMode PowerModeArbiter::GetActiveModeForTesting() {
  base::AutoLock lock(lock_);
  return active_mode_.mode();
}

void PowerModeArbiter::SetOnBatteryPowerForTesting(bool on_battery_power) {
  charging_voter_->SetOnBatteryPowerForTesting(on_battery_power);  // IN-TEST
}

void PowerModeArbiter::UpdateTraceObserver() {
  {
    base::AutoLock lock(lock_);

    // Can't add the observer yet if the task runner isn't available.
    if (!task_runner_)
      return;
  }

  // Lock while adding or removing the observer, because OnTaskRunnerAvailable()
  // may run concurrently to OnTraceLogEnabled/Disabled(). We need a different
  // lock than |lock_| since that one is acquired by Add/RemoveObserver().
  base::AutoLock lock(trace_observer_lock_);
  bool power_tracing_enabled =
      *TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("power");
  if (power_tracing_enabled && !trace_observer_added_) {
    trace_observer_added_ = true;
    // Add a no-op observer which ensures that reset tasks are executing while
    // tracing is enabled.
    AddObserver(trace_observer_.get());
  } else if (!power_tracing_enabled && trace_observer_added_) {
    trace_observer_added_ = false;
    RemoveObserver(trace_observer_.get());
  }
}

void PowerModeArbiter::OnTraceLogEnabled() {
  {
    base::AutoLock lock(lock_);
    for (auto& voter_and_vote : votes_)
      voter_and_vote.second.OnTraceLogEnabled();
    active_mode_.OnTraceLogEnabled();
  }

  UpdateTraceObserver();
}

void PowerModeArbiter::OnTraceLogDisabled() {
  UpdateTraceObserver();
}

void PowerModeArbiter::OnIncrementalStateCleared() {
  base::AutoLock lock(lock_);
  for (auto& voter_and_vote : votes_)
    voter_and_vote.second.OnIncrementalStateCleared();
  active_mode_.OnIncrementalStateCleared();
}

}  // namespace power_scheduler
