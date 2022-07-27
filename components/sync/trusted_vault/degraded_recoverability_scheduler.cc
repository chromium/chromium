// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/degraded_recoverability_scheduler.h"

#include <utility>
#include "base/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"

namespace {
base::TimeDelta ComputeTimeUntilNextRefresh(
    const base::TimeDelta& refresh_period,
    const base::TimeTicks& last_refresh_time) {
  if (last_refresh_time.is_null()) {
    return base::TimeDelta();
  }
  const base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - last_refresh_time;
  if (elapsed_time > refresh_period) {
    return base::TimeDelta();
  }
  return refresh_period - elapsed_time;
}

}  // namespace

namespace syncer {

DegradedRecoverabilityScheduler::DegradedRecoverabilityScheduler(
    Delegate* delegate,
    base::RepeatingClosure refresh_callback)
    : delegate_(delegate),
      refresh_callback_(std::move(refresh_callback)),
      current_refresh_period_(kLongDegradedRecoverabilityRefreshPeriod) {
  // TODO(crbug.com/1247990): read `last_refresh_time_`, convert it to
  // TimeTicks, and schedule next refresh.
  NOTIMPLEMENTED();
  Start();
}

DegradedRecoverabilityScheduler::~DegradedRecoverabilityScheduler() = default;

void DegradedRecoverabilityScheduler::StartLongIntervalRefreshing() {
  current_refresh_period_ = kLongDegradedRecoverabilityRefreshPeriod;
  Start();
}

void DegradedRecoverabilityScheduler::StartShortIntervalRefreshing() {
  current_refresh_period_ = kShortDegradedRecoverabilityRefreshPeriod;
  Start();
}

void DegradedRecoverabilityScheduler::RefreshImmediately() {
  // TODO(crbug.com/1247990): Currently if the timer is not running, then this
  // means that Refresh() has just invoked. Probably this would be changed
  // later, then we need to take care.
  if (!next_refresh_timer_.IsRunning()) {
    return;
  }
  next_refresh_timer_.FireNow();
}

void DegradedRecoverabilityScheduler::Start() {
  next_refresh_timer_.Start(
      FROM_HERE,
      ComputeTimeUntilNextRefresh(current_refresh_period_, last_refresh_time_),
      this, &DegradedRecoverabilityScheduler::Refresh);
}

void DegradedRecoverabilityScheduler::Refresh() {
  // TODO(crbug.com/1247990): To be implemented, make sure the to schedule the
  // next Refresh() after the current one is completed.
  NOTIMPLEMENTED();
  last_refresh_time_ = base::TimeTicks::Now();
  delegate_->WriteDegradedRecoverabilityState(GetDegradedRecoverabilityState());
  refresh_callback_.Run();
  next_refresh_timer_.Start(FROM_HERE, current_refresh_period_, this,
                            &DegradedRecoverabilityScheduler::Refresh);
}

sync_pb::LocalTrustedVaultDegradedRecoverabilityState
DegradedRecoverabilityScheduler::GetDegradedRecoverabilityState() const {
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  // TODO(crbug.com/1247990): Should set `is_recoverability_degraded` once the
  // connection passed to the scheduler.
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(base::Time::Now()));
  return degraded_recoverability_state;
}

}  // namespace syncer
