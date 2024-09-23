// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "build/buildflag.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/get_logins_with_affiliations_request_handler.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/login_database_async_helper.h"
#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_backend_metrics_recorder.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_util.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync/model/proxy_data_type_controller_delegate.h"

#if !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
#include "components/password_manager/core/browser/password_store/password_data_type_controller_delegate_android.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#endif  // !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

namespace password_manager {

namespace {

using SuccessStatus = PasswordStoreBackendMetricsRecorder::SuccessStatus;

// Template function to create a callback which accepts LoginsResultOrError or
// PasswordChangesOrError as a result.
template <typename Result>
base::OnceCallback<Result(Result)> ReportMetricsForResultCallback(
    MethodName method_name) {
  PasswordStoreBackendMetricsRecorder metrics_reporter(
      BackendInfix("BuiltInBackend"), method_name,
      PasswordStoreBackendMetricsRecorder::PasswordStoreAndroidBackendType::
          kNone);
  return base::BindOnce(
      [](PasswordStoreBackendMetricsRecorder reporter,
         Result result) -> Result {
        if (absl::holds_alternative<PasswordStoreBackendError>(result)) {
          reporter.RecordMetrics(SuccessStatus::kError,
                                 absl::get<PasswordStoreBackendError>(result));
        } else {
          reporter.RecordMetrics(SuccessStatus::kSuccess, std::nullopt);
        }
        return result;
      },
      std::move(metrics_reporter));
}

std::unique_ptr<os_crypt_async::Encryptor> ConvertToUniquePtr(
    os_crypt_async::Encryptor encryptor,
    bool success) {
  if (!success) {
    return nullptr;
  }
  return std::make_unique<os_crypt_async::Encryptor>(std::move(encryptor));
}

// Records in a pref that passwords were deleted via sync. The pref is used to
// report metrics.
std::optional<PasswordStoreChangeList> MaybeRecordPasswordDeletionViaSync(
    base::RepeatingCallback<void(password_manager::IsAccountStore)>
        write_prefs_callback,
    std::optional<PasswordStoreChangeList> password_store_change_list,
    bool is_account_store) {
  bool hasCredentialRemoval = base::ranges::any_of(
      password_store_change_list.value(), [](PasswordStoreChange change) {
        return change.type() == PasswordStoreChange::REMOVE;
      });
  if (hasCredentialRemoval) {
    write_prefs_callback.Run(
        password_manager::IsAccountStore(is_account_store));
  }
  return password_store_change_list;
}

}  // namespace

PasswordStoreBuiltInBackend::PasswordStoreBuiltInBackend(
    std::unique_ptr<LoginDatabase> login_db,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior,
    PrefService* prefs,
    os_crypt_async::OSCryptAsync* os_crypt_async,
    UnsyncedCredentialsDeletionNotifier notifier)
    : pref_service_(prefs), os_crypt_async_(os_crypt_async) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  if (base::FeatureList::IsEnabled(
          features::kClearLoginDatabaseForAllMigratedUPMUsers)) {
    // This backend shouldn't be created for the users migrated to UPM with
    // split stores.
    CHECK_NE(
        prefs->GetInteger(
            password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores),
        static_cast<int>(prefs::UseUpmLocalAndSeparateStoresState::kOn));
  }
#endif  // BUILDFLAG(IS_ANDROID) && !BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)

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

// TODO: crbug.com/350656597 - Test tracking
// PasswordManagerCredentialRemovalReason::kSync via an integration test
void PasswordStoreBuiltInBackend::NotifyCredentialsChangedForTesting(
    base::PassKey<class PasswordStoreBuiltInBackendPasswordLossMetricsTest>,
    const PasswordStoreChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PasswordStoreSync::NotifyCredentialsChanged,
          base::Unretained(static_cast<PasswordStoreSync*>(helper_.get())),
          changes));
}

void PasswordStoreBuiltInBackend::Shutdown(
    base::OnceClosure shutdown_completed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_ptr_factory_.InvalidateWeakPtrs();
  affiliated_match_helper_ = nullptr;
  subscription_ = {};
  if (helper_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(helper_));
    std::move(shutdown_completed).Run();
  }
}

bool PasswordStoreBuiltInBackend::IsAbleToSavePasswords() {
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  return is_database_initialized_successfully_;
#else
  CHECK(pref_service_);
  // Database was not initialized siccessfully, disable saving.
  if (!is_database_initialized_successfully_) {
    return false;
  }

  // Login database is not empty continue saving passwords.
  if (!pref_service_->GetBoolean(prefs::kEmptyProfileStoreLoginDatabase)) {
    return true;
  }

  // Login database is empty, disable saving.
  return false;
#endif
}

void PasswordStoreBuiltInBackend::InitBackend(
    AffiliatedMatchHelper* affiliated_match_helper,
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  affiliated_match_helper_ = affiliated_match_helper;

#if !BUILDFLAG(IS_ANDROID)
  // To ensure that groups of the kClearUndecryptablePasswords will stay
  // balanced, after the cleanup is done an additional flag check is needed.
  // Users won't reach the flag the normal way since the LoginDB is working
  // correctly and thus flag is never reached.
  // TODO(b/40286735): Remove after this feature is launched.
  if (pref_service_->GetBoolean(prefs::kClearingUndecryptablePasswords)) {
    base::FeatureList::IsEnabled(features::kClearUndecryptablePasswords);
  }
#endif

  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LoginDatabaseAsyncHelper::CreateSyncBackend,
                                base::Unretained(helper_.get())));

  auto init_database_callback = base::BindOnce(
      &PasswordStoreBuiltInBackend::OnEncryptorReceived,
      weak_ptr_factory_.GetWeakPtr(), std::move(remote_form_changes_received),
      std::move(sync_enabled_or_disabled_cb), std::move(completion));

  if (!os_crypt_async_) {
    std::move(init_database_callback).Run(nullptr);
    return;
  }
  os_crypt_async::Encryptor::Option option =
      base::FeatureList::IsEnabled(features::kUseNewEncryptionMethod)
          ? os_crypt_async::Encryptor::Option::kNone
          : os_crypt_async::Encryptor::Option::kEncryptSyncCompat;

  subscription_ = os_crypt_async_->GetInstance(
      metrics_util::TimeCallback(
          base::BindOnce(&ConvertToUniquePtr)
              .Then(std::move(init_database_callback)),
          "PasswordManager.OsCryptAsync.GetInstanceTime"),
      option);
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
          MethodName("GetAllLoginsAsync"))
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
          MethodName("GetAutofillableLoginsAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::GetAllLoginsForAccountAsync(
    std::string account,
    LoginsOrErrorReply callback) {
  NOTREACHED_IN_MIGRATION();
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
          MethodName("FillMatchingLoginsAsync"))
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
          MethodName("AddLoginAsync"))
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
          MethodName("UpdateLoginAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::RemoveLoginAsync(
    const base::Location& location,
    const PasswordForm& form,
    PasswordChangesOrErrorReply callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(helper_);
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::RemoveLogin,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          location, form),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MethodName("RemoveLoginAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::RemoveLoginsCreatedBetweenAsync(
    const base::Location& location,
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
          location, delete_begin, delete_end),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MethodName("RemoveLoginsCreatedBetweenAsync"))
          .Then(std::move(callback)));
}

void PasswordStoreBuiltInBackend::RemoveLoginsByURLAndTimeAsync(
    const base::Location& location,
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
          location, url_filter, delete_begin, delete_end,
          std::move(sync_completion)),
      ReportMetricsForResultCallback<PasswordChangesOrError>(
          MethodName("RemoveLoginsByURLAndTimeAsync"))
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

std::unique_ptr<syncer::DataTypeControllerDelegate>
PasswordStoreBuiltInBackend::CreateSyncControllerDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  DCHECK(helper_);
  // Note that a callback is bound for
  // GetSyncControllerDelegate() because this getter itself
  // must also run in the backend sequence, and the proxy object below will take
  // care of that.
  // Since the controller delegate can (only in theory) invoke the factory after
  // `Shutdown` was called, it only returns nullptr then to prevent a UAF.
  return std::make_unique<syncer::ProxyDataTypeControllerDelegate>(
      background_task_runner_,
      base::BindRepeating(&LoginDatabaseAsyncHelper::GetSyncControllerDelegate,
                          base::Unretained(helper_.get())));
#else
  return std::make_unique<PasswordDataTypeControllerDelegateAndroid>();
#endif  // BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}

void PasswordStoreBuiltInBackend::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {}

void PasswordStoreBuiltInBackend::RecordAddLoginAsyncCalledFromTheStore() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.BuiltInBackend.AddLoginCalledOnStore",
      true);
}

void PasswordStoreBuiltInBackend::RecordUpdateLoginAsyncCalledFromTheStore() {
  base::UmaHistogramBoolean(
      "PasswordManager.PasswordStore.BuiltInBackend.UpdateLoginCalledOnStore",
      true);
}

base::WeakPtr<PasswordStoreBackend> PasswordStoreBuiltInBackend::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

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

void PasswordStoreBuiltInBackend::OnInitComplete(
    base::OnceCallback<void(bool)> completion,
    bool result) {
  is_database_initialized_successfully_ = result;
  std::move(completion).Run(result);
}

void PasswordStoreBuiltInBackend::OnEncryptorReceived(
    RemoteChangesReceived remote_form_changes_received,
    base::RepeatingClosure sync_enabled_or_disabled_cb,
    base::OnceCallback<void(bool)> completion,
    std::unique_ptr<os_crypt_async::Encryptor> encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramBoolean("PasswordManager.OnEncryptorReceived.Success",
                            !encryptor);

  // Piggyback on |remote_form_changes_received| to record password deletion
  // coming from sync.
  auto remote_form_changes_with_store_callback =
      base::BindRepeating(
          &MaybeRecordPasswordDeletionViaSync,
          base::BindRepeating(
              &PasswordStoreBuiltInBackend::WritePasswordRemovalReasonPrefs,
              weak_ptr_factory_.GetWeakPtr()))
          .Then(std::move(remote_form_changes_received));

  auto on_undecryptable_passwords_removed =
#if BUILDFLAG(IS_ANDROID)
      base::DoNothing();
#else
      base::BindPostTaskToCurrentDefault(base::BindRepeating(
          &PasswordStoreBuiltInBackend::
              SetClearingUndecryptablePasswordsIsEnabledPref,
          weak_ptr_factory_.GetWeakPtr()));
#endif

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &LoginDatabaseAsyncHelper::Initialize,
          base::Unretained(helper_.get()),  // Safe until `Shutdown()`.
          std::move(remote_form_changes_with_store_callback),
          std::move(sync_enabled_or_disabled_cb),
          std::move(on_undecryptable_passwords_removed), std::move(encryptor)),
      base::BindOnce(&PasswordStoreBuiltInBackend::OnInitComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(completion)));
}

#if !BUILDFLAG(IS_ANDROID)
void PasswordStoreBuiltInBackend::
    SetClearingUndecryptablePasswordsIsEnabledPref(
        IsAccountStore is_account_store) {
  CHECK(pref_service_);
  pref_service_->SetBoolean(prefs::kClearingUndecryptablePasswords, true);
  if (base::FeatureList::IsEnabled(features::kClearUndecryptablePasswords)) {
    AddPasswordRemovalReason(
        pref_service_, is_account_store,
        metrics_util::PasswordManagerCredentialRemovalReason::
            kDeletingUndecryptablePasswords);
  }
}
#endif

void PasswordStoreBuiltInBackend::WritePasswordRemovalReasonPrefs(
    IsAccountStore is_account_store) {
  AddPasswordRemovalReason(
      pref_service_, password_manager::IsAccountStore(is_account_store),
      metrics_util::PasswordManagerCredentialRemovalReason::kSync);
}
}  // namespace password_manager
