// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_
#define COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_

#include <optional>

#include "components/signin/public/identity_manager/account_info.h"
#include "components/trusted_vault/local_recovery_factor.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "components/trusted_vault/trusted_vault_throttling_connection.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

// This class represents the iCloud Keychain as recovery factor.
// It stores required (private) keys in the iCloud Keychain.
class ICloudKeychainRecoveryFactor : public LocalRecoveryFactor {
 public:
  // `storage` must not be null and must outlive this object.
  // TODO(crbug.com/405381481): Refactor / remove the usage of
  // StandaloneTrustedVaultStorage in this class.
  ICloudKeychainRecoveryFactor(StandaloneTrustedVaultStorage* storage,
                               std::optional<CoreAccountInfo> primary_account);
  ICloudKeychainRecoveryFactor(const ICloudKeychainRecoveryFactor&) = delete;
  ICloudKeychainRecoveryFactor& operator=(ICloudKeychainRecoveryFactor&) =
      delete;
  ~ICloudKeychainRecoveryFactor() override;

  LocalRecoveryFactorType GetRecoveryFactorType() const override;

  void AttemptRecovery(TrustedVaultThrottlingConnection* connection,
                       AttemptRecoveryCallback cb) override;

  bool IsRegistered() override;
  void MarkAsNotRegistered() override;

  void ClearRegistrationAttemptInfo(const GaiaId& gaia_id) override;

  TrustedVaultRecoveryFactorRegistrationStateForUMA MaybeRegister(
      TrustedVaultThrottlingConnection* connection,
      RegisterCallback cb) override;

 private:
  const raw_ptr<StandaloneTrustedVaultStorage> storage_;
  const std::optional<CoreAccountInfo> primary_account_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_ICLOUD_KEYCHAIN_RECOVERY_FACTOR_H_
