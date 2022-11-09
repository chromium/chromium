// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/trusted_vault_degraded_recoverability_handler.h"

#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/trusted_vault_histograms.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"

namespace syncer {

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
MakeDegradedRecoverabilityState(
    sync_pb::DegradedRecoverabilityValue degraded_recoverability_value,
    const base::Time& last_refresh_time) {
  sync_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      degraded_recoverability_value);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      syncer::TimeToProtoTime(last_refresh_time));
  return degraded_recoverability_state;
}

}  // namespace

TrustedVaultDegradedRecoverabilityHandler::
    TrustedVaultDegradedRecoverabilityHandler(
        TrustedVaultConnection* connection,
        Delegate* delegate,
        const CoreAccountInfo& account_info,
        const sync_pb::LocalTrustedVaultDegradedRecoverabilityState&
            degraded_recoverability_state)
    : connection_(connection),
      delegate_(delegate),
      account_info_(account_info) {
  long_degraded_recoverability_refresh_period_ =
      kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling.Get();
  short_degraded_recoverability_refresh_period_ =
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get();
  current_refresh_period_ = long_degraded_recoverability_refresh_period_;
  degraded_recoverability_value_ =
      degraded_recoverability_state.degraded_recoverability_value();
  base::UmaHistogramExactLinear("Sync.TrustedVaultDegradedRecoverabilityValue",
                                degraded_recoverability_value_,
                                sync_pb::DegradedRecoverabilityValue_ARRAYSIZE);
  base::Time last_refresh_time =
      ProtoTimeToTime(degraded_recoverability_state
                          .last_refresh_time_millis_since_unix_epoch());
  if (base::Time::Now() > last_refresh_time) {
    last_refresh_time_ =
        base::TimeTicks::Now() - (base::Time::Now() - last_refresh_time);
  }
}

TrustedVaultDegradedRecoverabilityHandler::
    ~TrustedVaultDegradedRecoverabilityHandler() = default;

void TrustedVaultDegradedRecoverabilityHandler::
    HintDegradedRecoverabilityChanged(
        TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA reason) {
  RecordTrustedVaultHintDegradedRecoverabilityChangedReason(reason);
  RefreshImmediately();
}

void TrustedVaultDegradedRecoverabilityHandler::StartLongIntervalRefreshing() {
  current_refresh_period_ = long_degraded_recoverability_refresh_period_;
  Start();
}

void TrustedVaultDegradedRecoverabilityHandler::StartShortIntervalRefreshing() {
  current_refresh_period_ = short_degraded_recoverability_refresh_period_;
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
  base::UmaHistogramEnumeration(
      "Sync.TrustedVaultRecoverabilityStatusOnRequestCompletion", status);
  sync_pb::DegradedRecoverabilityValue old_degraded_recoverability_value =
      degraded_recoverability_value_;
  switch (status) {
    case TrustedVaultRecoverabilityStatus::kDegraded:
      degraded_recoverability_value_ =
          sync_pb::DegradedRecoverabilityValue::kDegraded;
      break;
    case TrustedVaultRecoverabilityStatus::kNotDegraded:
      degraded_recoverability_value_ =
          sync_pb::DegradedRecoverabilityValue::kNotDegraded;
      break;
    case TrustedVaultRecoverabilityStatus::kError:
      // TODO(crbug.com/1247990): To be handled.
      break;
  }
  if (degraded_recoverability_value_ != old_degraded_recoverability_value) {
    delegate_->OnDegradedRecoverabilityChanged();
  }
  last_refresh_time_ = base::TimeTicks::Now();
  delegate_->WriteDegradedRecoverabilityState(MakeDegradedRecoverabilityState(
      degraded_recoverability_value_, base::Time::Now()));
  next_refresh_timer_.Start(
      FROM_HERE, current_refresh_period_, this,
      &TrustedVaultDegradedRecoverabilityHandler::Refresh);
}

}  // namespace syncer
