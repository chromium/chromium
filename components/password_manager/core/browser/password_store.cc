// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_manager_impl.h"
#include "components/password_manager/core/browser/password_store_backend.h"
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
void InvokeCallbacksForSuspectedChanges(
    base::OnceCallback<void(PasswordChanges)> notifying_callback,
    base::OnceCallback<void(bool)> completion_callback,
    PasswordChanges changes) {
  DCHECK(notifying_callback);
  // Two cases *presumably* have changes that need to be reported:
  // 1. `changes` contains a non-empty PasswordStoreChangeList.
  // 2. `changes` contains nullopt PasswordStoreChangeList because the
  //    backend can't compute it. A full list will be requested instead.
  // Only if `changes` contains an empty PasswordStoreChangeList, Chrome knows
  // for certain that no changes have happened:
  bool completed = !changes.has_value() || !changes->empty();

  // In any case, we want to indicate the completed operation:
  std::move(notifying_callback).Run(std::move(changes));
  if (completion_callback) {
    std::move(completion_callback).Run(completed);
  }
}

}  // namespace

PasswordStore::PasswordStore(std::unique_ptr<PasswordStoreBackend> backend)
    : backend_(std::move(backend)) {}

void PasswordStore::Init(
    PrefService* prefs,
    std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper) {
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  DCHECK(main_task_runner_);
  prefs_ = prefs;
  affiliated_match_helper_ = std::move(affiliated_match_helper);

  DCHECK(backend_);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
      "passwords", "PasswordStore::InitOnBackgroundSequence", this);
  backend_->InitBackend(
      base::BindRepeating(&PasswordStore::NotifyLoginsChangedOnMainSequence,
                          this, LoginsChangedTrigger::ExternalUpdate),
      base::BindPostTask(
          main_task_runner_,
          base::BindRepeating(
              &PasswordStore::NotifySyncEnabledOrDisabledOnMainSequence, this)),
      base::BindOnce(&PasswordStore::OnInitCompleted, this));
}

void PasswordStore::AddLogin(const PasswordForm& form,
                             base::OnceClosure completion) {
  AddLogins({form}, std::move(completion));
}

void PasswordStore::AddLogins(const std::vector<PasswordForm>& forms,
                              base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  auto barrier_callback = base::BarrierCallback<PasswordChangesOrError>(
      forms.size(), base::BindOnce(&JoinPasswordStoreChanges)
                        .Then(base::BindOnce(
                            &PasswordStore::NotifyLoginsChangedOnMainSequence,
                            this, LoginsChangedTrigger::Addition))
                        .Then(std::move(completion)));

  for (const PasswordForm& form : forms) {
    CHECK(!form.blocked_by_user ||
          (form.username_value.empty() && form.password_value.empty()));
    backend_->AddLoginAsync(form, barrier_callback);
  }
}

void PasswordStore::UpdateLogin(const PasswordForm& form,
                                base::OnceClosure completion) {
  UpdateLogins({form}, std::move(completion));
}

void PasswordStore::UpdateLogins(const std::vector<PasswordForm>& forms,
                                 base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  auto barrier_callback = base::BarrierCallback<PasswordChangesOrError>(
      forms.size(), base::BindOnce(&JoinPasswordStoreChanges)
                        .Then(base::BindOnce(
                            &PasswordStore::NotifyLoginsChangedOnMainSequence,
                            this, LoginsChangedTrigger::Update))
                        .Then(std::move(completion)));

  for (const PasswordForm& form : forms) {
    CHECK(!form.blocked_by_user ||
          (form.username_value.empty() && form.password_value.empty()));
    backend_->UpdateLoginAsync(form, barrier_callback);
  }
}

void PasswordStore::UpdateLoginWithPrimaryKey(
    const PasswordForm& new_form,
    const PasswordForm& old_primary_key,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  PasswordForm new_form_with_correct_password_issues = new_form;
  // TODO(crbug.com/1223022): Re-evaluate this once all places that call
  // UpdateLoginWithPrimaryKey() have properly set the |password_issues|
  // field.
  if (new_form.password_value != old_primary_key.password_value) {
    // If the password changes, the password issues aren't valid
    // any more. Make sure they are cleared before storing the new form.
    new_form_with_correct_password_issues.password_issues.clear();
  } else if (new_form.username_value != old_primary_key.username_value) {
    // If the username changed then the phished and leaked issues aren't valid
    // any more. Make sure they are erased before storing the new form.
    new_form_with_correct_password_issues.password_issues.erase(
        InsecureType::kLeaked);
    new_form_with_correct_password_issues.password_issues.erase(
        InsecureType::kPhished);
  }

  auto barrier_callback = base::BarrierCallback<PasswordChangesOrError>(
      2, base::BindOnce(&JoinPasswordStoreChanges)
             .Then(base::BindOnce(
                 &PasswordStore::NotifyLoginsChangedOnMainSequence, this,
                 LoginsChangedTrigger::Update))
             .Then(std::move(completion)));

  backend_->RemoveLoginAsync(old_primary_key, barrier_callback);
  backend_->AddLoginAsync(new_form_with_correct_password_issues,
                          barrier_callback);
}

void PasswordStore::RemoveLogin(const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  backend_->RemoveLoginAsync(
      form, base::BindOnce(&GetPasswordChangesOrNulloptOnFailure)
                .Then(base::BindOnce(
                    &PasswordStore::NotifyLoginsChangedOnMainSequence, this,
                    LoginsChangedTrigger::Deletion)));
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
      base::BindOnce(&GetPasswordChangesOrNulloptOnFailure)
          .Then(
              base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence,
                             this, LoginsChangedTrigger::BatchDeletion))
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
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this,
                     LoginsChangedTrigger::BatchDeletion);
  backend_->RemoveLoginsCreatedBetweenAsync(
      delete_begin, delete_end,
      base::BindOnce(&GetPasswordChangesOrNulloptOnFailure)
          .Then(base::BindOnce(&InvokeCallbacksForSuspectedChanges,
                               std::move(callback), std::move(completion))));
}

void PasswordStore::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  backend_->DisableAutoSignInForOriginsAsync(origin_filter,
                                             std::move(completion));
}

void PasswordStore::Unblocklist(const PasswordFormDigest& form_digest,
                                base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  backend_->FillMatchingLoginsAsync(
      base::BindOnce(&GetLoginsOrEmptyListOnFailure)
          .Then(base::BindOnce(&PasswordStore::UnblocklistInternal, this,
                               std::move(completion))),
      /*include_psl=*/false, {form_digest});
}

void PasswordStore::GetLogins(const PasswordFormDigest& form,
                              base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "PasswordStore::GetLogins",
                                    consumer.get());

  auto consumer_reply = base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this));

  auto affiliation_injection =
      base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                     this, std::move(consumer_reply));

  if (affiliated_match_helper_) {
    GetLoginsWithAffiliationsRequestHandler(std::move(form), backend_.get(),
                                            affiliated_match_helper_.get(),
                                            std::move(affiliation_injection));
    return;
  }

  backend_->FillMatchingLoginsAsync(std::move(affiliation_injection),
                                    FormSupportsPSL(form), {form});
}

void PasswordStore::GetAutofillableLogins(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  backend_->GetAutofillableLoginsAsync(base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this)));
}

void PasswordStore::GetAllLogins(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  backend_->GetAllLoginsAsync(base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this)));
}

void PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  auto consumer_reply = base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this));

  auto affiliation_injection =
      base::BindOnce(&PasswordStore::InjectAffiliationAndBrandingInformation,
                     this, std::move(consumer_reply));
  backend_->GetAllLoginsAsync(std::move(affiliation_injection));
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

  // Prevent in-flight tasks posted from the backend to invoke the callback
  // after shutdown.
  sync_enabled_or_disabled_cbs_.reset();

  // The AffiliationService must be destroyed from the main sequence.
  affiliated_match_helper_.reset();

  if (backend_) {
    backend_->Shutdown(base::BindOnce(
        [](std::unique_ptr<PasswordStoreBackend> backend) { backend.reset(); },
        std::move(backend_)));
    // Now, backend_ == nullptr (guaranteed by move).
  }
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStore::CreateSyncControllerDelegate() {
  return backend_ ? backend_->CreateSyncControllerDelegate() : nullptr;
}

void PasswordStore::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  if (backend_) {
    backend_->OnSyncServiceInitialized(sync_service);
  }
}

base::CallbackListSubscription PasswordStore::AddSyncEnabledOrDisabledCallback(
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  DCHECK(sync_enabled_or_disabled_cbs_);
  return sync_enabled_or_disabled_cbs_->Add(
      std::move(sync_enabled_or_disabled_cb));
}

PasswordStoreBackend* PasswordStore::GetBackendForTesting() {
  return backend_.get();
}

PasswordStore::~PasswordStore() {
  DCHECK(!backend_) << "Shutdown() needs to be called before destruction!";
}

void PasswordStore::OnInitCompleted(bool success) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  init_status_ = success ? InitStatus::kSuccess : InitStatus::kFailure;

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "passwords", "PasswordStore::InitOnBackgroundSequence", this);
}

void PasswordStore::NotifyLoginsChangedOnMainSequence(
    LoginsChangedTrigger logins_changed_trigger,
    absl::optional<PasswordStoreChangeList> changes) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  // Don't propagate reference to this store after its shutdown. No caller
  // should expect any notifications from a shut down store in any case.
  if (!backend_) {
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  // Record that an OnLoginsRetained call may be required here already since
  // issuing the list call seems to be the most relevant and expensive step.
  base::UmaHistogramEnumeration(
      "PasswordManager.PasswordStore.OnLoginsRetained", logins_changed_trigger);
  if (!changes.has_value()) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(
        "passwords", "LoginsRetrievedForOnLoginsRetained", this);
    // If the changes aren't provided, the store propagates the latest logins.
    backend_->GetAllLoginsAsync(base::BindOnce(
        &PasswordStore::NotifyLoginsRetainedOnMainSequence, this));
    return;
  }
#else
  if (!changes.has_value()) {
    // TODO(crbug/1423425): Record the silent failure.
    return;
  }
#endif

  if (changes->empty()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnLoginsChanged(this, changes.value());
  }

  base::UmaHistogramBoolean("PasswordManager.PasswordStore.OnLoginsChanged",
                            true);
}

void PasswordStore::NotifyLoginsRetainedOnMainSequence(
    LoginsResultOrError result) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // Don't propagate reference to this store after its shutdown. No caller
  // should expect any notifications from a shut down store in any case.
  if (!backend_) {
    return;
  }

  // Clients don't expect errors yet, so just wait for the next notification.
  if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
    return;
  }

  std::vector<PasswordForm> retained_logins;
  retained_logins.reserve(absl::get<LoginsResult>(result).size());
  for (auto& login : absl::get<LoginsResult>(result)) {
    retained_logins.push_back(std::move(*login));
  }

  for (auto& observer : observers_) {
    observer.OnLoginsRetained(this, retained_logins);
  }

#if BUILDFLAG(IS_ANDROID)
  TRACE_EVENT_NESTABLE_ASYNC_END0("passwords",
                                  "LoginsRetrievedForOnLoginsRetained", this);
#endif
}

void PasswordStore::NotifySyncEnabledOrDisabledOnMainSequence() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (sync_enabled_or_disabled_cbs_) {
    sync_enabled_or_disabled_cbs_->Notify();
  }
}

void PasswordStore::UnblocklistInternal(
    base::OnceClosure completion,
    std::vector<std::unique_ptr<PasswordForm>> forms) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  TRACE_EVENT0("passwords", "PasswordStore::UnblocklistInternal");

  std::vector<PasswordForm> forms_to_remove;
  for (auto& form : forms) {
    if (form->blocked_by_user) {
      forms_to_remove.push_back(std::move(*form));
    }
  }

  if (forms_to_remove.empty()) {
    if (completion) {
      std::move(completion).Run();
    }
    return;
  }

  auto notify_callback =
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this,
                     LoginsChangedTrigger::Unblocklisting);
  if (completion) {
    notify_callback = std::move(notify_callback).Then(std::move(completion));
  }

  auto barrier_callback = base::BarrierCallback<PasswordChangesOrError>(
      forms_to_remove.size(), base::BindOnce(&JoinPasswordStoreChanges)
                                  .Then(std::move(notify_callback)));

  for (const auto& form : forms_to_remove) {
    backend_->RemoveLoginAsync(form, barrier_callback);
  }
}

void PasswordStore::InjectAffiliationAndBrandingInformation(
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
