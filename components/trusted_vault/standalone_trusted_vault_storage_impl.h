// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_IMPL_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_IMPL_H_

#include "components/trusted_vault/standalone_trusted_vault_storage.h"

namespace trusted_vault {

// Helper class for StandaloneTrustedVaultBackend which handles file
// operations.
// It's responsible for mapping per user / per security domain storage to files,
// and also takes care of required data migrations.
// This class is expected to be constructed and run the same way as
// StandaloneTrustedVaultBackend wrt. sequences.
class StandaloneTrustedVaultStorageImpl : public StandaloneTrustedVaultStorage {
 public:
  StandaloneTrustedVaultStorageImpl(const base::FilePath& file_path,
                                    SecurityDomainId security_domain_id);
  StandaloneTrustedVaultStorageImpl(
      const StandaloneTrustedVaultStorageImpl& other) = delete;
  StandaloneTrustedVaultStorageImpl& operator=(
      const StandaloneTrustedVaultStorageImpl& other) = delete;
  ~StandaloneTrustedVaultStorageImpl() override;

  void ReadDataFromDisk() override;
  void WriteDataToDisk() override;

  trusted_vault_pb::LocalTrustedVaultPerUser* AddUserVault(
      const GaiaId& gaia_id) override;
  trusted_vault_pb::LocalTrustedVaultPerUser* FindUserVault(
      const GaiaId& gaia_id) override;
  void RemoveUserVaults(
      base::FunctionRef<bool(const trusted_vault_pb::LocalTrustedVaultPerUser&)>
          predicate) override;

 private:
  const base::FilePath file_path_;

  const SecurityDomainId security_domain_id_;

  trusted_vault_pb::LocalTrustedVault data_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_IMPL_H_
