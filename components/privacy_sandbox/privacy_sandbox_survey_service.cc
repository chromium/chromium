// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privacy_sandbox_survey_service.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "privacy_sandbox_prefs.h"

namespace privacy_sandbox {

PrivacySandboxSurveyService::PrivacySandboxSurveyService(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager)
    : pref_service_(pref_service), identity_manager_(identity_manager) {
  CHECK(identity_manager_);
}

PrivacySandboxSurveyService::~PrivacySandboxSurveyService() = default;

bool PrivacySandboxSurveyService::ShouldShowSentimentSurvey() {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kPrivacySandboxSentimentSurvey);
}

void PrivacySandboxSurveyService::RecordSentimentSurveyStatus(
    PrivacySandboxSentimentSurveyStatus status) {
  base::UmaHistogramEnumeration("PrivacySandbox.SentimentSurvey.Status",
                                status);
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
