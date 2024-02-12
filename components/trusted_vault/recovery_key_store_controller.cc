// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_controller.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/trusted_vault/recovery_key_store_connection.h"

namespace trusted_vault {

RecoveryKeyStoreController::ApplicationKey::ApplicationKey(
    std::string name,
    std::vector<uint8_t> public_key)
    : name(std::move(name)), public_key(std::move(public_key)) {}
RecoveryKeyStoreController::ApplicationKey::ApplicationKey(ApplicationKey&) =
    default;
RecoveryKeyStoreController::ApplicationKey::ApplicationKey(ApplicationKey&&) =
    default;
RecoveryKeyStoreController::ApplicationKey&
RecoveryKeyStoreController::ApplicationKey::operator=(ApplicationKey&) =
    default;
RecoveryKeyStoreController::ApplicationKey&
RecoveryKeyStoreController::ApplicationKey::operator=(ApplicationKey&&) =
    default;
RecoveryKeyStoreController::ApplicationKey::~ApplicationKey() = default;

RecoveryKeyStoreController::RecoveryKeyProvider::~RecoveryKeyProvider() =
    default;

RecoveryKeyStoreController::OngoingUpdate::OngoingUpdate() = default;
RecoveryKeyStoreController::OngoingUpdate::OngoingUpdate(OngoingUpdate&&) =
    default;
RecoveryKeyStoreController::OngoingUpdate&
RecoveryKeyStoreController::OngoingUpdate::operator=(OngoingUpdate&&) = default;
RecoveryKeyStoreController::OngoingUpdate::~OngoingUpdate() = default;

RecoveryKeyStoreController::RecoveryKeyStoreController(
    CoreAccountInfo account_info,
    std::unique_ptr<RecoveryKeyProvider> recovery_key_provider,
    std::unique_ptr<RecoveryKeyStoreConnection> connection,
    Observer* observer,
    base::Time last_update,
    base::TimeDelta update_period)
    : account_info_(std::move(account_info)),
      recovery_key_provider_(std::move(recovery_key_provider)),
      connection_(std::move(connection)),
      observer_(observer),
      update_period_(update_period) {
  CHECK(recovery_key_provider_);
  CHECK(connection_);
  CHECK(observer_);

  // Schedule the next update. If an update has occurred previously, delay the
  // update by the remainder of the partially elapsed `update_period`. Note that
  // `last_update` may actually be in the future.
  auto now = base::Time::Now();
  last_update = std::min(now, last_update);
  base::TimeDelta delay;
  if (!last_update.is_null() && (last_update + update_period) > now) {
    base::TimeDelta elapsed = now - last_update;
    delay = update_period - elapsed;
  }
  ScheduleNextUpdate(delay);
}

RecoveryKeyStoreController::~RecoveryKeyStoreController() = default;

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
    std::optional<trusted_vault_pb::UpdateVaultRequest> update_vault_request) {
  CHECK(ongoing_update_);

  if (!update_vault_request ||
      update_vault_request->vault().application_keys().empty()) {
    CompleteUpdateRequest({});
    return;
  }

  std::vector<ApplicationKey> uploaded_application_keys;
  for (const trusted_vault_pb::ApplicationKey& key :
       update_vault_request->vault().application_keys()) {
    uploaded_application_keys.push_back(ApplicationKey{
        key.key_name(),
        std::vector<uint8_t>(key.asymmetric_key_pair().public_key().begin(),
                             key.asymmetric_key_pair().public_key().end())});
  }
  ongoing_update_->request = connection_->UpdateRecoveryKeyStore(
      account_info_, std::move(*update_vault_request),
      base::BindOnce(&RecoveryKeyStoreController::OnUpdateRecoveryKeyStore,
                     weak_factory_.GetWeakPtr(),
                     std::move(uploaded_application_keys)));
}

void RecoveryKeyStoreController::OnUpdateRecoveryKeyStore(
    std::vector<ApplicationKey> application_keys,
    UpdateRecoveryKeyStoreStatus status) {
  if (status != UpdateRecoveryKeyStoreStatus::kSuccess) {
    DVLOG(1) << "UpdateRecoveryKeyStore failed: " << static_cast<int>(status);
    CompleteUpdateRequest({});
    return;
  }

  CompleteUpdateRequest(application_keys);
}

void RecoveryKeyStoreController::CompleteUpdateRequest(
    const std::vector<ApplicationKey>& application_keys) {
  CHECK(ongoing_update_);
  CHECK(ongoing_update_);
  ongoing_update_ = std::nullopt;
  if (!application_keys.empty()) {
    observer_->OnUpdateRecoveryKeyStore(application_keys);
  }
  ScheduleNextUpdate(update_period_);
}

}  // namespace trusted_vault
