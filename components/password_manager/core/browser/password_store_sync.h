// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SYNC_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SYNC_H_

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/sync/model/sync_metadata_store.h"

namespace autofill {
struct PasswordForm;
}

namespace syncer {
class MetadataBatch;
}

namespace password_manager {

using PrimaryKeyToFormMap =
    std::map<int, std::unique_ptr<autofill::PasswordForm>>;

// This enum is used to determine result status when deleting undecryptable
// logins from database.
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
  kEncrytionServiceFailure,
};

// Error values for adding a login to the store.
// Used in metrics: "PasswordManager.MergeSyncData.AddLoginSyncError" and
// "PasswordManager.ApplySyncChanges.AddLoginSyncError". These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class AddLoginError {
  // Success.
  kNone = 0,
  // Database not available.
  kDbNotAvailable = 1,
  // The form doesn't the satisfy the constraints.
  kConstraintViolation = 2,
  // A service-level failure (e.g., on a platform using a keyring, the keyring
  // is temporarily unavailable).
  kEncrytionServiceFailure = 3,
  // Database error.
  kDbError = 4,

  kMaxValue = kDbError,
};

// Error values for updating a login in the store.
// Used in metrics: "PasswordManager.MergeSyncData.UpdateLoginSyncError" and
// "PasswordManager.ApplySyncChanges.UpdateLoginSyncError". These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class UpdateLoginError {
  // Success.
  kNone = 0,
  // Database not available.
  kDbNotAvailable = 1,
  // No records were updated.
  kNoUpdatedRecords = 2,
  // A service-level failure (e.g., on a platform using a keyring, the keyring
  // is temporarily unavailable).
  kEncrytionServiceFailure = 3,
  // Database error.
  kDbError = 4,

  kMaxValue = kDbError,
};

// PasswordStore interface for PasswordSyncableService. It provides access to
// synchronous methods of PasswordStore which shouldn't be accessible to other
// classes. These methods are to be called on the PasswordStore background
// thread only.
class PasswordStoreSync {
 public:
  class MetadataStore : public syncer::SyncMetadataStore {
   public:
    // Reads and returns all the stored sync metadata for passwords.
    virtual std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata() = 0;

    // Deletes all the stored sync metadata for passwords.
    virtual void DeleteAllSyncMetadata() = 0;
  };

  PasswordStoreSync();

  // TODO(http://crbug.com/925307) Move the following 2 APIs to PasswordStore
  // upon full migration to USS Sync architecture.
  // Overwrites |forms| with all stored non-blacklisted credentials. Returns
  // true on success.
  virtual bool FillAutofillableLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms)
      WARN_UNUSED_RESULT = 0;

  // Overwrites |forms| with all stored blacklisted credentials. Returns true on
  // success.
  virtual bool FillBlacklistLogins(
      std::vector<std::unique_ptr<autofill::PasswordForm>>* forms)
      WARN_UNUSED_RESULT = 0;

  // Overwrites |key_to_form_map| with a map from the DB primary key to the
  // corresponding form for all stored credentials. Returns true on success.
  virtual FormRetrievalResult ReadAllLogins(
      PrimaryKeyToFormMap* key_to_form_map) WARN_UNUSED_RESULT = 0;

  // Deletes logins that cannot be decrypted.
  virtual DatabaseCleanupResult DeleteUndecryptableLogins() = 0;

  // Synchronous implementation to add the given login.
  virtual PasswordStoreChangeList AddLoginSync(
      const autofill::PasswordForm& form,
      AddLoginError* error = nullptr) = 0;

  // Synchronous implementation to update the given login.
  virtual PasswordStoreChangeList UpdateLoginSync(
      const autofill::PasswordForm& form,
      UpdateLoginError* error = nullptr) = 0;

  // Synchronous implementation to remove the given login.
  virtual PasswordStoreChangeList RemoveLoginSync(
      const autofill::PasswordForm& form) = 0;

  // Synchronous implementation to remove the login with the given primary key.
  virtual PasswordStoreChangeList RemoveLoginByPrimaryKeySync(
      int primary_key) = 0;

  // Notifies observers that password store data may have been changed.
  virtual void NotifyLoginsChanged(const PasswordStoreChangeList& changes) = 0;

  // The methods below adds transaction support to the password store that's
  // required by sync to guarantee atomic writes of data and sync metadata.
  // TODO(crbug.com/902349): The introduction of the three functions below
  // question the existence of NotifyLoginsChanged() above and all the round
  // trips with PasswordStoreChangeList in the earlier functions. Instead,
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

 protected:
  virtual ~PasswordStoreSync();

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordStoreSync);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_SYNC_H_
