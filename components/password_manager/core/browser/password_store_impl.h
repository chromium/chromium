// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

class PasswordSyncBridge;

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreImpl : protected PasswordStoreSync,
                          public PasswordStore,
                          public PasswordStoreBackend {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background sequence.
  explicit PasswordStoreImpl(std::unique_ptr<LoginDatabase> login_db);

  void ShutdownOnUIThread() override;

  // To be used only for testing or in subclasses.
  LoginDatabase* login_db() const { return login_db_.get(); }

 protected:
  ~PasswordStoreImpl() override;

  // Implements PasswordStore interface.
  void SetUnsyncedCredentialsDeletionNotifier(
      std::unique_ptr<UnsyncedCredentialsDeletionNotifier> deletion_notifier)
      override;
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled,
                         BulkCheckDone bulk_check_done) override;
  PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) override;
  bool RemoveStatisticsByOriginAndTimeImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLogins(
      const PasswordFormDigest& form) override;
  std::vector<std::unique_ptr<PasswordForm>> FillMatchingLoginsByPassword(
      const std::u16string& plain_text_password) override;
  DatabaseCleanupResult DeleteUndecryptableLogins() override;
  void AddSiteStatsImpl(const InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  std::vector<InteractionsStats> GetSiteStatsImpl(
      const GURL& origin_domain) override;
  PasswordStoreChangeList AddInsecureCredentialImpl(
      const InsecureCredential& insecure_credential) override;
  PasswordStoreChangeList RemoveInsecureCredentialsImpl(
      const std::string& signon_realm,
      const std::u16string& username,
      RemoveInsecureCredentialsReason reason) override;
  std::vector<InsecureCredential> GetAllInsecureCredentialsImpl() override;
  std::vector<InsecureCredential> GetMatchingInsecureCredentialsImpl(
      const std::string& signon_realm) override;

  void AddFieldInfoImpl(const FieldInfo& field_info) override;
  std::vector<FieldInfo> GetAllFieldInfoImpl() override;
  void RemoveFieldInfoByTimeImpl(base::Time remove_begin,
                                 base::Time remove_end) override;

  bool IsEmpty() override;
  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateOnBackgroundSequence() override;

  // Implements PasswordStoreSync interface.
  PasswordStoreChangeList AddLoginSync(const PasswordForm& form,
                                       AddLoginError* error) override;
  bool AddInsecureCredentialsSync(
      base::span<const InsecureCredential> credentials) override;
  PasswordStoreChangeList UpdateLoginSync(const PasswordForm& form,
                                          UpdateLoginError* error) override;
  bool UpdateInsecureCredentialsSync(
      const PasswordForm& form,
      base::span<const InsecureCredential> credentials) override;
  PasswordStoreChangeList RemoveLoginSync(const PasswordForm& form) override;
  void NotifyLoginsChanged(const PasswordStoreChangeList& changes) override;
  void NotifyDeletionsHaveSynced(bool success) override;
  void NotifyUnsyncedCredentialsWillBeDeleted(
      std::vector<PasswordForm> unsynced_credentials) override;
  bool BeginTransaction() override;
  void RollbackTransaction() override;
  bool CommitTransaction() override;
  FormRetrievalResult ReadAllLogins(
      PrimaryKeyToFormMap* key_to_form_map) override;
  std::vector<InsecureCredential> ReadSecurityIssues(
      FormPrimaryKey parent_key) override;
  PasswordStoreChangeList RemoveLoginByPrimaryKeySync(
      FormPrimaryKey primary_key) override;
  PasswordStoreSync::MetadataStore* GetMetadataStore() override;
  bool IsAccountStore() const override;
  bool DeleteAndRecreateDatabaseFile() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest,
                           UpdatePasswordsStoredForAffiliatedWebsites);
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest, AddInsecureCredentialsSync);
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest, UpdateInsecureCredentialsSync);

  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void GetAllLoginsAsync(LoginsReply callback) override;
  void GetAutofillableLoginsAsync(LoginsReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordStoreChangeListReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordStoreChangeListReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordStoreChangeListReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordStoreChangeListReply callback) override;

  // Opens |login_db_| and creates |sync_bridge_| on the background sequence.
  bool InitOnBackgroundSequence(
      RemoteChangesReceived remote_form_changes_received,
      base::RepeatingClosure sync_enabled_or_disabled_cb);

  // Resets |login_db_| and |sync_bridge_| on the background sequence.
  void DestroyOnBackgroundSequence();

  // Synchronous implementation of GetAllLoginsAsync.
  LoginsResult GetAllLoginsInternal();

  // Synchronous implementation of GetAutofillableLoginsAsync.
  LoginsResult GetAutofillableLoginsInternal();

  // Synchronous implementation of FillMatchingLoginsAsync.
  LoginsResult FillMatchingLoginsInternal(
      const std::vector<PasswordFormDigest>& forms);

  PasswordStoreChangeList AddLoginInternal(const PasswordForm& form);
  PasswordStoreChangeList UpdateLoginInternal(const PasswordForm& form);
  PasswordStoreChangeList RemoveLoginInternal(const PasswordForm& form);
  PasswordStoreChangeList RemoveLoginsCreatedBetweenInternal(
      base::Time delete_begin,
      base::Time delete_end);
  PasswordStoreChangeList RemoveLoginsByURLAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion);

  // The login SQL database. The LoginDatabase instance is received via the
  // in an uninitialized state, so as to allow injecting mocks, then Init() is
  // called on the background sequence in a deferred manner. If opening the DB
  // fails, |login_db_| will be reset and stay NULL for the lifetime of |this|.
  std::unique_ptr<LoginDatabase> login_db_;

  std::unique_ptr<PasswordSyncBridge> sync_bridge_;

  // Whenever 'sync_bridge_'receive remote changes this callback is used to
  // notify PasswordStore observers about them. Called on a main sequence from
  // the 'NotifyLoginsChanged'.
  RemoteChangesReceived remote_forms_changes_received_callback_;

  std::unique_ptr<UnsyncedCredentialsDeletionNotifier> deletion_notifier_;

  // A list of callbacks that should be run once all pending deletions have been
  // sent to the Sync server. Note that the vector itself lives on the
  // background thread, but the callbacks must be run on the main thread!
  std::vector<base::OnceCallback<void(bool)>> deletions_have_synced_callbacks_;
  // Timeout closure that runs if sync takes too long to propagate deletions.
  base::CancelableOnceClosure deletions_have_synced_timeout_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreImpl);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_IMPL_H_
