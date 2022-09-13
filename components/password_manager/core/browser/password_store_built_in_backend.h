// Copyright 2014 The Chromium Authors
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
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/smart_bubble_stats_store.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {
class SyncService;
}  // namespace syncer

namespace password_manager {

class LoginDatabase;
class LoginDatabaseAsyncHelper;
class UnsyncedCredentialsDeletionNotifier;

struct FieldInfo;

// Simple password store implementation that delegates everything to
// the LoginDatabaseAsyncHelper. Works only on the main sequence.
class PasswordStoreBuiltInBackend : public PasswordStoreBackend,
                                    public SmartBubbleStatsStore,
                                    protected FieldInfoStore {
 public:
  // The |login_db| must not have been Init()-ed yet. It will be initialized in
  // a deferred manner on the background sequence.
  PasswordStoreBuiltInBackend(
      std::unique_ptr<LoginDatabase> login_db,
      std::unique_ptr<UnsyncedCredentialsDeletionNotifier> notifier = nullptr);

  ~PasswordStoreBuiltInBackend() override;

 private:
  // Implements PasswordStoreBackend interface.
  void InitBackend(RemoteChangesReceived remote_form_changes_received,
                   base::RepeatingClosure sync_enabled_or_disabled_cb,
                   base::OnceCallback<void(bool)> completion) override;
  void Shutdown(base::OnceClosure shutdown_completed) override;
  void GetAllLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAutofillableLoginsAsync(LoginsOrErrorReply callback) override;
  void GetAllLoginsForAccountAsync(absl::optional<std::string> account,
                                   LoginsOrErrorReply callback) override;
  void FillMatchingLoginsAsync(
      LoginsOrErrorReply callback,
      bool include_psl,
      const std::vector<PasswordFormDigest>& forms) override;
  void AddLoginAsync(const PasswordForm& form,
                     PasswordChangesOrErrorReply callback) override;
  void UpdateLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginAsync(const PasswordForm& form,
                        PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsCreatedBetweenAsync(
      base::Time delete_begin,
      base::Time delete_end,
      PasswordChangesOrErrorReply callback) override;
  void RemoveLoginsByURLAndTimeAsync(
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end,
      base::OnceCallback<void(bool)> sync_completion,
      PasswordChangesOrErrorReply callback) override;
  void DisableAutoSignInForOriginsAsync(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::OnceClosure completion) override;
  SmartBubbleStatsStore* GetSmartBubbleStatsStore() override;
  FieldInfoStore* GetFieldInfoStore() override;
  std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
  CreateSyncControllerDelegate() override;
  void ClearAllLocalPasswords() override;
  void OnSyncServiceInitialized(syncer::SyncService* sync_service) override;

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

  // Ensures that all methods are called on the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // The helper, owned by this backend instance, but
  // living on the |background_task_runner_|. It will be deleted asynchronously
  // during shutdown on the |background_task_runner_|, so it will outlive |this|
  // along with all its in-flight tasks.
  std::unique_ptr<LoginDatabaseAsyncHelper> helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // TaskRunner for all the background operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BUILT_IN_BACKEND_H_
