// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/physical_device_recovery_factor.h"

#include "base/task/bind_post_task.h"
#include "components/trusted_vault/proto_string_bytes_conversion.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"

namespace trusted_vault {

namespace {

constexpr int kCurrentDeviceRegistrationVersion = 1;

TrustedVaultDownloadKeysStatusForUMA GetDownloadKeysStatusForUMAFromResponse(
    TrustedVaultDownloadKeysStatus response_status) {
  switch (response_status) {
    case TrustedVaultDownloadKeysStatus::kSuccess:
      return TrustedVaultDownloadKeysStatusForUMA::kSuccess;
    case TrustedVaultDownloadKeysStatus::kMemberNotFound:
      return TrustedVaultDownloadKeysStatusForUMA::kMemberNotFound;
    case TrustedVaultDownloadKeysStatus::kMembershipNotFound:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipNotFound;
    case TrustedVaultDownloadKeysStatus::kMembershipCorrupted:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipCorrupted;
    case TrustedVaultDownloadKeysStatus::kMembershipEmpty:
      return TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty;
    case TrustedVaultDownloadKeysStatus::kNoNewKeys:
      return TrustedVaultDownloadKeysStatusForUMA::kNoNewKeys;
    case TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed:
      return TrustedVaultDownloadKeysStatusForUMA::kKeyProofsVerificationFailed;
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
      return TrustedVaultDownloadKeysStatusForUMA::kAccessTokenFetchingFailure;
    case TrustedVaultDownloadKeysStatus::kNetworkError:
      return TrustedVaultDownloadKeysStatusForUMA::kNetworkError;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      return TrustedVaultDownloadKeysStatusForUMA::kOtherError;
  }

  NOTREACHED();
}

}  // namespace

PhysicalDeviceRecoveryFactor::PhysicalDeviceRecoveryFactor(
    SecurityDomainId security_domain_id,
    StandaloneTrustedVaultStorage* storage,
    TrustedVaultThrottlingConnection* connection,
    CoreAccountInfo primary_account)
    : security_domain_id_(security_domain_id),
      storage_(storage),
      connection_(connection),
      primary_account_(primary_account) {
  CHECK(storage_);
  CHECK(connection_);
}
PhysicalDeviceRecoveryFactor::~PhysicalDeviceRecoveryFactor() = default;

LocalRecoveryFactorType PhysicalDeviceRecoveryFactor::GetRecoveryFactorType()
    const {
  return LocalRecoveryFactorType::kPhysicalDevice;
}

void PhysicalDeviceRecoveryFactor::AttemptRecovery(AttemptRecoveryCallback cb) {
  auto* per_user_vault = GetPrimaryAccountVault();

  if (!GetPrimaryAccountVault()
           ->local_device_registration_info()
           .device_registered()) {
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered,
        std::move(cb));
    return;
  }

  if (connection_->AreRequestsThrottled(primary_account_)) {
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide,
        std::move(cb));
    return;
  }

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          ProtoStringToBytes(per_user_vault->local_device_registration_info()
                                 .private_key_material()));
  if (!key_pair) {
    // Corrupted state: device is registered, but `key_pair` can't be imported.
    // TODO(crbug.com/40699425): restore from this state (throw away the key
    // and trigger device registration again).
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kCorruptedLocalDeviceRegistration,
        std::move(cb));
    return;
  }

  // Guaranteed by `device_registered` check above.
  CHECK(!per_user_vault->vault_key().empty());
  ongoing_request_ = connection_->DownloadNewKeys(
      primary_account_,
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

bool PhysicalDeviceRecoveryFactor::IsRegistered() {
  auto* per_user_vault = GetPrimaryAccountVault();
  return per_user_vault->local_device_registration_info().device_registered();
}

void PhysicalDeviceRecoveryFactor::MarkAsNotRegistered() {
  auto* per_user_vault = GetPrimaryAccountVault();
  per_user_vault->mutable_local_device_registration_info()
      ->set_device_registered(false);
  per_user_vault->mutable_local_device_registration_info()
      ->clear_device_registered_version();
  storage_->WriteDataToDisk();
}

TrustedVaultRecoveryFactorRegistrationStateForUMA
PhysicalDeviceRecoveryFactor::MaybeRegister(RegisterCallback cb) {
  auto* per_user_vault = GetPrimaryAccountVault();

  if (per_user_vault->local_device_registration_info().device_registered()) {
    static_assert(kCurrentDeviceRegistrationVersion == 1);
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kAlreadyRegisteredV1;
  }

  if (per_user_vault->last_registration_returned_local_data_obsolete()) {
    // Client already knows that existing vault keys (or their absence) isn't
    // sufficient for device registration. Fresh keys should be obtained
    // first.
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kLocalKeysAreStale;
  }

  if (connection_->AreRequestsThrottled(primary_account_)) {
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kThrottledClientSide;
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
    ongoing_registration_request_ = connection_->RegisterAuthenticationFactor(
        primary_account_,
        GetTrustedVaultKeysWithVersions(
            StandaloneTrustedVaultStorage::GetAllVaultKeys(*per_user_vault),
            per_user_vault->last_vault_key_version()),
        key_pair->public_key(), LocalPhysicalDevice(),
        base::BindOnce(&PhysicalDeviceRecoveryFactor::OnRegistered,
                       base::Unretained(this), std::move(cb), true));
  } else {
    ongoing_registration_request_ = connection_->RegisterLocalDeviceWithoutKeys(
        primary_account_, key_pair->public_key(),
        base::BindOnce(&PhysicalDeviceRecoveryFactor::OnRegistered,
                       base::Unretained(this), std::move(cb), false));
  }

  CHECK(ongoing_registration_request_);

  return had_generated_key_pair
             ? TrustedVaultRecoveryFactorRegistrationStateForUMA::
                   kAttemptingRegistrationWithExistingKeyPair
             : TrustedVaultRecoveryFactorRegistrationStateForUMA::
                   kAttemptingRegistrationWithNewKeyPair;
}

trusted_vault_pb::LocalTrustedVaultPerUser*
PhysicalDeviceRecoveryFactor::GetPrimaryAccountVault() {
  auto* per_user_vault = storage_->FindUserVault(primary_account_.gaia);
  // PhysicalDeviceRecoveryFactor is only constructed by
  // StandaloneTrustedVaultBackend when a primary account is set, and it also
  // ensures that there is a user vault in storage at the same time.
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

  RecordTrustedVaultDownloadKeysStatus(
      LocalRecoveryFactorType::kPhysicalDevice, security_domain_id_,
      GetDownloadKeysStatusForUMAFromResponse(status));

  RecoveryStatus recovery_status = RecoveryStatus::kFailure;
  switch (status) {
    case TrustedVaultDownloadKeysStatus::kSuccess: {
      recovery_status = RecoveryStatus::kSuccess;
      break;
    }
    case TrustedVaultDownloadKeysStatus::kMemberNotFound:
    case TrustedVaultDownloadKeysStatus::kMembershipNotFound:
    case TrustedVaultDownloadKeysStatus::kMembershipCorrupted:
    case TrustedVaultDownloadKeysStatus::kMembershipEmpty:
    case TrustedVaultDownloadKeysStatus::kKeyProofsVerificationFailed: {
      // Unable to download new keys due to known protocol errors. The only way
      // to go out of these states is to receive new vault keys through external
      // means. It's safe to mark device as not registered regardless of the
      // cause (device registration will be triggered once new vault keys are
      // available).
      MarkAsNotRegistered();
      break;
    }
    case TrustedVaultDownloadKeysStatus::kNoNewKeys: {
      // The registration itself exists, but there's no additional keys to
      // download. This is bad because key download attempts are triggered for
      // the case where local keys have been marked as stale, which means the
      // user is likely in an unrecoverable state.
      connection_->RecordFailedRequestForThrottling(primary_account_);
      recovery_status = RecoveryStatus::kNoNewKeys;
      break;
    }
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
    case TrustedVaultDownloadKeysStatus::kNetworkError:
      // Request wasn't sent to the server, so there is no need for throttling.
      break;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      connection_->RecordFailedRequestForThrottling(primary_account_);
      break;
  }

  std::move(cb).Run(recovery_status, new_vault_keys, last_vault_key_version);
}

void PhysicalDeviceRecoveryFactor::FulfillRecoveryWithFailure(
    TrustedVaultDownloadKeysStatusForUMA status_for_uma,
    AttemptRecoveryCallback cb) {
  RecordTrustedVaultDownloadKeysStatus(LocalRecoveryFactorType::kPhysicalDevice,
                                       security_domain_id_, status_for_uma);

  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(cb), RecoveryStatus::kFailure,
                     std::vector<std::vector<uint8_t>>(), 0))
      .Run();
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

  switch (status) {
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      // kAlreadyRegistered handled as success, because it only means that
      // client doesn't fully handled successful device registration before.
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered(true);
      per_user_vault->mutable_local_device_registration_info()
          ->set_device_registered_version(kCurrentDeviceRegistrationVersion);
      per_user_vault->clear_last_registration_returned_local_data_obsolete();
      storage_->WriteDataToDisk();
      break;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      per_user_vault->set_last_registration_returned_local_data_obsolete(true);
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
