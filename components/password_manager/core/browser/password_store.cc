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
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/android_affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

bool FormSupportsPSL(const PasswordFormDigest& digest) {
  return digest.scheme == PasswordForm::Scheme::kHtml &&
         !GetRegistryControlledDomain(GURL(digest.signon_realm)).empty();
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

PasswordStore::PasswordStore(std::unique_ptr<PasswordStoreBackend> backend)
    : PasswordStore() {
  backend_deleter_ = std::move(backend);
  backend_ = backend_deleter_.get();
}

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
  backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                             std::move(completion));
}

void PasswordStore::Unblocklist(const PasswordFormDigest& form_digest,
                                base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  backend_->FillMatchingLoginsAsync(
      base::BindOnce(&PasswordStore::UnblocklistInternal, this,
                     std::move(completion)),
      FormSupportsPSL(form_digest), {form_digest});
}

void PasswordStore::GetLogins(const PasswordFormDigest& form,
                              PasswordStoreConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "PasswordStore::GetLogins",
                                    consumer);

  scoped_refptr<GetLoginsWithAffiliationsRequestHandler> request_handler =
      new GetLoginsWithAffiliationsRequestHandler(form, consumer->GetWeakPtr(),
                                                  /*store=*/this);

  if (affiliated_match_helper_) {
    auto branding_injection_for_affiliations_callback =
        base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                       this, request_handler->AffiliatedLoginsClosure());
    // The backend *is* the password_store and can therefore be passed with
    // base::Unretained.
    affiliated_match_helper_->GetAffiliatedAndroidAndWebRealms(
        form, request_handler->AffiliationsClosure().Then(base::BindOnce(
                  &PasswordStoreBackend::FillMatchingLoginsAsync,
                  base::Unretained(backend_),
                  std::move(branding_injection_for_affiliations_callback),
                  /*include_psl=*/false)));
  } else {
    request_handler->AffiliatedLoginsClosure().Run({});
  }

  auto branding_injection_callback =
      base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                     this, request_handler->LoginsForFormClosure());

  backend_->FillMatchingLoginsAsync(std::move(branding_injection_callback),
                                    FormSupportsPSL(form), {form});
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
  return backend_->GetSmartBubbleStatsStore();
}

FieldInfoStore* PasswordStore::GetFieldInfoStore() {
  return backend_->GetFieldInfoStore();
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
  return backend_->CreateSyncControllerDelegateFactory();
}

PasswordStoreBackend* PasswordStore::GetBackendForTesting() {
  return backend_;
}

PasswordStore::~PasswordStore() {
  DCHECK(shutdown_called_);
}

scoped_refptr<base::SequencedTaskRunner>
PasswordStore::CreateBackgroundTaskRunner() const {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

void PasswordStore::ReportMetricsImpl(const std::string& sync_username,
                                      bool custom_passphrase_sync_enabled,
                                      BulkCheckDone bulk_check_done) {
  // TODO(crbug.com/1217070): Move as implementation detail into backend.
  LOG(ERROR) << "Called function without implementation: " << __func__;
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

void PasswordStore::InjectAffiliationAndBrandingInformation(
    LoginsReply callback,
    LoginsResult forms) {
  if (affiliated_match_helper_ && !forms.empty()) {
    affiliated_match_helper_->InjectAffiliationAndBrandingInformation(
        std::move(forms), AndroidAffiliationService::StrategyOnCacheMiss::FAIL,
        std::move(callback));
  } else {
    std::move(callback).Run(std::move(forms));
  }
}

}  // namespace password_manager
