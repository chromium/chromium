// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/post_task.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
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
#include "components/password_manager/core/browser/password_store_util.h"
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
    base::OnceCallback<void(PasswordStoreChangeList changes)>
        notifying_callback,
    base::OnceCallback<void(bool)> completion_callback,
    PasswordStoreChangeList changes) {
  DCHECK(notifying_callback);
  bool is_change_empty = changes.empty();
  std::move(notifying_callback).Run(std::move(changes));
  if (completion_callback)
    std::move(completion_callback).Run(!is_change_empty);
}

LoginsResult GetLoginsOrEmptyListOnFailure(LoginsResultOrError result) {
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    return {};
  }
  return std::move(absl::get<LoginsResult>(result));
}

}  // namespace

PasswordStore::PasswordStore(std::unique_ptr<PasswordStoreBackend> backend) {
  backend_deleter_ = std::move(backend);
  backend_ = backend_deleter_.get();
}

bool PasswordStore::Init(
    PrefService* prefs,
    std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper,
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);
  prefs_ = prefs;
  affiliated_match_helper_ = std::move(affiliated_match_helper);

  // TODO(crbug.bom/1226042): Backend might be null in tests, remove this after
  // tests switch to MockPasswordStoreInterface.
  if (backend_) {
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

void PasswordStore::AddLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
  backend_->AddLoginAsync(
      form,
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this));
}

void PasswordStore::UpdateLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
  backend_->UpdateLoginAsync(
      form,
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this));
}

void PasswordStore::UpdateLoginWithPrimaryKey(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
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

  auto barrier_callback = base::BarrierCallback<PasswordStoreChangeList>(
      2, base::BindOnce(&JoinPasswordStoreChanges)
             .Then(base::BindOnce(
                 &PasswordStore::NotifyLoginsChangedOnMainSequence, this)));

  backend_->RemoveLoginAsync(old_primary_key, barrier_callback);
  backend_->AddLoginAsync(new_form_with_correct_password_issues,
                          barrier_callback);
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
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
  if (!backend_) {
    std::move(sync_completion).Run(false);
    return;  // Once the shutdown started, ignore new requests.
  }
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
  if (!backend_) {
    std::move(completion).Run(false);
    return;  // Once the shutdown started, ignore new requests.
  }
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
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
  backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                             std::move(completion));
}

void PasswordStore::Unblocklist(const PasswordFormDigest& form_digest,
                                base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
  backend_->FillMatchingLoginsAsync(
      base::BindOnce(&PasswordStore::UnblocklistInternal, this,
                     std::move(completion)),
      FormSupportsPSL(form_digest), {form_digest});
}

void PasswordStore::GetLogins(const PasswordFormDigest& form,
                              base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "PasswordStore::GetLogins",
                                    consumer.get());

  scoped_refptr<GetLoginsWithAffiliationsRequestHandler> request_handler =
      new GetLoginsWithAffiliationsRequestHandler(form, consumer,
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

void PasswordStore::GetAutofillableLogins(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.

  backend_->GetAutofillableLoginsAsync(
      base::BindOnce(&GetLoginsOrEmptyListOnFailure)
          .Then(base::BindOnce(
              &PasswordStoreConsumer::OnGetPasswordStoreResultsFrom, consumer,
              base::RetainedRef(this))));
}

void PasswordStore::GetAllLogins(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.

  backend_->GetAllLoginsAsync(
      base::BindOnce(&GetLoginsOrEmptyListOnFailure)
          .Then(base::BindOnce(
              &PasswordStoreConsumer::OnGetPasswordStoreResultsFrom, consumer,
              base::RetainedRef(this))));
}

void PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.

  auto consumer_reply =
      base::BindOnce(&PasswordStoreConsumer::OnGetPasswordStoreResultsFrom,
                     consumer, base::RetainedRef(this));

  auto affiliation_injection =
      base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                     this, std::move(consumer_reply));
  backend_->GetAllLoginsAsync(base::BindOnce(&GetLoginsOrEmptyListOnFailure)
                                  .Then(std::move(affiliation_injection)));
}

SmartBubbleStatsStore* PasswordStore::GetSmartBubbleStatsStore() {
  return backend_ ? backend_->GetSmartBubbleStatsStore() : nullptr;
}

FieldInfoStore* PasswordStore::GetFieldInfoStore() {
  return backend_ ? backend_->GetFieldInfoStore() : nullptr;
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PasswordStore::IsAbleToSavePasswords() const {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  return init_status_ == InitStatus::kSuccess && backend_;
}

void PasswordStore::ShutdownOnUIThread() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // The AffiliationService must be destroyed from the main sequence.
  affiliated_match_helper_.reset();
  if (backend_) {
    backend_->Shutdown(base::BindOnce(
        [](std::unique_ptr<PasswordStoreBackend> backend) { backend.reset(); },
        std::move(backend_deleter_)));
    backend_ = nullptr;
  }
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStore::CreateSyncControllerDelegate() {
  return backend_ ? backend_->CreateSyncControllerDelegate() : nullptr;
}

PasswordStoreBackend* PasswordStore::GetBackendForTesting() {
  return backend_;
}

PasswordStore::~PasswordStore() {
  DCHECK(!backend_) << "Shutdown() needs to be called before destruction!";
}

void PasswordStore::OnInitCompleted(bool success) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  init_status_ = success ? InitStatus::kSuccess : InitStatus::kFailure;

  base::UmaHistogramBoolean("PasswordManager.PasswordStoreInitResult", success);
  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "passwords", "PasswordStore::InitOnBackgroundSequence", this);

  if (affiliated_match_helper_)
    affiliated_match_helper_->Initialize(this);
}

void PasswordStore::NotifyLoginsChangedOnMainSequence(
    PasswordStoreChangeList changes) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (changes.empty())
    return;

  // Don't propagate reference to this store after its shutdown. No caller
  // should expect any notifications from a shut down store in any case.
  if (!backend_)
    return;

  for (auto& observer : observers_) {
    observer.OnLoginsChanged(this, changes);
  }
}

void PasswordStore::UnblocklistInternal(
    base::OnceClosure completion,
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_)
    return;  // Once the shutdown started, ignore new requests.
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

  auto notify_callback =
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this);
  if (completion)
    notify_callback = std::move(notify_callback).Then(std::move(completion));

  auto barrier_callback = base::BarrierCallback<PasswordStoreChangeList>(
      forms_to_remove.size(), base::BindOnce(&JoinPasswordStoreChanges)
                                  .Then(std::move(notify_callback)));

  for (const auto& form : forms_to_remove) {
    backend_->RemoveLoginAsync(form, barrier_callback);
  }
}

void PasswordStore::InjectAffiliationAndBrandingInformation(
    LoginsReply callback,
    LoginsResult forms) {
  if (affiliated_match_helper_ && !forms.empty()) {
    affiliated_match_helper_->get_affiliation_service()
        ->InjectAffiliationAndBrandingInformation(
            std::move(forms), AffiliationService::StrategyOnCacheMiss::FAIL,
            std::move(callback));
  } else {
    std::move(callback).Run(std::move(forms));
  }
}

}  // namespace password_manager
