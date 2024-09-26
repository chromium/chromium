// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service.h"

#include <memory>
#include <random>
#include <vector>

#include "base/barrier_closure.h"
#include "base/json/values_util.h"
#include "base/metrics/user_metrics.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/ui/safety_hub/password_status_check_result.h"
#include "chrome/browser/ui/safety_hub/safety_hub_constants.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

using password_manager::InsecureType;

namespace {

// The map for each day to hold pref name as key and weight as the value.
std::map<std::string, int> GetDayPrefWeightMap() {
  return {{safety_hub_prefs::kPasswordCheckMonWeight,
           features::kPasswordCheckMonWeight.Get()},
          {safety_hub_prefs::kPasswordCheckTueWeight,
           features::kPasswordCheckTueWeight.Get()},
          {safety_hub_prefs::kPasswordCheckWedWeight,
           features::kPasswordCheckWedWeight.Get()},
          {safety_hub_prefs::kPasswordCheckThuWeight,
           features::kPasswordCheckThuWeight.Get()},
          {safety_hub_prefs::kPasswordCheckFriWeight,
           features::kPasswordCheckFriWeight.Get()},
          {safety_hub_prefs::kPasswordCheckSatWeight,
           features::kPasswordCheckSatWeight.Get()},
          {safety_hub_prefs::kPasswordCheckSunWeight,
           features::kPasswordCheckSunWeight.Get()}};
}

// Password check will be scheduled randomly but based on the weights of the
// days to prevent load on Mondays. This function finds a new check time
// randomly while respecting the weights.
base::TimeDelta FindNewCheckTime() {
  // The password check will be scheculed starting from tomorrow.
  base::Time tomorrow_midnight =
      base::Time::Now().LocalMidnight() + base::Days(1);
  base::Time::Exploded tomorrow_midnight_exploded;
  tomorrow_midnight.LocalExplode(&tomorrow_midnight_exploded);
  int tomorrow_day_of_week = tomorrow_midnight_exploded.day_of_week;

  // Hold the weights of days in a vector.
  std::vector<int> weight_for_days{features::kPasswordCheckSunWeight.Get(),
                                   features::kPasswordCheckMonWeight.Get(),
                                   features::kPasswordCheckTueWeight.Get(),
                                   features::kPasswordCheckWedWeight.Get(),
                                   features::kPasswordCheckThuWeight.Get(),
                                   features::kPasswordCheckFriWeight.Get(),
                                   features::kPasswordCheckSatWeight.Get()};

  // Generate weighted random distribution for the following N days, where N is
  // kBackgroundPasswordCheckInterval.
  const int update_interval_in_days =
      features::kBackgroundPasswordCheckInterval.Get().InDays();
  std::vector<int> weights(update_interval_in_days);
  for (int i = 0; i < update_interval_in_days; i++) {
    int day_of_week = tomorrow_day_of_week + i;
    weights[i] = weight_for_days[day_of_week % 7];
  }

  // Select an offset from the days with discrete_distribution. The check will
  // run after as many days later as the selected_day_offset.
  std::discrete_distribution<int> distribution(weights.begin(), weights.end());
  base::RandomBitGenerator generator;
  int selected_day_offset = distribution(generator);

  // Find a random time in the selected day.
  base::Time beg_of_selected_day =
      tomorrow_midnight + base::Days(selected_day_offset);
  base::Time end_of_selected_day = beg_of_selected_day + base::Days(1);

  // Pick a random time in the selected day.
  return base::RandTimeDelta(beg_of_selected_day.ToDeltaSinceWindowsEpoch(),
                             end_of_selected_day.ToDeltaSinceWindowsEpoch());
}

// Returns true if a new check time should be saved. This is the case when:
// - There is no existing time available, e.g. in first run.
// - The configuration for the interval has changed. This is to ensure changes
//   in the interval are applied without large delays in case the interval is
//   so short that it exceeds backend capacity.
// - The configuration for the weights of each day has changed. This is to
//   ensure changes in the weights are applied as soon as browser is started,
//   instead of waiting for the next run.
bool ShouldFindNewCheckTime(Profile* profile) {
  // The pref dict looks like this:
  // {
  //   ...
  //   kBackgroundPasswordCheckTimeAndInterval: {
  //     kPasswordCheckIntervalKey: "1728000000000",
  //     kNextPasswordCheckTimeKey: "13333556059805713",
  //     kPasswordCheckMonWeight: "8"
  //     kPasswordCheckTueWeight: "8"
  //     kPasswordCheckWedWeight: "8"
  //     kPasswordCheckThuWeight: "8"
  //     kPasswordCheckFriWeight: "8"
  //     kPasswordCheckSatWeight: "8"
  //     kPasswordCheckSunWeight: "8"
  //   },
  //   ...
  // }
  const base::Value::Dict& check_schedule_dict = profile->GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);

  // If the check time is not set yet, a new check time should be found.
  bool uninitialized =
      !check_schedule_dict.Find(safety_hub_prefs::kNextPasswordCheckTimeKey) ||
      !check_schedule_dict.Find(safety_hub_prefs::kPasswordCheckIntervalKey);
  if (uninitialized) {
    return true;
  }

  // If the current check interval length is different than the scheduled one, a
  // new check time should be found.
  base::TimeDelta update_interval =
      features::kBackgroundPasswordCheckInterval.Get();
  std::optional<base::TimeDelta> interval_used_for_scheduling =
      base::ValueToTimeDelta(check_schedule_dict.Find(
          safety_hub_prefs::kPasswordCheckIntervalKey));
  if (!interval_used_for_scheduling.has_value() ||
      update_interval != interval_used_for_scheduling.value()) {
    return true;
  }

  // If the weight for any day is different than the previous one, a new check
  // time should be found.
  auto map = GetDayPrefWeightMap();
  for (auto day = map.begin(); day != map.end(); day++) {
    std::optional<int> old_weight_val = check_schedule_dict.FindInt(day->first);
    // When the first time the weights are introduced, the old weight values
    // will be non-set. In this case, schedule time should reset.
    if (!old_weight_val.has_value()) {
      return true;
    }

    int new_weight = day->second;
    if (old_weight_val.value() != new_weight) {
      return true;
    }
  }

  return false;
}

// Helper functions for displaying passwords in the UI
base::Value::Dict GetCompromisedPasswordCardData(int compromised_count) {
  base::Value::Dict result;

  result.Set(safety_hub::kCardHeaderKey,
             l10n_util::GetPluralStringFUTF16(
                 IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_COUNT,
                 compromised_count));
  result.Set(safety_hub::kCardSubheaderKey,
             l10n_util::GetPluralStringFUTF16(
                 IDS_PASSWORD_MANAGER_UI_HAS_COMPROMISED_PASSWORDS,
                 compromised_count));
  result.Set(safety_hub::kCardStateKey,
             static_cast<int>(safety_hub::SafetyHubCardState::kWarning));
  return result;
}

base::Value::Dict GetReusedPasswordCardData(int reused_count, bool signed_in) {
  base::Value::Dict result;

  result.Set(safety_hub::kCardHeaderKey,
             l10n_util::GetPluralStringFUTF16(
                 IDS_PASSWORD_MANAGER_UI_REUSED_PASSWORDS_COUNT, reused_count));
  result.Set(
      safety_hub::kCardSubheaderKey,
      l10n_util::GetStringUTF16(
          signed_in
              ? IDS_PASSWORD_MANAGER_UI_HAS_REUSED_PASSWORDS
              : IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SIGN_IN));
  result.Set(safety_hub::kCardStateKey,
             static_cast<int>(safety_hub::SafetyHubCardState::kWeak));
  return result;
}

base::Value::Dict GetWeakPasswordCardData(int weak_count, bool signed_in) {
  base::Value::Dict result;

  result.Set(safety_hub::kCardHeaderKey,
             l10n_util::GetPluralStringFUTF16(
                 IDS_PASSWORD_MANAGER_UI_WEAK_PASSWORDS_COUNT, weak_count));
  result.Set(
      safety_hub::kCardSubheaderKey,
      l10n_util::GetStringUTF16(
          signed_in
              ? IDS_PASSWORD_MANAGER_UI_HAS_WEAK_PASSWORDS
              : IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SIGN_IN));
  result.Set(safety_hub::kCardStateKey,
             static_cast<int>(safety_hub::SafetyHubCardState::kWeak));
  return result;
}

base::Value::Dict GetSafePasswordCardData(base::Time last_check) {
  base::Value::Dict result;

  result.Set(safety_hub::kCardHeaderKey,
             l10n_util::GetPluralStringFUTF16(
                 IDS_PASSWORD_MANAGER_UI_COMPROMISED_PASSWORDS_COUNT, 0));
  // The subheader string depends on how much time has passed since the last
  // check.
  base::TimeDelta time_delta = base::Time::Now() - last_check;
  if (time_delta < base::Minutes(1)) {
    result.Set(safety_hub::kCardSubheaderKey,
               l10n_util::GetStringUTF16(
                   IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_RECENTLY));
  } else {
    std::u16string last_check_string =
        ui::TimeFormat::Simple(ui::TimeFormat::Format::FORMAT_DURATION,
                               ui::TimeFormat::Length::LENGTH_LONG, time_delta);
    result.Set(
        safety_hub::kCardSubheaderKey,
        l10n_util::GetStringFUTF16(
            IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SOME_TIME_AGO,
            last_check_string));
  }
  result.Set(safety_hub::kCardStateKey,
             static_cast<int>(safety_hub::SafetyHubCardState::kSafe));
  return result;
}

base::Value::Dict GetNoWeakOrReusedPasswordCardData(bool signed_in) {
  base::Value::Dict result;
  result.Set(
      safety_hub::kCardHeaderKey,
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_HEADER_NO_WEAK_OR_REUSED));
  result.Set(
      safety_hub::kCardSubheaderKey,
      l10n_util::GetStringUTF16(
          signed_in
              ? IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_GO_TO_PASSWORD_MANAGER
              : IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_SIGN_IN));
  result.Set(safety_hub::kCardStateKey,
             static_cast<int>(safety_hub::SafetyHubCardState::kInfo));
  return result;
}

base::Value::Dict GetNoPasswordCardData(bool password_saving_allowed) {
  base::Value::Dict result;

  result.Set(safety_hub::kCardHeaderKey,
             l10n_util::GetStringUTF16(
                 IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_HEADER_NO_PASSWORDS));
  result.Set(
      safety_hub::kCardSubheaderKey,
      l10n_util::GetStringUTF16(
          password_saving_allowed
              ? IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_NO_PASSWORDS
              : IDS_SETTINGS_SAFETY_HUB_PASSWORD_CHECK_SUBHEADER_NO_PASSWORDS_POLICY));
  result.Set(safety_hub::kCardStateKey,
             static_cast<int>(safety_hub::SafetyHubCardState::kInfo));
  return result;
}

bool ShouldAddToCompromisedPasswords(
    const password_manager::PasswordForm form) {
  auto& issues = form.password_issues;

  // If the password is leaked but muted, then do not add to compromised
  // passwords.
  if (issues.contains(InsecureType::kLeaked) &&
      issues.at(InsecureType::kLeaked).is_muted) {
    return false;
  }

  // If the password is phished but muted, then do not add to compromised
  // passwords.
  if (issues.contains(InsecureType::kPhished) &&
      issues.at(InsecureType::kPhished).is_muted) {
    return false;
  }

  // Add to compromised passwords, if leaked or phished
  return issues.contains(InsecureType::kLeaked) ||
         issues.contains(InsecureType::kPhished);
}

// Returns saved password forms number
int GetSavedPasswordsCount(
    password_manager::SavedPasswordsPresenter* saved_passwords_presenter) {
  CHECK(saved_passwords_presenter);
  const auto& credential_entries =
      saved_passwords_presenter->GetSavedPasswords();
  int saved_password_forms = 0;
  // Each CredentialUIEntry may contain one or more password forms.
  for (const auto& entry : credential_entries) {
    saved_password_forms = saved_password_forms + entry.facets.size();
  }
  return saved_password_forms;
}

}  // namespace

PasswordStatusCheckService::PasswordStatusCheckService(Profile* profile)
    : profile_(profile) {
  scoped_refptr<password_manager::PasswordStoreInterface> profile_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);

  scoped_refptr<password_manager::PasswordStoreInterface> account_store =
      AccountPasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);

  // ProfilePasswordStore may not exist for some cases like non-user profiles on
  // Ash. If ProfilePasswordStore does not exist, the service should not be
  // created by the factory.
  DCHECK(profile_store);

  profile_password_store_observation_.Observe(profile_store.get());
  if (account_store) {
    account_password_store_observation_.Observe(account_store.get());
  }

  StartRepeatedUpdates();
  UpdateInsecureCredentialCountAsync();
}

PasswordStatusCheckService::~PasswordStatusCheckService() = default;

void PasswordStatusCheckService::Shutdown() {
  password_check_timer_.Stop();
  bulk_leak_check_observation_.Reset();
  insecure_credentials_manager_observation_.Reset();
  profile_password_store_observation_.Reset();
  account_password_store_observation_.Reset();

  bulk_leak_check_service_adapter_.reset();
  insecure_credentials_manager_.reset();
  saved_passwords_presenter_.reset();
}

bool PasswordStatusCheckService::is_password_check_running() const {
  return BulkLeakCheckServiceFactory::GetForProfile(profile_)->GetState() ==
         password_manager::BulkLeakCheckServiceInterface::State::kRunning;
}

void PasswordStatusCheckService::StartRepeatedUpdates() {
  if (ShouldFindNewCheckTime(profile_)) {
    SetPasswordCheckSchedulePrefsWithInterval(
        base::Time::FromDeltaSinceWindowsEpoch(FindNewCheckTime()));
  }

  // If the scheduled time for the password check is not yet overdue, it should
  // run at that time. If password check is overdue, pick a random time in the
  // next hour.
  base::TimeDelta password_check_run_delta =
      GetScheduledPasswordCheckTime() - base::Time::Now();
  if (password_check_run_delta.is_negative()) {
    password_check_run_delta =
        base::RandTimeDeltaUpTo(features::kPasswordCheckOverdueInterval.Get());
  }

  // Check compromised passwords with the interval of password_check_run_delta.
  password_check_timer_.Start(
      FROM_HERE, password_check_run_delta,
      base::BindOnce(
          &PasswordStatusCheckService::
              MaybeInitializePasswordCheckInfrastructure,
          weak_ptr_factory_.GetWeakPtr(),
          base::BindOnce(&PasswordStatusCheckService::RunPasswordCheckAsync,
                         weak_ptr_factory_.GetWeakPtr())));

  // Check weak and reuse passwords daily.
  weak_and_reuse_check_timer_.Start(
      FROM_HERE, base::Days(1),
      base::BindRepeating(
          &PasswordStatusCheckService::UpdateInsecureCredentialCountAsync,
          weak_ptr_factory_.GetWeakPtr()));
}

void PasswordStatusCheckService::UpdateInsecureCredentialCountAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In infrastructure is initilizided OnInsecureCredentialsChanged() will
  // update cache anyway.
  if (IsInfrastructureReady()) {
    return;
  }

  // Initializing the infra will cause the check for weak/reused passwords.
  MaybeInitializePasswordCheckInfrastructure(
      base::BindOnce(&PasswordStatusCheckService::RunWeakReusedCheckAsync,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordStatusCheckService::RunPasswordCheckAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // In case there is a running check, do not start checks again.
  if (is_password_check_running()) {
    return;
  }

  // InitializePasswordCheckInfrastructure may return before the infra is ready.
  // This is unexpected to happen just before the password check starts.
  CHECK(IsInfrastructureReady());

  saved_credential_count_ =
      GetSavedPasswordsCount(saved_passwords_presenter_.get());

  bulk_leak_check_service_adapter_->StartBulkLeakCheck(
      password_manager::LeakDetectionInitiator::
          kDesktopProactivePasswordCheckup);

  // In case there is a running check, do not start checks again.
  RunWeakReusedCheckAsync();
  base::RecordAction(base::UserMetricsAction("SafetyHub_PasswordCheckRun"));
}

void PasswordStatusCheckService::RunWeakReusedCheckAsync() {
  CHECK(IsInfrastructureReady());

  saved_credential_count_ =
      GetSavedPasswordsCount(saved_passwords_presenter_.get());

  if (std::exchange(running_weak_reused_check_, true)) {
    // Return early if the check is already running.
    return;
  }

  base::RepeatingClosure on_done = base::BarrierClosure(
      /*num_closures=*/2,
      base::BindOnce(&PasswordStatusCheckService::OnWeakAndReuseChecksDone,
                     weak_ptr_factory_.GetWeakPtr()));
  insecure_credentials_manager_->StartWeakCheck(on_done);
  insecure_credentials_manager_->StartReuseCheck(on_done);
}

void PasswordStatusCheckService::OnWeakAndReuseChecksDone() {
  // Mark the check as completed.
  CHECK(running_weak_reused_check_);
  running_weak_reused_check_ = false;

  UpdateInsecureCredentialCount();
  MaybeResetInfrastructure();
}

void PasswordStatusCheckService::OnStateChanged(
    password_manager::BulkLeakCheckServiceInterface::State state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(IsInfrastructureReady());

  switch (state) {
    case password_manager::BulkLeakCheckServiceInterface::State::kRunning:
      return;
    case password_manager::BulkLeakCheckService::State::kServiceError:
    case password_manager::BulkLeakCheckServiceInterface::State::kIdle:
    case password_manager::BulkLeakCheckServiceInterface::State::kCanceled:
    case password_manager::BulkLeakCheckServiceInterface::State::kSignedOut:
    case password_manager::BulkLeakCheckServiceInterface::State::
        kTokenRequestFailure:
    case password_manager::BulkLeakCheckServiceInterface::State::
        kHashingFailure:
    case password_manager::BulkLeakCheckServiceInterface::State::kNetworkError:
    case password_manager::BulkLeakCheckServiceInterface::State::kQuotaLimit: {
      ScheduleCheckAndStartRepeatedUpdates();
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&PasswordStatusCheckService::MaybeResetInfrastructure,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void PasswordStatusCheckService::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  CHECK(IsInfrastructureReady());
  if (is_leaked) {
    insecure_credentials_manager_->SaveInsecureCredential(
        credential, password_manager::TriggerBackendNotification(true));
  }
}

void PasswordStatusCheckService::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const password_manager::PasswordStoreChangeList& changes) {
  // latest_result_ might be null during start up, if
  // `UpdateInsecureCredentialCountAsync` is not run yet. Ignore
  // `OnLoginsChanged` call in that case, since weak and reuse checks will be
  // run after start up is completed.
  if (!latest_result_) {
    return;
  }

  std::vector<password_manager::PasswordForm> forms_to_add;
  std::vector<password_manager::PasswordForm> forms_to_remove;
  for (const password_manager::PasswordStoreChange& change : changes) {
    // Ignore federated or blocked entries.
    const auto& form = change.form();
    if (form.IsFederatedCredential() || form.blocked_by_user) {
      continue;
    }
    switch (change.type()) {
      case password_manager::PasswordStoreChange::ADD:
        forms_to_add.push_back(form);
        break;
      case password_manager::PasswordStoreChange::UPDATE:
        forms_to_remove.push_back(form);
        forms_to_add.push_back(form);
        break;
      case password_manager::PasswordStoreChange::REMOVE:
        forms_to_remove.push_back(form);
        break;
    }
  }

  const std::set<PasswordPair>& stored_password =
      latest_result_->GetCompromisedPasswords();
  std::set<PasswordPair> updated_passwords = stored_password;

  // Remove deleted forms
  for (const auto& form : forms_to_remove) {
    saved_credential_count_--;
    updated_passwords.erase(
        PasswordPair(form.url.spec(), base::UTF16ToUTF8(form.username_value)));
  }

  // Add new forms
  for (const auto& form : forms_to_add) {
    saved_credential_count_++;
    if (ShouldAddToCompromisedPasswords(form)) {
      updated_passwords.insert(PasswordPair(
          form.url.spec(), base::UTF16ToUTF8(form.username_value)));
    }
  }

  // Update cached values
  latest_result_ = std::make_unique<PasswordStatusCheckResult>();
  compromised_credential_count_ = 0;
  for (const PasswordPair& password : updated_passwords) {
    compromised_credential_count_++;
    latest_result_->AddToCompromisedPasswords(password.origin,
                                              password.username);
  }
}

void PasswordStatusCheckService::OnLoginsRetained(
    password_manager::PasswordStoreInterface* store,
    const std::vector<password_manager::PasswordForm>& retained_passwords) {}

void PasswordStatusCheckService::OnInsecureCredentialsChanged() {
  CHECK(IsInfrastructureReady());
  UpdateInsecureCredentialCount();
}

void PasswordStatusCheckService::MaybeInitializePasswordCheckInfrastructure(
    base::OnceClosure completion) {
  if (IsInfrastructureReady()) {
    std::move(completion).Run();
    return;
  }

  // ProfilePasswordStore must exist, otherwise the infrastructure couldn't have
  // been initialized and this method could never have been called.
  scoped_refptr<password_manager::PasswordStoreInterface> profile_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  CHECK(profile_store);

  saved_passwords_presenter_ =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          AffiliationServiceFactory::GetForProfile(profile_), profile_store,
          AccountPasswordStoreFactory::GetForProfile(
              profile_, ServiceAccessType::IMPLICIT_ACCESS));
  saved_passwords_presenter_->Init(std::move(completion));

  insecure_credentials_manager_ =
      std::make_unique<password_manager::InsecureCredentialsManager>(
          saved_passwords_presenter_.get());

  bulk_leak_check_service_adapter_ =
      std::make_unique<password_manager::BulkLeakCheckServiceAdapter>(
          saved_passwords_presenter_.get(),
          BulkLeakCheckServiceFactory::GetForProfile(profile_),
          profile_->GetPrefs());

  // Observe new BulkLeakCheckService.
  bulk_leak_check_observation_.Reset();
  bulk_leak_check_observation_.Observe(
      BulkLeakCheckServiceFactory::GetForProfile(profile_));

  insecure_credentials_manager_observation_.Reset();
  insecure_credentials_manager_observation_.Observe(
      insecure_credentials_manager_.get());
}

void PasswordStatusCheckService::UpdateInsecureCredentialCount() {
  CHECK(IsInfrastructureReady());
  auto insecure_credentials =
      insecure_credentials_manager_->GetInsecureCredentialEntries();

  compromised_credential_count_ = 0;
  weak_credential_count_ = 0;
  reused_credential_count_ = 0;

  latest_result_ = std::make_unique<PasswordStatusCheckResult>();
  for (const auto& entry : insecure_credentials) {
    if (entry.IsMuted()) {
      continue;
    }
    if (password_manager::IsCompromised(entry)) {
      compromised_credential_count_++;
      latest_result_->AddToCompromisedPasswords(
          entry.GetURL().spec(), base::UTF16ToUTF8(entry.username));
    }
    if (entry.IsWeak()) {
      weak_credential_count_++;
    }
    if (entry.IsReused()) {
      reused_credential_count_++;
    }
  }
}

void PasswordStatusCheckService::MaybeResetInfrastructure() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsUpdateRunning()) {
    return;
  }

  bulk_leak_check_observation_.Reset();
  insecure_credentials_manager_observation_.Reset();

  bulk_leak_check_service_adapter_.reset();
  insecure_credentials_manager_.reset();
  saved_passwords_presenter_.reset();
}

bool PasswordStatusCheckService::IsInfrastructureReady() const {
  if (saved_passwords_presenter_ || insecure_credentials_manager_ ||
      bulk_leak_check_service_adapter_) {
    // `saved_passwords_presenter_`, `insecure_credentials_manager_`, and
    // `bulk_leak_check_service_adapter_` should always be initialized at the
    // same time.
    CHECK(saved_passwords_presenter_);
    CHECK(insecure_credentials_manager_);
    CHECK(bulk_leak_check_service_adapter_);
    return true;
  }

  return false;
}

void PasswordStatusCheckService::SetPasswordCheckSchedulePrefsWithInterval(
    base::Time check_time) {
  base::TimeDelta check_interval =
      features::kBackgroundPasswordCheckInterval.Get();

  base::Value::Dict dict;
  dict.Set(safety_hub_prefs::kNextPasswordCheckTimeKey,
           base::TimeToValue(check_time));
  dict.Set(safety_hub_prefs::kPasswordCheckIntervalKey,
           base::TimeDeltaToValue(check_interval));
  // Save current weights for days on prefs.
  auto map = GetDayPrefWeightMap();
  for (auto day = map.begin(); day != map.end(); day++) {
    dict.Set(day->first, base::Value(day->second));
  }

  profile_->GetPrefs()->SetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval,
      std::move(dict));
}

base::Time PasswordStatusCheckService::GetScheduledPasswordCheckTime() const {
  const base::Value::Dict& check_schedule_dict = profile_->GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);
  std::optional<base::Time> check_time = base::ValueToTime(
      check_schedule_dict.Find(safety_hub_prefs::kNextPasswordCheckTimeKey));
  CHECK(check_time.has_value());
  return check_time.value();
}

base::TimeDelta PasswordStatusCheckService::GetScheduledPasswordCheckInterval()
    const {
  const base::Value::Dict& check_schedule_dict = profile_->GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);
  std::optional<base::TimeDelta> check_interval = base::ValueToTimeDelta(
      check_schedule_dict.Find(safety_hub_prefs::kPasswordCheckIntervalKey));
  CHECK(check_interval.has_value());
  return check_interval.value();
}

base::Value::Dict PasswordStatusCheckService::GetPasswordCardData(
    bool signed_in) {
  if (no_passwords_saved()) {
    bool password_saving_allowed = profile_->GetPrefs()->GetBoolean(
        password_manager::prefs::kCredentialsEnableService);
    return GetNoPasswordCardData(password_saving_allowed);
  }

  if (compromised_credential_count_ > 0) {
    return GetCompromisedPasswordCardData(compromised_credential_count_);
  }

  if (reused_credential_count_ > 0) {
    return GetReusedPasswordCardData(reused_credential_count_, signed_in);
  }

  if (weak_credential_count_ > 0) {
    return GetWeakPasswordCardData(weak_credential_count_, signed_in);
  }

  CHECK(compromised_credential_count_ == 0);
  CHECK(reused_credential_count_ == 0);
  CHECK(weak_credential_count_ == 0);

  base::Time last_check_completed =
      base::Time::FromTimeT(profile_->GetPrefs()->GetDouble(
          password_manager::prefs::kLastTimePasswordCheckCompleted));
  if (!last_check_completed.is_null() && signed_in) {
    return GetSafePasswordCardData(last_check_completed);
  }

  return GetNoWeakOrReusedPasswordCardData(signed_in);
}

void PasswordStatusCheckService::ScheduleCheckAndStartRepeatedUpdates() {
  // Set time for next password check and schedule the next run.
  base::Time scheduled_check_time = GetScheduledPasswordCheckTime();
  // If current check was scheduled to run a long time ago (larger than the
  // interval) we make sure the next run is in the future with a minimum
  // distance between subsequent checks.
  while (scheduled_check_time <
         base::Time::Now() + safety_hub::kMinTimeBetweenPasswordChecks) {
    scheduled_check_time += features::kBackgroundPasswordCheckInterval.Get();
  }

  SetPasswordCheckSchedulePrefsWithInterval(scheduled_check_time);

  StartRepeatedUpdates();
}

void PasswordStatusCheckService::OnBulkCheckServiceShutDown() {
  // Stop observing BulkLeakCheckService when the service shuts down.
  CHECK(bulk_leak_check_observation_.IsObservingSource(
      BulkLeakCheckServiceFactory::GetForProfile(profile_)));
  bulk_leak_check_observation_.Reset();
}

bool PasswordStatusCheckService::IsUpdateRunning() const {
  // There is ongoing weak/reuse credential check.
  if (running_weak_reused_check_) {
    return true;
  }

  // If password_check_state_ is not kStopped, then the password check is still
  // ongoing.
  if (is_password_check_running()) {
    return true;
  }

  return false;
}

// TODO(crbug.com/40267370): Consider pass by value for GetCachedResult
// functions.
std::optional<std::unique_ptr<SafetyHubService::Result>>
PasswordStatusCheckService::GetCachedResult() {
  if (latest_result_) {
    return std::make_unique<PasswordStatusCheckResult>(*latest_result_);
  }
  return std::nullopt;
}
