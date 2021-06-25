// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager.h"

#include "base/bind.h"
#include "base/task/thread_pool.h"
#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace password_manager {

namespace {

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
      int saved_passwords) override;

 private:
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  const base::WeakPtr<PasswordReuseDetectorConsumer> consumer_weak_;
};

CheckReuseRequest::CheckReuseRequest(PasswordReuseDetectorConsumer* consumer)
    : origin_task_runner_(base::SequencedTaskRunnerHandle::Get()),
      consumer_weak_(consumer->AsWeakPtr()) {}

CheckReuseRequest::~CheckReuseRequest() = default;

void CheckReuseRequest::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    absl::optional<PasswordHashData> reused_protected_password_hash,
    const std::vector<MatchingReusedCredential>& matching_reused_credentials,
    int saved_passwords) {
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordReuseDetectorConsumer::OnReuseCheckDone,
                     consumer_weak_, is_reuse_found, password_length,
                     reused_protected_password_hash,
                     matching_reused_credentials, saved_passwords));
}

void CheckReuseHelper(std::unique_ptr<CheckReuseRequest> request,
                      const std::u16string& input,
                      const std::string& domain,
                      PasswordReuseDetector* reuse_detector) {
  reuse_detector->CheckReuse(input, domain, request.get());
}

}  // namespace

PasswordReuseManager::PasswordReuseManager() = default;

PasswordReuseManager::~PasswordReuseManager() {
  if (notifier_)
    notifier_->UnsubscribeFromSigninEvents();

  if (reuse_detector_) {
    background_task_runner_->DeleteSoon(FROM_HERE, reuse_detector_);
    reuse_detector_ = nullptr;
  }
}

void PasswordReuseManager::Init(PrefService* prefs,
                                PasswordStoreInterface* store) {
  prefs_ = prefs;
  hash_password_manager_.set_prefs(prefs_);
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  DCHECK(main_task_runner_);

  if (IsPasswordReuseDetectionEnabled()) {
    background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    DCHECK(background_task_runner_);

    reuse_detector_ = new PasswordReuseDetector();
    DCHECK(store);
    ScheduleTask(base::BindOnce(&PasswordReuseDetector::Init,
                                base::Unretained(reuse_detector_),
                                base::RetainedRef(store)));
    store->GetAutofillableLogins(this);
  }
}

void PasswordReuseManager::ReportMetrics(const std::string& username,
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

void PasswordReuseManager::CheckReuse(const std::u16string& input,
                                      const std::string& domain,
                                      PasswordReuseDetectorConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!reuse_detector_) {
    consumer->OnReuseCheckDone(false, 0, absl::nullopt, {}, 0);
    return;
  }
  ScheduleTask(base::BindOnce(
      &CheckReuseHelper, std::make_unique<CheckReuseRequest>(consumer), input,
      domain, base::Unretained(reuse_detector_)));
}

void PasswordReuseManager::PreparePasswordHashData(
    const std::string& sync_username,
    const bool is_signed_in) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  SchedulePasswordHashUpdate(/*should_log_metrics=*/true,
                             !sync_username.empty(), is_signed_in);
  ScheduleEnterprisePasswordURLUpdate();
}

void PasswordReuseManager::SaveGaiaPasswordHash(const std::string& username,
                                                const std::u16string& password,
                                                bool is_primary_account,
                                                GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  SaveProtectedPasswordHash(username, password, is_primary_account,
                            /*is_gaia_password=*/true, event);
}

void PasswordReuseManager::SaveEnterprisePasswordHash(
    const std::string& username,
    const std::u16string& password) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  SaveProtectedPasswordHash(
      username, password, /*is_primary_account=*/false,
      /*is_gaia_password=*/false,
      GaiaPasswordHashChange::NON_GAIA_ENTERPRISE_PASSWORD_CHANGE);
}

void PasswordReuseManager::SaveProtectedPasswordHash(
    const std::string& username,
    const std::u16string& password,
    bool is_primary_account,
    bool is_gaia_password,
    GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (hash_password_manager_.SavePasswordHash(username, password,
                                              is_gaia_password)) {
    if (is_gaia_password) {
      metrics_util::LogGaiaPasswordHashChange(event, is_primary_account);
    }
    // This method is not being called on startup so it shouldn't log metrics.
    // |is_signed_in| is only used when |should_log_metrics| is true so
    // it doesn't matter what the value is here.
    SchedulePasswordHashUpdate(/*should_log_metrics=*/false, is_primary_account,
                               /*is_signed_in=*/false);
  }
}

void PasswordReuseManager::SaveSyncPasswordHash(
    const PasswordHashData& sync_password_data,
    GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (hash_password_manager_.SavePasswordHash(sync_password_data)) {
    metrics_util::LogGaiaPasswordHashChange(event,
                                            /*is_sync_password=*/true);
    SchedulePasswordHashUpdate(/*should_log_metrics=*/false,
                               /*does_primary_account_exists=*/false,
                               /*is_signed_in=*/false);
  }
}

void PasswordReuseManager::ClearGaiaPasswordHash(const std::string& username) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearSavedPasswordHash(username,
                                                /*is_gaia_password=*/true);
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearGaiaPasswordHash,
                              base::Unretained(reuse_detector_), username));
}

void PasswordReuseManager::ClearAllGaiaPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ true);
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearAllGaiaPasswordHash,
                              base::Unretained(reuse_detector_)));
}

void PasswordReuseManager::ClearAllEnterprisePasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ false);
  if (!reuse_detector_)
    return;
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllEnterprisePasswordHash,
                     base::Unretained(reuse_detector_)));
}

void PasswordReuseManager::ClearAllNonGmailPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllNonGmailPasswordHash();
  if (!reuse_detector_)
    return;
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllNonGmailPasswordHash,
                     base::Unretained(reuse_detector_)));
}

base::CallbackListSubscription
PasswordReuseManager::RegisterStateCallbackOnHashPasswordManager(
    const base::RepeatingCallback<void(const std::string& username)>&
        callback) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  return hash_password_manager_.RegisterStateCallback(callback);
}

void PasswordReuseManager::SetPasswordStoreSigninNotifier(
    std::unique_ptr<PasswordStoreSigninNotifier> notifier) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!notifier_);
  DCHECK(notifier);
  notifier_ = std::move(notifier);
  // TODO(crbug.bom/715987): Uncomment when PasswordReuseManager is used to
  // monitor Signin events. notifier_->SubscribeToSigninEvents(this);
}

void PasswordReuseManager::SchedulePasswordHashUpdate(
    bool should_log_metrics,
    bool does_primary_account_exists,
    bool is_signed_in) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  PasswordHashDataList protected_password_data_list =
      hash_password_manager_.RetrieveAllPasswordHashes();

  if (!reuse_detector_ || !protected_password_data_list.has_value())
    return;

  std::vector<PasswordHashData> gaia_password_hash_list;
  std::vector<PasswordHashData> enterprise_password_hash_list;
  for (PasswordHashData& password_hash : *protected_password_data_list) {
    if (password_hash.is_gaia_password)
      gaia_password_hash_list.push_back(std::move(password_hash));
    else
      enterprise_password_hash_list.push_back(std::move(password_hash));
  }

  if (should_log_metrics) {
    metrics_util::LogProtectedPasswordHashCounts(
        gaia_password_hash_list.size(), enterprise_password_hash_list.size(),
        does_primary_account_exists, is_signed_in);
  }

  ScheduleTask(base::BindOnce(&PasswordReuseDetector::UseGaiaPasswordHash,
                              base::Unretained(reuse_detector_),
                              std::move(gaia_password_hash_list)));

  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::UseNonGaiaEnterprisePasswordHash,
                     base::Unretained(reuse_detector_),
                     std::move(enterprise_password_hash_list)));
}

void PasswordReuseManager::ScheduleEnterprisePasswordURLUpdate() {
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
                              base::Unretained(reuse_detector_),
                              std::move(enterprise_login_urls),
                              std::move(enterprise_change_password_url)));
}

void PasswordReuseManager::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!reuse_detector_)
    return;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnGetPasswordStoreResults,
                              base::Unretained(reuse_detector_),
                              std::move(results)));
}

bool PasswordReuseManager::ScheduleTask(base::OnceClosure task) {
  return background_task_runner_ &&
         background_task_runner_->PostTask(FROM_HERE, std::move(task));
}

}  // namespace password_manager
