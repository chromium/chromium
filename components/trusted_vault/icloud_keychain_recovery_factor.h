// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {
class ICloudRecoveryKey;

// This class represents the iCloud Keychain as recovery factor.
// It stores required (private) keys in the iCloud Keychain.
class ICloudKeychainRecoveryFactor : public LocalRecoveryFactor {
 public:
  // `registration_storage`, `key_storage`, and `connection` must not be null
  // and must outlive this object.
  ICloudKeychainRecoveryFactor(
      const std::string& icloud_keychain_access_group_prefix,
      RecoveryFactorRegistrationStorage* registration_storage,
      KeyStorage* key_storage,
      TrustedVaultThrottlingConnection* connection,
      CoreAccountInfo primary_account);
  ICloudKeychainRecoveryFactor(const ICloudKeychainRecoveryFactor&) = delete;
  ICloudKeychainRecoveryFactor& operator=(const ICloudKeychainRecoveryFactor&) =
      delete;
  ~ICloudKeychainRecoveryFactor() override;

  LocalRecoveryFactorType GetRecoveryFactorType() const override;

  void AttemptRecovery(SecurityDomainId security_domain_id,
                       AttemptRecoveryCallback cb) override;

  bool IsRegistered(SecurityDomainId security_domain_id) override;
  void MarkAsNotRegistered(SecurityDomainId security_domain_id) override;

  TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      SecurityDomainId security_domain_id,
      RegisterCallback cb) override;

 private:
  // Holds all in-flight state for an ongoing AttemptRecovery() request for a
  // given security domain. Destroying an instance (e.g. by erasing it from
  // `ongoing_recoveries_` or during class destruction) cancels any active
  // network request via RAII and invalidates pending Keychain worker thread
  // callbacks via `weak_ptr_factory`, without invoking `callback`.
  struct OngoingRecovery {
    OngoingRecovery(ICloudKeychainRecoveryFactor* parent,
                    AttemptRecoveryCallback cb);
    OngoingRecovery(OngoingRecovery&&);
    OngoingRecovery& operator=(OngoingRecovery&&);
    ~OngoingRecovery();

    AttemptRecoveryCallback callback;
    std::unique_ptr<TrustedVaultConnection::Request> request;
    std::unique_ptr<base::WeakPtrFactory<ICloudKeychainRecoveryFactor>>
        weak_ptr_factory;
  };

  // Holds all in-flight state for an ongoing MaybeRegister() request for a
  // given security domain. Destroying an instance (e.g. by erasing it from
  // `ongoing_registrations_` or during class destruction) cancels any active
  // network request via RAII and invalidates pending Keychain worker thread
  // callbacks via `weak_ptr_factory`, without invoking `callback`.
  struct OngoingRegistration {
    OngoingRegistration(ICloudKeychainRecoveryFactor* parent,
                        RegisterCallback cb);
    OngoingRegistration(OngoingRegistration&&);
    OngoingRegistration& operator=(OngoingRegistration&&);
    ~OngoingRegistration();

    RegisterCallback callback;
    std::unique_ptr<TrustedVaultConnection::Request> request;
    std::unique_ptr<base::WeakPtrFactory<ICloudKeychainRecoveryFactor>>
        weak_ptr_factory;
  };

  void OnICloudKeysRetrievedForRecovery(
      SecurityDomainId security_domain_id,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys);
  void OnRecoveryFactorStateDownloadedForRecovery(
      SecurityDomainId security_domain_id,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
      DownloadAuthenticationFactorsRegistrationStateResult result);
  void FulfillOngoingRecoveryWithFailure(
      SecurityDomainId security_domain_id,
      TrustedVaultDownloadKeysStatusForUMA status_for_uma);
  void FulfillRecoveryWithFailure(
      SecurityDomainId security_domain_id,
      TrustedVaultDownloadKeysStatusForUMA status_for_uma,
      AttemptRecoveryCallback cb);

  void MarkAsRegistered(SecurityDomainId security_domain_id);

  void OnICloudKeysRetrievedForRegistration(
      SecurityDomainId security_domain_id,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys);
  void OnRecoveryFactorStateDownloadedForRegistration(
      SecurityDomainId security_domain_id,
      std::vector<std::unique_ptr<ICloudRecoveryKey>> local_icloud_keys,
      DownloadAuthenticationFactorsRegistrationStateResult result);
  void OnICloudKeyCreatedForRegistration(
      SecurityDomainId security_domain_id,
      std::unique_ptr<ICloudRecoveryKey> local_icloud_key);
  void OnRegistered(SecurityDomainId security_domain_id,
                    TrustedVaultRegistrationStatus status,
                    int key_version);
  void FulfillOngoingRegistrationWithFailure(
      SecurityDomainId security_domain_id,
      TrustedVaultRegistrationStatus status);
  void FulfillRegistrationWithFailure(SecurityDomainId security_domain_id,
                                      TrustedVaultRegistrationStatus status,
                                      RegisterCallback cb);

  const std::string icloud_keychain_access_group_;
  const raw_ptr<RecoveryFactorRegistrationStorage> registration_storage_;
  const raw_ptr<KeyStorage> key_storage_;
  const raw_ptr<TrustedVaultThrottlingConnection> connection_;
  const CoreAccountInfo primary_account_;

  base::flat_map<SecurityDomainId, OngoingRecovery> ongoing_recoveries_;
  base::flat_map<SecurityDomainId, OngoingRegistration> ongoing_registrations_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_
