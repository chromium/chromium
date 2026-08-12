// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

using UserVault = trusted_vault_pb::LocalTrustedVaultPerUser;

enum class SecurityDomainId;

// Storage helper for StandaloneTrustedVaultBackend handling file operations.
class StandaloneTrustedVaultStorage {
 public:
  // Interface for actual file access. Can be swapped with a fake for tests.
  class FileAccess {
   public:
    FileAccess() = default;
    FileAccess(const FileAccess&) = delete;
    FileAccess& operator=(const FileAccess&) = delete;
    virtual ~FileAccess() = default;

    virtual trusted_vault_pb::LocalTrustedVault ReadFromDisk() = 0;
    virtual void WriteToDisk(
        const trusted_vault_pb::LocalTrustedVault& data) = 0;
  };

  // Create with non-default FileAccess. Only used for testing.
  static std::unique_ptr<StandaloneTrustedVaultStorage> CreateForTesting(
      std::unique_ptr<FileAccess> file_access);

  StandaloneTrustedVaultStorage(const base::FilePath& base_dir,
                                SecurityDomainId security_domain_id);
  StandaloneTrustedVaultStorage(const StandaloneTrustedVaultStorage& other) =
      delete;
  StandaloneTrustedVaultStorage& operator=(
      const StandaloneTrustedVaultStorage& other) = delete;
  ~StandaloneTrustedVaultStorage();

  // Restores state saved in storage, should be called before using the object.
  void ReadDataFromDisk();

  // Adds a new per-user vault in for |gaia_id|.
  // There must be no existing per-user vault for |gaia_id|. The lifetime of
  // the returned pointer is bound to the lifetime of |this|, but it becomes
  // invalid when ReadDataFromDisk() is called.
  // This never returns null.
  const UserVault* AddUserVault(const GaiaId& gaia_id);

  // Finds the per-user vault for |gaia_id|. Returns null if not found. The
  // lifetime of the returned pointer is bound to the lifetime of |this|, but it
  // becomes invalid when ReadDataFromDisk() is called.
  const UserVault* FindUserVault(const GaiaId& gaia_id) const;

  // Finds and returns a reference to the per-user vault for |gaia_id|.
  // Triggers a CHECK failure if the vault does not exist. The lifetime of
  // the returned reference is bound to the lifetime of |this|, but it becomes
  // invalid when ReadDataFromDisk() is called.
  const UserVault& GetUserVault(const GaiaId& gaia_id) const;

  // Executes |mutator| on the per-user vault for |gaia_id|, creating the
  // user vault if it does not exist, and commits changes to disk once upon
  // completion. Returns a const reference to the mutated per-user vault.
  const UserVault& MutateUserVault(const GaiaId& gaia_id,
                                   base::FunctionRef<void(UserVault&)> mutator);

  // Removes the per-user vaults that match |predicate|.
  void RemoveUserVaults(base::FunctionRef<bool(const UserVault&)> predicate);

  // Checks whether there is any non-constant key in |per_user_vault|.
  // This indicates that the corresponding security domain is not in the
  // pre-enrollment state, but contains usable key material.
  static bool HasNonConstantKey(const UserVault& per_user_vault);

  // Helper method to get all keys in |per_user_vault|.
  static std::vector<std::vector<uint8_t>> GetAllVaultKeys(
      const UserVault& per_user_vault);

 private:
  explicit StandaloneTrustedVaultStorage(
      std::unique_ptr<FileAccess> file_access);

  UserVault* AddUserVaultImpl(const GaiaId& gaia_id);
  UserVault* FindUserVaultImpl(const GaiaId& gaia_id);
  const UserVault* FindUserVaultImpl(const GaiaId& gaia_id) const;

  std::unique_ptr<FileAccess> file_access_;
  trusted_vault_pb::LocalTrustedVault data_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
