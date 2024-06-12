// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_survey_service.h"

#include "base/feature_list.h"
#include "base/time/time_delta_from_string.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "tracking_protection_prefs.h"
#include "tracking_protection_reminder_service.h"
#include "tracking_protection_survey_service.h"

namespace privacy_sandbox {
namespace {

bool IsSentimentSurveyEnabled() {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kTrackingProtectionSentimentSurvey);
}

base::TimeDelta GetTimeToSurvey() {
  return privacy_sandbox::kTrackingProtectionTimeToSurvey.Get();
}

privacy_sandbox::TrackingProtectionSurveyAnchor GetSurveyAnchor() {
  return static_cast<privacy_sandbox::TrackingProtectionSurveyAnchor>(
      privacy_sandbox::kTrackingProtectionSurveyAnchor.Get());
}

void MaybeUpdateSurveyWindowStartTime(PrefService* pref_service,
                                      base::Time onboarded_timestamp) {
  if (!IsSentimentSurveyEnabled()) {
    return;
  }
  // If a start time already exists we shouldn't override it.
  if (pref_service->HasPrefPath(
          prefs::kTrackingProtectionSurveyWindowStartTime)) {
    return;
  }
  pref_service->SetTime(prefs::kTrackingProtectionSurveyWindowStartTime,
                        onboarded_timestamp + GetTimeToSurvey());
}

}  // namespace

TrackingProtectionSurveyService::TrackingProtectionSurveyService(
    PrefService* pref_service,
    TrackingProtectionOnboarding* onboarding_service,
    TrackingProtectionReminderService* reminder_service)
    : pref_service_(pref_service),
      onboarding_service_(onboarding_service),
      reminder_service_(reminder_service) {
  if (onboarding_service_) {
    onboarding_observation_.Observe(onboarding_service_);
  }
  if (reminder_service_) {
    reminder_service_observation_.Observe(reminder_service_);
  }
}

TrackingProtectionSurveyService::~TrackingProtectionSurveyService() = default;

void TrackingProtectionSurveyService::OnTrackingProtectionOnboardingUpdated(
    TrackingProtectionOnboarding::OnboardingStatus onboarding_status) {
  std::optional<base::Time> onboarded_timestamp =
      onboarding_service_->GetOnboardingTimestamp();
  if (onboarded_timestamp.has_value() &&
      GetSurveyAnchor() ==
          privacy_sandbox::TrackingProtectionSurveyAnchor::kOnboarding) {
    MaybeUpdateSurveyWindowStartTime(pref_service_, *onboarded_timestamp);
  }
}

void TrackingProtectionSurveyService::
    OnTrackingProtectionSilentOnboardingUpdated(
        TrackingProtectionOnboarding::SilentOnboardingStatus
            onboarding_status) {
  std::optional<base::Time> onboarded_timestamp =
      onboarding_service_->GetSilentOnboardingTimestamp();
  if (onboarded_timestamp.has_value() &&
      GetSurveyAnchor() ==
          privacy_sandbox::TrackingProtectionSurveyAnchor::kOnboarding) {
    MaybeUpdateSurveyWindowStartTime(pref_service_, *onboarded_timestamp);
  }
}

void TrackingProtectionSurveyService::OnTrackingProtectionReminderStatusChanged(
    tracking_protection::TrackingProtectionReminderStatus status) {
  // TODO(crbug.com/345806678): Implement this. We should update the survey
  // status on successful reminders.
}

}  // namespace privacy_sandbox
