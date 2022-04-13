// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_built_in_backend.h"

#include "base/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/login_database_async_helper.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

PasswordStoreBuiltInBackend::PasswordStoreBuiltInBackend(
    std::unique_ptr<LoginDatabase> login_db,
    std::unique_ptr<UnsyncedCredentialsDeletionNotifier> notifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
  helper_ = std::make_unique<LoginDatabaseAsyncHelper>(
      std::move(login_db), std::move(notifier),
      base::SequencedTaskRunnerHandle::Get());
}

PasswordStoreBuiltInBackend::~PasswordStoreBuiltInBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PasswordStoreBuiltInBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (helper_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(helper_));
    std::move(shutdown_completed).Run();
  }
}

void PasswordStoreBuiltInBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::Initialize,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          std::move(remote_form_changes_received),
          std::move(sync_enabled_or_disabled_cb)),
      std::move(completion));
}

void PasswordStoreBuiltInBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix("BuiltInBackend"),
                                          MetricInfix("GetAllLoginsAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::GetAllLogins,
                     base::Unretained(helper_.get()),
                     std::move(metrics_recorder)),  // Safe until `Shutdown()`.
      std::move(callback));
}

void PasswordStoreBuiltInBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(
          BackendInfix("BuiltInBackend"),
          MetricInfix("GetAutofillableLoginsAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::GetAutofillableLogins,
                     base::Unretained(helper_.get()),
                     std::move(metrics_recorder)),  // Safe until `Shutdown()`.
      std::move(callback));
}

void PasswordStoreBuiltInBackend::GetAllLoginsForAccountAsync(
    absl::optional<std::string> account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void PasswordStoreBuiltInBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(
          BackendInfix("BuiltInBackend"),
          MetricInfix("FillMatchingLoginsAsync"));
  if (forms.empty()) {
    std::move(callback).Run({});
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::FillMatchingLogins,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          forms, include_psl, std::move(metrics_recorder)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix("BuiltInBackend"),
                                          MetricInfix("AddLoginAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::AddLogin,
                     base::Unretained(helper_.get()), form,
                     std::move(metrics_recorder)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix("BuiltInBackend"),
                                          MetricInfix("UpdateLoginAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::UpdateLogin,
                     base::Unretained(helper_.get()), form,
                     std::move(metrics_recorder)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(BackendInfix("BuiltInBackend"),
                                          MetricInfix("RemoveLoginAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLogin,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          form, std::move(metrics_recorder)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(
          BackendInfix("BuiltInBackend"),
          MetricInfix("RemoveLoginsCreatedBetweenAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLoginsCreatedBetween,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          delete_begin, delete_end, std::move(metrics_recorder)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  PasswordStoreBackendMetricsRecorder metrics_recorder =
      PasswordStoreBackendMetricsRecorder(
          BackendInfix("BuiltInBackend"),
          MetricInfix("RemoveLoginsByURLAndTimeAsync"));
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLoginsByURLAndTime,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          url_filter, delete_begin, delete_end, std::move(sync_completion),
          std::move(metrics_recorder)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &LoginDatabaseAsyncHelper::DisableAutoSignInForOrigins),
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          origin_filter),
      std::move(completion));
}

SmartBubbleStatsStore* PasswordStoreBuiltInBackend::GetSmartBubbleStatsStore() {
  return this;
}

FieldInfoStore* PasswordStoreBuiltInBackend::GetFieldInfoStore() {
  return this;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreBuiltInBackend::CreateSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  // Note that a callback is bound for
  // GetSyncControllerDelegate() because this getter itself
  // must also run in the backend sequence, and the proxy object below will take
  // care of that.
  // Since the controller delegate can (only in theory) invoke the factory after
  // `Shutdown` was called, it only returns nullptr then to prevent a UAF.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      background_task_runner_,
      base::BindRepeating(&LoginDatabaseAsyncHelper::GetSyncControllerDelegate,
                          base::Unretained(helper_.get())));
}

void PasswordStoreBuiltInBackend::ClearAllLocalPasswords() {
  NOTREACHED();
}

void PasswordStoreBuiltInBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {}

void PasswordStoreBuiltInBackend::AddSiteStats(const InteractionsStats& stats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LoginDatabaseAsyncHelper::AddSiteStats,
                                base::Unretained(helper_.get()), stats));
}

void PasswordStoreBuiltInBackend::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::RemoveSiteStats,
                     base::Unretained(helper_.get()), origin_domain));
}

void PasswordStoreBuiltInBackend::GetSiteStats(
    const GURL& origin_domain,
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::GetSiteStats,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          origin_domain),
      base::BindOnce(&PasswordStoreConsumer::OnGetSiteStatistics, consumer));
}

void PasswordStoreBuiltInBackend::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::RemoveStatisticsByOriginAndTime,
                     base::Unretained(helper_.get()), origin_filter,
                     delete_begin, delete_end),
      std::move(completion));
}

void PasswordStoreBuiltInBackend::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LoginDatabaseAsyncHelper::AddFieldInfo,
                                base::Unretained(helper_.get()), field_info));
}

void PasswordStoreBuiltInBackend::GetAllFieldInfo(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::GetAllFieldInfo,
          base::Unretained(helper_.get())),  // Safe until `Shutdown()`.
      base::BindOnce(&PasswordStoreConsumer::OnGetAllFieldInfo, consumer));
}

void PasswordStoreBuiltInBackend::RemoveFieldInfoByTime(
    base::Time remove_begin,
    base::Time remove_end,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::RemoveFieldInfoByTime,
                     base::Unretained(helper_.get()), remove_begin, remove_end),
      std::move(completion));
}

}  // namespace password_manager
