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

ICloudKeychainRecoveryFactor::ICloudKeychainRecoveryFactor(
    const std::string& icloud_keychain_access_group_prefix,
    SecurityDomainId security_domain_id,
    StandaloneTrustedVaultStorage* storage,
    TrustedVaultThrottlingConnection* connection,
    CoreAccountInfo primary_account)
    : icloud_keychain_access_group_(
          base::StrCat({icloud_keychain_access_group_prefix,
                        kICloudKeychainRecoveryKeyAccessGroupSuffix})),
      security_domain_id_(security_domain_id),
      storage_(storage),
      connection_(connection),
      primary_account_(primary_account) {
  CHECK(storage_);
  CHECK(connection_);
}
ICloudKeychainRecoveryFactor::~ICloudKeychainRecoveryFactor() = default;

LocalRecoveryFactorType ICloudKeychainRecoveryFactor::GetRecoveryFactorType()
    const {
  return LocalRecoveryFactorType::kICloudKeychain;
}

void ICloudKeychainRecoveryFactor::AttemptRecovery(AttemptRecoveryCallback cb) {
  auto* per_user_vault = GetPrimaryAccountVault();

  if (StandaloneTrustedVaultStorage::HasNonConstantKey(*per_user_vault)) {
    // iCloud Keychain is only used to recover keys if there were no
    // non-constant keys available previously.
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kKeyProofVerificationNotSupported,
        std::move(cb));
    return;
  }

  // ICloudRecoveryKey::Retrieve() can't be cancelled, so we use a weak pointer
  // for the callback.
  ICloudRecoveryKey::Retrieve(
      base::BindOnce(
          &ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRecovery,
          weak_ptr_factory_.GetWeakPtr(), std::move(cb)),
      security_domain_id_, icloud_keychain_access_group_);
}

void ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRecovery(
    AttemptRecoveryCallback cb,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys) {
  if (local_icloud_keys.empty()) {
    MarkAsNotRegistered();
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered,
        std::move(cb));
    return;
  }

  if (connection_->AreRequestsThrottled(primary_account_)) {
    // Keys download attempt is not possible.
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide,
        std::move(cb));
    return;
  }

  ongoing_download_registration_state_request_for_recovery_ =
      connection_->DownloadAuthenticationFactorsRegistrationState(
          primary_account_,
          {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_ICLOUD_KEYCHAIN},
          base::BindOnce(&ICloudKeychainRecoveryFactor::
                             OnRecoveryFactorStateDownloadedForRecovery,
                         // `this` outlives `ongoing_request_for_recovery_`.
                         base::Unretained(this), std::move(cb),
                         std::move(local_icloud_keys)),
          base::NullCallback());
  CHECK(ongoing_download_registration_state_request_for_recovery_);
}

void ICloudKeychainRecoveryFactor::OnRecoveryFactorStateDownloadedForRecovery(
    AttemptRecoveryCallback cb,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
    DownloadAuthenticationFactorsRegistrationStateResult result) {
  // This method should be called only as a result of
  // `ongoing_request_for_recovery_` completion/failure, verify this condition
  // and destroy `ongoing_request_for_recovery_` as it's not  needed anymore.
  CHECK(ongoing_download_registration_state_request_for_recovery_);
  ongoing_download_registration_state_request_for_recovery_ = nullptr;

  if (result.state ==
      DownloadAuthenticationFactorsRegistrationStateResult::State::kError) {
    connection_->RecordFailedRequestForThrottling(primary_account_);
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kNetworkError, std::move(cb));
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
          LocalRecoveryFactorType::kICloudKeychain, security_domain_id_,
          TrustedVaultDownloadKeysStatusForUMA::kSuccess);

      int last_vault_key_version =
          std::ranges::max_element(recovery_icloud_key.member_keys, {},
                                   &MemberKeys::version)
              ->version;
      MarkAsRegistered();
      std::move(cb).Run(RecoveryStatus::kSuccess, *new_vault_keys,
                        last_vault_key_version);
      return;
    }
  }

  // None of the retrieved iCloud Keychain keys is in the security domain -
  // fail with the last status. This makes sure to record that status only once
  // per recovery attempt rather than once per vault member, which could skew
  // the metrics.
  MarkAsNotRegistered();
  FulfillRecoveryWithFailure(last_status, std::move(cb));
}

void ICloudKeychainRecoveryFactor::FulfillRecoveryWithFailure(
    TrustedVaultDownloadKeysStatusForUMA status_for_uma,
    AttemptRecoveryCallback cb) {
  RecordTrustedVaultDownloadKeysStatus(LocalRecoveryFactorType::kICloudKeychain,
                                       security_domain_id_, status_for_uma);

  base::BindPostTaskToCurrentDefault(
      base::BindOnce(std::move(cb), RecoveryStatus::kFailure,
                     /*new_vault_keys=*/std::vector<std::vector<uint8_t>>(),
                     /*last_vault_key_version=*/0))
      .Run();
}

bool ICloudKeychainRecoveryFactor::IsRegistered() {
  auto* per_user_vault = GetPrimaryAccountVault();
  return per_user_vault->icloud_keychain_registration_info().registered();
}

void ICloudKeychainRecoveryFactor::MarkAsNotRegistered() {
  auto* per_user_vault = GetPrimaryAccountVault();
  per_user_vault->mutable_icloud_keychain_registration_info()->set_registered(
      false);
  storage_->WriteDataToDisk();
}

void ICloudKeychainRecoveryFactor::MarkAsRegistered() {
  auto* per_user_vault = GetPrimaryAccountVault();
  per_user_vault->mutable_icloud_keychain_registration_info()->set_registered(
      true);
  storage_->WriteDataToDisk();
}

TrustedVaultRecoveryFactorRegistrationStateForUMA
ICloudKeychainRecoveryFactor::MaybeRegister(RegisterCallback cb) {
  auto* per_user_vault = GetPrimaryAccountVault();

  if (per_user_vault->icloud_keychain_registration_info().registered()) {
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kAlreadyRegisteredV1;
  }

  if (per_user_vault->last_registration_returned_local_data_obsolete()) {
    // Client already knows that existing vault keys (or their absence) isn't
    // sufficient for registration. Fresh keys should be obtained first.
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kLocalKeysAreStale;
  }

  if (connection_->AreRequestsThrottled(primary_account_)) {
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kThrottledClientSide;
  }

  if (!StandaloneTrustedVaultStorage::HasNonConstantKey(*per_user_vault)) {
    // Registration without non-constant keys isn't supported for iCloud
    // Keychain.
    return TrustedVaultRecoveryFactorRegistrationStateForUMA::
        kRegistrationWithConstantKeyNotSupported;
  }

  // ICloudRecoveryKey::Retrieve() can't be cancelled, so we use a weak pointer
  // for the callback.
  ICloudRecoveryKey::Retrieve(
      base::BindOnce(
          &ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRegistration,
          weak_ptr_factory_.GetWeakPtr(), std::move(cb)),
      security_domain_id_, icloud_keychain_access_group_);

  // We don't know yet whether there's an existing key pair in iCloud Keychain.
  // However, if there is one that's not yet registered with the security
  // domain, then we have to create a new key pair anyways. Thus, returning
  // `kAttemptingRegistrationWithNewKeyPair` is the most appropriate status
  // here.
  return TrustedVaultRecoveryFactorRegistrationStateForUMA::
      kAttemptingRegistrationWithNewKeyPair;
}

void ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRegistration(
    RegisterCallback cb,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys) {
  if (local_icloud_keys.empty()) {
    // No local iCloud Keychain key. We need to create a new one and register
    // it.
    ICloudRecoveryKey::Create(
        base::BindOnce(
            &ICloudKeychainRecoveryFactor::OnICloudKeyCreatedForRegistration,
            weak_ptr_factory_.GetWeakPtr(), std::move(cb)),
        security_domain_id_, icloud_keychain_access_group_);
    return;
  }

  ongoing_download_registration_state_request_for_registration_ =
      connection_->DownloadAuthenticationFactorsRegistrationState(
          primary_account_,
          {trusted_vault_pb::SecurityDomainMember::MEMBER_TYPE_ICLOUD_KEYCHAIN},
          base::BindOnce(&ICloudKeychainRecoveryFactor::
                             OnRecoveryFactorStateDownloadedForRegistration,
                         // `this` outlives `ongoing_request_for_registration_`.
                         base::Unretained(this), std::move(cb),
                         std::move(local_icloud_keys)),
          base::NullCallback());
  CHECK(ongoing_download_registration_state_request_for_registration_);
}

void ICloudKeychainRecoveryFactor::
    OnRecoveryFactorStateDownloadedForRegistration(
        RegisterCallback cb,
        std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
        DownloadAuthenticationFactorsRegistrationStateResult result) {
  // This method should be called only as a result of
  // `ongoing_request_for_registration_` completion/failure, verify this
  // condition and destroy `ongoing_request_for_registration_` as it's not
  // needed anymore.
  CHECK(ongoing_download_registration_state_request_for_registration_);
  ongoing_download_registration_state_request_for_registration_ = nullptr;

  if (result.state ==
      DownloadAuthenticationFactorsRegistrationStateResult::State::kError) {
    connection_->RecordFailedRequestForThrottling(primary_account_);
    FulfillRegistrationWithFailure(
        TrustedVaultRegistrationStatus::kNetworkError, std::move(cb));
    return;
  }

  for (const VaultMember& recovery_icloud_key : result.icloud_keys) {
    std::vector<uint8_t> public_key =
        recovery_icloud_key.public_key->ExportToBytes();
    const auto local_icloud_key_it = std::ranges::find_if(
        local_icloud_keys,
        [&public_key](const auto& key) { return key->id() == public_key; });
    if (local_icloud_key_it != local_icloud_keys.end()) {
      MarkAsRegistered();
      int last_vault_key_version =
          std::ranges::max_element(recovery_icloud_key.member_keys, {},
                                   &MemberKeys::version)
              ->version;
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(std::move(cb),
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
          weak_ptr_factory_.GetWeakPtr(), std::move(cb)),
      security_domain_id_, icloud_keychain_access_group_);
}

void ICloudKeychainRecoveryFactor::OnICloudKeyCreatedForRegistration(
    RegisterCallback cb,
    std::unique_ptr<ICloudRecoveryKey> local_icloud_key) {
  if (!local_icloud_key) {
    FulfillRegistrationWithFailure(TrustedVaultRegistrationStatus::kOtherError,
                                   std::move(cb));
    return;
  }

  auto* per_user_vault = GetPrimaryAccountVault();

  ongoing_registration_request_ = connection_->RegisterAuthenticationFactor(
      primary_account_,
      GetTrustedVaultKeysWithVersions(
          StandaloneTrustedVaultStorage::GetAllVaultKeys(*per_user_vault),
          per_user_vault->last_vault_key_version()),
      local_icloud_key->key()->public_key(), ICloudKeychain(),
      base::BindOnce(&ICloudKeychainRecoveryFactor::OnRegistered,
                     base::Unretained(this), std::move(cb)));
  CHECK(ongoing_registration_request_);
}

void ICloudKeychainRecoveryFactor::OnRegistered(
    RegisterCallback cb,
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
      per_user_vault->mutable_icloud_keychain_registration_info()
          ->set_registered(true);
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

  std::move(cb).Run(status,
                    /*key_version=*/key_version,
                    /*had_local_keys=*/true);
}

void ICloudKeychainRecoveryFactor::FulfillRegistrationWithFailure(
    TrustedVaultRegistrationStatus status,
    RegisterCallback cb) {
  std::move(cb).Run(status,
                    /*key_version=*/0,
                    /*had_local_keys=*/true);
}

trusted_vault_pb::LocalTrustedVaultPerUser*
ICloudKeychainRecoveryFactor::GetPrimaryAccountVault() {
  auto* per_user_vault = storage_->FindUserVault(primary_account_.gaia);
  // ICloudKeychainRecoveryFactor is only constructed by
  // StandaloneTrustedVaultBackend when a primary account is set, and it also
  // ensures that there is a user vault in storage at the same time.
  CHECK(per_user_vault);
  return per_user_vault;
}

}  // namespace trusted_vault
