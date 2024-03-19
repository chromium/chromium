// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_controller.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/proto_time_conversion.h"
#include "components/trusted_vault/recovery_key_store_connection.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"

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
                           &RecoveryKeyStoreController::StartUpdateCycle);
}

void RecoveryKeyStoreController::StartUpdateCycle() {
  CHECK(!ongoing_update_);

  // For each update cycle we fetch the current recovery key, add it to the
  // security domain, upload it to recovery key store. Once any step fails or
  // all of them complete, we schedule the next cycle.
  ongoing_update_.emplace(OngoingUpdate{});
  recovery_key_provider_->GetCurrentRecoveryKeyStoreData(base::BindOnce(
      &RecoveryKeyStoreController::OnGetCurrentRecoveryKeyStoreData,
      weak_factory_.GetWeakPtr()));
}

void RecoveryKeyStoreController::OnGetCurrentRecoveryKeyStoreData(
    std::optional<trusted_vault_pb::Vault> vault) {
  CHECK(ongoing_update_);

  if (!vault || vault->application_keys().empty()) {
    CompleteUpdateCycle();
    return;
  }

  CHECK_EQ(vault->application_keys().size(), 1);
  const trusted_vault_pb::ApplicationKey& application_key =
      *vault->application_keys().begin();
  const std::string& application_public_key =
      application_key.asymmetric_key_pair().public_key();
  if (!SecureBoxPublicKey::CreateByImport(
          ProtoStringToBytes(application_public_key))) {
    DLOG(ERROR) << "Invalid public key";
    CompleteUpdateCycle();
    return;
  }

  ongoing_update_->current_vault_proto = std::move(*vault);

  if (application_public_key != state_.public_key()) {
    state_.set_public_key(application_public_key);
    state_.set_recovery_key_is_registered_to_security_domain(false);
  }

  MaybeAddRecoveryKeyToSecurityDomain();
}

void RecoveryKeyStoreController::MaybeAddRecoveryKeyToSecurityDomain() {
  CHECK(ongoing_update_);
  CHECK(!state_.public_key().empty());

  if (state_.recovery_key_is_registered_to_security_domain()) {
    UpdateRecoveryKeyStore();
    return;
  }

  // `delegate_` outlives `this`, so base::Unretained() is safe to use here.
  delegate_->AddRecoveryKeyToSecurityDomain(
      ProtoStringToBytes(state_.public_key()),
      base::BindOnce(
          &RecoveryKeyStoreController::OnRecoveryKeyAddedToSecurityDomain,
          base::Unretained(this)));
}

void RecoveryKeyStoreController::OnRecoveryKeyAddedToSecurityDomain(
    TrustedVaultRegistrationStatus status) {
  CHECK(ongoing_update_);

  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      state_.set_recovery_key_is_registered_to_security_domain(true);
      delegate_->WriteRecoveryKeyStoreState(state_);
      UpdateRecoveryKeyStore();
      break;
    case trusted_vault::TrustedVaultRegistrationStatus::kLocalDataObsolete:
    case trusted_vault::TrustedVaultRegistrationStatus::kNetworkError:
    case trusted_vault::TrustedVaultRegistrationStatus::kOtherError:
    case trusted_vault::TrustedVaultRegistrationStatus::
        kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case trusted_vault::TrustedVaultRegistrationStatus::
        kTransientAccessTokenFetchError:
      DVLOG(1) << "AddRecoveryKeyToSecurityDomain failed: "
               << static_cast<int>(status);
      CompleteUpdateCycle();
      break;
  }
}

void RecoveryKeyStoreController::UpdateRecoveryKeyStore() {
  CHECK(ongoing_update_->current_vault_proto);
  CHECK(state_.recovery_key_is_registered_to_security_domain());

  // Recovery keys need to be refreshed server-side periodically, so we do
  // an upload even if we potentially have done one before.
  ongoing_update_->request = connection_->UpdateRecoveryKeyStore(
      *account_info_, std::move(*ongoing_update_->current_vault_proto),
      base::BindOnce(&RecoveryKeyStoreController::OnUpdateRecoveryKeyStore,
                     weak_factory_.GetWeakPtr()));
}

void RecoveryKeyStoreController::OnUpdateRecoveryKeyStore(
    UpdateRecoveryKeyStoreStatus status) {
  if (status != UpdateRecoveryKeyStoreStatus::kSuccess) {
    state_.set_last_recovery_key_store_update_millis_since_unix_epoch(
        base::Time::Now().InMillisecondsSinceUnixEpoch());
    delegate_->WriteRecoveryKeyStoreState(state_);
  } else {
    DVLOG(1) << "UpdateRecoveryKeyStore failed: " << static_cast<int>(status);
  }
  CompleteUpdateCycle();
}

void RecoveryKeyStoreController::CompleteUpdateCycle() {
  CHECK(ongoing_update_);
  ongoing_update_ = std::nullopt;
  ScheduleNextUpdate(update_period_);
}

}  // namespace trusted_vault
