// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_survey_service.h"

#include "base/feature_list.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "privacy_sandbox_prefs.h"

namespace privacy_sandbox {

namespace {

// Cooldown period before we can show a profile another sentiment survey.
constexpr base::TimeDelta kMinimumTimeBetweenSentimentSurveys = base::Days(180);

bool IsSentimentSurveyOnCooldown(PrefService* pref_service) {
  // Check if the survey was last seen and if the cooldown period has elapsed.
  return pref_service->HasPrefPath(
             prefs::kPrivacySandboxSentimentSurveyLastSeen) &&
         pref_service->GetTime(prefs::kPrivacySandboxSentimentSurveyLastSeen) +
                 kMinimumTimeBetweenSentimentSurveys >
             base::Time::Now();
}

}  // namespace

PrivacySandboxSurveyService::PrivacySandboxSurveyService(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service), identity_manager_(identity_manager) {
  CHECK(identity_manager_);
}

PrivacySandboxSurveyService::~PrivacySandboxSurveyService() = default;

bool PrivacySandboxSurveyService::ShouldShowSentimentSurvey() {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxSentimentSurvey)) {
    return false;
  }

  // We shouldn't show the survey if it's still in the cooldown period.
  return !IsSentimentSurveyOnCooldown(pref_service_);
}

void PrivacySandboxSurveyService::OnSuccessfulSentimentSurvey() {
  pref_service_->SetTime(prefs::kPrivacySandboxSentimentSurveyLastSeen,
                         base::Time::Now());
}

std::map<std::string, bool>
PrivacySandboxSurveyService::GetSentimentSurveyPsb() {
  return {
      {"Topics enabled",
       pref_service_->GetBoolean(prefs::kPrivacySandboxM1TopicsEnabled)},
      {"Protected audience enabled",
       pref_service_->GetBoolean(prefs::kPrivacySandboxM1FledgeEnabled)},
      {"Measurement enabled",
       pref_service_->GetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled)},
      {"Signed in",
       identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)}};
}

}  // namespace privacy_sandbox
