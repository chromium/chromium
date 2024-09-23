// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_STORE_SYNC_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_STORE_SYNC_H_

#include <map>
#include <memory>
#include <vector>

#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/sync/model/sync_metadata_store.h"

namespace syncer {
class MetadataBatch;
}

namespace sync_pb {
class PasswordSpecificsData;
}

namespace password_manager {

using PrimaryKeyToPasswordSpecificsDataMap =
    std::map<FormPrimaryKey, std::unique_ptr<sync_pb::PasswordSpecificsData>>;

// This enum is used to determine result status when deleting undecryptable
// credentials from database.
enum class DatabaseCleanupResult {
  kSuccess,
  kItemFailure,
  kDatabaseUnavailable,
  kEncryptionUnavailable,
};

// Result values for retrieving form from the store.
enum class FormRetrievalResult {
  // Success.
  kSuccess,
  // Database error.
  kDbError,
  // A service-level failure (e.g., on a platform using a keyring, the keyring
  // is temporarily unavailable).
  kEncryptionServiceFailure,
  // A service-level failure, but some forms can be retrieved successfully.
  kEncryptionServiceFailureWithPartialData,
};

// Error values for adding a credential to the store.
// Used in metrics: "PasswordManager.MergeSyncData.AddLoginSyncError" and
// "PasswordManager.ApplySyncChanges.AddLoginSyncError". These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class AddCredentialError {
  // Success.
  kNone = 0,
  // Database not available.
  kDbNotAvailable = 1,
  // The form doesn't the satisfy the constraints.
  kConstraintViolation = 2,
  // A service-level failure (e.g., on a platform using a keyring, the keyring
  // is temporarily unavailable).
  kEncryptionServiceFailure = 3,
  // Database error.
  kDbError = 4,

  kMaxValue = kDbError,
};

// Error values for updating a credential in the store.
// Used in metrics: "PasswordManager.MergeSyncData.UpdateLoginSyncError" and
// "PasswordManager.ApplySyncChanges.UpdateLoginSyncError". These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class UpdateCredentialError {
  // Success.
  kNone = 0,
  // Database not available.
  kDbNotAvailable = 1,
  // No records were updated.
  kNoUpdatedRecords = 2,
  // A service-level failure (e.g., on a platform using a keyring, the keyring
  // is temporarily unavailable).
  kEncryptionServiceFailure = 3,
  // Database error.
  kDbError = 4,

  kMaxValue = kDbError,
};

// PasswordStore interface for PasswordSyncBridge. It provides access to
// synchronous methods of PasswordStore which shouldn't be accessible to other
// classes. These methods are to be called on the PasswordStore background
// thread only.
class PasswordStoreSync {
 public:
  class MetadataStore : public syncer::SyncMetadataStore {
   public:
    // Reads and returns all the stored sync metadata for |data_type|. This is
    // currently used only for passwords.
    virtual std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata(
        syncer::DataType data_type) = 0;

    // Deletes all the stored sync metadata for |data_type|. This is currently
    // used only for passwords.
    virtual void DeleteAllSyncMetadata(syncer::DataType data_type) = 0;

    // Registers a callback that will be invoked whenever all pending (unsynced)
    // deletions are gone. If they were committed to the server (or, rarely, the
    // entity was undeleted), the |callback| will be run with "true". If the
    // deletions are gone because Sync was permanently turned off, it'll be run
    // with "false" instead.
    // Note that there can be only one such callback; if one was already
    // registered, it'll be overridden by the new |callback|.
    virtual void SetPasswordDeletionsHaveSyncedCallback(
        base::RepeatingCallback<void(bool)> callback) = 0;

    // Returns whether there are any pending deletions that have not been sent
    // to the Sync server yet.
    virtual bool HasUnsyncedPasswordDeletions() = 0;
  };

  PasswordStoreSync();

  PasswordStoreSync(const PasswordStoreSync&) = delete;
  PasswordStoreSync& operator=(const PasswordStoreSync&) = delete;

  // Overwrites |key_to_specifics_map| with a map from the DB primary key to the
  // corresponding form for all stored credentials. Returns true on success.
  [[nodiscard]] virtual FormRetrievalResult ReadAllCredentials(
      PrimaryKeyToPasswordSpecificsDataMap* key_to_specifics_map) = 0;

  // Deletes credentials that cannot be decrypted.
  virtual DatabaseCleanupResult DeleteUndecryptableCredentials() = 0;

  // Synchronous implementation to add the given credential.
  virtual PasswordStoreChangeList AddCredentialSync(
      const sync_pb::PasswordSpecificsData& specifics,
      AddCredentialError* error = nullptr) = 0;

  // Synchronous implementation to update the given credential.
  virtual PasswordStoreChangeList UpdateCredentialSync(
      const sync_pb::PasswordSpecificsData& specifics,
      UpdateCredentialError* error = nullptr) = 0;

  // Synchronous implementation to remove the credential with the given primary
  // key.
  virtual PasswordStoreChangeList RemoveCredentialByPrimaryKeySync(
      FormPrimaryKey primary_key) = 0;

  // Notifies observers that password store data may have been changed.
  virtual void NotifyCredentialsChanged(
      const PasswordStoreChangeList& changes) = 0;

  // Notifies any waiting callback that all pending deletions have been
  // committed to the Sync server now, or that Sync definitely won't commit
  // them (because Sync was turned off permanently).
  virtual void NotifyDeletionsHaveSynced(bool success) = 0;

  // Notifies the UI that some unsynced credentials will be deleted on sign-out
  // in order to offer the user the option of saving them in the profile store.
  // Should only be called for the account store.
  virtual void NotifyUnsyncedCredentialsWillBeDeleted(
      std::vector<PasswordForm> unsynced_credentials) = 0;

  // The methods below adds transaction support to the password store that's
  // required by sync to guarantee atomic writes of data and sync metadata.
  // TODO(crbug.com/40601175): The introduction of the three functions below
  // question the existence of NotifyCredentialsChanged() above and all the
  // round trips with PasswordStoreChangeList in the earlier functions. Instead,
  // observers could be notified inside CommitTransaction().
  virtual bool BeginTransaction() = 0;
  virtual void RollbackTransaction() = 0;
  virtual bool CommitTransaction() = 0;

  // Returns a SyncMetadataStore that sync machinery would use to persist the
  // sync metadata.
  virtual MetadataStore* GetMetadataStore() = 0;

  // Returns whether this is the profile-scoped or the account-scoped storage:
  // true:  Gaia-account-scoped store, which is used for signed-in but not
  //        syncing users.
  // false: Profile-scoped store, which is used for local storage and for
  //        syncing users.
  virtual bool IsAccountStore() const = 0;

  virtual bool DeleteAndRecreateDatabaseFile() = 0;

  virtual std::optional<bool> WereUndecryptableLoginsDeleted() const = 0;
  // Called after all sync metadata was deleted as a result of deleting
  // undecryptable passwords. Used to avoid unnecessary sync by clearing
  // LoginDatabase member responsible for triggering them.
  virtual void ClearWereUndecryptableLoginsDeleted() = 0;

 protected:
  virtual ~PasswordStoreSync();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_STORE_SYNC_H_
