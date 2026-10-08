// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_LOCAL_DOMAINS_STORAGE_H_
#define COMPONENTS_TRUSTED_VAULT_LOCAL_DOMAINS_STORAGE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "components/trusted_vault/proto/local_domains_data.pb.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "components/trusted_vault/standalone_trusted_vault_storage.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

// Multi-domain storage engine implementing StandaloneTrustedVaultStorage backed
// by `local_domains_data.proto`.
class LocalDomainsStorage : public StandaloneTrustedVaultStorage {
 public:
  // Interface for actual file access. Can be swapped with a fake for tests.
  class StorageFileAccess {
   public:
    StorageFileAccess() = default;
    StorageFileAccess(const StorageFileAccess&) = delete;
    StorageFileAccess& operator=(const StorageFileAccess&) = delete;
    virtual ~StorageFileAccess() = default;

    // Reads and returns the local domains data from disk. If no file exists on
    // disk, migrates data from the legacy storage file if present; otherwise
    // returns an empty `LocalDomainsData`.
    virtual trusted_vault_pb::LocalDomainsData ReadFromDisk() = 0;

    // Writes `data` to disk.
    virtual void WriteToDisk(
        const trusted_vault_pb::LocalDomainsData& data) = 0;
  };

  // Creates an instance of `LocalDomainsStorage` that reads and writes data
  // inside `base_dir`. `ReadDataFromDisk()` must be called before using the
  // returned object.
  static std::unique_ptr<LocalDomainsStorage> Create(
      const base::FilePath& base_dir);

  // Creates an instance of `LocalDomainsStorage` with a custom
  // `StorageFileAccess` implementation. Only used for testing.
  static std::unique_ptr<LocalDomainsStorage> CreateForTesting(
      std::unique_ptr<StorageFileAccess> file_access);

  LocalDomainsStorage(const LocalDomainsStorage&) = delete;
  LocalDomainsStorage& operator=(const LocalDomainsStorage&) = delete;
  ~LocalDomainsStorage() override;

  // StandaloneTrustedVaultStorage implementation:
  void ReadDataFromDisk() override;
  void ClearDataForUser(const GaiaId& gaia_id) override;
  void ClearDataForUnknownUsersOrMarkForDeletion(
      const base::flat_set<GaiaId>& known_gaia_ids,
      const std::optional<GaiaId>& primary_account_gaia_id) override;
  void ClearDataForUsersMarkedForDeletion(
      const std::optional<GaiaId>& primary_account_gaia_id) override;

  // PhysicalDeviceStorage implementation:
  PhysicalDeviceRecoveryFactorData GetPhysicalDeviceRecoveryFactorData(
      const GaiaId& gaia_id) const override;
  void MutatePhysicalDeviceRecoveryFactorData(
      const GaiaId& gaia_id,
      base::FunctionRef<void(PhysicalDeviceRecoveryFactorData&)> mutator)
      override;

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
                                      bool keys_marked_as_stale) override;
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
  explicit LocalDomainsStorage(std::unique_ptr<StorageFileAccess> file_access);

  trusted_vault_pb::UserDomainData* FindUser(const GaiaId& gaia_id);
  const trusted_vault_pb::UserDomainData* FindUser(const GaiaId& gaia_id) const;
  trusted_vault_pb::UserDomainData* GetOrCreateUser(const GaiaId& gaia_id);

  trusted_vault_pb::DomainData* FindDomainData(
      trusted_vault_pb::UserDomainData* user,
      SecurityDomainId domain);
  const trusted_vault_pb::DomainData* FindDomainData(
      const trusted_vault_pb::UserDomainData* user,
      SecurityDomainId domain) const;
  trusted_vault_pb::DomainData* GetOrCreateDomainData(
      trusted_vault_pb::UserDomainData* user,
      SecurityDomainId domain);

  void RemoveUsers(
      base::FunctionRef<bool(const trusted_vault_pb::UserDomainData&)>
          predicate);
  void MutateUser(
      const GaiaId& gaia_id,
      base::FunctionRef<void(trusted_vault_pb::UserDomainData&)> mutator);
  void MutateDomain(
      const GaiaId& gaia_id,
      SecurityDomainId domain,
      base::FunctionRef<void(trusted_vault_pb::DomainData&)> mutator);

  void WriteToDisk();

  std::unique_ptr<StorageFileAccess> file_access_;
  trusted_vault_pb::LocalDomainsData data_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_LOCAL_DOMAINS_STORAGE_H_
