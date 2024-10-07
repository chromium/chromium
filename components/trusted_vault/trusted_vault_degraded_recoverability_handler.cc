// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/trusted_vault_degraded_recoverability_handler.h"

#include <utility>
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"

namespace trusted_vault {

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

trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
MakeDegradedRecoverabilityState(
    trusted_vault_pb::DegradedRecoverabilityValue degraded_recoverability_value,
    const base::Time& last_refresh_time) {
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
      degraded_recoverability_state;
  degraded_recoverability_state.set_degraded_recoverability_value(
      degraded_recoverability_value);
  degraded_recoverability_state.set_last_refresh_time_millis_since_unix_epoch(
      TimeToProtoTime(last_refresh_time));
  return degraded_recoverability_state;
}

}  // namespace

TrustedVaultDegradedRecoverabilityHandler::
    TrustedVaultDegradedRecoverabilityHandler(
        TrustedVaultConnection* connection,
        Delegate* delegate,
        const CoreAccountInfo& account_info,
        const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
            degraded_recoverability_state)
    : connection_(connection),
      delegate_(delegate),
      account_info_(account_info) {
  degraded_recoverability_value_ =
      degraded_recoverability_state.degraded_recoverability_value();
  if (degraded_recoverability_state
          .has_last_refresh_time_millis_since_unix_epoch()) {
    base::Time last_refresh_time =
        ProtoTimeToTime(degraded_recoverability_state
                            .last_refresh_time_millis_since_unix_epoch());
    if (base::Time::Now() >= last_refresh_time) {
      last_refresh_time_ =
          base::TimeTicks::Now() - (base::Time::Now() - last_refresh_time);
    }
  }

  long_degraded_recoverability_refresh_period_ =
      kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling.Get();
  short_degraded_recoverability_refresh_period_ =
      kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling.Get();
  UpdateCurrentRefreshPeriod();
}

TrustedVaultDegradedRecoverabilityHandler::
    ~TrustedVaultDegradedRecoverabilityHandler() = default;

void TrustedVaultDegradedRecoverabilityHandler::
    HintDegradedRecoverabilityChanged(
        TrustedVaultHintDegradedRecoverabilityChangedReasonForUMA reason) {
  if (next_refresh_timer_.IsRunning()) {
    trusted_vault::RecordTrustedVaultHintDegradedRecoverabilityChangedReason(
        reason);
    next_refresh_timer_.FireNow();
  }
}

void TrustedVaultDegradedRecoverabilityHandler::GetIsRecoverabilityDegraded(
    base::OnceCallback<void(bool)> cb) {
  if (last_refresh_time_.is_null()) {
    pending_get_is_recoverability_degraded_callback_ = std::move(cb);
  } else {
    std::move(cb).Run(degraded_recoverability_value_ ==
                      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
  }
  if (!next_refresh_timer_.IsRunning()) {
    Start();
  }
}

void TrustedVaultDegradedRecoverabilityHandler::UpdateCurrentRefreshPeriod() {
  if (degraded_recoverability_value_ ==
      trusted_vault_pb::DegradedRecoverabilityValue::kDegraded) {
    current_refresh_period_ = short_degraded_recoverability_refresh_period_;
    return;
  }
  current_refresh_period_ = long_degraded_recoverability_refresh_period_;
}

void TrustedVaultDegradedRecoverabilityHandler::Start() {
  base::UmaHistogramExactLinear(
      "Sync.TrustedVaultDegradedRecoverabilityValue2",
      degraded_recoverability_value_,
      trusted_vault_pb::DegradedRecoverabilityValue_ARRAYSIZE);
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
  trusted_vault_pb::DegradedRecoverabilityValue
      old_degraded_recoverability_value = degraded_recoverability_value_;
  switch (status) {
    case TrustedVaultRecoverabilityStatus::kDegraded:
      degraded_recoverability_value_ =
          trusted_vault_pb::DegradedRecoverabilityValue::kDegraded;
      break;
    case TrustedVaultRecoverabilityStatus::kNotDegraded:
      degraded_recoverability_value_ =
          trusted_vault_pb::DegradedRecoverabilityValue::kNotDegraded;
      break;
    case TrustedVaultRecoverabilityStatus::kError:
      // TODO(crbug.com/40790270): To be handled.
      break;
  }
  if (!pending_get_is_recoverability_degraded_callback_.is_null()) {
    std::move(pending_get_is_recoverability_degraded_callback_)
        .Run(degraded_recoverability_value_ ==
             trusted_vault_pb::DegradedRecoverabilityValue::kDegraded);
    pending_get_is_recoverability_degraded_callback_ = base::NullCallback();
  }
  if (degraded_recoverability_value_ != old_degraded_recoverability_value) {
    delegate_->OnDegradedRecoverabilityChanged();
    UpdateCurrentRefreshPeriod();
  }
  last_refresh_time_ = base::TimeTicks::Now();
  delegate_->WriteDegradedRecoverabilityState(MakeDegradedRecoverabilityState(
      degraded_recoverability_value_, base::Time::Now()));
  next_refresh_timer_.Start(
      FROM_HERE, current_refresh_period_, this,
      &TrustedVaultDegradedRecoverabilityHandler::Refresh);
}

}  // namespace trusted_vault
