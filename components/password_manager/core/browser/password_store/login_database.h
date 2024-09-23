// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_LOGIN_DATABASE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_LOGIN_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/encrypt_decrypt_intrface.h"
#include "components/password_manager/core/browser/password_store/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_store/password_notes_table.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/browser/password_store/statistics_table.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"

#if BUILDFLAG(IS_IOS)
#include "base/gtest_prod_util.h"
#endif

namespace affiliations {
class SQLTableBuilder;
}  // namespace affiliations

namespace syncer {
class MetadataBatch;
}

namespace password_manager {

extern const int kCurrentVersionNumber;
extern const int kCompatibleVersionNumber;

// Interface to the database storage of login information, intended as a helper
// for PasswordStore on platforms that need internal storage of some or all of
// the login information.
class LoginDatabase : public EncryptDecryptInterface {
 public:
  struct LoginDatabaseEmptinessState {
    // True if the login database has 0 passwords stored.
    bool no_login_found = true;
    // True if the database has autofillable credentials. Used to decide whether
    // password suggestions can be displayed via manual fallbacks. Not
    // autofillable entries are: blocklisted entries, federated credentials and
    // username-only credentials.
    bool autofillable_credentials_exist = false;

    friend bool operator==(const LoginDatabaseEmptinessState&,
                           const LoginDatabaseEmptinessState&) = default;
  };

  using IsEmptyCallback =
      base::RepeatingCallback<void(LoginDatabaseEmptinessState)>;
  using DeletingUndecryptablePasswordsEnabled =
      base::StrongAlias<class DeletingUndecryptablePasswordsEnabledTag, bool>;
  using OnUndecryptablePasswordsRemoved =
      base::RepeatingCallback<void(password_manager::IsAccountStore)>;

  LoginDatabase(const base::FilePath& db_path,
                IsAccountStore is_account_store,
                DeletingUndecryptablePasswordsEnabled can_delete =
                    DeletingUndecryptablePasswordsEnabled(true));
  LoginDatabase(const LoginDatabase&) = delete;
  LoginDatabase& operator=(const LoginDatabase&) = delete;

  virtual ~LoginDatabase();

  // Returns whether this is the profile-scoped or the account-scoped storage:
  // true:  Gaia-account-scoped store, which is used for signed-in but not
  //        syncing users.
  // false: Profile-scoped store, which is used for local storage and for
  //        syncing users.
  bool is_account_store() const { return is_account_store_.value(); }

  // Actually creates/opens the database. If false is returned, no other method
  // should be called.
  virtual bool Init(
      OnUndecryptablePasswordsRemoved on_undecryptable_passwords_removed,
      std::unique_ptr<os_crypt_async::Encryptor> encryptor);

  // Reports metrics regarding inaccessible passwords and bubble usages to UMA.
  void ReportMetrics();

  // Adds |form| to the list of remembered password forms. Returns the list of
  // changes applied ({}, {ADD}, {REMOVE, ADD}). If it returns {REMOVE, ADD}
  // then the REMOVE is associated with the form that was added. Thus only the
  // primary key columns contain the values associated with the removed form. In
  // case of error, it sets |error| if |error| isn't null.
  [[nodiscard]] PasswordStoreChangeList AddLogin(
      const PasswordForm& form,
      AddCredentialError* error = nullptr);

  // Updates existing password form. Returns the list of applied changes ({},
  // {UPDATE}). The password is looked up by the tuple {origin,
  // username_element, username_value, password_element, signon_realm}. These
  // columns stay intact. In case of error, it sets |error| if |error| isn't
  // null.
  [[nodiscard]] PasswordStoreChangeList UpdateLogin(
      const PasswordForm& form,
      UpdateCredentialError* error = nullptr);

  // Removes |form| from the list of remembered password forms. Returns true if
  // |form| was successfully removed from the database. If |changes| is not be
  // null, it will be used to populate the change list of the removed forms if
  // any.
  [[nodiscard]] bool RemoveLogin(const PasswordForm& form,
                                 PasswordStoreChangeList* changes);

  // Removes the form with |primary_key| from the list of remembered password
  // forms. Returns true if the form was successfully removed from the database.
  [[nodiscard]] bool RemoveLoginByPrimaryKey(FormPrimaryKey primary_key,
                                             PasswordStoreChangeList* changes);

  // Removes all logins created from |delete_begin| onwards (inclusive) and
  // before |delete_end|. You may use a null Time value to do an unbounded
  // delete in either direction. If |changes| is not be null, it will be used to
  // populate the change list of the removed forms if any.
  bool RemoveLoginsCreatedBetween(base::Time delete_begin,
                                  base::Time delete_end,
                                  PasswordStoreChangeList* changes);

  // Sets the 'skip_zero_click' flag on all forms on |origin| to 'true'.
  bool DisableAutoSignInForOrigin(const GURL& origin);

  // All Get* methods below overwrite |forms| with the returned credentials. On
  // success, those methods return true.

  // Gets a list of credentials matching |form|, including blocklisted matches
  // and federated credentials.
  // |should_PSL_matching_apply| controls if the PSL matches are included or
  // only the exact matches.
  [[nodiscard]] bool GetLogins(const PasswordFormDigest& form,
                               bool should_PSL_matching_apply,
                               std::vector<PasswordForm>* forms);

  // Gets all logins created from |begin| onwards (inclusive) and before |end|.
  // You may use a null Time value to do an unbounded search in either
  // direction. |forms| must not be null and will be used to return
  // the results.
  [[nodiscard]] bool GetLoginsCreatedBetween(base::Time begin,
                                             base::Time end,
                                             std::vector<PasswordForm>* forms);

  // Gets the complete list of all credentials.
  [[nodiscard]] FormRetrievalResult GetAllLogins(
      std::vector<PasswordForm>* forms);

  // Gets list of logins which match |signon_realm| and |username|.
  [[nodiscard]] FormRetrievalResult GetLoginsBySignonRealmAndUsername(
      const std::string& signon_realm,
      const std::u16string& username,
      std::vector<PasswordForm>* forms);

  // Gets the complete list of not blocklisted credentials.
  [[nodiscard]] bool GetAutofillableLogins(std::vector<PasswordForm>* forms);

  // Gets the complete list of blocklisted credentials.
  [[nodiscard]] bool GetBlocklistLogins(std::vector<PasswordForm>* forms);

  // Gets the list of auto-sign-inable credentials.
  [[nodiscard]] bool GetAutoSignInLogins(std::vector<PasswordForm>* forms);

  // Deletes the login database file on disk, and creates a new, empty database.
  // This can be used after migrating passwords to some other store, to ensure
  // that SQLite doesn't leave fragments of passwords in the database file.
  // Returns true on success; otherwise, whether the file was deleted and
  // whether further use of this login database will succeed is unspecified.
  bool DeleteAndRecreateDatabaseFile();

  LoginDatabaseEmptinessState IsEmpty();

  // On MacOS, it deletes all logins from the database that cannot be decrypted
  // when encryption key from Keychain is available. If the Keychain is locked,
  // it does nothing and returns ENCRYPTION_UNAVAILABLE. If it's not running on
  // MacOS, it does nothing and returns SUCCESS. This can be used when syncing
  // logins from the cloud to rewrite entries that can't be used anymore (due to
  // modification of the encryption key). If one of the logins couldn't be
  // removed from the database, returns ITEM_FAILURE.
  DatabaseCleanupResult DeleteUndecryptableLogins();

  // Callers that requires transaction support should call these methods to
  // begin, rollback and commit transactions. They delegate to the transaction
  // support of the underlying database. Only one transaction may exist at a
  // time.
  bool BeginTransaction();
  void RollbackTransaction();
  bool CommitTransaction();

  // `is_empty_cb`is called to signal whether the database is empty (i.e.
  // without any logins *or* blocklists) and whether there are any autofillable
  // logins. The call happens when initializing the database and when
  // adding/removing entries, regardless of success.
  void SetIsEmptyCb(IsEmptyCallback is_empty_cb);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  void SetIsUserDataDirPolicySet(bool is_set) {
    is_user_data_dir_policy_set_ = is_set;
  }
#endif

  StatisticsTable& stats_table() { return stats_table_; }
  InsecureCredentialsTable& insecure_credentials_table() {
    return insecure_credentials_table_;
  }
  PasswordNotesTable& password_notes_table() { return password_notes_table_; }

  PasswordStoreSync::MetadataStore& password_sync_metadata_store() {
    return password_sync_metadata_store_;
  }

  // After `were_undecryptable_logins_deleted_` is used by the
  // `PasswordSyncBridge` it should be cleared to avoid unnecessary sync calls.
  void clear_were_undecryptable_logins_deleted() {
    were_undecryptable_logins_deleted_ = false;
  }

  // Returns whether there were undecryptable logins present in the login db and
  // if they were successfully removed.
  std::optional<bool> were_undecryptable_logins_deleted() const {
    return were_undecryptable_logins_deleted_;
  }

 private:
  // EncryptDecryptInterface implementation.
  [[nodiscard]] EncryptionResult EncryptedString(
      const std::u16string& plain_text,
      std::string* cipher_text) const override;
  [[nodiscard]] EncryptionResult DecryptedString(
      const std::string& cipher_text,
      std::u16string* plain_text) const override;

  struct PrimaryKeyAndPassword;
  class SyncMetadataStore : public PasswordStoreSync::MetadataStore {
   public:
    // |db| must be not null and must outlive |this|.
    explicit SyncMetadataStore(sql::Database* db);
    SyncMetadataStore(const SyncMetadataStore&) = delete;
    SyncMetadataStore& operator=(const SyncMetadataStore&) = delete;
    ~SyncMetadataStore() override;

    // Test-only variant of the private function with a similar name.
    std::unique_ptr<sync_pb::EntityMetadata>
    GetSyncEntityMetadataForStorageKeyForTest(syncer::DataType data_type,
                                              const std::string& storage_key);

   private:
    // Reads all the stored sync entities metadata for |data_type| in a
    // MetadataBatch. Returns nullptr in case of failure. This is currently used
    // only for passwords.
    std::unique_ptr<syncer::MetadataBatch> GetAllSyncEntityMetadata(
        syncer::DataType data_type);

    // Reads sync entity data for an individual |data_type| and entity
    // identified by |storage_key|. Returns null if no entry is found or an
    // error occurred.
    std::unique_ptr<sync_pb::EntityMetadata> GetSyncEntityMetadataForStorageKey(
        syncer::DataType data_type,
        const std::string& storage_key);

    // Reads the stored DataTypeState for |data_type|. Returns nullptr in case
    // of failure. This is currently used only for passwords.
    std::unique_ptr<sync_pb::DataTypeState> GetDataTypeState(
        syncer::DataType data_type);

    // PasswordStoreSync::MetadataStore implementation.
    std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata(
        syncer::DataType data_type) override;
    void DeleteAllSyncMetadata(syncer::DataType data_type) override;
    bool UpdateEntityMetadata(syncer::DataType data_type,
                              const std::string& storage_key,
                              const sync_pb::EntityMetadata& metadata) override;
    bool ClearEntityMetadata(syncer::DataType data_type,
                             const std::string& storage_key) override;
    bool UpdateDataTypeState(
        syncer::DataType data_type,
        const sync_pb::DataTypeState& data_type_state) override;

    bool ClearDataTypeState(syncer::DataType data_type) override;
    void SetPasswordDeletionsHaveSyncedCallback(
        base::RepeatingCallback<void(bool)> callback) override;
    bool HasUnsyncedPasswordDeletions() override;

    raw_ptr<sql::Database> const db_;
    // A callback to be invoked whenever all pending deletions have been
    // processed
    // by Sync - see
    // PasswordStoreSync::MetadataStore::SetDeletionsHaveSyncedCallback for more
    // details.
    base::RepeatingCallback<void(bool)>
        password_deletions_have_synced_callback_;
  };

  FRIEND_TEST_ALL_PREFIXES(LoginDatabaseSyncMetadataTest,
                           GetSyncEntityMetadataForStorageKey);

  void ReportNumberOfAccountsMetrics(bool custom_passphrase_sync_enabled);
  void ReportTimesPasswordUsedMetrics(bool custom_passphrase_sync_enabled);
  void ReportSyncingAccountStateMetrics(const std::string& sync_username);
  void ReportEmptyUsernamesMetrics();
  void ReportLoginsWithSchemesMetrics();
  void ReportBubbleSuppressionMetrics();
  void ReportInaccessiblePasswordsMetrics();
  void ReportDuplicateCredentialsMetrics();

  // Fills |form| from the values in the given statement (which is assumed to be
  // of the form used by the Get*Logins methods).
  // WARNING: Password value itself is absent, callers have to decrypt it if
  // needed.
  [[nodiscard]] PasswordForm GetFormWithoutPasswordFromStatement(
      sql::Statement& s) const;

  // Gets all blocklisted or all non-blocklisted (depending on |blocklisted|)
  // credentials. On success returns true and overwrites |forms| with the
  // result.
  bool GetAllLoginsWithBlocklistSetting(bool blocklisted,
                                        std::vector<PasswordForm>* forms);

  // Returns the DB primary key for the specified |form| and decrypted/encrypted
  // password. Returns {-1, "", ""} if the row for this |form| is not found.
  PrimaryKeyAndPassword GetPrimaryKeyAndPassword(
      const PasswordForm& form) const;

  // Overwrites |key_to_form_map| with credentials retrieved from |statement|.
  // If |matched_form| is not null, filters out all results but those
  // PSL-matching |*matched_form| or federated credentials for it. If feature
  // for recovering passwords is enabled, it removes all passwords that couldn't
  // be decrypted when encryption was available from the database. On success
  // returns true.
  // |forms| must not be null and will be used to return the results.
  [[nodiscard]] FormRetrievalResult StatementToForms(
      sql::Statement* statement,
      const PasswordFormDigest* matched_form,
      std::vector<PasswordForm>* forms);

  // Initializes all the *_statement_ data members with appropriate SQL
  // fragments based on |builder|.
  void InitializeStatementStrings(const affiliations::SQLTableBuilder& builder);

  // Returns either kProfileStore or kAccountStore depending on the value of
  // `is_account_store_`.
  PasswordForm::Store GetStore() const;

  // Returns insecure credentials corresponding to the `primary_key`.
  base::flat_map<InsecureType, InsecurityMetadata> GetPasswordIssues(
      FormPrimaryKey primary_key) const;

  // Updates data in the `insecure_credentials_table_` with the password issues
  // data from `password_issues`. Returns whether any insecure credential entry
  // was changed.
  InsecureCredentialsChanged UpdateInsecureCredentials(
      FormPrimaryKey primary_key,
      const base::flat_map<InsecureType, InsecurityMetadata>& password_issues);

  // Returns password notes corresponding to `primary_key`.
  std::vector<PasswordNote> GetPasswordNotes(FormPrimaryKey primary_key) const;

  // Updates the `password_notes` table if `notes` changed for `primary_key`.
  void UpdatePasswordNotes(FormPrimaryKey primary_key,
                           const std::vector<PasswordNote>& notes);

  // If a non-null `is_empty_cb` was passed on construction, computes whether
  // the DB is empty (SQL statement) and invokes the callback with the result.
  void TriggerIsEmptyCb();

  const base::FilePath db_path_;
  const IsAccountStore is_account_store_;
  IsEmptyCallback is_empty_cb_ = base::NullCallback();

  // `on_undecryptable_passwords_removed_`is called to signal whether user
  // interacted with the kClearUndecryptablePasswords experiment. It is needed
  // to ensure that experiment groups stay balanced. This callback will be
  // deleted after a successful rollout.
  // TODO(b/40286735): Remove after this feature is launched.
  OnUndecryptablePasswordsRemoved on_undecryptable_passwords_removed_ =
      base::NullCallback();

  mutable sql::Database db_;
  sql::MetaTable meta_table_;
  StatisticsTable stats_table_;
  InsecureCredentialsTable insecure_credentials_table_;
  PasswordNotesTable password_notes_table_;
  SyncMetadataStore password_sync_metadata_store_{&db_};
  std::unique_ptr<os_crypt_async::Encryptor> encryptor_;

  std::optional<bool> were_undecryptable_logins_deleted_;
  bool is_user_data_dir_policy_set_ = false;
  DeletingUndecryptablePasswordsEnabled
      is_deleting_undecryptable_logins_enabled_by_policy_;

  // These cached strings are used to build SQL statements.
  std::string add_statement_;
  std::string add_replace_statement_;
  std::string update_statement_;
  std::string delete_statement_;
  std::string delete_by_id_statement_;
  std::string autosignin_statement_;
  std::string get_statement_;
  std::string get_statement_psl_;
  std::string get_statement_federated_;
  std::string get_statement_psl_federated_;
  std::string get_statement_username_;
  std::string created_statement_;
  std::string blocklisted_statement_;
  std::string id_and_password_statement_;
};

#if BUILDFLAG(IS_IOS)
// Adds |plain_text| to keychain and provides lookup in
// |keychain_identifier|. Returns true or false to indicate success/failure.
bool CreateKeychainIdentifier(const std::u16string& plain_text,
                              std::string* keychain_identifier);

// Retrieves |plain_text| from keychain using |keychain_identifier|. Returns
// the status of the operation.
OSStatus GetTextFromKeychainIdentifier(const std::string& keychain_identifier,
                                       std::u16string* plain_text);

// Removes the keychain item corresponding to the look-up key
// |keychain_identifier|. It's stored as the encrypted password value.
void DeleteEncryptedPasswordFromKeychain(
    const std::string& keychain_identifier);

// Retrieves everything from the keychain. |key_password_pairs| contains
// pairs of UUID and plaintext password.
OSStatus GetAllPasswordsFromKeychain(
    std::unordered_map<std::string, std::u16string>* key_password_pairs);
#endif

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_LOGIN_DATABASE_H_
