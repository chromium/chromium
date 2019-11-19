// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/sync_scheduler_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {

namespace device_sync {

namespace {

// Returns a human readable string given a |time_delta|.
std::string TimeDeltaToString(const base::TimeDelta& time_delta) {
  if (time_delta.InDays() > 0)
    return base::StringPrintf("%d days", time_delta.InDays());

  if (time_delta.InHours() > 0)
    return base::StringPrintf("%d hours", time_delta.InHours());

  if (time_delta.InMinutes() > 0)
    return base::StringPrintf("%d minutes", time_delta.InMinutes());

  return base::StringPrintf("%d seconds",
                            base::saturated_cast<int>(time_delta.InSeconds()));
}

}  // namespace

SyncSchedulerImpl::SyncSchedulerImpl(Delegate* delegate,
                                     base::TimeDelta refresh_period,
                                     base::TimeDelta base_recovery_period,
                                     double max_jitter_ratio,
                                     const std::string& scheduler_name)
    : delegate_(delegate),
      refresh_period_(refresh_period),
      base_recovery_period_(base_recovery_period),
      max_jitter_ratio_(max_jitter_ratio),
      scheduler_name_(scheduler_name),
      strategy_(Strategy::PERIODIC_REFRESH),
      sync_state_(SyncState::NOT_STARTED),
      failure_count_(0) {}

SyncSchedulerImpl::~SyncSchedulerImpl() {}

void SyncSchedulerImpl::Start(
    const base::TimeDelta& elapsed_time_since_last_sync,
    Strategy strategy) {
  strategy_ = strategy;
  sync_state_ = SyncState::WAITING_FOR_REFRESH;
  // We reset the failure backoff when the scheduler is started again, as the
  // configuration that caused the previous attempts to fail most likely won't
  // be present after a restart.
  if (strategy_ == Strategy::AGGRESSIVE_RECOVERY)
    failure_count_ = 1;

  // To take into account the time waited when the system is powered off, we
  // subtract the time elapsed with a normal sync period to the initial time
  // to wait.
  base::TimeDelta sync_delta =
      GetJitteredPeriod() - elapsed_time_since_last_sync;

  // The elapsed time may be negative if the system clock is changed. In this
  // case, we immediately schedule a sync.
  base::TimeDelta zero_delta = base::TimeDelta::FromSeconds(0);
  if (elapsed_time_since_last_sync < zero_delta || sync_delta < zero_delta)
    sync_delta = zero_delta;

  ScheduleNextSync(sync_delta);
}

void SyncSchedulerImpl::ForceSync() {
  OnTimerFired();
}

base::TimeDelta SyncSchedulerImpl::GetTimeToNextSync() const {
  if (!timer_)
    return base::TimeDelta::FromSeconds(0);
  return timer_->GetCurrentDelay();
}

SyncScheduler::Strategy SyncSchedulerImpl::GetStrategy() const {
  return strategy_;
}

SyncScheduler::SyncState SyncSchedulerImpl::GetSyncState() const {
  return sync_state_;
}

void SyncSchedulerImpl::OnTimerFired() {
  timer_.reset();
  if (strategy_ == Strategy::PERIODIC_REFRESH) {
    PA_LOG(VERBOSE) << "Timer fired for periodic refresh, making request...";
    sync_state_ = SyncState::SYNC_IN_PROGRESS;
  } else if (strategy_ == Strategy::AGGRESSIVE_RECOVERY) {
    PA_LOG(VERBOSE) << "Timer fired for aggressive recovery, making request...";
    sync_state_ = SyncState::SYNC_IN_PROGRESS;
  } else {
    NOTREACHED();
    return;
  }

  delegate_->OnSyncRequested(
      std::make_unique<SyncRequest>(weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<base::OneShotTimer> SyncSchedulerImpl::CreateTimer() {
  return std::make_unique<base::OneShotTimer>();
}

void SyncSchedulerImpl::ScheduleNextSync(const base::TimeDelta& sync_delta) {
  if (sync_state_ != SyncState::WAITING_FOR_REFRESH) {
    PA_LOG(ERROR) << "Unexpected state when scheduling next sync: sync_state="
                  << static_cast<int>(sync_state_);
    return;
  }

  bool is_aggressive_recovery = (strategy_ == Strategy::AGGRESSIVE_RECOVERY);
  PA_LOG(VERBOSE) << "Scheduling next sync for " << scheduler_name_ << ":\n"
                  << "    Strategy: "
                  << (is_aggressive_recovery ? "Aggressive Recovery"
                                             : "Periodic Refresh")
                  << "\n"
                  << "    Time Delta: " << TimeDeltaToString(sync_delta)
                  << (is_aggressive_recovery
                          ? base::StringPrintf(
                                "\n    Previous Failures: %d",
                                base::saturated_cast<int>(failure_count_))
                          : "");

  timer_ = CreateTimer();
  timer_->Start(FROM_HERE, sync_delta,
                base::BindOnce(&SyncSchedulerImpl::OnTimerFired,
                               weak_ptr_factory_.GetWeakPtr()));
}

void SyncSchedulerImpl::OnSyncCompleted(bool success) {
  if (sync_state_ != SyncState::SYNC_IN_PROGRESS) {
    PA_LOG(ERROR) << "Unexpected state when sync completed: sync_state="
                  << static_cast<int>(sync_state_)
                  << ", strategy_=" << static_cast<int>(strategy_);
    return;
  }
  sync_state_ = SyncState::WAITING_FOR_REFRESH;

  if (success) {
    strategy_ = Strategy::PERIODIC_REFRESH;
    failure_count_ = 0;
  } else {
    strategy_ = Strategy::AGGRESSIVE_RECOVERY;
    ++failure_count_;
  }

  ScheduleNextSync(GetJitteredPeriod());
}

base::TimeDelta SyncSchedulerImpl::GetJitteredPeriod() {
  double jitter = 2 * max_jitter_ratio_ * (base::RandDouble() - 0.5);
  base::TimeDelta period = GetPeriod();
  base::TimeDelta jittered_time_delta = period + (period * jitter);
  if (jittered_time_delta.InMilliseconds() < 0)
    jittered_time_delta = base::TimeDelta::FromMilliseconds(0);
  return jittered_time_delta;
}

base::TimeDelta SyncSchedulerImpl::GetPeriod() {
  if (strategy_ == Strategy::PERIODIC_REFRESH)
    return refresh_period_;
  if (strategy_ == Strategy::AGGRESSIVE_RECOVERY && failure_count_ > 0) {
    // The backoff for each consecutive failure is exponentially doubled until
    // it is equal to the normal refresh period.
    // Note: |backoff_factor| may evaulate to INF if |failure_count_| is large,
    // but multiplication operations for TimeDelta objects are saturated.
    double backoff_factor = pow(2, failure_count_ - 1);
    base::TimeDelta backoff_period = base_recovery_period_ * backoff_factor;
    return backoff_period < refresh_period_ ? backoff_period : refresh_period_;
  }
  PA_LOG(ERROR) << "Error getting period for strategy: "
                << static_cast<int>(strategy_);
  return base::TimeDelta();
}

}  // namespace device_sync

}  // namespace chromeos
