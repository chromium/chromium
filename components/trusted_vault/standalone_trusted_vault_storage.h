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

enum class SecurityDomainId;

// Storage helper for StandaloneTrustedVaultBackend handling file operations.
// TODO(crbug.com/405381481): This interface currently exposes pointers to
// internal data structures (|data_|). Consider rewriting it to avoid this, and
// potentially also get rid of ReadDataFromDisk() and WriteDataToDisk().
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

  // Writes data back to disk.
  void WriteDataToDisk();

  // Adds a new per-user vault in for |gaia_id|.
  // There must be no existing per-user vault for |gaia_id|. The lifetime of
  // the returned pointer is bound to the lifetime of |this|, but it becomes
  // invalid when ReadDataFromDisk() is called.
  // This never returns null.
  trusted_vault_pb::LocalTrustedVaultPerUser* AddUserVault(
      const GaiaId& gaia_id);

  // Finds the per-user vault for |gaia_id|. Returns null if not found. The
  // lifetime of the returned pointer is bound to the lifetime of |this|, but it
  // becomes invalid when ReadDataFromDisk() is called.
  trusted_vault_pb::LocalTrustedVaultPerUser* FindUserVault(
      const GaiaId& gaia_id);

  // Removes the per-user vaults that match |predicate|.
  void RemoveUserVaults(
      base::FunctionRef<bool(const trusted_vault_pb::LocalTrustedVaultPerUser&)>
          predicate);

  // Checks whether there is any non-constant key in |per_user_vault|.
  // This indicates that the corresponding security domain is not in the
  // pre-enrollment state, but contains usable key material.
  static bool HasNonConstantKey(
      const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault);

  // Helper method to get all keys in |per_user_vault|.
  static std::vector<std::vector<uint8_t>> GetAllVaultKeys(
      const trusted_vault_pb::LocalTrustedVaultPerUser& per_user_vault);

 private:
  explicit StandaloneTrustedVaultStorage(
      std::unique_ptr<FileAccess> file_access);

  std::unique_ptr<FileAccess> file_access_;
  trusted_vault_pb::LocalTrustedVault data_;
};

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
