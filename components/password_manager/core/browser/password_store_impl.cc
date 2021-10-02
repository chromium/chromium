// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_impl.h"

#include <iterator>
#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
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

PasswordStoreImpl::PasswordStoreImpl(std::unique_ptr<LoginDatabase> login_db)
    : login_db_(std::move(login_db)) {
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);
  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
}

PasswordStoreImpl::PasswordStoreImpl(
    std::unique_ptr<LoginDatabase> login_db,
    std::unique_ptr<PasswordStore::UnsyncedCredentialsDeletionNotifier>
        notifier)
    : PasswordStoreImpl(std::move(login_db)) {
  DCHECK(notifier);
  deletion_notifier_ = std::move(notifier);
}

PasswordStoreImpl::~PasswordStoreImpl() = default;

void PasswordStoreImpl::Shutdown(base::OnceClosure shutdown_completed) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  was_shutdown_ = true;
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::DestroyOnBackgroundSequence,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(shutdown_completed));
}

PasswordStoreChangeList PasswordStoreImpl::DisableAutoSignInForOriginsImpl(
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

DatabaseCleanupResult PasswordStoreImpl::DeleteUndecryptableLogins() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!login_db_)
    return DatabaseCleanupResult::kDatabaseUnavailable;
  return login_db_->DeleteUndecryptableLogins();
}

void PasswordStoreImpl::AddSiteStatsInternal(const InteractionsStats& stats) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().AddRow(stats);
}

void PasswordStoreImpl::RemoveSiteStatsInternal(const GURL& origin_domain) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (login_db_)
    login_db_->stats_table().RemoveRow(origin_domain);
}

std::vector<InteractionsStats> PasswordStoreImpl::GetSiteStatsInternal(
    const GURL& origin_domain) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  return login_db_ ? login_db_->stats_table().GetRows(origin_domain)
                   : std::vector<InteractionsStats>();
}

void PasswordStoreImpl::RemoveStatisticsByOriginAndTimeInternal(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end) {
  if (login_db_) {
    login_db_->stats_table().RemoveStatsByOriginAndTime(
        origin_filter, delete_begin, delete_end);
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
PasswordStoreImpl::GetSyncControllerDelegateOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(sync_bridge_);
  return sync_bridge_->change_processor()->GetControllerDelegate();
}

void PasswordStoreImpl::ReportMetrics() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!login_db_)
    return;
  login_db_->ReportMetrics();
}

PasswordStoreChangeList PasswordStoreImpl::AddLoginSync(
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

PasswordStoreChangeList PasswordStoreImpl::UpdateLoginSync(
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

void PasswordStoreImpl::NotifyLoginsChanged(
    const PasswordStoreChangeList& changes) {
  if (!remote_forms_changes_received_callback_)
    return;
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(remote_forms_changes_received_callback_, changes));
}

void PasswordStoreImpl::NotifyDeletionsHaveSynced(bool success) {
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

void PasswordStoreImpl::NotifyUnsyncedCredentialsWillBeDeleted(
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

bool PasswordStoreImpl::BeginTransaction() {
  if (login_db_)
    return login_db_->BeginTransaction();
  return false;
}

void PasswordStoreImpl::RollbackTransaction() {
  if (login_db_)
    login_db_->RollbackTransaction();
}

bool PasswordStoreImpl::CommitTransaction() {
  if (login_db_)
    return login_db_->CommitTransaction();
  return false;
}

FormRetrievalResult PasswordStoreImpl::ReadAllLogins(
    PrimaryKeyToFormMap* key_to_form_map) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  if (!login_db_)
    return FormRetrievalResult::kDbError;
  return login_db_->GetAllLogins(key_to_form_map);
}

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginByPrimaryKeySync(
    FormPrimaryKey primary_key) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  PasswordStoreChangeList changes;
  if (login_db_ && login_db_->RemoveLoginByPrimaryKey(primary_key, &changes)) {
    return changes;
  }
  return PasswordStoreChangeList();
}

PasswordStoreSync::MetadataStore* PasswordStoreImpl::GetMetadataStore() {
  return login_db_.get();
}

bool PasswordStoreImpl::IsAccountStore() const {
  return login_db_ && login_db_->is_account_store();
}

bool PasswordStoreImpl::DeleteAndRecreateDatabaseFile() {
  return login_db_ && login_db_->DeleteAndRecreateDatabaseFile();
}

void PasswordStoreImpl::InitBackend(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::InitOnBackgroundSequence,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     std::move(remote_form_changes_received),
                     std::move(sync_enabled_or_disabled_cb)),
      std::move(completion));
}

void PasswordStoreImpl::GetAllLoginsAsync(LoginsReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::GetAllLoginsInternal,
                     base::Unretained(this)),  // Safe until `Shutdown()`.
      std::move(callback));
}

void PasswordStoreImpl::GetAutofillableLoginsAsync(LoginsReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::GetAutofillableLoginsInternal,
                     base::Unretained(this)),  // Safe until `Shutdown()`.
      std::move(callback));
}

void PasswordStoreImpl::FillMatchingLoginsAsync(
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
      base::BindOnce(&PasswordStoreImpl::FillMatchingLoginsInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     forms, include_psl),
      std::move(callback));
}

void PasswordStoreImpl::AddLoginAsync(const PasswordForm& form,
                                      PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::AddLoginInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     form),
      std::move(callback));
}

void PasswordStoreImpl::UpdateLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::UpdateLoginInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     form),
      std::move(callback));
}

void PasswordStoreImpl::RemoveLoginAsync(
    const PasswordForm& form,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::RemoveLoginInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     form),
      std::move(callback));
}

void PasswordStoreImpl::RemoveLoginsCreatedBetweenAsync(
    base::Time delete_begin,
    base::Time delete_end,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::RemoveLoginsCreatedBetweenInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     delete_begin, delete_end),
      std::move(callback));
}

void PasswordStoreImpl::RemoveLoginsByURLAndTimeAsync(
    const base::RepeatingCallback<bool(const GURL&)>& url_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceCallback<void(bool)> sync_completion,
    PasswordStoreChangeListReply callback) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::RemoveLoginsByURLAndTimeInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     url_filter, delete_begin, delete_end,
                     std::move(sync_completion)),
      std::move(callback));
}

void PasswordStoreImpl::DisableAutoSignInForOriginsAsync(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::OnceClosure completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(
                         &PasswordStoreImpl::DisableAutoSignInForOriginsImpl),
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     origin_filter),
      std::move(completion));
}

SmartBubbleStatsStore* PasswordStoreImpl::GetSmartBubbleStatsStore() {
  return this;
}

FieldInfoStore* PasswordStoreImpl::GetFieldInfoStore() {
  return this;
}

std::unique_ptr<syncer::ProxyModelTypeControllerDelegate>
PasswordStoreImpl::CreateSyncControllerDelegateFactory() {
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
          [](base::WeakPtr<PasswordStoreImpl> backend) {
            if (!backend)
              return base::WeakPtr<syncer::ModelTypeControllerDelegate>(
                  nullptr);
            return backend->GetSyncControllerDelegateOnBackgroundSequence();
          },
          weak_ptr_factory_.GetWeakPtr()));
}

void PasswordStoreImpl::AddSiteStats(const InteractionsStats& stats) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreImpl::AddSiteStatsInternal,
                                weak_ptr_factory_.GetWeakPtr(), stats));
}

void PasswordStoreImpl::RemoveSiteStats(const GURL& origin_domain) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreImpl::RemoveSiteStatsInternal,
                                weak_ptr_factory_.GetWeakPtr(), origin_domain));
}

void PasswordStoreImpl::GetSiteStats(const GURL& origin_domain,
                                     PasswordStoreConsumer* consumer) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::GetSiteStatsInternal,
                     base::Unretained(this),  // Safe until `Shutdown()`.
                     origin_domain),
      base::BindOnce(&PasswordStoreConsumer::OnGetSiteStatistics,
                     consumer->GetWeakPtr()));
}

void PasswordStoreImpl::RemoveStatisticsByOriginAndTime(
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
    base::Time delete_begin,
    base::Time delete_end,
    base::OnceClosure completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreImpl::RemoveStatisticsByOriginAndTimeInternal,
          weak_ptr_factory_.GetWeakPtr(), origin_filter, delete_begin,
          delete_end),
      std::move(completion));
}

void PasswordStoreImpl::AddFieldInfo(const FieldInfo& field_info) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PasswordStoreImpl::AddFieldInfoInternal,
                                weak_ptr_factory_.GetWeakPtr(), field_info));
}

void PasswordStoreImpl::GetAllFieldInfo(PasswordStoreConsumer* consumer) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  consumer->cancelable_task_tracker()->PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::GetAllFieldInfoInternal,
                     base::Unretained(this)),  // Safe until `Shutdown()`.
      base::BindOnce(&PasswordStoreConsumer::OnGetAllFieldInfo,
                     consumer->GetWeakPtr()));
}

void PasswordStoreImpl::RemoveFieldInfoByTime(base::Time remove_begin,
                                              base::Time remove_end,
                                              base::OnceClosure completion) {
  DCHECK(!was_shutdown_);
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  background_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PasswordStoreImpl::RemoveFieldInfoByTimeInternal,
                     weak_ptr_factory_.GetWeakPtr(), remove_begin, remove_end),
      std::move(completion));
}

bool PasswordStoreImpl::InitOnBackgroundSequence(
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
    login_db_->SetDeletionsHaveSyncedCallback(
        base::BindRepeating(&PasswordStoreImpl::NotifyDeletionsHaveSynced,
                            weak_ptr_factory_.GetWeakPtr()));

    // Delay the actual reporting by 30 seconds, to ensure it doesn't happen
    // during the "hot phase" of Chrome startup.
    background_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PasswordStoreImpl::ReportMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Seconds(30));
  }

  sync_bridge_ = base::WrapUnique(new PasswordSyncBridge(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::PASSWORDS, base::DoNothing()),
      /*password_store_sync=*/this, sync_enabled_or_disabled_cb));

  return success;
}

void PasswordStoreImpl::DestroyOnBackgroundSequence() {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  remote_forms_changes_received_callback_.Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  login_db_.reset();
  sync_bridge_.reset();
  // No task should be running on (or send to) the background runner.
  background_task_runner_.reset();
  main_task_runner_.reset();
}

LoginsResult PasswordStoreImpl::GetAllLoginsInternal() {
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

LoginsResult PasswordStoreImpl::GetAutofillableLoginsInternal() {
  std::vector<std::unique_ptr<PasswordForm>> results;
  if (!login_db_ || !login_db_->GetAutofillableLogins(&results))
    return {};
  return results;
}

LoginsResult PasswordStoreImpl::FillMatchingLoginsInternal(
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

PasswordStoreChangeList PasswordStoreImpl::AddLoginInternal(
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

PasswordStoreChangeList PasswordStoreImpl::UpdateLoginInternal(
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

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginInternal(
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

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginsCreatedBetweenInternal(
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

PasswordStoreChangeList PasswordStoreImpl::RemoveLoginsByURLAndTimeInternal(
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
        base::BindOnce(&PasswordStoreImpl::NotifyDeletionsHaveSynced,
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

void PasswordStoreImpl::AddFieldInfoInternal(const FieldInfo& field_info) {
  if (login_db_)
    login_db_->field_info_table().AddRow(field_info);
}

std::vector<FieldInfo> PasswordStoreImpl::GetAllFieldInfoInternal() {
  return login_db_ ? login_db_->field_info_table().GetAllRows()
                   : std::vector<FieldInfo>();
}

void PasswordStoreImpl::RemoveFieldInfoByTimeInternal(base::Time remove_begin,
                                                      base::Time remove_end) {
  if (login_db_)
    login_db_->field_info_table().RemoveRowsByTime(remove_begin, remove_end);
}

}  // namespace password_manager
