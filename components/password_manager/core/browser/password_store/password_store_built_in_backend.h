// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BUILT_IN_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BUILT_IN_BACKEND_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/actionable_error.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "components/sync/service/sync_service_observer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {
class SyncService;
}  // namespace syncer

namespace os_crypt_async {
class OSCryptAsync;
class Encryptor;
}  // namespace os_crypt_async

namespace password_manager {

class LoginDatabase;
class LoginDatabaseAsyncHelper;

// Simple password store implementation that delegates everything to
// the LoginDatabaseAsyncHelper. Works only on the main sequence.
class PasswordStoreBuiltInBackend : public PasswordStoreBackend,
                                    public SmartBubbleStatsStore,
                                    public syncer::SyncServiceObserver {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background sequence.
  PasswordStoreBuiltInBackend(std::unique_ptr<LoginDatabase> login_db,
                              syncer::WipeModelUponSyncDisabledBehavior
                                  wipe_model_upon_sync_disabled_behavior,
                              PrefService* prefs,
                              os_crypt_async::OSCryptAsync* os_crypt_async);

  ~PasswordStoreBuiltInBackend() override;

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  void NotifyCredentialsChangedForTesting(
      base::PassKey<class PasswordStoreBuiltInBackendPasswordLossMetricsTest>,
      const PasswordStoreChangeList& changes);

 private:
  // Implements PasswordStoreBackend interface.
  void InitBackend(AffiliatedMatchHelper* affiliated_match_helper,
                   RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  ActionableError GetError() override;
  void GetAllLoginsAsync(BackendLoginsOrErrorReply callback) override;
  void GetAllLoginsWithAffiliationAndBrandingAsync(
      BackendLoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(BackendLoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      BackendLoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void GetGroupedMatchingLoginsAsync(
      const PasswordFormDigest& form_digest,
      BackendLoginsOrErrorReply callback) override;
  void AddLoginAsync(StoredCredential cred,
                     PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(StoredCredential cred,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(const base::Location& location,
                        StoredCredential cred,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      const base::Location& location,
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;
  base::WeakPtr<PasswordStoreBackend> AsWeakPtr() override;

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

  // If |forms_or_error| contains forms, it retrieves and fills in affiliation
  // and branding information for Android credentials in the forms and invokes
  // |callback| with the result. If an error was received instead, it directly
  // invokes |callback| with it, as no forms could be fetched. Called on
  // the main sequence.
  void InjectAffiliationAndBrandingInformation(
      BackendLoginsOrErrorReply callback,
      BackendLoginsResultOrError forms_or_error);

  void OnEncryptorReceived(RemoteChangesReceived remote_form_changes_received,
                           base::RepeatingClosure sync_enabled_or_disabled_cb,
                           base::OnceCallback<void(bool)> completion,
                           os_crypt_async::Encryptor encryptor);

  void WritePasswordRemovalReasonPrefs(IsAccountStore is_account_store);

  void OnInitComplete(base::OnceCallback<void(bool)> completion, bool result);

  // Sets the pref responsible for maintaining groups population in
  // the kClearUndecryptablePasswords experiment.
  // Records the passwords removal reason prefs.
  // TODO(b/40286735): Remove after this feature is launched.
  void SetClearingUndecryptablePasswordsIsEnabledPref(
      IsAccountStore is_account_store);

  // Ensures that all methods are called on the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // The helper, owned by this backend instance, but
  // living on the |background_task_runner_|. It will be deleted asynchronously
  // during shutdown on the |background_task_runner_|, so it will outlive |this|
  // along with all its in-flight tasks.
  std::unique_ptr<LoginDatabaseAsyncHelper> helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  raw_ptr<AffiliatedMatchHelper> affiliated_match_helper_;

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool is_database_initialized_successfully_ = false;

  // Used to get information if there are any passwords saved to the login db.
  raw_ptr<PrefService> pref_service_;

  raw_ptr<os_crypt_async::OSCryptAsync> const os_crypt_async_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Propagates potential password changes to observers.
  RemoteChangesReceived remote_form_changes_received_callback_;

  // Invoked whenever sync is enabled or disabled.
  base::RepeatingClosure sync_enabled_or_disabled_cb_;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_observation_{this};

  base::WeakPtrFactory<PasswordStoreBuiltInBackend> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_STORE_BUILT_IN_BACKEND_H_
