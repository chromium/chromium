// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace password_manager {

namespace {

#if BUILDFLAG(IS_ANDROID)
// Time in seconds by which calls to the password store happening on startup
// should be delayed.
constexpr base::TimeDelta kPasswordStoreCallDelaySeconds = base::Seconds(5);
#endif

bool IsPasswordReuseDetectionEnabled() {
  return base::FeatureList::IsEnabled(features::kPasswordReuseDetectionEnabled);
}

// Represents a single CheckReuse() request. Implements functionality to
// listen to reuse events and propagate them to |consumer| on the sequence on
// which CheckReuseRequest is created.
class CheckReuseRequest : public PasswordReuseDetectorConsumer {
 public:
  // |consumer| must not be null.
  explicit CheckReuseRequest(PasswordReuseDetectorConsumer* consumer);
  ~CheckReuseRequest() override;

  CheckReuseRequest(const CheckReuseRequest&) = delete;
  CheckReuseRequest& operator=(const CheckReuseRequest&) = delete;

  // PasswordReuseDetectorConsumer
  void OnReuseCheckDone(
      bool is_reuse_found,
      size_t password_length,
      absl::optional<PasswordHashData> reused_protected_password_hash,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      int saved_passwords,
      const std::string& domain,
      uint64_t reused_password_hash) override;

 private:
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  const base::WeakPtr<PasswordReuseDetectorConsumer> consumer_weak_;
};

CheckReuseRequest::CheckReuseRequest(PasswordReuseDetectorConsumer* consumer)
    : origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      consumer_weak_(consumer->AsWeakPtr()) {}

CheckReuseRequest::~CheckReuseRequest() = default;

void CheckReuseRequest::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    absl::optional<PasswordHashData> reused_protected_password_hash,
    const std::vector<MatchingReusedCredential>& matching_reused_credentials,
    int saved_passwords,
    const std::string& domain,
    uint64_t reused_password_hash) {
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordReuseDetectorConsumer::OnReuseCheckDone,
                     consumer_weak_, is_reuse_found, password_length,
                     reused_protected_password_hash,
                     matching_reused_credentials, saved_passwords, domain,
                     reused_password_hash));
}

void CheckReuseHelper(std::unique_ptr<CheckReuseRequest> request,
                      const std::u16string& input,
                      const std::string& domain,
                      PasswordReuseDetector* reuse_detector) {
  reuse_detector->CheckReuse(input, domain, request.get());
}

}  // namespace

PasswordReuseManagerImpl::PasswordReuseManagerImpl() = default;
PasswordReuseManagerImpl::~PasswordReuseManagerImpl() = default;

void PasswordReuseManagerImpl::Shutdown() {
  if (profile_store_) {
    profile_store_->RemoveObserver(this);
    profile_store_.reset();
  }
  if (account_store_) {
    account_store_->RemoveObserver(this);
    profile_store_.reset();
  }

  if (notifier_)
    notifier_->UnsubscribeFromSigninEvents();

  if (reuse_detector_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(reuse_detector_));
  }
}

void PasswordReuseManagerImpl::Init(PrefService* prefs,
                                    PasswordStoreInterface* profile_store,
                                    PasswordStoreInterface* account_store) {
  prefs_ = prefs;
  hash_password_manager_.set_prefs(prefs_);
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  DCHECK(main_task_runner_);

  if (!IsPasswordReuseDetectionEnabled())
    return;

  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
  DCHECK(profile_store);

  reuse_detector_ = std::make_unique<PasswordReuseDetector>();

  account_store_ = account_store;
  profile_store_ = profile_store;
#if BUILDFLAG(IS_ANDROID)
  // Calls to the password store result in a call to Google Play Services which
  // can be resource-intesive. In order not to slow down other startup
  // operations, requesting logins is delayed by
  // `kPasswordStoreCallDelaySeconds`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordReuseManagerImpl::RequestLoginsFromStores,
                     weak_ptr_factory_.GetWeakPtr()),
      kPasswordStoreCallDelaySeconds);
  return;
#else
  RequestLoginsFromStores();
#endif
}

void PasswordReuseManagerImpl::ReportMetrics(
    const std::string& username,
    bool is_under_advanced_protection) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (username.empty())
    return;

  auto hash_password_state =
      hash_password_manager_.HasPasswordHash(username,
                                             /*is_gaia_password=*/true)
          ? metrics_util::IsSyncPasswordHashSaved::SAVED_VIA_LIST_PREF
          : metrics_util::IsSyncPasswordHashSaved::NOT_SAVED;
  metrics_util::LogIsSyncPasswordHashSaved(hash_password_state,
                                           is_under_advanced_protection);
}

void PasswordReuseManagerImpl::CheckReuse(
    const std::u16string& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!reuse_detector_) {
    consumer->OnReuseCheckDone(false, 0, absl::nullopt, {}, 0, std::string(),
                               0);
    return;
  }
  ScheduleTask(base::BindOnce(
      &CheckReuseHelper, std::make_unique<CheckReuseRequest>(consumer), input,
      domain, base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::PreparePasswordHashData(
    metrics_util::SignInState sign_in_state_for_metrics) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  SchedulePasswordHashUpdate(sign_in_state_for_metrics);
  ScheduleEnterprisePasswordURLUpdate();
}

void PasswordReuseManagerImpl::SaveGaiaPasswordHash(
    const std::string& username,
    const std::u16string& password,
    bool is_sync_password_for_metrics,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  RecordAction(
      UserMetricsAction("PasswordProtection.Gaia.HashedPasswordSaved"));
  SaveProtectedPasswordHash(username, password, is_sync_password_for_metrics,
                            /*is_gaia_password=*/true, event);
}

void PasswordReuseManagerImpl::SaveEnterprisePasswordHash(
    const std::string& username,
    const std::u16string& password) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  RecordAction(UserMetricsAction(
      "PasswordProtection.NonGaiaEnterprise.HashedPasswordSaved"));
  SaveProtectedPasswordHash(username, password,
                            /*is_sync_password_for_metrics=*/false,
                            /*is_gaia_password=*/false,
                            metrics_util::GaiaPasswordHashChange::
                                NON_GAIA_ENTERPRISE_PASSWORD_CHANGE);
}

void PasswordReuseManagerImpl::SaveProtectedPasswordHash(
    const std::string& username,
    const std::u16string& password,
    bool is_sync_password_for_metrics,
    bool is_gaia_password,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (hash_password_manager_.SavePasswordHash(username, password,
                                              is_gaia_password)) {
    if (is_gaia_password) {
      metrics_util::LogGaiaPasswordHashChange(event,
                                              is_sync_password_for_metrics);
    }
    // This method is not being called on startup so it shouldn't log metrics.
    SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/absl::nullopt);
  }
}

void PasswordReuseManagerImpl::SaveSyncPasswordHash(
    const PasswordHashData& sync_password_data,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (hash_password_manager_.SavePasswordHash(sync_password_data)) {
    metrics_util::LogGaiaPasswordHashChange(event,
                                            /*is_sync_password=*/true);
    SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/absl::nullopt);
  }
}

void PasswordReuseManagerImpl::ClearGaiaPasswordHash(
    const std::string& username) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearSavedPasswordHash(username,
                                                /*is_gaia_password=*/true);
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get()),
                              username));
}

void PasswordReuseManagerImpl::ClearAllGaiaPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ true);
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearAllGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::ClearAllEnterprisePasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ false);
  if (!reuse_detector_)
    return;
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllEnterprisePasswordHash,
                     base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::ClearAllNonGmailPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllNonGmailPasswordHash();
  if (!reuse_detector_)
    return;
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllNonGmailPasswordHash,
                     base::Unretained(reuse_detector_.get())));
}

base::CallbackListSubscription
PasswordReuseManagerImpl::RegisterStateCallbackOnHashPasswordManager(
    const base::RepeatingCallback<void(const std::string& username)>&
        callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  return hash_password_manager_.RegisterStateCallback(callback);
}

void PasswordReuseManagerImpl::SetPasswordStoreSigninNotifier(
    std::unique_ptr<PasswordStoreSigninNotifier> notifier) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!notifier_);
  DCHECK(notifier);
  notifier_ = std::move(notifier);
  notifier_->SubscribeToSigninEvents(this);
}

void PasswordReuseManagerImpl::SchedulePasswordHashUpdate(
    absl::optional<metrics_util::SignInState> sign_in_state_for_metrics) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!reuse_detector_) {
    return;
  }

  std::vector<PasswordHashData> protected_password_data_list =
      hash_password_manager_.RetrieveAllPasswordHashes();

  std::vector<PasswordHashData> gaia_password_hash_list;
  std::vector<PasswordHashData> enterprise_password_hash_list;
  for (PasswordHashData& password_hash : protected_password_data_list) {
    if (password_hash.is_gaia_password)
      gaia_password_hash_list.push_back(std::move(password_hash));
    else
      enterprise_password_hash_list.push_back(std::move(password_hash));
  }

  if (sign_in_state_for_metrics) {
    metrics_util::LogProtectedPasswordHashCounts(gaia_password_hash_list.size(),
                                                 *sign_in_state_for_metrics);
  }

  ScheduleTask(base::BindOnce(&PasswordReuseDetector::UseGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get()),
                              std::move(gaia_password_hash_list)));

  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::UseNonGaiaEnterprisePasswordHash,
                     base::Unretained(reuse_detector_.get()),
                     std::move(enterprise_password_hash_list)));
}

void PasswordReuseManagerImpl::ScheduleEnterprisePasswordURLUpdate() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!prefs_)
    return;
  std::vector<GURL> enterprise_login_urls;
  safe_browsing::GetPasswordProtectionLoginURLsPref(*prefs_,
                                                    &enterprise_login_urls);
  GURL enterprise_change_password_url =
      safe_browsing::GetPasswordProtectionChangePasswordURLPref(*prefs_);
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::UseEnterprisePasswordURLs,
                              base::Unretained(reuse_detector_.get()),
                              std::move(enterprise_login_urls),
                              std::move(enterprise_change_password_url)));
}

void PasswordReuseManagerImpl::RequestLoginsFromStores() {
  profile_store_->AddObserver(this);
  profile_store_->GetAutofillableLogins(
      /*consumer=*/weak_ptr_factory_.GetWeakPtr());
  if (account_store_) {
    account_store_->AddObserver(this);
    account_store_->GetAutofillableLogins(
        /*consumer=*/weak_ptr_factory_.GetWeakPtr());
    // base::Unretained() is safe because `this` outlives the subscription.
    account_store_cb_list_subscription_ =
        account_store_->AddSyncEnabledOrDisabledCallback(base::BindRepeating(
            &PasswordReuseManagerImpl::AccountStoreStateChanged,
            base::Unretained(this)));
  }
}

void PasswordReuseManagerImpl::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnGetPasswordStoreResults,
                              base::Unretained(reuse_detector_.get()),
                              std::move(results)));
}

void PasswordReuseManagerImpl::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const password_manager::PasswordStoreChangeList& changes) {
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnLoginsChanged,
                              base::Unretained(reuse_detector_.get()),
                              changes));
}

void PasswordReuseManagerImpl::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<PasswordForm>& retained_passwords) {
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnLoginsRetained,
                              base::Unretained(reuse_detector_.get()),
                              retained_passwords));
}

bool PasswordReuseManagerImpl::ScheduleTask(base::OnceClosure task) {
  return background_task_runner_ &&
         background_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void PasswordReuseManagerImpl::AccountStoreStateChanged() {
  DCHECK(account_store_);
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearCachedAccountStorePasswords,
                     base::Unretained(reuse_detector_.get())));
  account_store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

}  // namespace password_manager
