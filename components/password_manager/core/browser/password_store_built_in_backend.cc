// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_built_in_backend.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/login_database_async_helper.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_backend_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_util.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

// Template function to create a callback which accepts LoginsResultOrError or
// PasswordChangesOrError as a result.
template <typename Result>
base::OnceCallback<Result(Result)> ReportMetricsForResultCallback(
    MetricInfix infix) {
  PasswordStoreBackendMetricsRecorder metrics_reporter(
      BackendInfix("BuiltInBackend"), infix);
  return base::BindOnce(
      [](PasswordStoreBackendMetricsRecorder reporter,
         Result result) -> Result {
        if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
          reporter.RecordMetrics(SuccessStatus::kError,
                                 absl::get<PasswordStoreBackendError>(result));
        } else {
          reporter.RecordMetrics(SuccessStatus::kSuccess, absl::nullopt);
        }
        return result;
      },
      std::move(metrics_reporter));
}

}  // namespace

PasswordStoreBuiltInBackend::PasswordStoreBuiltInBackend(
    std::unique_ptr<LoginDatabase> login_db,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior,
    std::unique_ptr<UnsyncedCredentialsDeletionNotifier> notifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
  helper_ = std::make_unique<LoginDatabaseAsyncHelper>(
      std::move(login_db), std::move(notifier),
      base::SequencedTaskRunner::GetCurrentDefault(),
      wipe_model_upon_sync_disabled_behavior);
}

PasswordStoreBuiltInBackend::~PasswordStoreBuiltInBackend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PasswordStoreBuiltInBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  affiliated_match_helper_ = nullptr;
  if (helper_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(helper_));
    std::move(shutdown_completed).Run();
  }
}

void PasswordStoreBuiltInBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  affiliated_match_helper_ = affiliated_match_helper;
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
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::GetAllLogins,
          base::Unretained(helper_.get())),  // Safe until `Shutdown()`.
      ReportMetricsForResultCallback<LoginsResultOrError>(
          MetricInfix("GetAllLoginsAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::GetAllLoginsWithAffiliationAndBrandingAsync(
    LoginsOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(helper_);
  auto affiliation_injection = base::BindOnce(
      &PasswordStoreBuiltInBackend::InjectAffiliationAndBrandingInformation,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  GetAllLoginsAsync(std::move(affiliation_injection));
}

void PasswordStoreBuiltInBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::GetAutofillableLogins,
          base::Unretained(helper_.get())),  // Safe until `Shutdown()`.
      ReportMetricsForResultCallback<LoginsResultOrError>(
          MetricInfix("GetAutofillableLoginsAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::GetAllLoginsForAccountAsync(
    absl::optional<std::string> account,
    LoginsOrErrorReply callback) {
  NOTREACHED();
}

void PasswordStoreBuiltInBackend::FillMatchingLoginsAsync(
    LoginsOrErrorReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  if (forms.empty()) {
    std::move(callback).Run(LoginsResult());
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::FillMatchingLogins,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          forms, include_psl),
      ReportMetricsForResultCallback<LoginsResultOrError>(
          MetricInfix("FillMatchingLoginsAsync"))
          .Then(base::BindOnce(&GetLoginsOrEmptyListOnFailure))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::GetGroupedMatchingLoginsAsync(
    const PasswordFormDigest& form_digest,
    LoginsOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);

  GetLoginsWithAffiliationsRequestHandler(
      form_digest, this, affiliated_match_helper_.get(), std::move(callback));
}

void PasswordStoreBuiltInBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::AddLogin,
                     base::Unretained(helper_.get()), form),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MetricInfix("AddLoginAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LoginDatabaseAsyncHelper::UpdateLogin,
                     base::Unretained(helper_.get()), form),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MetricInfix("UpdateLoginAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLogin,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          form),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MetricInfix("RemoveLoginAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordChangesOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLoginsCreatedBetween,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          delete_begin, delete_end),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MetricInfix("RemoveLoginsCreatedBetweenAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordChangesOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLoginsByURLAndTime,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          url_filter, delete_begin, delete_end, std::move(sync_completion)),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MetricInfix("RemoveLoginsByURLAndTimeAsync"))
          .Then(std::move(callback)));
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

void PasswordStoreBuiltInBackend::InjectAffiliationAndBrandingInformation(
    LoginsOrErrorReply callback,
    LoginsResultOrError forms_or_error) {
  if (!affiliated_match_helper_ ||
      absl::holds_alternative<PasswordStoreBackendError>(forms_or_error) ||
      absl::get<LoginsResult>(forms_or_error).empty()) {
    std::move(callback).Run(std::move(forms_or_error));
    return;
  }
  affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
      std::move(absl::get<LoginsResult>(forms_or_error)), std::move(callback));
}

}  // namespace password_manager
