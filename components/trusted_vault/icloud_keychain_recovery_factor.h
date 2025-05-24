// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {
class ICloudRecoveryKey;

// This class represents the iCloud Keychain as recovery factor.
// It stores required (private) keys in the iCloud Keychain.
class ICloudKeychainRecoveryFactor : public LocalRecoveryFactor {
 public:
  // `storage` and `connection` must not be null and must outlive this object.
  // `storage` must contain a vault for `primary_account` when calling any
  // method of this class.
  // TODO(crbug.com/405381481): Refactor / remove the usage of
  // StandaloneTrustedVaultStorage in this class.
  ICloudKeychainRecoveryFactor(
      const std::string& icloud_keychain_access_group_prefix,
      const SecurityDomainId security_domain_id,
      StandaloneTrustedVaultStorage* storage,
      TrustedVaultThrottlingConnection* connection,
      CoreAccountInfo primary_account);
  ICloudKeychainRecoveryFactor(const ICloudKeychainRecoveryFactor&) = delete;
  ICloudKeychainRecoveryFactor& operator=(ICloudKeychainRecoveryFactor&) =
      delete;
  ~ICloudKeychainRecoveryFactor() override;

  LocalRecoveryFactorType GetRecoveryFactorType() const override;

  void AttemptRecovery(AttemptRecoveryCallback cb) override;

  bool IsRegistered() override;
  void MarkAsNotRegistered() override;

  TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      RegisterCallback cb) override;

 private:
  trusted_vault_pb::LocalTrustedVaultPerUser* GetPrimaryAccountVault();

  void OnICloudKeysRetrievedForRecovery(
      AttemptRecoveryCallback cb,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys);
  void OnRecoveryFactorStateDownloadedForRecovery(
      AttemptRecoveryCallback cb,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
      DownloadAuthenticationFactorsRegistrationStateResult result);
  void FulfillRecoveryWithFailure(
      TrustedVaultDownloadKeysStatusForUMA status_for_uma,
      AttemptRecoveryCallback cb);

  void MarkAsRegistered();

  void OnICloudKeysRetrievedForRegistration(
      RegisterCallback cb,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys);
  void OnRecoveryFactorStateDownloadedForRegistration(
      RegisterCallback cb,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
      DownloadAuthenticationFactorsRegistrationStateResult result);
  void OnICloudKeyCreatedForRegistration(
      RegisterCallback cb,
      std::unique_ptr<ICloudRecoveryKey> local_icloud_key);
  void OnRegistered(RegisterCallback cb,
                    TrustedVaultRegistrationStatus status,
                    int key_version);
  void FulfillRegistrationWithFailure(TrustedVaultRegistrationStatus status,
                                      RegisterCallback cb);

  const std::string icloud_keychain_access_group_;
  const SecurityDomainId security_domain_id_;
  const raw_ptr<StandaloneTrustedVaultStorage> storage_;
  const raw_ptr<TrustedVaultThrottlingConnection> connection_;
  const CoreAccountInfo primary_account_;

  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_download_registration_state_request_for_recovery_;
  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_download_registration_state_request_for_registration_;
  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_registration_request_;

  base::WeakPtrFactory<ICloudKeychainRecoveryFactor> weak_ptr_factory_{this};
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_
