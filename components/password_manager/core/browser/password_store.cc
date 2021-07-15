// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/insecure_credentials_consumer.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

void CloseTraceAndCallBack(
    const char* trace_name,
    PasswordStoreConsumer* consumer,
    base::OnceCallback<void(std::vector<std::unique_ptr<PasswordForm>>)>
        callback,
    std::vector<std::unique_ptr<PasswordForm>> results) {
  TRACE_EVENT_NESTABLE_ASYNC_END0("passwords", trace_name, consumer);

  std::move(callback).Run(std::move(results));
}

std::vector<PasswordFormDigest> ConvertToForms(
    const std::vector<std::string>& realms) {
  std::vector<PasswordFormDigest> forms;
  for (const auto& realm : realms)
    forms.emplace_back(PasswordForm::Scheme::kHtml, realm, GURL(realm));
  return forms;
}

// Helper function which invokes |notifying_callback| and |completion_callback|
// when changes are received.
void InvokeCallbackOnChanges(
    base::OnceCallback<void(const PasswordStoreChangeList& changes)>
        notifying_callback,
    base::OnceCallback<void(bool)> completion_callback,
    const PasswordStoreChangeList& changes) {
  DCHECK(notifying_callback);
  std::move(notifying_callback).Run(changes);
  if (completion_callback)
    std::move(completion_callback).Run(!changes.empty());
}

// Helper object which aggregates results from multiple operations and invokes
// completion callback when all the operations are finished.
class OperationHandler {
 public:
  static OperationHandler* CreateOperationHandler() {
    return new OperationHandler();
  }

  void AwaitOperation(
      base::OnceCallback<void(PasswordStoreChangeListReply)> operation) {
    std::move(operation).Run(
        base::BindOnce(&OperationHandler::OnPasswordStoreChangesReceived,
                       base::Unretained(this)));
    operations_++;
  }

  // After |InvokeOnCompletion| was called the object shouldn't be used.
  void InvokeOnCompletion(PasswordStoreChangeListReply callback) {
    DCHECK_NE(0, operations_);
    changes_received_ = base::BarrierClosure(
        operations_, base::BindOnce(&OperationHandler::OnAllOperationsFinished,
                                    base::Owned(this), std::move(callback)));
  }

 private:
  OperationHandler() = default;

  void OnPasswordStoreChangesReceived(const PasswordStoreChangeList& changes) {
    operations_--;
    changes_.insert(changes_.end(), changes.begin(), changes.end());
    if (changes_received_)
      changes_received_.Run();
  }

  void OnAllOperationsFinished(PasswordStoreChangeListReply callback) {
    std::move(callback).Run(changes_);
  }

  PasswordStoreChangeList changes_;

  base::RepeatingClosure changes_received_;

  int operations_ = 0;
};

}  // namespace

PasswordStore::PasswordStore() = default;

bool PasswordStore::Init(PrefService* prefs,
                         base::RepeatingClosure sync_enabled_or_disabled_cb) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);
  background_task_runner_ = CreateBackgroundTaskRunner();
  DCHECK(background_task_runner_);
  prefs_ = prefs;

  // TODO(crbug.bom/1226042): Backend might be null in tests, remove this after
  // tests switch to MockPasswordStoreInterface.
  if (background_task_runner_ && backend_) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "passwords", "PasswordStore::InitOnBackgroundSequence", this);
    backend_->InitBackend(
        base::BindRepeating(&PasswordStore::NotifyLoginsChangedOnMainSequence,
                            this),
        std::move(sync_enabled_or_disabled_cb),
        base::BindOnce(&PasswordStore::OnInitCompleted, this));
  }

  return true;
}

void PasswordStore::SetAffiliatedMatchHelper(
    std::unique_ptr<AffiliatedMatchHelper> helper) {
  affiliated_match_helper_ = std::move(helper);
}

void PasswordStore::AddLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  backend_->AddLoginAsync(
      form,
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  backend_->UpdateLoginAsync(
      form,
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this));
}

void PasswordStore::UpdateLoginWithPrimaryKey(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PasswordForm new_form_with_correct_password_issues = new_form;
  // TODO(crbug.com/1223022): Re-evaluate this once all places that call
  // UpdateLoginWithPrimaryKey() have properly set the |password_issues|
  // field.
  if (new_form.username_value != old_primary_key.username_value ||
      new_form.password_value != old_primary_key.password_value) {
    // If the password or the username changes, the password issues aren't valid
    // any more. Make sure they are cleared before storing the new form.
    new_form_with_correct_password_issues.password_issues =
        base::flat_map<InsecureType, InsecurityMetadata>();
  }

  OperationHandler* handler = OperationHandler::CreateOperationHandler();
  handler->AwaitOperation(
      base::BindOnce(&PasswordStoreBackend::RemoveLoginAsync,
                     base::Unretained(backend_), old_primary_key));
  handler->AwaitOperation(base::BindOnce(
      &PasswordStoreBackend::AddLoginAsync, base::Unretained(backend_),
      new_form_with_correct_password_issues));
  handler->InvokeOnCompletion(
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this));
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  backend_->RemoveLoginAsync(
      form,
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this));
}

void PasswordStore::RemoveLoginsByURLAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion,
    base::OnceCallback<void(bool)> sync_completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  backend_->RemoveLoginsByURLAndTimeAsync(
      url_filter, delete_begin, delete_end, std::move(sync_completion),
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this)
          .Then(std::move(completion)));
}

void PasswordStore::RemoveLoginsCreatedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback =
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this);
  backend_->RemoveLoginsCreatedBetweenAsync(
      delete_begin, delete_end,
      base::BindOnce(&InvokeCallbackOnChanges, std::move(callback),
                     std::move(completion)));
}

void PasswordStore::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::DisableAutoSignInForOriginsInternal, this,
                     base::RepeatingCallback<bool(const GURL&)>(origin_filter),
                     std::move(completion)));
}

void PasswordStore::Unblocklist(const PasswordFormDigest& form_digest,
                                base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  backend_->FillMatchingLoginsAsync(
      base::BindOnce(&PasswordStore::UnblocklistInternal, this,
                     std::move(completion)),
      {form_digest});
}

void PasswordStore::GetLogins(const PasswordFormDigest& form,
                              PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "PasswordStore::GetLogins",
                                    consumer);

  scoped_refptr<GetLoginsWithAffiliationsRequestHandler> request_handler =
      new GetLoginsWithAffiliationsRequestHandler(consumer->GetWeakPtr(), this);

  if (affiliated_match_helper_) {
    // The backend *is* the password_store and can therefore be passed with
    // base::Unretained.
    affiliated_match_helper_->GetAffiliatedAndroidAndWebRealms(
        form,
        base::BindOnce(ConvertToForms)
            .Then(base::BindOnce(&PasswordStoreBackend::FillMatchingLoginsAsync,
                                 base::Unretained(backend_),
                                 request_handler->AffiliatedLoginsClosure())));
  } else {
    request_handler->AffiliatedLoginsClosure().Run({});
  }

  backend_->FillMatchingLoginsAsync(request_handler->LoginsForFormClosure(),
                                    {form});
}

void PasswordStore::GetLoginsByPassword(
    const std::u16string& plain_text_password,
    PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostLoginsTaskAndReplyToConsumerWithResult(
      consumer, base::BindOnce(&PasswordStore::GetLoginsByPasswordImpl, this,
                               plain_text_password));
}

void PasswordStore::GetAutofillableLogins(PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  backend_->GetAutofillableLoginsAsync(
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
}

void PasswordStore::GetAllLogins(PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  backend_->GetAllLoginsAsync(
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
}

void PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation(
    PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  auto consumer_reply =
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this));

  auto affiliation_injection =
      base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                     this, std::move(consumer_reply));

  backend_->GetAllLoginsAsync(std::move(affiliation_injection));
}

SmartBubbleStatsStore* PasswordStore::GetSmartBubbleStatsStore() {
  return this;
}

void PasswordStore::ReportMetrics(const std::string& sync_username,
                                  bool custom_passphrase_sync_enabled,
                                  bool is_under_advanced_protection) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (background_task_runner_) {
    base::OnceClosure task = base::BindOnce(
        &PasswordStore::ReportMetricsImpl, this, sync_username,
        custom_passphrase_sync_enabled,
        BulkCheckDone(prefs_ && prefs_->HasPrefPath(
                                    prefs::kLastTimePasswordCheckCompleted)));
    background_task_runner_->PostDelayedTask(FROM_HERE, std::move(task),
                                             base::TimeDelta::FromSeconds(30));
  }
}
void PasswordStore::AddInsecureCredential(
    const InsecureCredential& insecure_credential) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback = base::BindOnce(&PasswordStore::AddInsecureCredentialImpl,
                                 this, insecure_credential);
  ScheduleTask(base::BindOnce(
      &PasswordStore::InvokeAndNotifyAboutInsecureCredentialsChange, this,
      std::move(callback)));
}

void PasswordStore::RemoveInsecureCredentials(
    const std::string& signon_realm,
    const std::u16string& username,
    RemoveInsecureCredentialsReason reason) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto callback = base::BindOnce(&PasswordStore::RemoveInsecureCredentialsImpl,
                                 this, signon_realm, username, reason);
  ScheduleTask(base::BindOnce(
      &PasswordStore::InvokeAndNotifyAboutInsecureCredentialsChange, this,
      std::move(callback)));
}

void PasswordStore::GetAllInsecureCredentials(
    InsecureCredentialsConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostInsecureCredentialsTaskAndReplyToConsumerWithResult(
      consumer,
      base::BindOnce(&PasswordStore::GetAllInsecureCredentialsImpl, this));
}

void PasswordStore::GetMatchingInsecureCredentials(
    const std::string& signon_realm,
    InsecureCredentialsConsumer* consumer) {
  if (affiliated_match_helper_) {
    PasswordFormDigest form(PasswordForm::Scheme::kHtml, signon_realm,
                            GURL(signon_realm));
    affiliated_match_helper_->GetAffiliatedAndroidAndWebRealms(
        form,
        base::BindOnce(
            &PasswordStore::ScheduleGetInsecureCredentialsWithAffiliations,
            this, consumer->GetWeakPtr(), signon_realm));
  } else {
    PostInsecureCredentialsTaskAndReplyToConsumerWithResult(
        consumer,
        base::BindOnce(&PasswordStore::GetMatchingInsecureCredentialsImpl, this,
                       signon_realm));
  }
}

void PasswordStore::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::AddFieldInfoImpl, this, field_info));
}

void PasswordStore::GetAllFieldInfo(PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  auto get_all_field_info_task =
      base::BindOnce(&PasswordStore::GetAllFieldInfoImpl, this);
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      std::move(get_all_field_info_task),
      base::BindOnce(&PasswordStoreConsumer::OnGetAllFieldInfo,
                     consumer->GetWeakPtr()));
}

void PasswordStore::RemoveFieldInfoByTime(base::Time remove_begin,
                                          base::Time remove_end,
                                          base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::RemoveFieldInfoByTimeInternal,
                              this, remove_begin, remove_end,
                              std::move(completion)));
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PasswordStore::ScheduleTask(base::OnceClosure task) {
  return background_task_runner_ &&
         background_task_runner_->PostTask(FROM_HERE, std::move(task));
}

bool PasswordStore::IsAbleToSavePasswords() const {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  return init_status_ == InitStatus::kSuccess;
}

void PasswordStore::ShutdownOnUIThread() {
  // The AffiliationService must be destroyed from the main sequence.
  affiliated_match_helper_.reset();
  shutdown_called_ = true;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStore::CreateSyncControllerDelegate() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // Note that a callback is bound for
  // GetSyncControllerDelegateOnBackgroundSequence() because this getter itself
  // must also run in the backend sequence, and the proxy object below will take
  // care of that.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      background_task_runner_,
      base::BindRepeating(
          &PasswordStore::GetSyncControllerDelegateOnBackgroundSequence,
          base::Unretained(this)));
}

PasswordStore::~PasswordStore() {
  DCHECK(shutdown_called_);
}

void PasswordStore::AddSiteStats(const InteractionsStats& stats) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(&PasswordStore::AddSiteStatsImpl, this, stats));
}

void PasswordStore::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(
      base::BindOnce(&PasswordStore::RemoveSiteStatsImpl, this, origin_domain));
}

void PasswordStore::GetSiteStats(const GURL& origin_domain,
                                 PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PostStatsTaskAndReplyToConsumerWithResult(
      consumer,
      base::BindOnce(&PasswordStore::GetSiteStatsImpl, this, origin_domain));
}

void PasswordStore::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  ScheduleTask(base::BindOnce(
      &PasswordStore::RemoveStatisticsByOriginAndTimeInternal, this,
      origin_filter, delete_begin, delete_end, std::move(completion)));
}

scoped_refptr<base::SequencedTaskRunner>
PasswordStore::CreateBackgroundTaskRunner() const {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

void PasswordStore::InvokeAndNotifyAboutInsecureCredentialsChange(
    base::OnceCallback<PasswordStoreChangeList()> callback) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes = std::move(callback).Run();
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this,
                     changes));
}

void PasswordStore::OnInitCompleted(bool success) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  init_status_ = success ? InitStatus::kSuccess : InitStatus::kFailure;

  base::UmaHistogramBoolean("PasswordManager.PasswordStoreInitResult", success);
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "passwords", "PasswordStore::InitOnBackgroundSequence", this);
}

void PasswordStore::NotifyLoginsChangedOnMainSequence(
    const PasswordStoreChangeList& changes) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (changes.empty())
    return;

  for (auto& observer : observers_) {
    observer.OnLoginsChanged(this, changes);
  }
}

void PasswordStore::PostLoginsTaskAndReplyToConsumerWithResult(
    PasswordStoreConsumer* consumer,
    LoginsTask task) {
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
}

void PasswordStore::PostLoginsTaskAndReplyToConsumerWithProcessedResult(
    const char* trace_name,
    PasswordStoreConsumer* consumer,
    LoginsTask task,
    LoginsResultProcessor processor) {
  auto call_consumer = base::BindOnce(
      CloseTraceAndCallBack, trace_name, consumer,
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(std::move(processor), std::move(call_consumer)));
}

void PasswordStore::PostStatsTaskAndReplyToConsumerWithResult(
    PasswordStoreConsumer* consumer,
    StatsTask task) {
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(&PasswordStoreConsumer::OnGetSiteStatistics,
                     consumer->GetWeakPtr()));
}

void PasswordStore::PostInsecureCredentialsTaskAndReplyToConsumerWithResult(
    InsecureCredentialsConsumer* consumer,
    InsecureCredentialsTask task) {
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE, std::move(task),
      base::BindOnce(&InsecureCredentialsConsumer::OnGetInsecureCredentialsFrom,
                     consumer->GetWeakPtr(), base::RetainedRef(this)));
}

void PasswordStore::RemoveStatisticsByOriginAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords",
               "PasswordStore::RemoveStatisticsByOriginAndTimeInternal");
  RemoveStatisticsByOriginAndTimeImpl(origin_filter, delete_begin, delete_end);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void PasswordStore::DisableAutoSignInForOriginsInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords",
               "PasswordStore::DisableAutoSignInForOriginsInternal");
  DisableAutoSignInForOriginsImpl(origin_filter);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

void PasswordStore::UnblocklistInternal(
    base::OnceClosure completion,
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::UnblocklistInternal");

  std::vector<PasswordForm> forms_to_remove;
  for (auto& form : forms) {
    // Ignore PSL matches for blocked entries.
    if (form->blocked_by_user && !form->is_public_suffix_match)
      forms_to_remove.push_back(std::move(*form));
  }

  if (forms_to_remove.empty()) {
    if (completion)
      std::move(completion).Run();
    return;
  }

  OperationHandler* handler = OperationHandler::CreateOperationHandler();

  for (const auto& form : forms_to_remove) {
    handler->AwaitOperation(
        base::BindOnce(&PasswordStoreBackend::RemoveLoginAsync,
                       base::Unretained(backend_), form));
  }

  auto notify_callback =
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this);
  if (completion)
    notify_callback = std::move(notify_callback).Then(std::move(completion));

  handler->InvokeOnCompletion(std::move(notify_callback));
}

void PasswordStore::RemoveFieldInfoByTimeInternal(
    base::Time remove_begin,
    base::Time remove_end,
    base::OnceClosure completion) {
  RemoveFieldInfoByTimeImpl(remove_begin, remove_end);
  if (completion)
    main_task_runner_->PostTask(FROM_HERE, std::move(completion));
}

std::vector<std::unique_ptr<PasswordForm>> PasswordStore::GetLoginsImpl(
    const PasswordFormDigest& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return FillMatchingLogins(form);
}

std::vector<std::unique_ptr<PasswordForm>>
PasswordStore::GetLoginsByPasswordImpl(
    const std::u16string& plain_text_password) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT0("passwords", "PasswordStore::GetLoginsByPasswordImpl");
  return FillMatchingLoginsByPassword(plain_text_password);
}

std::vector<InsecureCredential>
PasswordStore::GetInsecureCredentialsWithAffiliationsImpl(
    const std::string& signon_realm,
    const std::vector<std::string>& additional_affiliated_realms) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<InsecureCredential> results(
      GetMatchingInsecureCredentialsImpl(signon_realm));
  for (const std::string& realm : additional_affiliated_realms) {
    std::vector<InsecureCredential> more_results(
        GetMatchingInsecureCredentialsImpl(realm));
    results.insert(results.end(), std::make_move_iterator(more_results.begin()),
                   std::make_move_iterator(more_results.end()));
  }

  return results;
}

void PasswordStore::InjectAffiliationAndBrandingInformation(
    LoginsReply callback,
    LoginsResult forms) {
  if (affiliated_match_helper_) {
    affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
        std::move(forms), AndroidAffiliationService::StrategyOnCacheMiss::FAIL,
        std::move(callback));
  } else {
    std::move(callback).Run(std::move(forms));
  }
}

void PasswordStore::ScheduleGetInsecureCredentialsWithAffiliations(
    base::WeakPtr<InsecureCredentialsConsumer> consumer,
    const std::string& signon_realm,
    const std::vector<std::string>& additional_affiliated_realms) {
  if (consumer) {
    PostInsecureCredentialsTaskAndReplyToConsumerWithResult(
        consumer.get(),
        base::BindOnce(
            &PasswordStore::GetInsecureCredentialsWithAffiliationsImpl, this,
            signon_realm, additional_affiliated_realms));
  }
}

std::unique_ptr<PasswordForm> PasswordStore::GetLoginImpl(
    const PasswordForm& primary_key) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  std::vector<std::unique_ptr<PasswordForm>> candidates(
      FillMatchingLogins(PasswordFormDigest(primary_key)));
  for (auto& candidate : candidates) {
    if (ArePasswordFormUniqueKeysEqual(*candidate, primary_key) &&
        !candidate->is_public_suffix_match) {
      return std::move(candidate);
    }
  }
  return nullptr;
}

}  // namespace password_manager
