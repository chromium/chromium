// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_LEGACY_STANDALONE_TRUSTED_VAULT_STORAGE_ADAPTER_H_
#define COMPONENTS_TRUSTED_VAULT_LEGACY_STANDALONE_TRUSTED_VAULT_STORAGE_ADAPTER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "components/trusted_vault/legacy_standalone_trusted_vault_storage.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"

namespace trusted_vault {

// Adapter that implements abstract StandaloneTrustedVaultStorage by delegating
// to concrete LegacyStandaloneTrustedVaultStorage.
class LegacyStandaloneTrustedVaultStorageAdapter
    : public StandaloneTrustedVaultStorage {
 public:
  explicit LegacyStandaloneTrustedVaultStorageAdapter(
      std::unique_ptr<LegacyStandaloneTrustedVaultStorage> storage);
  LegacyStandaloneTrustedVaultStorageAdapter(
      const LegacyStandaloneTrustedVaultStorageAdapter&) = delete;
  LegacyStandaloneTrustedVaultStorageAdapter& operator=(
      const LegacyStandaloneTrustedVaultStorageAdapter&) = delete;
  ~LegacyStandaloneTrustedVaultStorageAdapter() override;

  // StandaloneTrustedVaultStorage implementation:
  void ReadDataFromDisk() override;
  void ClearDataForUser(const GaiaId& gaia_id) override;
  void ClearDataForUnknownUsersOrMarkForDeletion(
      const base::flat_set<GaiaId>& known_gaia_ids,
      const std::optional<GaiaId>& primary_account_gaia_id) override;
  void ClearDataForUsersMarkedForDeletion(
      const std::optional<GaiaId>& primary_account_gaia_id) override;

  // RecoveryFactorRegistrationStorage implementation:
  bool IsRecoveryFactorRegistered(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      LocalRecoveryFactorType factor_type) const override;
  void SetRecoveryFactorRegistered(const GaiaId& gaia_id,
                                   SecurityDomainId domain,
                                   LocalRecoveryFactorType factor_type,
                                   bool registered) override;
  bool GetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id,
      SecurityDomainId domain) const override;
  void SetLastRegistrationReturnedLocalDataObsolete(const GaiaId& gaia_id,
                                                    SecurityDomainId domain,
                                                    bool obsolete) override;

  // PhysicalDeviceStorage implementation:
  const LocalDeviceRegistrationInfo& GetLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id) const override;
  void MutateLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id,
      base::FunctionRef<void(LocalDeviceRegistrationInfo&)> mutator) override;

  // KeyStorage implementation:
  std::vector<std::vector<uint8_t>> GetVaultKeys(
      const GaiaId& gaia_id,
      SecurityDomainId domain) const override;
  int GetLastKeyVersion(const GaiaId& gaia_id,
                        SecurityDomainId domain) const override;
  void SetVaultKeys(const GaiaId& gaia_id,
                    SecurityDomainId domain,
                    const std::vector<std::vector<uint8_t>>& keys,
                    int last_key_version) override;
  bool GetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id,
                                      SecurityDomainId domain) const override;
  void SetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id,
                                      SecurityDomainId domain,
                                      bool stale) override;
  bool HasNonConstantKey(const GaiaId& gaia_id,
                         SecurityDomainId domain) const override;

  // ConnectionThrottlingStorage implementation:
  int64_t GetLastFailedRequestMillis(const GaiaId& gaia_id,
                                     SecurityDomainId domain) const override;
  void SetLastFailedRequestMillis(const GaiaId& gaia_id,
                                  SecurityDomainId domain,
                                  int64_t last_failed_request_millis) override;

  // DegradedRecoverabilityStorage implementation:
  trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState
  GetDegradedRecoverabilityState(const GaiaId& gaia_id,
                                 SecurityDomainId domain) const override;
  void SetDegradedRecoverabilityState(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      const trusted_vault_pb::LocalTrustedVaultDegradedRecoverabilityState&
          state) override;

 private:
  const std::unique_ptr<LegacyStandaloneTrustedVaultStorage> storage_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_LEGACY_STANDALONE_TRUSTED_VAULT_STORAGE_ADAPTER_H_
