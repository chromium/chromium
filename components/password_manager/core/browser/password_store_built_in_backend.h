// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BUILT_IN_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BUILT_IN_BACKEND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store.h"

namespace syncer {
class ModelTypeControllerDelegate;
}  // namespace syncer

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace password_manager {

class PasswordSyncBridge;

struct FieldInfo;

// Simple password store implementation that delegates everything to
// the LoginDatabase.
class PasswordStoreBuiltInBackend : protected PasswordStoreSync,
                                    public PasswordStoreBackend,
                                    public SmartBubbleStatsStore,
                                    protected FieldInfoStore {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background sequence.
  explicit PasswordStoreBuiltInBackend(std::unique_ptr<LoginDatabase> login_db);

  PasswordStoreBuiltInBackend(
      std::unique_ptr<LoginDatabase> login_db,
      std::unique_ptr<PasswordStore::UnsyncedCredentialsDeletionNotifier>
          notifier);

  ~PasswordStoreBuiltInBackend() override;

 protected:
  // Implements PasswordStore interface.
  PasswordStoreChangeList DisableAutoSignInForOriginsImpl(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);
  DatabaseCleanupResult DeleteUndecryptableLogins() override;

  // Implements PasswordStoreSync interface.
  PasswordStoreChangeList AddLoginSync(const PasswordForm& form,
                                       AddLoginError* error) override;
  PasswordStoreChangeList UpdateLoginSync(const PasswordForm& form,
                                          UpdateLoginError* error) override;
  void NotifyLoginsChanged(const PasswordStoreChangeList& changes) override;
  void NotifyDeletionsHaveSynced(bool success) override;
  void NotifyUnsyncedCredentialsWillBeDeleted(
      std::vector<PasswordForm> unsynced_credentials) override;
  bool BeginTransaction() override;
  void RollbackTransaction() override;
  bool CommitTransaction() override;
  FormRetrievalResult ReadAllLogins(
      PrimaryKeyToFormMap* key_to_form_map) override;
  PasswordStoreChangeList RemoveLoginByPrimaryKeySync(
      FormPrimaryKey primary_key) override;
  PasswordStoreSync::MetadataStore* GetMetadataStore() override;
  bool IsAccountStore() const override;
  bool DeleteAndRecreateDatabaseFile() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordStoreTest,
                           UpdatePasswordsStoredForAffiliatedWebsites);

  // Implements PasswordStoreBackend interface.
  base::WeakPtr<PasswordStoreBackend> GetWeakPtr() override;
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsReply callback,
      bool include_psl,
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
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;

  // SmartBubbleStatsStore:
  void AddSiteStats(const InteractionsStats& stats) override;
  void RemoveSiteStats(const GURL& origin_domain) override;
  void GetSiteStats(const GURL& origin_domain,
                    base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void RemoveStatisticsByOriginAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceClosure completion) override;

  // FieldInfoStore:
  void AddFieldInfo(const FieldInfo& field_info) override;
  void GetAllFieldInfo(base::WeakPtr<PasswordStoreConsumer> consumer) override;
  void RemoveFieldInfoByTime(base::Time remove_begin,
                             base::Time remove_end,
                             base::OnceClosure completion) override;

  // Opens |login_db_| and creates |sync_bridge_| on the background sequence.
  bool InitOnBackgroundSequence(
      RemoteChangesReceived remote_form_changes_received,
      base::RepeatingClosure sync_enabled_or_disabled_cb);

  // Resets all members on the background sequence but ensures that the
  // backend deletion is happening on the given `main_task_runner` after the
  // backend work is concluded.
  void DestroyOnBackgroundSequence();

  // Synchronous implementation of GetAllLoginsAsync.
  LoginsResult GetAllLoginsInternal();

  // Synchronous implementation of GetAutofillableLoginsAsync.
  LoginsResult GetAutofillableLoginsInternal();

  // Synchronous implementation of FillMatchingLoginsAsync.
  LoginsResult FillMatchingLoginsInternal(
      const std::vector<PasswordFormDigest>& forms,
      bool include_psl);

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

  // Synchronous implementation for manipulating with statistics.
  void AddSiteStatsInternal(const InteractionsStats& stats);
  void RemoveSiteStatsInternal(const GURL& origin_domain);
  std::vector<InteractionsStats> GetSiteStatsInternal(
      const GURL& origin_domain);
  void RemoveStatisticsByOriginAndTimeInternal(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end);

  // Synchronous implementation for manipulating with field info.
  void AddFieldInfoInternal(const FieldInfo& field_info);
  std::vector<FieldInfo> GetAllFieldInfoInternal();
  void RemoveFieldInfoByTimeInternal(base::Time remove_begin,
                                     base::Time remove_end);

  base::WeakPtr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegateOnBackgroundSequence();

  // Reports password store metrics that aren't reported by the
  // StoreMetricsReporter. Namely, metrics related to inaccessible passwords,
  // and bubble statistics.
  void ReportMetrics();

  // Used to trigger DCHECKs if tasks are posted after shut down.
  bool was_shutdown_{false};

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

  std::unique_ptr<PasswordStore::UnsyncedCredentialsDeletionNotifier>
      deletion_notifier_;

  // A list of callbacks that should be run once all pending deletions have been
  // sent to the Sync server. Note that the vector itself lives on the
  // background thread, but the callbacks must be run on the main thread!
  std::vector<base::OnceCallback<void(bool)>> deletions_have_synced_callbacks_;
  // Timeout closure that runs if sync takes too long to propagate deletions.
  base::CancelableOnceClosure deletions_have_synced_timeout_;

  // TaskRunner for tasks that run on the main sequence (usually the UI thread).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<PasswordStoreBuiltInBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BUILT_IN_BACKEND_H_
