// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_STORAGE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "components/trusted_vault/proto/local_trusted_vault.pb.h"
#include "google_apis/gaia/gaia_id.h"

namespace trusted_vault {

enum class SecurityDomainId;

using UserVault = trusted_vault_pb::LocalTrustedVaultPerUser;
using LocalDeviceRegistrationInfo =
    trusted_vault_pb::LocalDeviceRegistrationInfo;
using ICloudKeychainRegistrationInfo =
    trusted_vault_pb::ICloudKeychainRegistrationInfo;

// Storage interface for PhysicalDeviceRecoveryFactor.
class PhysicalDeviceStorage {
 public:
  virtual ~PhysicalDeviceStorage() = default;

  // Finds and returns a reference to the local device registration info for
  // `gaia_id`. Triggers a CHECK failure if the vault does not exist. The
  // lifetime of the returned reference is bound to the lifetime of `this`, but
  // it becomes invalid when ReadDataFromDisk() is called.
  virtual const LocalDeviceRegistrationInfo& GetLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id) const = 0;

  // Executes `mutator` on the local device registration info for `gaia_id`,
  // creating the user vault if it does not exist, and commits changes to disk
  // once upon completion.
  virtual void MutateLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id,
      base::FunctionRef<void(LocalDeviceRegistrationInfo&)> mutator) = 0;

  // Returns whether last registration returned local data obsolete for
  // `gaia_id`. Returns false if user vault does not exist.
  virtual bool GetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id) const = 0;

  // Sets whether last registration returned local data obsolete for `gaia_id`.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id,
      bool obsolete) = 0;
};

// Storage interface for ICloudKeychainRecoveryFactor.
class ICloudKeychainStorage {
 public:
  virtual ~ICloudKeychainStorage() = default;

  // Finds and returns a reference to the iCloud Keychain registration info for
  // `gaia_id`. Triggers a CHECK failure if the vault does not exist. The
  // lifetime of the returned reference is bound to the lifetime of `this`, but
  // it becomes invalid when ReadDataFromDisk() is called.
  virtual const ICloudKeychainRegistrationInfo&
  GetICloudKeychainRegistrationInfo(const GaiaId& gaia_id) const = 0;

  // Executes `mutator` on the iCloud Keychain registration info for `gaia_id`,
  // creating the user vault if it does not exist, and commits changes to disk
  // once upon completion.
  virtual void MutateICloudKeychainRegistrationInfo(
      const GaiaId& gaia_id,
      base::FunctionRef<void(ICloudKeychainRegistrationInfo&)> mutator) = 0;

  // Returns whether last registration returned local data obsolete for
  // `gaia_id`. Returns false if user vault does not exist.
  virtual bool GetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id) const = 0;

  // Sets whether last registration returned local data obsolete for `gaia_id`.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id,
      bool obsolete) = 0;
};

// Storage interface for vault key access and updates.
class KeyStorage {
 public:
  virtual ~KeyStorage() = default;

  // Finds and returns all vault keys for `gaia_id`. Returns an empty vector if
  // the user vault does not exist.
  virtual std::vector<std::vector<uint8_t>> GetVaultKeys(
      const GaiaId& gaia_id) const = 0;

  // Returns the version corresponding to the last vault key for `gaia_id`.
  // Returns 0 if the user vault does not exist.
  virtual int GetLastKeyVersion(const GaiaId& gaia_id) const = 0;

  // Replaces all vault keys and updates last vault key version for `gaia_id`.
  // Resets keys_marked_as_stale_by_consumer to false, creates the user vault
  // if it does not exist, and commits changes to disk.
  virtual void SetVaultKeys(const GaiaId& gaia_id,
                            const std::vector<std::vector<uint8_t>>& keys,
                            int last_key_version) = 0;

  // Returns whether keys for `gaia_id` are marked as stale by consumer.
  // Returns false if user vault does not exist.
  virtual bool GetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id) const = 0;

  // Sets whether keys for `gaia_id` are marked as stale by consumer.
  // Creates the user vault if it does not exist, and commits changes to disk.
  virtual void SetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id,
                                              bool stale) = 0;

  // Checks whether there is any non-constant key for `gaia_id`.
  // This indicates that the corresponding security domain is not in the
  // pre-enrollment state, but contains usable key material.
  // Returns false if the user vault does not exist or only contains constant
  // keys.
  virtual bool HasNonConstantKey(const GaiaId& gaia_id) const = 0;
};

// Storage interface for connection request throttling state.
class ConnectionThrottlingStorage {
 public:
  virtual ~ConnectionThrottlingStorage() = default;

  // Returns the time (in milliseconds since UNIX epoch) at which last failed
  // request was sent for `gaia_id`. Returns 0 if no failed requests were
  // recorded or if the user vault does not exist.
  virtual int64_t GetLastFailedRequestMillis(const GaiaId& gaia_id) const = 0;

  // Sets the time (in milliseconds since UNIX epoch) at which last failed
  // request was sent for `gaia_id`. Creates the user vault if it does not
  // exist, and commits changes to disk.
  virtual void SetLastFailedRequestMillis(
      const GaiaId& gaia_id,
      int64_t last_failed_request_millis) = 0;
};

// Storage helper for StandaloneTrustedVaultBackend handling file operations.
class StandaloneTrustedVaultStorage : public PhysicalDeviceStorage,
                                      public ICloudKeychainStorage,
                                      public KeyStorage,
                                      public ConnectionThrottlingStorage {
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
  ~StandaloneTrustedVaultStorage() override;

  // Restores state saved in storage, should be called before using the object.
  void ReadDataFromDisk();

  // Adds a new per-user vault in for `gaia_id`.
  // There must be no existing per-user vault for `gaia_id`. The lifetime of
  // the returned pointer is bound to the lifetime of `this`, but it becomes
  // invalid when ReadDataFromDisk() is called.
  // This never returns null.
  const UserVault* AddUserVault(const GaiaId& gaia_id);

  // Finds the per-user vault for `gaia_id`. Returns null if not found. The
  // lifetime of the returned pointer is bound to the lifetime of `this`, but it
  // becomes invalid when ReadDataFromDisk() is called.
  const UserVault* FindUserVault(const GaiaId& gaia_id) const;

  // Finds and returns a reference to the per-user vault for `gaia_id`.
  // Triggers a CHECK failure if the vault does not exist. The lifetime of
  // the returned reference is bound to the lifetime of `this`, but it becomes
  // invalid when ReadDataFromDisk() is called.
  const UserVault& GetUserVault(const GaiaId& gaia_id) const;

  // Executes `mutator` on the per-user vault for `gaia_id`, creating the
  // user vault if it does not exist, and commits changes to disk once upon
  // completion. Returns a const reference to the mutated per-user vault.
  const UserVault& MutateUserVault(const GaiaId& gaia_id,
                                   base::FunctionRef<void(UserVault&)> mutator);

  // Removes the per-user vaults that match `predicate`.
  void RemoveUserVaults(base::FunctionRef<bool(const UserVault&)> predicate);

  // PhysicalDeviceStorage implementation:
  const LocalDeviceRegistrationInfo& GetLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id) const override;
  void MutateLocalDeviceRegistrationInfo(
      const GaiaId& gaia_id,
      base::FunctionRef<void(LocalDeviceRegistrationInfo&)> mutator) override;

  // ICloudKeychainStorage implementation:
  const ICloudKeychainRegistrationInfo& GetICloudKeychainRegistrationInfo(
      const GaiaId& gaia_id) const override;
  void MutateICloudKeychainRegistrationInfo(
      const GaiaId& gaia_id,
      base::FunctionRef<void(ICloudKeychainRegistrationInfo&)> mutator)
      override;

  // Shared PhysicalDeviceStorage / ICloudKeychainStorage implementation:
  bool GetLastRegistrationReturnedLocalDataObsolete(
      const GaiaId& gaia_id) const override;
  void SetLastRegistrationReturnedLocalDataObsolete(const GaiaId& gaia_id,
                                                    bool obsolete) override;

  // KeyStorage implementation:
  std::vector<std::vector<uint8_t>> GetVaultKeys(
      const GaiaId& gaia_id) const override;
  int GetLastKeyVersion(const GaiaId& gaia_id) const override;
  void SetVaultKeys(const GaiaId& gaia_id,
                    const std::vector<std::vector<uint8_t>>& keys,
                    int last_key_version) override;
  bool GetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id) const override;
  void SetKeysMarkedAsStaleByConsumer(const GaiaId& gaia_id,
                                      bool stale) override;
  bool HasNonConstantKey(const GaiaId& gaia_id) const override;

  // ConnectionThrottlingStorage implementation:
  int64_t GetLastFailedRequestMillis(const GaiaId& gaia_id) const override;
  void SetLastFailedRequestMillis(const GaiaId& gaia_id,
                                  int64_t last_failed_request_millis) override;

  // Helper method to get all keys in `per_user_vault`.
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
