// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_controller.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/recovery_key_store_connection.h"

namespace trusted_vault {

RecoveryKeyStoreController::RecoveryKeyProvider::~RecoveryKeyProvider() =
    default;

RecoveryKeyStoreController::OngoingUpdate::OngoingUpdate() = default;
RecoveryKeyStoreController::OngoingUpdate::OngoingUpdate(OngoingUpdate&&) =
    default;
RecoveryKeyStoreController::OngoingUpdate&
RecoveryKeyStoreController::OngoingUpdate::operator=(OngoingUpdate&&) = default;
RecoveryKeyStoreController::OngoingUpdate::~OngoingUpdate() = default;

RecoveryKeyStoreController::RecoveryKeyStoreController(
    std::unique_ptr<RecoveryKeyProvider> recovery_key_provider,
    std::unique_ptr<RecoveryKeyStoreConnection> connection,
    Delegate* delegate)
    : recovery_key_provider_(std::move(recovery_key_provider)),
      connection_(std::move(connection)),
      delegate_(delegate) {
  CHECK(recovery_key_provider_);
  CHECK(connection_);
  CHECK(delegate_);
}

RecoveryKeyStoreController::~RecoveryKeyStoreController() = default;

void RecoveryKeyStoreController::StartPeriodicUploads(
    CoreAccountInfo account_info,
    const trusted_vault_pb::RecoveryKeyStoreState& state,
    base::TimeDelta update_period) {
  // Cancel scheduled and in-progress uploads, if any.
  if (account_info_) {
    StopPeriodicUploads();
  }

  CHECK(!account_info_);
  CHECK(!next_update_timer_.IsRunning());
  CHECK(!ongoing_update_);

  update_period_ = update_period;
  state_ = state;
  CHECK(state_.recovery_key_store_upload_enabled());

  // Schedule the next update. If an update has occurred previously, delay the
  // update by the remainder of the partially elapsed `update_period`. Note that
  // `last_update` may actually be in the future.
  account_info_ = account_info;
  base::Time last_update;
  if (state_.last_recovery_key_store_update_millis_since_unix_epoch()) {
    last_update = ProtoTimeToTime(
        state_.last_recovery_key_store_update_millis_since_unix_epoch());
  }
  auto now = base::Time::Now();
  last_update = std::min(now, last_update);
  base::TimeDelta delay;
  if (!last_update.is_null() && (last_update + update_period) > now) {
    base::TimeDelta elapsed = now - last_update;
    delay = update_period - elapsed;
  }
  ScheduleNextUpdate(delay);
}

void RecoveryKeyStoreController::StopPeriodicUploads() {
  account_info_.reset();
  next_update_timer_.Stop();
  ongoing_update_.reset();
  state_ = {};
}

void RecoveryKeyStoreController::ScheduleNextUpdate(base::TimeDelta delay) {
  next_update_timer_.Start(FROM_HERE, delay, this,
                           &RecoveryKeyStoreController::UpdateRecoveryKeyStore);
}

void RecoveryKeyStoreController::UpdateRecoveryKeyStore() {
  CHECK(!ongoing_update_);

  ongoing_update_.emplace(OngoingUpdate{});
  recovery_key_provider_->GetCurrentRecoveryKeyStoreData(base::BindOnce(
      &RecoveryKeyStoreController::OnGetCurrentRecoveryKeyStoreData,
      weak_factory_.GetWeakPtr()));
}

void RecoveryKeyStoreController::OnGetCurrentRecoveryKeyStoreData(
    std::optional<trusted_vault_pb::Vault> vault) {
  CHECK(ongoing_update_);

  if (!vault || vault->application_keys().empty()) {
    CompleteUpdateRequest({});
    return;
  }

  CHECK_EQ(vault->application_keys().size(), 1);
  const trusted_vault_pb::ApplicationKey& uploaded_application_key =
      *vault->application_keys().begin();
  ongoing_update_->request = connection_->UpdateRecoveryKeyStore(
      *account_info_, std::move(*vault),
      base::BindOnce(&RecoveryKeyStoreController::OnUpdateRecoveryKeyStore,
                     weak_factory_.GetWeakPtr(), uploaded_application_key));
}

void RecoveryKeyStoreController::OnUpdateRecoveryKeyStore(
    trusted_vault_pb::ApplicationKey application_key,
    UpdateRecoveryKeyStoreStatus status) {
  if (status != UpdateRecoveryKeyStoreStatus::kSuccess) {
    DVLOG(1) << "UpdateRecoveryKeyStore failed: " << static_cast<int>(status);
    CompleteUpdateRequest({});
    return;
  }

  CompleteUpdateRequest(std::move(application_key));
}

void RecoveryKeyStoreController::CompleteUpdateRequest(
    std::optional<trusted_vault_pb::ApplicationKey> uploaded_application_key) {
  CHECK(ongoing_update_);
  ongoing_update_ = std::nullopt;
  if (uploaded_application_key) {
    state_.set_last_recovery_key_store_update_millis_since_unix_epoch(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    state_.set_public_key(
        uploaded_application_key->asymmetric_key_pair().public_key());
    // TODO: crbug.com/1223853 - Register the ApplicationKey as a security
    // domain member and keep track of the result in `state_`.
    delegate_->WriteRecoveryKeyStoreState(state_);
  }
  ScheduleNextUpdate(update_period_);
}

}  // namespace trusted_vault
