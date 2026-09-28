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

PhysicalDeviceRecoveryFactor::OngoingRecovery::OngoingRecovery(
    AttemptRecoveryCallback cb,
    std::unique_ptr<TrustedVaultConnection::Request> request)
    : callback(std::move(cb)), request(std::move(request)) {
  CHECK(this->callback);
  CHECK(this->request);
}

PhysicalDeviceRecoveryFactor::OngoingRecovery::OngoingRecovery(
    OngoingRecovery&&) = default;

PhysicalDeviceRecoveryFactor::OngoingRecovery&
PhysicalDeviceRecoveryFactor::OngoingRecovery::operator=(OngoingRecovery&&) =
    default;

PhysicalDeviceRecoveryFactor::OngoingRecovery::~OngoingRecovery() = default;

PhysicalDeviceRecoveryFactor::OngoingRegistration::OngoingRegistration(
    RegisterCallback cb,
    std::unique_ptr<TrustedVaultConnection::Request> request)
    : callback(std::move(cb)), request(std::move(request)) {
  CHECK(this->callback);
  CHECK(this->request);
}

PhysicalDeviceRecoveryFactor::OngoingRegistration::OngoingRegistration(
    OngoingRegistration&&) = default;

PhysicalDeviceRecoveryFactor::OngoingRegistration&
PhysicalDeviceRecoveryFactor::OngoingRegistration::operator=(
    OngoingRegistration&&) = default;

PhysicalDeviceRecoveryFactor::OngoingRegistration::~OngoingRegistration() =
    default;

PhysicalDeviceRecoveryFactor::PhysicalDeviceRecoveryFactor(
    PhysicalDeviceStorage* storage,
    RecoveryFactorRegistrationStorage* registration_storage,
    KeyStorage* key_storage,
    TrustedVaultThrottlingConnection* connection,
    CoreAccountInfo primary_account)
    : storage_(storage),
      registration_storage_(registration_storage),
      key_storage_(key_storage),
      connection_(connection),
      primary_account_(primary_account) {
  CHECK(storage_);
  CHECK(registration_storage_);
  CHECK(key_storage_);
  CHECK(connection_);
}
PhysicalDeviceRecoveryFactor::~PhysicalDeviceRecoveryFactor() = default;

LocalRecoveryFactorType PhysicalDeviceRecoveryFactor::GetRecoveryFactorType()
    const {
  return LocalRecoveryFactorType::kPhysicalDevice;
}

void PhysicalDeviceRecoveryFactor::AttemptRecovery(
    SecurityDomainId security_domain_id,
    AttemptRecoveryCallback cb) {
  if (!IsRegistered(security_domain_id)) {
    FulfillRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered,
        std::move(cb));
    return;
  }

  if (connection_->AreRequestsThrottled(primary_account_, security_domain_id)) {
    FulfillRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide,
        std::move(cb));
    return;
  }

  const PhysicalDeviceRecoveryFactorData physical_device_data =
      storage_->GetPhysicalDeviceRecoveryFactorData(primary_account_.gaia);
  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::CreateByPrivateKeyImport(
          ProtoStringToBytes(physical_device_data.private_key_material()));
  if (!key_pair) {
    // Corrupted state: device is registered, but `key_pair` can't be imported.
    // TODO(crbug.com/40699425): restore from this state (throw away the key
    // and trigger device registration again).
    FulfillRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kCorruptedLocalDeviceRegistration,
        std::move(cb));
    return;
  }

  std::vector<std::vector<uint8_t>> vault_keys =
      key_storage_->GetVaultKeys(primary_account_.gaia, security_domain_id);
  int last_vault_key_version = key_storage_->GetLastKeyVersion(
      primary_account_.gaia, security_domain_id);

  // Guaranteed by `device_registered` check above.
  CHECK(!vault_keys.empty());
  std::unique_ptr<TrustedVaultConnection::Request> request =
      connection_->DownloadNewKeys(
          primary_account_, security_domain_id,
          TrustedVaultKeyAndVersion(vault_keys.back(), last_vault_key_version),
          std::move(key_pair),
          // `this` outlives `ongoing_recoveries_`.
          base::BindOnce(&PhysicalDeviceRecoveryFactor::OnKeysDownloaded,
                         base::Unretained(this), security_domain_id));
  ongoing_recoveries_.insert_or_assign(
      security_domain_id, OngoingRecovery(std::move(cb), std::move(request)));
}

bool PhysicalDeviceRecoveryFactor::IsRegistered(
    SecurityDomainId security_domain_id) {
  return registration_storage_->IsRecoveryFactorRegistered(
      primary_account_.gaia, security_domain_id,
      LocalRecoveryFactorType::kPhysicalDevice);
}

void PhysicalDeviceRecoveryFactor::MarkAsNotRegistered(
    SecurityDomainId security_domain_id) {
  registration_storage_->SetRecoveryFactorRegistered(
      primary_account_.gaia, security_domain_id,
      LocalRecoveryFactorType::kPhysicalDevice, false);
}

TrustedVaultRecoveryFactorRegistrationStateForUMA
PhysicalDeviceRecoveryFactor::MaybeRegister(SecurityDomainId security_domain_id,
                                            RegisterCallback cb) {
  if (IsRegistered(security_domain_id)) {
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kAlreadyRegisteredV1;
  }

  if (registration_storage_->GetLastRegistrationReturnedLocalDataObsolete(
          primary_account_.gaia, security_domain_id)) {
    // Client already knows that existing vault keys (or their absence) isn't
    // sufficient for device registration. Fresh keys should be obtained
    // first.
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kLocalKeysAreStale;
  }

  if (!SupportsConstantKeyPreEnrollment(security_domain_id) &&
      !key_storage_->HasNonConstantKey(primary_account_.gaia,
                                       security_domain_id)) {
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kRegistrationWithConstantKeyNotSupported;
  }

  if (connection_->AreRequestsThrottled(primary_account_, security_domain_id)) {
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kThrottledClientSide;
  }

  const PhysicalDeviceRecoveryFactorData physical_device_data =
      storage_->GetPhysicalDeviceRecoveryFactorData(primary_account_.gaia);
  std::unique_ptr<SecureBoxKeyPair> key_pair;
  if (!physical_device_data.private_key_material().empty()) {
    key_pair = SecureBoxKeyPair::CreateByPrivateKeyImport(
        /*private_key_bytes=*/ProtoStringToBytes(
            physical_device_data.private_key_material()));
  }

  const bool had_generated_key_pair = key_pair != nullptr;

  if (!key_pair) {
    key_pair = SecureBoxKeyPair::GenerateRandom();
    // It's possible that device will be successfully registered, but the
    // client won't persist this state (for example response doesn't reach the
    // client or registration callback is cancelled). To avoid duplicated
    // registrations device key is stored before sending the registration
    // request, so the same key will be used for future registration attempts.
    storage_->MutatePhysicalDeviceRecoveryFactorData(
        primary_account_.gaia, [&](PhysicalDeviceRecoveryFactorData& data) {
          AssignBytesToProtoString(key_pair->private_key().ExportToBytes(),
                                   data.mutable_private_key_material());
        });
  }

  if (ongoing_registrations_.contains(security_domain_id)) {
    // Cancel ongoing request before starting a new one.
    FulfillOngoingRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationCancelled);
  }

  std::vector<std::vector<uint8_t>> vault_keys =
      key_storage_->GetVaultKeys(primary_account_.gaia, security_domain_id);
  int last_vault_key_version = key_storage_->GetLastKeyVersion(
      primary_account_.gaia, security_domain_id);

  std::unique_ptr<TrustedVaultConnection::Request> request;
  // `this` outlives `ongoing_registrations_`, so it's safe to use
  // base::Unretained() here.
  if (key_storage_->HasNonConstantKey(primary_account_.gaia,
                                      security_domain_id)) {
    request = connection_->RegisterAuthenticationFactor(
        primary_account_, security_domain_id,
        GetTrustedVaultKeysWithVersions(vault_keys, last_vault_key_version),
        key_pair->public_key(), LocalPhysicalDevice(),
        base::BindOnce(&PhysicalDeviceRecoveryFactor::OnRegistered,
                       base::Unretained(this), security_domain_id, true));
  } else {
    request = connection_->RegisterLocalDeviceWithoutKeys(
        primary_account_, security_domain_id, key_pair->public_key(),
        base::BindOnce(&PhysicalDeviceRecoveryFactor::OnRegistered,
                       base::Unretained(this), security_domain_id, false));
  }

  ongoing_registrations_.insert_or_assign(
      security_domain_id,
      OngoingRegistration(std::move(cb), std::move(request)));

  return had_generated_key_pair
             ? TrustedVaultRecoveryFactorRegistrationStateForUMA::
                   kAttemptingRegistrationWithExistingKeyPair
             : TrustedVaultRecoveryFactorRegistrationStateForUMA::
                   kAttemptingRegistrationWithNewKeyPair;
}

void PhysicalDeviceRecoveryFactor::OnKeysDownloaded(
    SecurityDomainId security_domain_id,
    TrustedVaultDownloadKeysStatus status,
    const std::vector<std::vector<uint8_t>>& new_vault_keys,
    int last_vault_key_version) {
  // This method should be called only as a result of
  // `ongoing_recoveries_[security_domain_id]` completion/failure, verify this
  // condition and destroy `ongoing_recoveries_[security_domain_id]` as it's not
  // needed anymore.
  auto ongoing_recovery = ongoing_recoveries_.find(security_domain_id);
  CHECK(ongoing_recovery != ongoing_recoveries_.end());
  AttemptRecoveryCallback cb = std::move(ongoing_recovery->second.callback);
  ongoing_recoveries_.erase(ongoing_recovery);

  RecordTrustedVaultDownloadKeysStatus(
      LocalRecoveryFactorType::kPhysicalDevice, security_domain_id,
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
      MarkAsNotRegistered(security_domain_id);
      break;
    }
    case TrustedVaultDownloadKeysStatus::kNoNewKeys: {
      // The registration itself exists, but there's no additional keys to
      // download. This is bad because key download attempts are triggered for
      // the case where local keys have been marked as stale, which means the
      // user is likely in an unrecoverable state.
      connection_->RecordFailedRequestForThrottling(primary_account_,
                                                    security_domain_id);
      recovery_status = RecoveryStatus::kNoNewKeys;
      break;
    }
    case TrustedVaultDownloadKeysStatus::kAccessTokenFetchingFailure:
    case TrustedVaultDownloadKeysStatus::kNetworkError:
      // Request wasn't sent to the server, so there is no need for throttling.
      break;
    case TrustedVaultDownloadKeysStatus::kOtherError:
      connection_->RecordFailedRequestForThrottling(primary_account_,
                                                    security_domain_id);
      break;
  }

  std::move(cb).Run(security_domain_id, recovery_status, new_vault_keys,
                    last_vault_key_version);
}

void PhysicalDeviceRecoveryFactor::FulfillRecoveryWithFailure(
    SecurityDomainId security_domain_id,
    TrustedVaultDownloadKeysStatusForUMA status_for_uma,
    AttemptRecoveryCallback cb) {
  RecordTrustedVaultDownloadKeysStatus(LocalRecoveryFactorType::kPhysicalDevice,
                                       security_domain_id, status_for_uma);

  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(cb), security_domain_id,
                     RecoveryStatus::kFailure,
                     std::vector<std::vector<uint8_t>>(), 0))
      .Run();
}

void PhysicalDeviceRecoveryFactor::OnRegistered(
    SecurityDomainId security_domain_id,
    bool had_local_keys,
    TrustedVaultRegistrationStatus status,
    int key_version) {
  // This method should be called only as a result of
  // `ongoing_registrations_[security_domain_id]` completion/failure, verify
  // this condition and destroy `ongoing_registrations_[security_domain_id]` as
  // it's not needed anymore.
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());
  RegisterCallback cb = std::move(ongoing_registration->second.callback);
  ongoing_registrations_.erase(ongoing_registration);

  switch (status) {
    case TrustedVaultRegistrationStatus::kRegistrationNotAttempted:
    case TrustedVaultRegistrationStatus::kRegistrationCancelled:
      NOTREACHED();
    case TrustedVaultRegistrationStatus::kSuccess:
    case TrustedVaultRegistrationStatus::kAlreadyRegistered:
      // kAlreadyRegistered handled as success, because it only means that
      // client doesn't fully handled successful device registration before.
      registration_storage_->SetRecoveryFactorRegistered(
          primary_account_.gaia, security_domain_id,
          LocalRecoveryFactorType::kPhysicalDevice, true);
      registration_storage_->SetLastRegistrationReturnedLocalDataObsolete(
          primary_account_.gaia, security_domain_id, false);
      break;
    case TrustedVaultRegistrationStatus::kLocalDataObsolete:
      registration_storage_->SetLastRegistrationReturnedLocalDataObsolete(
          primary_account_.gaia, security_domain_id, true);
      break;
    case TrustedVaultRegistrationStatus::kTransientAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kPersistentAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::
        kPrimaryAccountChangeAccessTokenFetchError:
    case TrustedVaultRegistrationStatus::kNetworkError:
    case TrustedVaultRegistrationStatus::kOtherError:
      break;
  }

  std::move(cb).Run(security_domain_id, status, key_version, had_local_keys);
}

void PhysicalDeviceRecoveryFactor::FulfillOngoingRegistrationWithFailure(
    SecurityDomainId security_domain_id,
    TrustedVaultRegistrationStatus status) {
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());
  RegisterCallback cb = std::move(ongoing_registration->second.callback);
  ongoing_registrations_.erase(ongoing_registration);
  FulfillRegistrationWithFailure(security_domain_id, status, std::move(cb));
}

void PhysicalDeviceRecoveryFactor::FulfillRegistrationWithFailure(
    SecurityDomainId security_domain_id,
    TrustedVaultRegistrationStatus status,
    RegisterCallback cb) {
  base::BindPostTaskToCurrentDefault(base::BindOnce(std::move(cb),
                                                    security_domain_id, status,
                                                    /*key_version=*/0,
                                                    /*had_local_keys=*/true))
      .Run();
}

}  // namespace trusted_vault
