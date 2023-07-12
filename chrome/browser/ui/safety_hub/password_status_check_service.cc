// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/password_status_check_service.h"

#include "base/json/values_util.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/common/chrome_features.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/prefs/pref_service.h"

namespace {

// Returns true if a new check time should be saved. This is the case when:
// - There is no existing time available, e.g. in first run.
// - The configuration for the interval has changed. This is to ensure changes
//   in the interval are applied without large delays in case the interval is so
//   short that it exceeds backend capacity.
bool ShouldFindNewCheckTime(Profile* profile) {
  // The pref dict looks like this:
  // {
  //   ...
  //   kBackgroundPasswordCheckTimeAndInterval: {
  //     kPasswordCheckIntervalKey: "1728000000000",
  //     kNextPasswordCheckTimeKey: "13333556059805713"
  //   },
  //   ...
  // }
  const base::Value::Dict& check_schedule_dict = profile->GetPrefs()->GetDict(
      safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval);

  bool uninitialized =
      !check_schedule_dict.Find(safety_hub_prefs::kNextPasswordCheckTimeKey) ||
      !check_schedule_dict.Find(safety_hub_prefs::kPasswordCheckIntervalKey);

  base::TimeDelta update_interval =
      features::kBackgroundPasswordCheckInterval.Get();
  absl::optional<base::TimeDelta> interval_used_for_scheduling =
      base::ValueToTimeDelta(check_schedule_dict.Find(
          safety_hub_prefs::kPasswordCheckIntervalKey));

  return uninitialized || !interval_used_for_scheduling.has_value() ||
         update_interval != interval_used_for_scheduling.value();
}

}  // namespace

PasswordStatusCheckService::PasswordStatusCheckService(Profile* profile)
    : profile_(profile) {
  StartRepeatedUpdates();
}

PasswordStatusCheckService::~PasswordStatusCheckService() = default;

void PasswordStatusCheckService::Shutdown() {
  saved_passwords_presenter_observation_.Reset();
  saved_passwords_presenter_.reset();
}

void PasswordStatusCheckService::StartRepeatedUpdates() {
  if (ShouldFindNewCheckTime(profile_)) {
    base::TimeDelta update_interval =
        features::kBackgroundPasswordCheckInterval.Get();

    base::TimeDelta delta = base::Microseconds(
        base::RandGenerator(update_interval.InMicroseconds()));
    base::Time scheduled_check_time = base::Time::Now() + delta;

    base::Value::Dict dict;
    dict.Set(safety_hub_prefs::kNextPasswordCheckTimeKey,
             base::TimeToValue(scheduled_check_time));
    dict.Set(safety_hub_prefs::kPasswordCheckIntervalKey,
             base::TimeDeltaToValue(update_interval));
    profile_->GetPrefs()->SetDict(
        safety_hub_prefs::kBackgroundPasswordCheckTimeAndInterval,
        std::move(dict));
  }

  // TODO(crbug.com/1443466): Create task to run password check at the scheduled
  // time.
}

void PasswordStatusCheckService::UpdateInsecureCredentialCountAsync() {
  saved_passwords_presenter_observation_.Reset();

  saved_passwords_presenter_ =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          AffiliationServiceFactory::GetForProfile(profile_),
          PasswordStoreFactory::GetForProfile(
              profile_, ServiceAccessType::IMPLICIT_ACCESS),
          AccountPasswordStoreFactory::GetForProfile(
              profile_, ServiceAccessType::IMPLICIT_ACCESS));
  saved_passwords_presenter_observation_.Observe(
      saved_passwords_presenter_.get());
  saved_passwords_presenter_->Init();
}

void PasswordStatusCheckService::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  extensions::IdGenerator credential_id_generator;
  auto password_check_delegate =
      std::make_unique<extensions::PasswordCheckDelegate>(
          profile_, saved_passwords_presenter_.get(), &credential_id_generator);

  std::vector<password_manager::CredentialUIEntry> insecure_credentials =
      password_check_delegate->GetInsecureCredentialsManager()
          ->GetInsecureCredentialEntries();

  compromised_credential_count_ = 0;
  weak_credential_count_ = 0;
  reused_credential_count_ = 0;

  for (const auto& entry : insecure_credentials) {
    if (entry.IsMuted()) {
      continue;
    }
    if (password_manager::IsCompromised(entry)) {
      compromised_credential_count_++;
    } else if (entry.IsWeak()) {
      weak_credential_count_++;
    } else if (entry.IsReused()) {
      reused_credential_count_++;
    }
  }

  password_check_delegate.reset();
  saved_passwords_presenter_observation_.Reset();
  saved_passwords_presenter_.reset();

  if (on_passwords_changed_finished_callback_for_test_) {
    on_passwords_changed_finished_callback_for_test_.Run();
  }
}

void PasswordStatusCheckService::RunPasswordCheck() {
  // TODO(crbug.com/1443466)
  NOTIMPLEMENTED();
}
