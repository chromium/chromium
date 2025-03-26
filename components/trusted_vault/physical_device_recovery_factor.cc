// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/physical_device_recovery_factor.h"

#include "base/task/bind_post_task.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"

namespace trusted_vault {

namespace {

constexpr int kCurrentDeviceRegistrationVersion = 1;

}  // namespace

PhysicalDeviceRecoveryFactor::PhysicalDeviceRecoveryFactor(
    StandaloneTrustedVaultStorage* storage,
    std::optional<CoreAccountInfo> primary_account)
    : storage_(storage), primary_account_(primary_account) {
  CHECK(storage_);
}
PhysicalDeviceRecoveryFactor::~PhysicalDeviceRecoveryFactor() = default;

void PhysicalDeviceRecoveryFactor::AttemptRecovery(
    TrustedVaultConnection* connection,
    bool connection_requests_throttled,
    AttemptRecoveryCallback cb,
    AttemptRecoveryFailureCallback failure_cb) {
  auto* per_user_vault = GetPrimaryAccountVault();

  if (!GetPrimaryAccountVault()
           ->local_device_registration_info()
           .device_registered()) {
    base::BindPostTaskToCurrentDefault(
        base::BindOnce(
            std::move(failure_cb),
            TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered))
        .Run();
    return;
  }

  if (connection_requests_throttled) {
    base::BindPostTaskToCurrentDefault(
        base::BindOnce(
            std::move(failure_cb),
            TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide))
        .Run();
    return;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          ProtoStringToBytes(per_user_vault->local_device_registration_info()
                                 .private_key_material()));
  if (!key_pair) {
    // Corrupted state: device is registered, but `key_pair` can't be
    // imported.
    // TODO(crbug.com/40699425): restore from this state (throw away the key
    // and trigger device registration again).
    base::BindPostTaskToCurrentDefault(
        base::BindOnce(std::move(failure_cb),
                       TrustedVaultDownloadKeysStatusForUMA::
                           kCorruptedLocalDeviceRegistration))
        .Run();
    return;
  }

  // Guaranteed by `device_registered` check above.
  CHECK(!per_user_vault->vault_key().empty());
  ongoing_request_ = connection->DownloadNewKeys(
      *primary_account_,
      TrustedVaultKeyAndVersion(
          ProtoStringToBytes(
              per_user_vault->vault_key().rbegin()->key_material()),
          per_user_vault->last_vault_key_version()),
      std::move(key_pair),
      // `this` outlives `ongoing_request_`.
      base::BindOnce(&PhysicalDeviceRecoveryFactor::OnKeysDownloaded,
                     base::Unretained(this), std::move(cb)));
  CHECK(ongoing_request_);
}

void PhysicalDeviceRecoveryFactor::MarkAsNotRegistered() {
  auto* per_user_vault = GetPrimaryAccountVault();
  per_user_vault->mutable_local_device_registration_info()
      ->set_device_registered(false);
  per_user_vault->mutable_local_device_registration_info()
      ->clear_device_registered_version();
  storage_->WriteDataToDisk();
}

void PhysicalDeviceRecoveryFactor::ClearRegistrationAttemptInfo(
    const GaiaId& gaia_id) {
  auto* per_user_vault = storage_->FindUserVault(gaia_id);
  CHECK(per_user_vault);

  per_user_vault->mutable_local_device_registration_info()
      ->set_last_registration_returned_local_data_obsolete(false);
  storage_->WriteDataToDisk();
}

TrustedVaultDeviceRegistrationStateForUMA
PhysicalDeviceRecoveryFactor::MaybeRegister(TrustedVaultConnection* connection,
                                            bool connection_requests_throttled,
                                            RegisterCallback cb) {
  auto* per_user_vault = GetPrimaryAccountVault();

  if (per_user_vault->local_device_registration_info().device_registered()) {
    static_assert(kCurrentDeviceRegistrationVersion == 1);
    return TrustedVaultDeviceRegistrationStateForUMA::kAlreadyRegisteredV1;
  }

  if (per_user_vault->local_device_registration_info()
          .last_registration_returned_local_data_obsolete()) {
    // Client already knows that existing vault keys (or their absence) isn't
    // sufficient for device registration. Fresh keys should be obtained
    // first.
    return TrustedVaultDeviceRegistrationStateForUMA::kLocalKeysAreStale;
  }

  if (connection_requests_throttled) {
    return TrustedVaultDeviceRegistrationStateForUMA::kThrottledClientSide;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair;
  if (per_user_vault->has_local_device_registration_info()) {
    key_pair = SecureBoxKeyPair::CreateByPrivateKeyImport(
        /*private_key_bytes=*/ProtoStringToBytes(
            per_user_vault->local_device_registration_info()
                .private_key_material()));
  }

  const bool had_generated_key_pair = key_pair != nullptr;

  if (!key_pair) {
    key_pair = SecureBoxKeyPair::GenerateRandom();
    // It's possible that device will be successfully registered, but the
    // client won't persist this state (for example response doesn't reach the
    // client or registration callback is cancelled). To avoid duplicated
    // registrations device key is stored before sending the registration
    // request, so the same key will be used for future registration attempts.
    AssignBytesToProtoString(
        key_pair->private_key().ExportToBytes(),
        per_user_vault->mutable_local_device_registration_info()
            ->mutable_private_key_material());
    storage_->WriteDataToDisk();
  }

  // `this` outlives `ongoing_registration_request_`, so it's safe to
  // use base::Unretained() here.
  if (StandaloneTrustedVaultStorage::HasNonConstantKey(*per_user_vault)) {
    ongoing_registration_request_ = connection->RegisterAuthenticationFactor(
        *primary_account_,
        GetTrustedVaultKeysWithVersions(
            StandaloneTrustedVaultStorage::GetAllVaultKeys(*per_user_vault),
            per_user_vault->last_vault_key_version()),
        key_pair->public_key(), LocalPhysicalDevice(),
        base::BindOnce(&PhysicalDeviceRecoveryFactor::OnRegistered,
                       base::Unretained(this), std::move(cb), true));
  } else {
    ongoing_registration_request_ = connection->RegisterLocalDeviceWithoutKeys(
        *primary_account_, key_pair->public_key(),
        base::BindOnce(&PhysicalDeviceRecoveryFactor::OnRegistered,
                       base::Unretained(this), std::move(cb), false));
  }

  CHECK(ongoing_registration_request_);

  return had_generated_key_pair ? TrustedVaultDeviceRegistrationStateForUMA::
                                      kAttemptingRegistrationWithExistingKeyPair
                                : TrustedVaultDeviceRegistrationStateForUMA::
                                      kAttemptingRegistrationWithNewKeyPair;
}

trusted_vault_pb::LocalTrustedVaultPerUser*
PhysicalDeviceRecoveryFactor::GetPrimaryAccountVault() {
  CHECK(primary_account_);
  auto* per_user_vault = storage_->FindUserVault(primary_account_->gaia);
  CHECK(per_user_vault);
  return per_user_vault;
}

void PhysicalDeviceRecoveryFactor::OnKeysDownloaded(
    AttemptRecoveryCallback cb,
    TrustedVaultDownloadKeysStatus status,
    const std::vector<std::vector<uint8_t>>& new_vault_keys,
    int last_vault_key_version) {
  // This method should be called only as a result of
  // `ongoing_request_` completion/failure, verify this
  // condition and destroy `ongoing_request_` as it's not
  // needed anymore.
  CHECK(ongoing_request_);
  ongoing_request_ = nullptr;

  std::move(cb).Run(status, new_vault_keys, last_vault_key_version);
}

void PhysicalDeviceRecoveryFactor::OnRegistered(
    RegisterCallback cb,
    bool had_local_keys,
    TrustedVaultRegistrationStatus status,
    int key_version) {
  // This method should be called only as a result of
  // `ongoing_registration_request_` completion/failure, verify this
  // condition and destroy `ongoing_registration_request_` as it's not
  // needed anymore.
  CHECK(ongoing_registration_request_);
  ongoing_registration_request_ = nullptr;

  auto* per_user_vault = GetPrimaryAccountVault();

  // Registration is only attempted if there was no previous failure with
  // `kLocalDataObsolete`. If this precondition wasn't guaranteed here, the
  // field would need to be reset for some cases below such as `kSuccess` and
  // `kAlreadyRegistered`.
  CHECK(!per_user_vault->local_device_registration_info()
             .last_registration_returned_local_data_obsolete());
  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      // kAlreadyRegistered handled as success, because it only means that
      // client doesn't fully handled successful device registration before.
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(true);
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered_version(kCurrentDeviceRegistrationVersion);
      storage_->WriteDataToDisk();
      break;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      per_user_vault->mutable_local_device_registration_info()
          ->set_last_registration_returned_local_data_obsolete(true);
      storage_->WriteDataToDisk();
      break;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kNetworkError:
    case TrustedVaultRegistrationStatus::kOtherError:
      break;
  }

  std::move(cb).Run(status, key_version, had_local_keys);
}

}  // namespace trusted_vault
