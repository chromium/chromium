// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_PHYSICAL_DEVICE_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_PHYSICAL_DEVICE_RECOVERY_FACTOR_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

// This class represents the local physical device as recovery factor.
// It stores required (private) keys on disk through the per-user
// StandaloneTrustedVaultStorage instance.
// TODO(crbug.com/405381481): Add unittests for this class (by moving tests from
// StandaloneTrustedVaultBackendTest).
class PhysicalDeviceRecoveryFactor : public LocalRecoveryFactor {
 public:
  // `storage` and `connection` must not be null and must outlive this object.
  // `storage` must contain a vault for `primary_account` when calling any
  // method of this class.
  // TODO(crbug.com/405381481): Refactor / remove the usage of
  // StandaloneTrustedVaultStorage in this class.
  PhysicalDeviceRecoveryFactor(SecurityDomainId security_domain_id,
                               StandaloneTrustedVaultStorage* storage,
                               TrustedVaultThrottlingConnection* connection,
                               CoreAccountInfo primary_account);
  PhysicalDeviceRecoveryFactor(const PhysicalDeviceRecoveryFactor&) = delete;
  PhysicalDeviceRecoveryFactor& operator=(PhysicalDeviceRecoveryFactor&) =
      delete;
  ~PhysicalDeviceRecoveryFactor() override;

  LocalRecoveryFactorType GetRecoveryFactorType() const override;

  void AttemptRecovery(AttemptRecoveryCallback cb) override;

  bool IsRegistered() override;
  void MarkAsNotRegistered() override;

  TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      RegisterCallback cb) override;

 private:
  trusted_vault_pb::LocalTrustedVaultPerUser* GetPrimaryAccountVault();

  void OnKeysDownloaded(AttemptRecoveryCallback cb,
                        TrustedVaultDownloadKeysStatus status,
                        const std::vector<std::vector<uint8_t>>& new_vault_keys,
                        int last_vault_key_version);
  void FulfillRecoveryWithFailure(
      TrustedVaultDownloadKeysStatusForUMA status_for_uma,
      AttemptRecoveryCallback cb);

  void OnRegistered(RegisterCallback cb,
                    bool had_local_keys,
                    TrustedVaultRegistrationStatus status,
                    int key_version);

  const SecurityDomainId security_domain_id_;
  const raw_ptr<StandaloneTrustedVaultStorage> storage_;
  const raw_ptr<TrustedVaultThrottlingConnection> connection_;
  const CoreAccountInfo primary_account_;

  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request> ongoing_request_;
  // Destroying this will cancel the ongoing request.
  std::unique_ptr<TrustedVaultConnection::Request>
      ongoing_registration_request_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_PHYSICAL_DEVICE_RECOVERY_FACTOR_H_
