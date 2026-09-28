// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_PHYSICAL_DEVICE_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_PHYSICAL_DEVICE_RECOVERY_FACTOR_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

// This class represents the local physical device as recovery factor.
// It stores required (private) keys on disk through the per-user
// storage instances.
class PhysicalDeviceRecoveryFactor : public LocalRecoveryFactor {
 public:
  // `storage`, `registration_storage`, `key_storage`, and `connection` must not
  // be null and must outlive this object.
  PhysicalDeviceRecoveryFactor(
      PhysicalDeviceStorage* storage,
      RecoveryFactorRegistrationStorage* registration_storage,
      KeyStorage* key_storage,
      TrustedVaultThrottlingConnection* connection,
      CoreAccountInfo primary_account);
  PhysicalDeviceRecoveryFactor(const PhysicalDeviceRecoveryFactor&) = delete;
  PhysicalDeviceRecoveryFactor& operator=(PhysicalDeviceRecoveryFactor&) =
      delete;
  ~PhysicalDeviceRecoveryFactor() override;

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
  // network request via RAII, without invoking `callback`.
  struct OngoingRecovery {
    OngoingRecovery(AttemptRecoveryCallback cb,
                    std::unique_ptr<TrustedVaultConnection::Request> request);
    OngoingRecovery(OngoingRecovery&&);
    OngoingRecovery& operator=(OngoingRecovery&&);
    ~OngoingRecovery();

    AttemptRecoveryCallback callback;
    std::unique_ptr<TrustedVaultConnection::Request> request;
  };

  // Holds all in-flight state for an ongoing MaybeRegister() request for a
  // given security domain. Destroying an instance (e.g. by erasing it from
  // `ongoing_registrations_` or during class destruction) cancels any active
  // network request via RAII, without invoking `callback`.
  struct OngoingRegistration {
    OngoingRegistration(
        RegisterCallback cb,
        std::unique_ptr<TrustedVaultConnection::Request> request);
    OngoingRegistration(OngoingRegistration&&);
    OngoingRegistration& operator=(OngoingRegistration&&);
    ~OngoingRegistration();

    RegisterCallback callback;
    std::unique_ptr<TrustedVaultConnection::Request> request;
  };

  void OnKeysDownloaded(SecurityDomainId security_domain_id,
                        TrustedVaultDownloadKeysStatus status,
                        const std::vector<std::vector<uint8_t>>& new_vault_keys,
                        int last_vault_key_version);
  void FulfillRecoveryWithFailure(
      SecurityDomainId security_domain_id,
      TrustedVaultDownloadKeysStatusForUMA status_for_uma,
      AttemptRecoveryCallback cb);

  void OnRegistered(SecurityDomainId security_domain_id,
                    bool had_local_keys,
                    TrustedVaultRegistrationStatus status,
                    int key_version);
  void FulfillOngoingRegistrationWithFailure(
      SecurityDomainId security_domain_id,
      TrustedVaultRegistrationStatus status);
  void FulfillRegistrationWithFailure(SecurityDomainId security_domain_id,
                                      TrustedVaultRegistrationStatus status,
                                      RegisterCallback cb);

  const raw_ptr<PhysicalDeviceStorage> storage_;
  const raw_ptr<RecoveryFactorRegistrationStorage> registration_storage_;
  const raw_ptr<KeyStorage> key_storage_;
  const raw_ptr<TrustedVaultThrottlingConnection> connection_;
  const CoreAccountInfo primary_account_;

  base::flat_map<SecurityDomainId, OngoingRecovery> ongoing_recoveries_;
  base::flat_map<SecurityDomainId, OngoingRegistration> ongoing_registrations_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_PHYSICAL_DEVICE_RECOVERY_FACTOR_H_
