// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_built_in_backend.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "components/password_manager/core/browser/sync/password_sync_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"

namespace password_manager {

namespace {

constexpr base::TimeDelta kSyncTaskTimeout = base::Seconds(30);

}  // namespace

PasswordStoreBuiltInBackend::PasswordStoreBuiltInBackend(
    std::unique_ptr<LoginDatabase> login_db)
    : login_db_(std::move(login_db)) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
}

PasswordStoreBuiltInBackend::PasswordStoreBuiltInBackend(
    std::unique_ptr<LoginDatabase> login_db,
    std::unique_ptr<PasswordStore::UnsyncedCredentialsDeletionNotifier>
        notifier)
    : PasswordStoreBuiltInBackend(std::move(login_db)) {
  DCHECK(notifier);
  deletion_notifier_ = std::move(notifier);
}

PasswordStoreBuiltInBackend::~PasswordStoreBuiltInBackend() = default;

void PasswordStoreBuiltInBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  was_shutdown_ = true;
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::DestroyOnBackgroundSequence,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(shutdown_completed));
}

PasswordStoreChangeList
PasswordStoreBuiltInBackend::DisableAutoSignInForOriginsImpl(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (!login_db_ || !login_db_->GetAutoSignInLogins(&key_to_form_map))
    return changes;

  std::set<GURL> origins_to_update;
  for (const auto& pair : key_to_form_map) {
    if (origin_filter.Run(pair.second->url))
      origins_to_update.insert(pair.second->url);
  }

  std::set<GURL> origins_updated;
  for (const GURL& origin : origins_to_update) {
    if (login_db_->DisableAutoSignInForOrigin(origin))
      origins_updated.insert(origin);
  }

  for (const auto& pair : key_to_form_map) {
    if (origins_updated.count(pair.second->url)) {
      changes.emplace_back(PasswordStoreChange::UPDATE, *pair.second,
                           FormPrimaryKey(pair.first));
    }
  }

  return changes;
}

DatabaseCleanupResult PasswordStoreBuiltInBackend::DeleteUndecryptableLogins() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!login_db_)
    return DatabaseCleanupResult::kDatabaseUnavailable;
  return login_db_->DeleteUndecryptableLogins();
}

void PasswordStoreBuiltInBackend::AddSiteStatsInternal(
    const InteractionsStats& stats) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void PasswordStoreBuiltInBackend::RemoveSiteStatsInternal(
    const GURL& origin_domain) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<InteractionsStats>
PasswordStoreBuiltInBackend::GetSiteStatsInternal(const GURL& origin_domain) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<InteractionsStats>();
}

void PasswordStoreBuiltInBackend::RemoveStatisticsByOriginAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  if (login_db_) {
    login_db_->stats_table().RemoveStatsByOriginAndTime(
        origin_filter, delete_begin, delete_end);
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordStoreBuiltInBackend::GetSyncControllerDelegateOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(sync_bridge_);
  return sync_bridge_->change_processor()->GetControllerDelegate();
}

void PasswordStoreBuiltInBackend::ReportMetrics() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!login_db_)
    return;
  login_db_->ReportMetrics();
}

PasswordStoreChangeList PasswordStoreBuiltInBackend::AddLoginSync(
    const PasswordForm& form,
    AddLoginError* error) {
  if (!login_db_) {
    if (error) {
      *error = AddLoginError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->AddLogin(form, error);
}

PasswordStoreChangeList PasswordStoreBuiltInBackend::UpdateLoginSync(
    const PasswordForm& form,
    UpdateLoginError* error) {
  if (!login_db_) {
    if (error) {
      *error = UpdateLoginError::kDbNotAvailable;
    }
    return PasswordStoreChangeList();
  }
  return login_db_->UpdateLogin(form, error);
}

void PasswordStoreBuiltInBackend::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  if (!remote_forms_changes_received_callback_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(remote_forms_changes_received_callback_, changes));
}

void PasswordStoreBuiltInBackend::NotifyDeletionsHaveSynced(bool success) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  // Either all deletions have been committed to the Sync server, or Sync is
  // telling us that it won't commit them (because Sync was turned off
  // permanently). In either case, run the corresponding callbacks now (on the
  // main task runner).
  DCHECK(!success || !GetMetadataStore()->HasUnsyncedDeletions());
  for (auto& callback : deletions_have_synced_callbacks_) {
    main_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), success));
  }
  deletions_have_synced_timeout_.Cancel();
  deletions_have_synced_callbacks_.clear();
}

void PasswordStoreBuiltInBackend::NotifyUnsyncedCredentialsWillBeDeleted(
    std::vector<PasswordForm> unsynced_credentials) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(IsAccountStore());
  // |deletion_notifier_| only gets set for desktop.
  if (deletion_notifier_) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PasswordStore::UnsyncedCredentialsDeletionNotifier::Notify,
            deletion_notifier_->GetWeakPtr(), std::move(unsynced_credentials)));
  }
}

bool PasswordStoreBuiltInBackend::BeginTransaction() {
  if (login_db_)
    return login_db_->BeginTransaction();
  return false;
}

void PasswordStoreBuiltInBackend::RollbackTransaction() {
  if (login_db_)
    login_db_->RollbackTransaction();
}

bool PasswordStoreBuiltInBackend::CommitTransaction() {
  if (login_db_)
    return login_db_->CommitTransaction();
  return false;
}

FormRetrievalResult PasswordStoreBuiltInBackend::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!login_db_)
    return FormRetrievalResult::kDbError;
  return login_db_->GetAllLogins(key_to_form_map);
}

PasswordStoreChangeList
PasswordStoreBuiltInBackend::RemoveLoginByPrimaryKeySync(
    FormPrimaryKey primary_key) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginByPrimaryKey(primary_key, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore*
PasswordStoreBuiltInBackend::GetMetadataStore() {
  return login_db_.get();
}

bool PasswordStoreBuiltInBackend::IsAccountStore() const {
  return login_db_ && login_db_->is_account_store();
}

bool PasswordStoreBuiltInBackend::DeleteAndRecreateDatabaseFile() {
  return login_db_ && login_db_->DeleteAndRecreateDatabaseFile();
}

base::WeakPtr<PasswordStoreBackend> PasswordStoreBuiltInBackend::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PasswordStoreBuiltInBackend::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::InitOnBackgroundSequence,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     std::move(remote_form_changes_received),
                     std::move(sync_enabled_or_disabled_cb)),
      std::move(completion));
}

void PasswordStoreBuiltInBackend::GetAllLoginsAsync(
    LoginsOrErrorReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::GetAllLoginsInternal,
                     base::Unretained(this)),  // Safe until `Shutdown()`.
      std::move(callback));
}

void PasswordStoreBuiltInBackend::GetAutofillableLoginsAsync(
    LoginsOrErrorReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreBuiltInBackend::GetAutofillableLoginsInternal,
          base::Unretained(this)),  // Safe until `Shutdown()`.
      std::move(callback));
}

void PasswordStoreBuiltInBackend::FillMatchingLoginsAsync(
    LoginsReply callback,
    bool include_psl,
    const std::vector<PasswordFormDigest>& forms) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (forms.empty()) {
    std::move(callback).Run({});
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::FillMatchingLoginsInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     forms, include_psl),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::AddLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::AddLoginInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     form),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::UpdateLoginInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     form),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::RemoveLoginInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     form),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreBuiltInBackend::RemoveLoginsCreatedBetweenInternal,
          base::Unretained(this),  // Safe until `Shutdown()`.
          delete_begin, delete_end),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreBuiltInBackend::RemoveLoginsByURLAndTimeInternal,
          base::Unretained(this),  // Safe until `Shutdown()`.
          url_filter, delete_begin, delete_end, std::move(sync_completion)),
      std::move(callback));
}

void PasswordStoreBuiltInBackend::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &PasswordStoreBuiltInBackend::DisableAutoSignInForOriginsImpl),
          base::Unretained(this),  // Safe until `Shutdown()`.
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
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // Note that a callback is bound for
  // GetSyncControllerDelegateOnBackgroundSequence() because this getter itself
  // must also run in the backend sequence, and the proxy object below will take
  // care of that.
  // Since the controller delegate can (only in theory) invoke the factory after
  // `Shutdown` was called, it only returns nullptr then to prevent a UAF.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      background_task_runner_,
      base::BindRepeating(
          [](base::WeakPtr<PasswordStoreBuiltInBackend> backend) {
            if (!backend)
              return base::WeakPtr<syncer::ModelTypeControllerDelegate>(
                  nullptr);
            return backend->GetSyncControllerDelegateOnBackgroundSequence();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void PasswordStoreBuiltInBackend::AddSiteStats(const InteractionsStats& stats) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::AddSiteStatsInternal,
                     weak_ptr_factory_.GetWeakPtr(), stats));
}

void PasswordStoreBuiltInBackend::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::RemoveSiteStatsInternal,
                     weak_ptr_factory_.GetWeakPtr(), origin_domain));
}

void PasswordStoreBuiltInBackend::GetSiteStats(
    const GURL& origin_domain,
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::GetSiteStatsInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     origin_domain),
      base::BindOnce(&PasswordStoreConsumer::OnGetSiteStatistics, consumer));
}

void PasswordStoreBuiltInBackend::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreBuiltInBackend::RemoveStatisticsByOriginAndTimeInternal,
          weak_ptr_factory_.GetWeakPtr(), origin_filter, delete_begin,
          delete_end),
      std::move(completion));
}

void PasswordStoreBuiltInBackend::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::AddFieldInfoInternal,
                     weak_ptr_factory_.GetWeakPtr(), field_info));
}

void PasswordStoreBuiltInBackend::GetAllFieldInfo(
    base::WeakPtr<PasswordStoreConsumer> consumer) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PasswordStoreBuiltInBackend::GetAllFieldInfoInternal,
                     base::Unretained(this)),  // Safe until `Shutdown()`.
      base::BindOnce(&PasswordStoreConsumer::OnGetAllFieldInfo, consumer));
}

void PasswordStoreBuiltInBackend::RemoveFieldInfoByTime(
    base::Time remove_begin,
    base::Time remove_end,
    base::OnceClosure completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreBuiltInBackend::RemoveFieldInfoByTimeInternal,
          weak_ptr_factory_.GetWeakPtr(), remove_begin, remove_end),
      std::move(completion));
}

bool PasswordStoreBuiltInBackend::InitOnBackgroundSequence(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  remote_forms_changes_received_callback_ =
      std::move(remote_form_changes_received);

  DCHECK(login_db_);
  bool success = true;
  if (!login_db_->Init()) {
    login_db_.reset();
    // The initialization should be continued, because PasswordSyncBridge
    // has to be initialized even if database initialization failed.
    success = false;
    LOG(ERROR) << "Could not create/open login database.";
  }
  if (success) {
    login_db_->SetDeletionsHaveSyncedCallback(base::BindRepeating(
        &PasswordStoreBuiltInBackend::NotifyDeletionsHaveSynced,
        weak_ptr_factory_.GetWeakPtr()));

    // Delay the actual reporting by 30 seconds, to ensure it doesn't happen
    // during the "hot phase" of Chrome startup.
    background_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PasswordStoreBuiltInBackend::ReportMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }

  sync_bridge_ = base::WrapUnique(new PasswordSyncBridge(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::PASSWORDS, base::DoNothing()),
      /*password_store_sync=*/this, sync_enabled_or_disabled_cb));

  return success;
}

void PasswordStoreBuiltInBackend::DestroyOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  remote_forms_changes_received_callback_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  login_db_.reset();
  sync_bridge_.reset();
  // No task should be running on (or send to) the background runner.
  background_task_runner_.reset();
  main_task_runner_.reset();
}

LoginsResult PasswordStoreBuiltInBackend::GetAllLoginsInternal() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  PrimaryKeyToFormMap key_to_form_map;

  if (!login_db_ || login_db_->GetAllLogins(&key_to_form_map) !=
                        FormRetrievalResult::kSuccess)
    return {};

  std::vector<std::unique_ptr<PasswordForm>> obtained_forms;
  for (auto& pair : key_to_form_map) {
    obtained_forms.push_back(std::move(pair.second));
  }
  return obtained_forms;
}

LoginsResult PasswordStoreBuiltInBackend::GetAutofillableLoginsInternal() {
  std::vector<std::unique_ptr<PasswordForm>> results;
  if (!login_db_ || !login_db_->GetAutofillableLogins(&results))
    return {};
  return results;
}

LoginsResult PasswordStoreBuiltInBackend::FillMatchingLoginsInternal(
    const std::vector<PasswordFormDigest>& forms,
    bool include_psl) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());

  std::vector<std::unique_ptr<PasswordForm>> results;
  for (const auto& form : forms) {
    std::vector<std::unique_ptr<PasswordForm>> matched_forms;
    if (login_db_ && !login_db_->GetLogins(form, include_psl, &matched_forms))
      continue;
    results.insert(results.end(),
                   std::make_move_iterator(matched_forms.begin()),
                   std::make_move_iterator(matched_forms.end()));
  }
  return results;
}

PasswordStoreChangeList PasswordStoreBuiltInBackend::AddLoginInternal(
    const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  BeginTransaction();
  PasswordStoreChangeList changes = AddLoginSync(form, /*error=*/nullptr);
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with the
  // login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList PasswordStoreBuiltInBackend::UpdateLoginInternal(
    const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  BeginTransaction();
  PasswordStoreChangeList changes = UpdateLoginSync(form, /*error=*/nullptr);
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with the
  // login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList PasswordStoreBuiltInBackend::RemoveLoginInternal(
    const PasswordForm& form) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  BeginTransaction();
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLogin(form, &changes)) {
    if (sync_bridge_ && !changes.empty())
      sync_bridge_->ActOnPasswordStoreChanges(changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with the
  // login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList
PasswordStoreBuiltInBackend::RemoveLoginsCreatedBetweenInternal(
    base::Time delete_begin,
    base::Time delete_end) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  BeginTransaction();
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginsCreatedBetween(
                       delete_begin, delete_end, &changes)) {
    if (sync_bridge_ && !changes.empty())
      sync_bridge_->ActOnPasswordStoreChanges(changes);
  }
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with the
  // login data.
  CommitTransaction();
  return changes;
}

PasswordStoreChangeList
PasswordStoreBuiltInBackend::RemoveLoginsByURLAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion) {
  BeginTransaction();
  PrimaryKeyToFormMap key_to_form_map;
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->GetLoginsCreatedBetween(delete_begin, delete_end,
                                                      &key_to_form_map)) {
    for (const auto& pair : key_to_form_map) {
      PasswordForm* form = pair.second.get();
      PasswordStoreChangeList remove_changes;
      if (url_filter.Run(form->url) &&
          login_db_->RemoveLogin(*form, &remove_changes)) {
        std::move(remove_changes.begin(), remove_changes.end(),
                  std::back_inserter(changes));
      }
    }
  }
  if (sync_bridge_ && !changes.empty())
    sync_bridge_->ActOnPasswordStoreChanges(changes);
  // Sync metadata get updated in ActOnPasswordStoreChanges(). Therefore,
  // CommitTransaction() must be called after ActOnPasswordStoreChanges(),
  // because sync codebase needs to update metadata atomically together with the
  // login data.
  CommitTransaction();

  if (sync_completion) {
    deletions_have_synced_callbacks_.push_back(std::move(sync_completion));
    // Start a timeout for sync, or restart it if it was already running.
    deletions_have_synced_timeout_.Reset(
        base::BindOnce(&PasswordStoreBuiltInBackend::NotifyDeletionsHaveSynced,
                       weak_ptr_factory_.GetWeakPtr(),
                       /*success=*/false));
    background_task_runner_->PostDelayedTask(
        FROM_HERE, deletions_have_synced_timeout_.callback(), kSyncTaskTimeout);

    // Do an immediate check for the case where there are already no unsynced
    // deletions.
    if (!GetMetadataStore()->HasUnsyncedDeletions())
      NotifyDeletionsHaveSynced(/*success=*/true);
  }
  return changes;
}

void PasswordStoreBuiltInBackend::AddFieldInfoInternal(
    const FieldInfo& field_info) {
  if (login_db_)
    login_db_->field_info_table().AddRow(field_info);
}

std::vector<FieldInfo> PasswordStoreBuiltInBackend::GetAllFieldInfoInternal() {
  return login_db_ ? login_db_->field_info_table().GetAllRows()
                   : std::vector<FieldInfo>();
}

void PasswordStoreBuiltInBackend::RemoveFieldInfoByTimeInternal(
    base::Time remove_begin,
    base::Time remove_end) {
  if (login_db_)
    login_db_->field_info_table().RemoveRowsByTime(remove_begin, remove_end);
}

}  // namespace password_manager
