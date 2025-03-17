// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

enum class SecurityDomainId;

// Interface for storage helper for |StandaloneTrustedVaultBackend| which
// handles file operations.
class StandaloneTrustedVaultStorage {
 public:
  StandaloneTrustedVaultStorage() = default;
  StandaloneTrustedVaultStorage(const StandaloneTrustedVaultStorage& other) =
      delete;
  StandaloneTrustedVaultStorage& operator=(
      const StandaloneTrustedVaultStorage& other) = delete;
  virtual ~StandaloneTrustedVaultStorage() = default;

  // Restores state saved in storage, should be called before using the object.
  virtual void ReadDataFromDisk() = 0;

  // Writes data back to disk.
  virtual void WriteDataToDisk() = 0;

  // Adds a new per-user vault in for |gaia_id|.
  // Checks that there isn't an existing entry for |gaia_id|. The lifetime of
  // the returned pointer is bound to the lifetime of |this|, but it becomes
  // invalid when |ReadDataFromDisk| is called.
  [[nodiscard]] virtual trusted_vault_pb::LocalTrustedVaultPerUser*
  AddUserVault(const GaiaId& gaia_id) = 0;

  // Finds the per-user vault for |gaia_id|. Returns null if not found. The
  // lifetime of the returned pointer is bound to the lifetime of |this|, but it
  // becomes invalid when |ReadDataFromDisk| is called.
  [[nodiscard]] virtual trusted_vault_pb::LocalTrustedVaultPerUser*
  FindUserVault(const GaiaId& gaia_id) = 0;

  // Removes the per-user vaults that match |predicate|.
  virtual void RemoveUserVaults(
      base::FunctionRef<bool(const trusted_vault_pb::LocalTrustedVaultPerUser&)>
          predicate) = 0;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
