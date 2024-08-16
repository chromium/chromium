// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store.h"

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
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/password_store/psl_matching_helper.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"

namespace password_manager {

namespace {

// Helper function which invokes |notifying_callback| with potential changes
// and |completion_callback| with success indication.
void InvokeCallbacksForSuspectedChanges(
    base::OnceCallback<void(PasswordChanges)> notifying_callback,
    base::OnceCallback<void(bool)> completion_callback,
    PasswordChangesOrError changes_or_error) {
  DCHECK(notifying_callback);
  bool success =
      !absl::holds_alternative<PasswordStoreBackendError>(changes_or_error);

  std::move(notifying_callback)
      .Run(GetPasswordChangesOrNulloptOnFailure(std::move(changes_or_error)));
  if (completion_callback) {
    std::move(completion_callback).Run(success);
  }
}

}  // namespace

PasswordStore::PasswordStore(std::unique_ptr<PasswordStoreBackend> backend)
    : backend_(std::move(backend)), construction_time_(base::Time::Now()) {}

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
      affiliated_match_helper_.get(),
      base::BindRepeating(&PasswordStore::NotifyLoginsChangedOnMainSequence,
                          this, LoginsChangedTrigger::ExternalUpdate),
      base::BindPostTask(
          main_task_runner_,
          base::BindRepeating(
              &PasswordStore::NotifySyncEnabledOrDisabledOnMainSequence, this)),
      metrics_util::TimeCallback(
          base::BindOnce(&PasswordStore::OnInitCompleted, this),
          "PasswordManager.PasswordStore.InitTime"));
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

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::AddLogins, this, forms,
                                 std::move(completion)));
    return;
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
    backend_->RecordAddLoginAsyncCalledFromTheStore();
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

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::UpdateLogins, this, forms,
                                 std::move(completion)));
    return;
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
    backend_->RecordUpdateLoginAsyncCalledFromTheStore();
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

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::UpdateLoginWithPrimaryKey,
                                 this, new_form, old_primary_key,
                                 std::move(completion)));
    return;
  }
  PasswordForm new_form_with_correct_password_issues = new_form;
  // TODO(crbug.com/40774419): Re-evaluate this once all places that call
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

  backend_->RemoveLoginAsync(FROM_HERE, old_primary_key, barrier_callback);
  backend_->AddLoginAsync(new_form_with_correct_password_issues,
                          barrier_callback);
  backend_->RecordAddLoginAsyncCalledFromTheStore();
}

void PasswordStore::RemoveLogin(const base::Location& location,
                                const PasswordForm& form) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  if (post_init_callback_) {
    post_init_callback_ = std::move(post_init_callback_)
                              .Then(base::BindOnce(&PasswordStore::RemoveLogin,
                                                   this, location, form));
    return;
  }

  backend_->RemoveLoginAsync(
      location, form,
      base::BindOnce(&GetPasswordChangesOrNulloptOnFailure)
          .Then(
              base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence,
                             this, LoginsChangedTrigger::Deletion)));
}

void PasswordStore::RemoveLoginsByURLAndTime(
    const base::Location& location,
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

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::RemoveLoginsByURLAndTime, this,
                                 location, url_filter, delete_begin, delete_end,
                                 std::move(completion),
                                 std::move(sync_completion)));
    return;
  }

  backend_->RemoveLoginsByURLAndTimeAsync(
      location, url_filter, delete_begin, delete_end,
      std::move(sync_completion),
      base::BindOnce(&GetPasswordChangesOrNulloptOnFailure)
          .Then(
              base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence,
                             this, LoginsChangedTrigger::BatchDeletion))
          .Then(std::move(completion)));
}

void PasswordStore::RemoveLoginsCreatedBetween(
    const base::Location& location,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    std::move(completion).Run(false);
    return;  // Once the shutdown started, ignore new requests.
  }

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::RemoveLoginsCreatedBetween,
                                 this, location, delete_begin, delete_end,
                                 std::move(completion)));
    return;
  }

  auto callback =
      base::BindOnce(&PasswordStore::NotifyLoginsChangedOnMainSequence, this,
                     LoginsChangedTrigger::BatchDeletion);
  backend_->RemoveLoginsCreatedBetweenAsync(
      location, delete_begin, delete_end,
      base::BindOnce(&InvokeCallbacksForSuspectedChanges, std::move(callback),
                     std::move(completion)));
}

void PasswordStore::DisableAutoSignInForOrigins(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::DisableAutoSignInForOrigins,
                                 this, origin_filter, std::move(completion)));
    return;
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

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::Unblocklist, this, form_digest,
                                 std::move(completion)));
    return;
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

  if (post_init_callback_) {
    post_init_callback_ = std::move(post_init_callback_)
                              .Then(base::BindOnce(&PasswordStore::GetLogins,
                                                   this, form, consumer));
    return;
  }

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("passwords", "PasswordStore::GetLogins",
                                    consumer.get());

  backend_->GetGroupedMatchingLoginsAsync(
      form, base::BindOnce(
                &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom,
                consumer, base::RetainedRef(this)));
}

void PasswordStore::GetAutofillableLogins(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::GetAutofillableLogins, this,
                                 consumer));
    return;
  }

  backend_->GetAutofillableLoginsAsync(base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this)));
  UmaHistogramMediumTimes("PasswordManager.GetAutofillableLogins.TimeSinceInit",
                          base::Time::Now() - construction_time_);
}

void PasswordStore::GetAllLogins(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(&PasswordStore::GetAllLogins, this, consumer));
    return;
  }

  backend_->GetAllLoginsAsync(base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this)));
  UmaHistogramMediumTimes("PasswordManager.GetAllLogins.TimeSinceInit",
                          base::Time::Now() - construction_time_);
}

void PasswordStore::GetAllLoginsWithAffiliationAndBrandingInformation(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }

  if (post_init_callback_) {
    post_init_callback_ =
        std::move(post_init_callback_)
            .Then(base::BindOnce(
                &PasswordStore::
                    GetAllLoginsWithAffiliationAndBrandingInformation,
                this, consumer));
    return;
  }

  auto consumer_reply = base::BindOnce(
      &PasswordStoreConsumer::OnGetPasswordStoreResultsOrErrorFrom, consumer,
      base::RetainedRef(this));
  backend_->GetAllLoginsWithAffiliationAndBrandingAsync(
      std::move(consumer_reply));
  UmaHistogramMediumTimes(
      "PasswordManager.GetAllLoginsWithAffiliationAndBrandingInformation."
      "TimeSinceInit",
      base::Time::Now() - construction_time_);
}

SmartBubbleStatsStore* PasswordStore::GetSmartBubbleStatsStore() {
  return backend_ ? backend_->GetSmartBubbleStatsStore() : nullptr;
}

void PasswordStore::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordStore::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PasswordStore::IsAbleToSavePasswords() const {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  return backend_ && backend_->IsAbleToSavePasswords();
}

void PasswordStore::ShutdownOnUIThread() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  post_init_callback_.Reset();

  // Prevent in-flight tasks posted from the backend to invoke the callback
  // after shutdown.
  sync_enabled_or_disabled_cbs_.reset();

  if (backend_) {
    backend_->Shutdown(base::BindOnce(
        [](std::unique_ptr<PasswordStoreBackend> backend) { backend.reset(); },
        std::move(backend_)));
    // Now, backend_ == nullptr (guaranteed by move).
  }

  // The AffiliationService must be destroyed from the main sequence.
  affiliated_match_helper_.reset();

  // PrefService is destroyed together with BrowserContext, and cannot be used
  // anymore.
  prefs_ = nullptr;
}

std::unique_ptr<syncer::DataTypeControllerDelegate>
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

  if (post_init_callback_) {
    std::move(post_init_callback_).Run();
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(
      "passwords", "PasswordStore::InitOnBackgroundSequence", this);
}

void PasswordStore::NotifyLoginsChangedOnMainSequence(
    LoginsChangedTrigger logins_changed_trigger,
    std::optional<PasswordStoreChangeList> changes) {
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
    // TODO(crbug.com/40260035): Record the silent failure.
    return;
  }
#endif

  if (changes->empty()) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnLoginsChanged(this, changes.value());
  }
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
    retained_logins.push_back(std::move(login));
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

void PasswordStore::UnblocklistInternal(base::OnceClosure completion,
                                        std::vector<PasswordForm> forms) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!backend_) {
    return;  // Once the shutdown started, ignore new requests.
  }
  TRACE_EVENT0("passwords", "PasswordStore::UnblocklistInternal");

  std::vector<PasswordForm> forms_to_remove;
  for (auto& form : forms) {
    if (form.blocked_by_user) {
      forms_to_remove.push_back(std::move(form));
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
    backend_->RemoveLoginAsync(FROM_HERE, form, barrier_callback);
  }
}

}  // namespace password_manager
