// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/icloud_keychain_recovery_factor.h"

#include <algorithm>
#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "components/trusted_vault/icloud_recovery_key_mac.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_crypto.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

namespace {

constexpr char kICloudKeychainRecoveryKeyAccessGroupSuffix[] =
    ".com.google.common.folsom";

std::optional<std::vector<std::vector<uint8_t>>> DecryptTrustedVaultWrappedKeys(
    const SecureBoxPrivateKey& private_key,
    const std::vector<MemberKeys>& member_keys) {
  std::vector<std::vector<uint8_t>> decrypted_keys;

  for (const auto& member_key : member_keys) {
    std::optional<std::vector<uint8_t>> decrypted_key =
        DecryptTrustedVaultWrappedKey(private_key, member_key.wrapped_key);
    if (!decrypted_key) {
      return std::nullopt;
    }

    decrypted_keys.emplace_back(*decrypted_key);
  }

  return decrypted_keys;
}

}  // namespace

ICloudKeychainRecoveryFactor::OngoingRecovery::OngoingRecovery(
    ICloudKeychainRecoveryFactor* parent,
    AttemptRecoveryCallback cb)
    : callback(std::move(cb)),
      weak_ptr_factory(
          std::make_unique<base::WeakPtrFactory<ICloudKeychainRecoveryFactor>>(
              parent)) {
  CHECK(callback);
}

ICloudKeychainRecoveryFactor::OngoingRecovery::OngoingRecovery(
    OngoingRecovery&&) = default;

ICloudKeychainRecoveryFactor::OngoingRecovery&
ICloudKeychainRecoveryFactor::OngoingRecovery::operator=(OngoingRecovery&&) =
    default;

ICloudKeychainRecoveryFactor::OngoingRecovery::~OngoingRecovery() = default;

ICloudKeychainRecoveryFactor::OngoingRegistration::OngoingRegistration(
    ICloudKeychainRecoveryFactor* parent,
    RegisterCallback cb)
    : callback(std::move(cb)),
      weak_ptr_factory(
          std::make_unique<base::WeakPtrFactory<ICloudKeychainRecoveryFactor>>(
              parent)) {
  CHECK(callback);
}

ICloudKeychainRecoveryFactor::OngoingRegistration::OngoingRegistration(
    OngoingRegistration&&) = default;

ICloudKeychainRecoveryFactor::OngoingRegistration&
ICloudKeychainRecoveryFactor::OngoingRegistration::operator=(
    OngoingRegistration&&) = default;

ICloudKeychainRecoveryFactor::OngoingRegistration::~OngoingRegistration() =
    default;

ICloudKeychainRecoveryFactor::ICloudKeychainRecoveryFactor(
    const std::string& icloud_keychain_access_group_prefix,
    RecoveryFactorRegistrationStorage* registration_storage,
    KeyStorage* key_storage,
    TrustedVaultThrottlingConnection* connection,
    CoreAccountInfo primary_account)
    : icloud_keychain_access_group_(
          base::StrCat({icloud_keychain_access_group_prefix,
                        kICloudKeychainRecoveryKeyAccessGroupSuffix})),
      registration_storage_(registration_storage),
      key_storage_(key_storage),
      connection_(connection),
      primary_account_(primary_account) {
  CHECK(registration_storage_);
  CHECK(key_storage_);
  CHECK(connection_);
}
ICloudKeychainRecoveryFactor::~ICloudKeychainRecoveryFactor() = default;

LocalRecoveryFactorType ICloudKeychainRecoveryFactor::GetRecoveryFactorType()
    const {
  return LocalRecoveryFactorType::kICloudKeychain;
}

void ICloudKeychainRecoveryFactor::AttemptRecovery(
    SecurityDomainId security_domain_id,
    AttemptRecoveryCallback cb) {
  if (key_storage_->HasNonConstantKey(primary_account_.gaia,
                                      security_domain_id)) {
    // iCloud Keychain is only used to recover keys if there were no
    // non-constant keys available previously.
    FulfillRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kKeyProofVerificationNotSupported,
        std::move(cb));
    return;
  }

  if (ongoing_recoveries_.contains(security_domain_id)) {
    // Cancel ongoing request before starting a new one.
    FulfillOngoingRecoveryWithFailure(
        security_domain_id, TrustedVaultDownloadKeysStatusForUMA::kAborted);
  }

  auto [ongoing_recovery, _] = ongoing_recoveries_.insert_or_assign(
      security_domain_id, OngoingRecovery(this, std::move(cb)));

  // ICloudRecoveryKey::Retrieve() can't be cancelled, so we use a weak pointer
  // for the callback.
  ICloudRecoveryKey::Retrieve(
      base::BindOnce(
          &ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRecovery,
          ongoing_recovery->second.weak_ptr_factory->GetWeakPtr(),
          security_domain_id),
      security_domain_id, icloud_keychain_access_group_);
}

void ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRecovery(
    SecurityDomainId security_domain_id,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys) {
  auto ongoing_recovery = ongoing_recoveries_.find(security_domain_id);
  CHECK(ongoing_recovery != ongoing_recoveries_.end());

  if (local_icloud_keys.empty()) {
    MarkAsNotRegistered(security_domain_id);
    FulfillOngoingRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered);
    return;
  }

  if (connection_->AreRequestsThrottled(primary_account_, security_domain_id)) {
    // Keys download attempt is not possible.
    FulfillOngoingRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide);
    return;
  }

  ongoing_recovery->second.request =
      connection_->DownloadAuthenticationFactorsRegistrationState(
          primary_account_, security_domain_id,
          {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_ICLOUD_KEYCHAIN},
          base::BindOnce(&ICloudKeychainRecoveryFactor::
                             OnRecoveryFactorStateDownloadedForRecovery,
                         // `this` outlives `ongoing_recovery->second.request`.
                         base::Unretained(this), security_domain_id,
                         std::move(local_icloud_keys)),
          base::NullCallback());
  CHECK(ongoing_recovery->second.request);
}

void ICloudKeychainRecoveryFactor::OnRecoveryFactorStateDownloadedForRecovery(
    SecurityDomainId security_domain_id,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
    DownloadAuthenticationFactorsRegistrationStateResult result) {
  // This method should be called only as a result of
  // `ongoing_recovery->second.request` completion/failure, verify this
  // condition.
  auto ongoing_recovery = ongoing_recoveries_.find(security_domain_id);
  CHECK(ongoing_recovery != ongoing_recoveries_.end());
  CHECK(ongoing_recovery->second.request);
  ongoing_recovery->second.request.reset();

  if (result.state ==
      DownloadAuthenticationFactorsRegistrationStateResult::State::kError) {
    connection_->RecordFailedRequestForThrottling(primary_account_,
                                                  security_domain_id);
    FulfillOngoingRecoveryWithFailure(
        security_domain_id,
        TrustedVaultDownloadKeysStatusForUMA::kNetworkError);
    return;
  }

  TrustedVaultDownloadKeysStatusForUMA last_status =
      TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered;
  for (const VaultMember& recovery_icloud_key : result.icloud_keys) {
    if (recovery_icloud_key.member_keys.size() == 0) {
      last_status = TrustedVaultDownloadKeysStatusForUMA::kMembershipEmpty;
      continue;
    }

    const std::vector<uint8_t> public_key =
        recovery_icloud_key.public_key->ExportToBytes();

    const auto local_icloud_key_it = std::ranges::find_if(
        local_icloud_keys,
        [&public_key](const auto& key) { return key->id() == public_key; });
    if (local_icloud_key_it != local_icloud_keys.end()) {
      std::optional<std::vector<std::vector<uint8_t>>> new_vault_keys =
          DecryptTrustedVaultWrappedKeys(
              (*local_icloud_key_it)->key()->private_key(),
              recovery_icloud_key.member_keys);

      if (!new_vault_keys) {
        last_status =
            TrustedVaultDownloadKeysStatusForUMA::kMembershipCorrupted;
        continue;
      }

      // Success: all keys were successfully decrypted.
      RecordTrustedVaultDownloadKeysStatus(
          LocalRecoveryFactorType::kICloudKeychain, security_domain_id,
          TrustedVaultDownloadKeysStatusForUMA::kSuccess);

      int last_vault_key_version =
          std::ranges::max_element(recovery_icloud_key.member_keys, {},
                                   &MemberKeys::version)
              ->version;
      MarkAsRegistered(security_domain_id);
      AttemptRecoveryCallback cb = std::move(ongoing_recovery->second.callback);
      ongoing_recoveries_.erase(ongoing_recovery);
      std::move(cb).Run(security_domain_id, RecoveryStatus::kSuccess,
                        *new_vault_keys, last_vault_key_version);
      return;
    }
  }

  // None of the retrieved iCloud Keychain keys is in the security domain -
  // fail with the last status. This makes sure to record that status only once
  // per recovery attempt rather than once per vault member, which could skew
  // the metrics.
  MarkAsNotRegistered(security_domain_id);
  FulfillOngoingRecoveryWithFailure(security_domain_id, last_status);
}

void ICloudKeychainRecoveryFactor::FulfillOngoingRecoveryWithFailure(
    SecurityDomainId security_domain_id,
    TrustedVaultDownloadKeysStatusForUMA status_for_uma) {
  auto ongoing_recovery = ongoing_recoveries_.find(security_domain_id);
  CHECK(ongoing_recovery != ongoing_recoveries_.end());
  AttemptRecoveryCallback cb = std::move(ongoing_recovery->second.callback);
  ongoing_recoveries_.erase(ongoing_recovery);
  FulfillRecoveryWithFailure(security_domain_id, status_for_uma, std::move(cb));
}

void ICloudKeychainRecoveryFactor::FulfillRecoveryWithFailure(
    SecurityDomainId security_domain_id,
    TrustedVaultDownloadKeysStatusForUMA status_for_uma,
    AttemptRecoveryCallback cb) {
  RecordTrustedVaultDownloadKeysStatus(LocalRecoveryFactorType::kICloudKeychain,
                                       security_domain_id, status_for_uma);

  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(cb), security_domain_id,
                     RecoveryStatus::kFailure,
                     /*new_vault_keys=*/std::vector<std::vector<uint8_t>>(),
                     /*last_vault_key_version=*/0))
      .Run();
}

bool ICloudKeychainRecoveryFactor::IsRegistered(
    SecurityDomainId security_domain_id) {
  return registration_storage_->IsRecoveryFactorRegistered(
      primary_account_.gaia, security_domain_id,
      LocalRecoveryFactorType::kICloudKeychain);
}

void ICloudKeychainRecoveryFactor::MarkAsNotRegistered(
    SecurityDomainId security_domain_id) {
  registration_storage_->SetRecoveryFactorRegistered(
      primary_account_.gaia, security_domain_id,
      LocalRecoveryFactorType::kICloudKeychain, false);
}

void ICloudKeychainRecoveryFactor::MarkAsRegistered(
    SecurityDomainId security_domain_id) {
  registration_storage_->SetRecoveryFactorRegistered(
      primary_account_.gaia, security_domain_id,
      LocalRecoveryFactorType::kICloudKeychain, true);
}

TrustedVaultRecoveryFactorRegistrationStateForUMA
ICloudKeychainRecoveryFactor::MaybeRegister(SecurityDomainId security_domain_id,
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
    // sufficient for registration. Fresh keys should be obtained first.
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kLocalKeysAreStale;
  }

  if (connection_->AreRequestsThrottled(primary_account_, security_domain_id)) {
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kThrottledClientSide;
  }

  if (!key_storage_->HasNonConstantKey(primary_account_.gaia,
                                       security_domain_id)) {
    // Registration without non-constant keys isn't supported for iCloud
    // Keychain.
    FulfillRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationNotAttempted,
        std::move(cb));
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kRegistrationWithConstantKeyNotSupported;
  }

  if (ongoing_registrations_.contains(security_domain_id)) {
    // Cancel ongoing requests before starting a new one.
    FulfillOngoingRegistrationWithFailure(
        security_domain_id,
        TrustedVaultRegistrationStatus::kRegistrationCancelled);
  }

  auto [ongoing_registration, _] = ongoing_registrations_.insert_or_assign(
      security_domain_id, OngoingRegistration(this, std::move(cb)));

  // ICloudRecoveryKey::Retrieve() can't be cancelled, so we use a weak pointer
  // for the callback.
  ICloudRecoveryKey::Retrieve(
      base::BindOnce(
          &ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRegistration,
          ongoing_registration->second.weak_ptr_factory->GetWeakPtr(),
          security_domain_id),
      security_domain_id, icloud_keychain_access_group_);

  // We don't know yet whether there's an existing key pair in iCloud Keychain.
  // However, if there is one that's not yet registered with the security
  // domain, then we have to create a new key pair anyways. Thus, returning
  // `kAttemptingRegistrationWithNewKeyPair` is the most appropriate status
  // here.
  return TrustedVaultRecoveryFactorRegistrationStateForUMA::
      kAttemptingRegistrationWithNewKeyPair;
}

void ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRegistration(
    SecurityDomainId security_domain_id,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys) {
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());

  if (local_icloud_keys.empty()) {
    // No local iCloud Keychain key. We need to create a new one and register
    // it.
    ICloudRecoveryKey::Create(
        base::BindOnce(
            &ICloudKeychainRecoveryFactor::OnICloudKeyCreatedForRegistration,
            ongoing_registration->second.weak_ptr_factory->GetWeakPtr(),
            security_domain_id),
        security_domain_id, icloud_keychain_access_group_);
    return;
  }

  ongoing_registration->second.request =
      connection_->DownloadAuthenticationFactorsRegistrationState(
          primary_account_, security_domain_id,
          {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_ICLOUD_KEYCHAIN},
          base::BindOnce(&ICloudKeychainRecoveryFactor::
                             OnRecoveryFactorStateDownloadedForRegistration,
                         // `this` outlives
                         // `ongoing_registration->second.request`.
                         base::Unretained(this), security_domain_id,
                         std::move(local_icloud_keys)),
          base::NullCallback());
  CHECK(ongoing_registration->second.request);
}

void ICloudKeychainRecoveryFactor::
    OnRecoveryFactorStateDownloadedForRegistration(
        SecurityDomainId security_domain_id,
        std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
        DownloadAuthenticationFactorsRegistrationStateResult result) {
  // This method should be called only as a result of
  // `ongoing_registration->second.request` completion/failure, verify this
  // condition.
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());
  CHECK(ongoing_registration->second.request);
  ongoing_registration->second.request.reset();

  if (result.state ==
      DownloadAuthenticationFactorsRegistrationStateResult::State::kError) {
    connection_->RecordFailedRequestForThrottling(primary_account_,
                                                  security_domain_id);
    FulfillOngoingRegistrationWithFailure(
        security_domain_id, TrustedVaultRegistrationStatus::kNetworkError);
    return;
  }

  for (const VaultMember& recovery_icloud_key : result.icloud_keys) {
    std::vector<uint8_t> public_key =
        recovery_icloud_key.public_key->ExportToBytes();
    const auto local_icloud_key_it = std::ranges::find_if(
        local_icloud_keys,
        [&public_key](const auto& key) { return key->id() == public_key; });
    if (local_icloud_key_it != local_icloud_keys.end()) {
      MarkAsRegistered(security_domain_id);
      int last_vault_key_version =
          std::ranges::max_element(recovery_icloud_key.member_keys, {},
                                   &MemberKeys::version)
              ->version;
      RegisterCallback cb = std::move(ongoing_registration->second.callback);
      ongoing_registrations_.erase(ongoing_registration);
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(std::move(cb), security_domain_id,
                         TrustedVaultRegistrationStatus::kAlreadyRegistered,
                         last_vault_key_version, /*had_local_keys=*/true))
          .Run();
      return;
    }
  }

  // None of the retrieved iCloud Keychain keys is in the security domain. We
  // need to create a new one and register it.
  ICloudRecoveryKey::Create(
      base::BindOnce(
          &ICloudKeychainRecoveryFactor::OnICloudKeyCreatedForRegistration,
          ongoing_registration->second.weak_ptr_factory->GetWeakPtr(),
          security_domain_id),
      security_domain_id, icloud_keychain_access_group_);
}

void ICloudKeychainRecoveryFactor::OnICloudKeyCreatedForRegistration(
    SecurityDomainId security_domain_id,
    std::unique_ptr<ICloudRecoveryKey> local_icloud_key) {
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());

  if (!local_icloud_key) {
    FulfillOngoingRegistrationWithFailure(
        security_domain_id, TrustedVaultRegistrationStatus::kOtherError);
    return;
  }

  std::vector<std::vector<uint8_t>> vault_keys =
      key_storage_->GetVaultKeys(primary_account_.gaia, security_domain_id);
  int last_vault_key_version = key_storage_->GetLastKeyVersion(
      primary_account_.gaia, security_domain_id);

  ongoing_registration->second.request =
      connection_->RegisterAuthenticationFactor(
          primary_account_, security_domain_id,
          GetTrustedVaultKeysWithVersions(vault_keys, last_vault_key_version),
          local_icloud_key->key()->public_key(), ICloudKeychain(),
          base::BindOnce(&ICloudKeychainRecoveryFactor::OnRegistered,
                         base::Unretained(this), security_domain_id));
  CHECK(ongoing_registration->second.request);
}

void ICloudKeychainRecoveryFactor::OnRegistered(
    SecurityDomainId security_domain_id,
    TrustedVaultRegistrationStatus status,
    int key_version) {
  // This method should be called only as a result of
  // `ongoing_registration->second.request` completion/failure, verify this
  // condition and destroy the ongoing registration as it's not needed anymore.
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());
  CHECK(ongoing_registration->second.request);
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
      MarkAsRegistered(security_domain_id);
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

  std::move(cb).Run(security_domain_id, status,
                    /*key_version=*/key_version,
                    /*had_local_keys=*/true);
}

void ICloudKeychainRecoveryFactor::FulfillOngoingRegistrationWithFailure(
    SecurityDomainId security_domain_id,
    TrustedVaultRegistrationStatus status) {
  auto ongoing_registration = ongoing_registrations_.find(security_domain_id);
  CHECK(ongoing_registration != ongoing_registrations_.end());
  RegisterCallback cb = std::move(ongoing_registration->second.callback);
  ongoing_registrations_.erase(ongoing_registration);
  FulfillRegistrationWithFailure(security_domain_id, status, std::move(cb));
}

void ICloudKeychainRecoveryFactor::FulfillRegistrationWithFailure(
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
