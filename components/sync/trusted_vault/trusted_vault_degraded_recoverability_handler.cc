// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_degraded_recoverability_handler.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"

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

sync_pb::LocalTrustedVaultDegradedRecoverabilityState
MakeDegradedRecoverabilityState(bool is_recoverability_degraded,
                                const base::Time& last_refresh_time) {
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_is_recoverability_degraded(
      is_recoverability_degraded);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      syncer::TimeToProtoTime(last_refresh_time));
  return degraded_recoverability_state;
}

}  // namespace

namespace syncer {

TrustedVaultDegradedRecoverabilityHandler::
    TrustedVaultDegradedRecoverabilityHandler(
        TrustedVaultConnection* connection,
        Delegate* delegate,
        const CoreAccountInfo& account_info)
    : connection_(connection),
      delegate_(delegate),
      account_info_(account_info),
      current_refresh_period_(kLongDegradedRecoverabilityRefreshPeriod) {
  // TODO(crbug.com/1247990): read `last_refresh_time_`, convert it to
  // TimeTicks, and schedule next refresh.
  NOTIMPLEMENTED();
  Start();
}

TrustedVaultDegradedRecoverabilityHandler::
    ~TrustedVaultDegradedRecoverabilityHandler() = default;

void TrustedVaultDegradedRecoverabilityHandler::StartLongIntervalRefreshing() {
  current_refresh_period_ = kLongDegradedRecoverabilityRefreshPeriod;
  Start();
}

void TrustedVaultDegradedRecoverabilityHandler::StartShortIntervalRefreshing() {
  current_refresh_period_ = kShortDegradedRecoverabilityRefreshPeriod;
  Start();
}

void TrustedVaultDegradedRecoverabilityHandler::RefreshImmediately() {
  if (!next_refresh_timer_.IsRunning()) {
    return;
  }
  next_refresh_timer_.FireNow();
}

void TrustedVaultDegradedRecoverabilityHandler::Start() {
  next_refresh_timer_.Start(
      FROM_HERE,
      ComputeTimeUntilNextRefresh(current_refresh_period_, last_refresh_time_),
      this, &TrustedVaultDegradedRecoverabilityHandler::Refresh);
}

void TrustedVaultDegradedRecoverabilityHandler::Refresh() {
  // Since destroying the request object causes actual request cancellation, so
  // it's safe to use base::Unretained() here.
  ongoing_get_recoverability_request_ =
      connection_->DownloadIsRecoverabilityDegraded(
          account_info_,
          base::BindOnce(&TrustedVaultDegradedRecoverabilityHandler::
                             OnRecoverabilityIsDegradedDownloaded,
                         base::Unretained(this)));
}

void TrustedVaultDegradedRecoverabilityHandler::
    OnRecoverabilityIsDegradedDownloaded(
        TrustedVaultRecoverabilityStatus status) {
  switch (status) {
    case TrustedVaultRecoverabilityStatus::kDegraded:
      is_recoverability_degraded_ = true;
      break;
    case TrustedVaultRecoverabilityStatus::kNotDegraded:
      is_recoverability_degraded_ = false;
      break;
    case TrustedVaultRecoverabilityStatus::kError:
      // TODO(crbug.com/1247990): To be handled.
      break;
  }
  last_refresh_time_ = base::TimeTicks::Now();
  delegate_->WriteDegradedRecoverabilityState(MakeDegradedRecoverabilityState(
      is_recoverability_degraded_, base::Time::Now()));
  next_refresh_timer_.Start(
      FROM_HERE, current_refresh_period_, this,
      &TrustedVaultDegradedRecoverabilityHandler::Refresh);
}

}  // namespace syncer
