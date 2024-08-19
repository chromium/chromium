// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_LOGIN_DATABASE_ASYNC_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_LOGIN_DATABASE_ASYNC_HELPER_H_

#include "base/cancelable_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"

namespace base {
class Location;
}  // namespace base

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace password_manager {

class LoginDatabase;
class PasswordSyncBridge;

struct InteractionsStats;

// Class which interacts directly with LoginDatabase. It is also responsible to
// sync passwords. Works only on background sequence.
class LoginDatabaseAsyncHelper : public PasswordStoreSync {
 public:
  LoginDatabaseAsyncHelper(
      std::unique_ptr<LoginDatabase> login_db,
      UnsyncedCredentialsDeletionNotifier notifier,
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      syncer::WipeModelUponSyncDisabledBehavior
          wipe_model_upon_sync_disabled_behavior);

  ~LoginDatabaseAsyncHelper() override;

  // Called soon after constructor but on the background sequence.
  void CreateSyncBackend();

  // Opens |login_db_|. CreateSyncBackend() must have been called before.
  bool Initialize(
      base::RepeatingCallback<void(std::optional<PasswordStoreChangeList>,
                                   bool)> remote_form_changes_received,
      base::RepeatingClosure sync_enabled_or_disabled_cb,
      base::RepeatingCallback<void(password_manager::IsAccountStore)>
          on_undecryptable_passwords_removed,
      std::unique_ptr<os_crypt_async::Encryptor> encryptor);

  // Synchronous implementation of PasswordStoreBackend interface.
  LoginsResultOrError GetAllLogins();
  LoginsResultOrError GetAutofillableLogins();
  LoginsResultOrError FillMatchingLogins(
      const std::vector<PasswordFormDigest>& forms,
      bool include_psl);

  PasswordChangesOrError AddLogin(const PasswordForm& form);
  PasswordChangesOrError UpdateLogin(const PasswordForm& form);
  PasswordChangesOrError RemoveLogin(const base::Location& location,
                                     const PasswordForm& form);
  PasswordChangesOrError RemoveLoginsCreatedBetween(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end);
  PasswordChangesOrError RemoveLoginsByURLAndTime(
      const base::Location& location,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion);
  PasswordStoreChangeList DisableAutoSignInForOrigins(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter);

  // Synchronous implementation of SmartBubbleStatsStore interface.
  void AddSiteStats(const InteractionsStats& stats);
  void RemoveSiteStats(const GURL& origin_domain);
  std::vector<InteractionsStats> GetSiteStats(const GURL& origin_domain);
  void RemoveStatisticsByOriginAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end);

  // Instantiates a proxy controller delegate to react to sync events.
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate();

  // `clearing_undecryptable_passwords`is called to signal whether user
  // interacted with the kClearUndecryptablePasswords experiment. It is needed
  // to ensure that experiment groups stay balaced. This method will be deleted
  // after a successful rollout.
  void SetClearingUndecryptablePasswordsCb(
      base::RepeatingCallback<void(bool)> clearing_undecryptable_passwords);

 private:
  // Implements PasswordStoreSync interface.
  PasswordStoreChangeList AddCredentialSync(
      const sync_pb::PasswordSpecificsData& password,
      AddCredentialError* error) override;
  PasswordStoreChangeList UpdateCredentialSync(
      const sync_pb::PasswordSpecificsData& password,
      UpdateCredentialError* error) override;
  void NotifyCredentialsChanged(
      const PasswordStoreChangeList& changes) override;
  void NotifyDeletionsHaveSynced(bool success) override;
  void NotifyUnsyncedCredentialsWillBeDeleted(
      std::vector<PasswordForm> unsynced_credentials) override;
  bool BeginTransaction() override;
  void RollbackTransaction() override;
  bool CommitTransaction() override;
  FormRetrievalResult ReadAllCredentials(
      PrimaryKeyToPasswordSpecificsDataMap* key_to_form_map) override;
  PasswordStoreChangeList RemoveCredentialByPrimaryKeySync(
      FormPrimaryKey primary_key) override;
  PasswordStoreSync::MetadataStore* GetMetadataStore() override;
  bool IsAccountStore() const override;
  bool DeleteAndRecreateDatabaseFile() override;
  DatabaseCleanupResult DeleteUndecryptableCredentials() override;
  std::optional<bool> WereUndecryptableLoginsDeleted() const override;
  void ClearWereUndecryptableLoginsDeleted() override;

  PasswordStoreChangeList AddLoginImpl(const PasswordForm& form,
                                       AddCredentialError* error);
  PasswordStoreChangeList UpdateLoginImpl(const PasswordForm& form,
                                          UpdateCredentialError* error);

  // Reports password store metrics that aren't reported by the
  // StoreMetricsReporter. Namely, metrics related to inaccessible passwords,
  // and bubble statistics.
  void ReportMetrics();

  // Ensures that all methods, excluding construction, are called on the same
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // The login SQL database. The LoginDatabase instance is received via the
  // constructor. It is passed in an uninitialized state, to allow injecting
  // mocks. It will be initilaized by calling Initialize. If opening the DB
  // fails, |login_db_| will be reset and stay NULL for the lifetime of |this|.
  std::unique_ptr<LoginDatabase> login_db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const syncer::WipeModelUponSyncDisabledBehavior
      wipe_model_upon_sync_disabled_behavior_;
  std::unique_ptr<PasswordSyncBridge> password_sync_bridge_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whenever 'password_sync_bridge_' receive remote changes this callback is
  // used to notify PasswordStore observers about them. Called on a main
  // sequence from the 'NotifyLoginsChanged'.
  // The bool indicates that the received changes are for the account store.
  base::RepeatingCallback<void(std::optional<PasswordStoreChangeList>, bool)>
      remote_forms_changes_received_callback_
          GUARDED_BY_CONTEXT(sequence_checker_);

  UnsyncedCredentialsDeletionNotifier deletion_notifier_;

  // A list of callbacks that should be run once all pending deletions have been
  // sent to the Sync server. Note that the vector itself lives on the
  // background thread, but the callbacks must be run on the main thread!
  std::vector<base::OnceCallback<void(bool)>> deletions_have_synced_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timeout closure that runs if sync takes too long to propagate deletions.
  base::CancelableOnceClosure deletions_have_synced_timeout_
      GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<LoginDatabaseAsyncHelper> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_LOGIN_DATABASE_ASYNC_HELPER_H_
