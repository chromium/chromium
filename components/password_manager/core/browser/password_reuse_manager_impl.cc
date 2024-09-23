// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif

using base::RecordAction;
using base::UserMetricsAction;

namespace password_manager {

namespace {

#if BUILDFLAG(IS_ANDROID)
// Time in seconds by which calls to the password store happening on startup
// should be delayed.
constexpr base::TimeDelta kPasswordStoreCallDelaySeconds = base::Seconds(5);
// Keys for accessing credentials passed from Android.
// Must be kept in sync with PasswordProtectionBroadcastReceiver.java.
constexpr char kLoginAccountIdentifier[] = "Login.accountIdentifier";
constexpr char kLoginHashedPassword[] = "Login.hashedPassword";
constexpr char kLoginSalt[] = "Login.salt";
#endif

bool IsPasswordReuseDetectionEnabled() {
  return base::FeatureList::IsEnabled(features::kPasswordReuseDetectionEnabled);
}

// Represents a single CheckReuse() request. Implements functionality to
// listen to reuse events and propagate them to |consumer| on the sequence on
// which CheckReuseRequest is created.
class CheckReuseRequest final : public PasswordReuseDetectorConsumer {
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
      std::optional<PasswordHashData> reused_protected_password_hash,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      int saved_passwords,
      const std::string& domain,
      uint64_t reused_password_hash) override;

  base::WeakPtr<PasswordReuseDetectorConsumer> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  const base::WeakPtr<PasswordReuseDetectorConsumer> consumer_weak_;
  base::WeakPtrFactory<CheckReuseRequest> weak_ptr_factory_{this};
};

CheckReuseRequest::CheckReuseRequest(PasswordReuseDetectorConsumer* consumer)
    : origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      consumer_weak_(consumer->AsWeakPtr()) {}

CheckReuseRequest::~CheckReuseRequest() = default;

void CheckReuseRequest::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    std::optional<PasswordHashData> reused_protected_password_hash,
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
  if (identity_manager_) {
    identity_manager_->RemoveObserver(this);
  }
  if (notifier_) {
    notifier_->UnsubscribeFromSigninEvents();
  }

  if (reuse_detector_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(reuse_detector_));
  }
}

void PasswordReuseManagerImpl::Init(
    PrefService* prefs,
    PrefService* local_prefs,
    PasswordStoreInterface* profile_store,
    PasswordStoreInterface* account_store,
    std::unique_ptr<PasswordReuseDetector> password_reuse_detector,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SharedPreferencesDelegate> shared_pref_delegate) {
  prefs_ = prefs;
  hash_password_manager_.set_prefs(prefs_);
  hash_password_manager_.set_local_prefs(local_prefs);
  hash_password_manager_.MigrateEnterprisePasswordHashes();
  identity_manager_ = identity_manager;
#if BUILDFLAG(IS_ANDROID)
  if (shared_pref_delegate) {
    shared_pref_delegate_ = std::move(shared_pref_delegate);
    identity_manager_->AddObserver(this);
  }
#endif
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  DCHECK(main_task_runner_);

  if (!IsPasswordReuseDetectionEnabled()) {
    return;
  }

  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
  DCHECK(profile_store);

  reuse_detector_ = std::move(password_reuse_detector);

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

void PasswordReuseManagerImpl::ReportMetrics(const std::string& username) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (username.empty()) {
    return;
  }

  auto hash_password_state =
      hash_password_manager_.HasPasswordHash(username,
                                             /*is_gaia_password=*/true)
          ? metrics_util::IsSyncPasswordHashSaved::SAVED_VIA_LIST_PREF
          : metrics_util::IsSyncPasswordHashSaved::NOT_SAVED;
  metrics_util::LogIsSyncPasswordHashSaved(hash_password_state);
}

void PasswordReuseManagerImpl::CheckReuse(
    const std::u16string& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!reuse_detector_) {
    consumer->OnReuseCheckDone(false, 0, std::nullopt, {}, 0, std::string(), 0);
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
    SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/std::nullopt);
  }
}

void PasswordReuseManagerImpl::SaveSyncPasswordHash(
    const PasswordHashData& sync_password_data,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (hash_password_manager_.SavePasswordHash(sync_password_data)) {
    metrics_util::LogGaiaPasswordHashChange(event,
                                            /*is_sync_password=*/true);
    SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/std::nullopt);
  }
}

void PasswordReuseManagerImpl::ClearGaiaPasswordHash(
    const std::string& username) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearSavedPasswordHash(username,
                                                /*is_gaia_password=*/true);
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get()),
                              username));
}

void PasswordReuseManagerImpl::ClearAllGaiaPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ true);
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearAllGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::ClearAllEnterprisePasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllPasswordHash(/* is_gaia_password= */ false);
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllEnterprisePasswordHash,
                     base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::ClearAllNonGmailPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  hash_password_manager_.ClearAllNonGmailPasswordHash();
  if (!reuse_detector_) {
    return;
  }
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

void PasswordReuseManagerImpl::SetPasswordReuseManagerSigninNotifier(
    std::unique_ptr<PasswordReuseManagerSigninNotifier> notifier) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!notifier_);
  DCHECK(notifier);
  notifier_ = std::move(notifier);
  notifier_->SubscribeToSigninEvents(this);
}

void PasswordReuseManagerImpl::SchedulePasswordHashUpdate(
    std::optional<metrics_util::SignInState> sign_in_state_for_metrics) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());

  if (!reuse_detector_) {
    return;
  }

  std::vector<PasswordHashData> protected_password_data_list =
      hash_password_manager_.RetrieveAllPasswordHashes();

  std::vector<PasswordHashData> gaia_password_hash_list;
  std::vector<PasswordHashData> enterprise_password_hash_list;
  for (PasswordHashData& password_hash : protected_password_data_list) {
    if (password_hash.is_gaia_password) {
      gaia_password_hash_list.push_back(std::move(password_hash));
    } else {
      enterprise_password_hash_list.push_back(std::move(password_hash));
    }
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

  if (!prefs_) {
    return;
  }
  std::vector<GURL> enterprise_login_urls;
  safe_browsing::GetPasswordProtectionLoginURLsPref(*prefs_,
                                                    &enterprise_login_urls);
  GURL enterprise_change_password_url =
      safe_browsing::GetPasswordProtectionChangePasswordURLPref(*prefs_);
  if (!reuse_detector_) {
    return;
  }
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
  if (!reuse_detector_) {
    return;
  }
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
  PasswordForm::Store store_type = store == account_store_
                                       ? PasswordForm::Store::kAccountStore
                                       : PasswordForm::Store::kProfileStore;
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnLoginsRetained,
                              base::Unretained(reuse_detector_.get()),
                              store_type, retained_passwords));
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

void PasswordReuseManagerImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (!shared_pref_delegate_) {
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // Check for a Chrome sign-in event.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    // On Android, check if there are any gaia credentials saved for this user.
    auto saved_creds = shared_pref_delegate_->GetCredentials("");
    if (saved_creds.empty()) {
      return;
    }
    auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
        saved_creds, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!parsed_json.has_value()) {
      LOG(ERROR) << "Error parsing JSON: " << parsed_json.error().message;
      return;
    }
    if (!parsed_json->is_list()) {
      LOG(ERROR) << "Error parsing JSON: Expected a list but got non-list.";
      return;
    }
    auto& saved_creds_list = parsed_json->GetList();
    if (saved_creds_list.empty()) {
      return;
    }
    for (size_t i = 0; i < saved_creds_list.size(); i++) {
      base::Value::Dict* saved_creds_entry = saved_creds_list[i].GetIfDict();
      const std::string* account_id =
          saved_creds_entry->FindString(kLoginAccountIdentifier);
      CHECK(account_id);
      if (identity_manager_
              ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email == *account_id) {
        PasswordHashData password_hash_data;
        password_hash_data.username = gaia::CanonicalizeEmail(*account_id);
        password_hash_data.length = 8;  // Min size for gaia passwords is 8.
        password_hash_data.salt = *saved_creds_entry->FindString(kLoginSalt);
        password_hash_data.hash = static_cast<uint64_t>(
            saved_creds_entry->FindDouble(kLoginHashedPassword).value());
        password_hash_data.force_update = true;
        hash_password_manager_.SavePasswordHash(password_hash_data);
        SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/std::nullopt);
        metrics_util::LogGaiaPasswordHashChange(
            metrics_util::GaiaPasswordHashChange::SAVED_ON_CHROME_SIGNIN,
            /*is_sync_password=*/true);
        // Remove the saved credential that matched the signed in user.
        saved_creds_list.EraseValue(saved_creds_list[i]);
        shared_pref_delegate_->SetCredentials(
            base::WriteJson(saved_creds_list).value());
        break;
      }
    }
  }
#endif
}

void PasswordReuseManagerImpl::MaybeSavePasswordHash(
    const PasswordForm* submitted_form,
    PasswordManagerClient* client) {
  if (!base::FeatureList::IsEnabled(features::kPasswordReuseDetectionEnabled)) {
    return;
  }
  // When |username_value| is empty, it's not clear whether the submitted
  // credentials are really Gaia or enterprise credentials. Don't save
  // password hash in that case.
  std::string username = base::UTF16ToUTF8(submitted_form->username_value);
  if (username.empty()) {
    return;
  }

  bool should_save_enterprise_pw =
      client->GetStoreResultFilter()->ShouldSaveEnterprisePasswordHash(
          *submitted_form);
  bool should_save_gaia_pw =
      client->GetStoreResultFilter()->ShouldSaveGaiaPasswordHash(
          *submitted_form);

  if (!should_save_enterprise_pw && !should_save_gaia_pw) {
    return;
  }

  if (password_manager_util::IsLoggingActive(client)) {
    BrowserSavePasswordProgressLogger logger(client->GetLogManager());
    logger.LogMessage(
        autofill::SavePasswordProgressLogger::STRING_SAVE_PASSWORD_HASH);
  }

  // Canonicalizes username if it is an email.
  if (username.find('@') != std::string::npos) {
    username = gaia::CanonicalizeEmail(username);
  }
  bool is_password_change = !submitted_form->new_password_element.empty();
  const std::u16string password = is_password_change
                                      ? submitted_form->new_password_value
                                      : submitted_form->password_value;

  if (should_save_enterprise_pw) {
    SaveEnterprisePasswordHash(username, password);
    return;
  }

  CHECK(should_save_gaia_pw);
  bool is_sync_account_email =
      client->GetStoreResultFilter()->IsSyncAccountEmail(username);
  metrics_util::GaiaPasswordHashChange event =
      is_sync_account_email
          ? (is_password_change
                 ? metrics_util::GaiaPasswordHashChange::CHANGED_IN_CONTENT_AREA
                 : metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA)
          : (is_password_change
                 ? metrics_util::GaiaPasswordHashChange::
                       NOT_SYNC_PASSWORD_CHANGE
                 : metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA);
  SaveGaiaPasswordHash(username, password,
                       /*is_sync_password_for_metrics=*/is_sync_account_email,
                       event);
}
}  // namespace password_manager
