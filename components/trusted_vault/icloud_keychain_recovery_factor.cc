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
    std::optional<CoreAccountInfo> primary_account)
    : icloud_keychain_access_group_(
          base::StrCat({icloud_keychain_access_group_prefix,
                        kICloudKeychainRecoveryKeyAccessGroupSuffix})),
      security_domain_id_(security_domain_id),
      storage_(storage),
      primary_account_(primary_account) {
  CHECK(storage_);
}
ICloudKeychainRecoveryFactor::~ICloudKeychainRecoveryFactor() = default;

LocalRecoveryFactorType ICloudKeychainRecoveryFactor::GetRecoveryFactorType()
    const {
  return LocalRecoveryFactorType::kICloudKeychain;
}

void ICloudKeychainRecoveryFactor::AttemptRecovery(
    TrustedVaultThrottlingConnection* connection,
    AttemptRecoveryCallback cb) {
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
          weak_ptr_factory_.GetWeakPtr(), connection, std::move(cb)),
      security_domain_id_, icloud_keychain_access_group_);
}

void ICloudKeychainRecoveryFactor::OnICloudKeysRetrievedForRecovery(
    TrustedVaultThrottlingConnection* connection,
    AttemptRecoveryCallback cb,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys) {
  CHECK(primary_account_);
  if (local_icloud_keys.empty()) {
    MarkAsNotRegistered();
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kDeviceNotRegistered,
        std::move(cb));
    return;
  }

  if (connection->AreRequestsThrottled(*primary_account_)) {
    // Keys download attempt is not possible.
    FulfillRecoveryWithFailure(
        TrustedVaultDownloadKeysStatusForUMA::kThrottledClientSide,
        std::move(cb));
    return;
  }

  ongoing_request_for_recovery_ =
      connection->DownloadAuthenticationFactorsRegistrationState(
          *primary_account_,
          base::BindOnce(&ICloudKeychainRecoveryFactor::
                             OnRecoveryFactorStateDownloadedForRecovery,
                         // `this` outlives `ongoing_request_for_recovery_`.
                         base::Unretained(this), connection, std::move(cb),
                         std::move(local_icloud_keys)),
          base::NullCallback());
  CHECK(ongoing_request_for_recovery_);
}

void ICloudKeychainRecoveryFactor::OnRecoveryFactorStateDownloadedForRecovery(
    TrustedVaultThrottlingConnection* connection,
    AttemptRecoveryCallback cb,
    std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
    DownloadAuthenticationFactorsRegistrationStateResult result) {
  // This method should be called only as a result of
  // `ongoing_request_for_recovery_` completion/failure, verify this condition
  // and destroy `ongoing_request_for_recovery_` as it's not  needed anymore.
  CHECK(ongoing_request_for_recovery_);
  ongoing_request_for_recovery_ = nullptr;

  if (result.state ==
      DownloadAuthenticationFactorsRegistrationStateResult::State::kError) {
    connection->RecordFailedRequestForThrottling(*primary_account_);
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
  NOTIMPLEMENTED();
  return false;
}

void ICloudKeychainRecoveryFactor::MarkAsNotRegistered() {
  NOTIMPLEMENTED();
}

void ICloudKeychainRecoveryFactor::ClearRegistrationAttemptInfo(
    const GaiaId& gaia_id) {
  NOTIMPLEMENTED();
}

TrustedVaultRecoveryFactorRegistrationStateForUMA
ICloudKeychainRecoveryFactor::MaybeRegister(
    TrustedVaultThrottlingConnection* connection,
    RegisterCallback cb) {
  NOTIMPLEMENTED();
  return TrustedVaultRecoveryFactorRegistrationStateForUMA::kLocalKeysAreStale;
}

trusted_vault_pb::LocalTrustedVaultPerUser*
ICloudKeychainRecoveryFactor::GetPrimaryAccountVault() {
  CHECK(primary_account_);
  auto* per_user_vault = storage_->FindUserVault(primary_account_->gaia);
  // ICloudKeychainRecoveryFactor is only constructed by
  // StandaloneTrustedVaultBackend when a primary account is set, and it also
  // ensures that there is a user vault in storage at the same time.
  CHECK(per_user_vault);
  return per_user_vault;
}

}  // namespace trusted_vault
