// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

namespace privacy_sandbox {

namespace {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

TrackingProtectionOnboardingStatus GetInternalOnboardingStatus(
    PrefService* pref_service) {
  return static_cast<TrackingProtectionOnboardingStatus>(
      pref_service->GetInteger(prefs::kTrackingProtectionOnboardingStatus));
}

void RecordActionMetrics(TrackingProtectionOnboarding::NoticeAction action) {
  switch (action) {
    case TrackingProtectionOnboarding::NoticeAction::kOther:
      base::RecordAction(
          base::UserMetricsAction("TrackingProtection.Notice.DismissedOther"));
      break;
    case TrackingProtectionOnboarding::NoticeAction::kGotIt:
      base::RecordAction(
          base::UserMetricsAction("TrackingProtection.Notice.GotItClicked"));
      break;
    case TrackingProtectionOnboarding::NoticeAction::kSettings:
      base::RecordAction(
          base::UserMetricsAction("TrackingProtection.Notice.SettingsClicked"));
      break;
    case TrackingProtectionOnboarding::NoticeAction::kLearnMore:
      base::RecordAction(base::UserMetricsAction(
          "TrackingProtection.Notice.LearnMoreClicked"));
      break;
    case TrackingProtectionOnboarding::NoticeAction::kClosed:
      base::RecordAction(
          base::UserMetricsAction("TrackingProtection.Notice.Closed"));
      break;
  }
}

}  // namespace

TrackingProtectionOnboarding::TrackingProtectionOnboarding(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kTrackingProtectionOnboardingStatus,
      base::BindRepeating(
          &TrackingProtectionOnboarding::OnOnboardingPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kTrackingProtectionOnboardingAcked,
      base::BindRepeating(
          &TrackingProtectionOnboarding::OnOnboardingAckedChanged,
          base::Unretained(this)));

  // If we're forcing eligibility, then let' set it now.
  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kTrackingProtectionOnboardingForceEligibility) &&
      GetInternalOnboardingStatus(pref_service_) ==
          TrackingProtectionOnboardingStatus::kIneligible) {
    MaybeMarkEligible();
  }
}

TrackingProtectionOnboarding::~TrackingProtectionOnboarding() = default;

void TrackingProtectionOnboarding::OnOnboardingPrefChanged() const {
  switch (GetInternalOnboardingStatus(pref_service_)) {
    case tracking_protection::TrackingProtectionOnboardingStatus::kIneligible:
    case tracking_protection::TrackingProtectionOnboardingStatus::kEligible:
      for (auto& observer : observers_) {
        observer.OnShouldShowNoticeUpdated();
      }
      break;
    case tracking_protection::TrackingProtectionOnboardingStatus::kOnboarded:
      for (auto& observer : observers_) {
        observer.OnTrackingProtectionOnboarded();
      }
      break;
  }
}

void TrackingProtectionOnboarding::OnOnboardingAckedChanged() const {
  for (auto& observer : observers_) {
    observer.OnShouldShowNoticeUpdated();
  }
}

void TrackingProtectionOnboarding::MaybeMarkEligible() {
  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kIneligible) {
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionEligibleSince,
                         base::Time::Now());
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kEligible));
}

void TrackingProtectionOnboarding::MaybeMarkIneligible() {
  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    return;
  }
  pref_service_->ClearPref(prefs::kTrackingProtectionEligibleSince);
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kIneligible));
}

void TrackingProtectionOnboarding::NoticeShown() {
  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.Notice.Shown"));

  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionOnboardedSince,
                         base::Time::Now());
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));
}

void TrackingProtectionOnboarding::NoticeActionTaken(
    TrackingProtectionOnboarding::NoticeAction action) {
  RecordActionMetrics(action);

  switch (action) {
    case NoticeAction::kOther:
      return;
    case NoticeAction::kGotIt:
    case NoticeAction::kSettings:
    case NoticeAction::kLearnMore:
    case NoticeAction::kClosed:
      pref_service_->SetBoolean(prefs::kTrackingProtectionOnboardingAcked,
                                true);
      return;
  }
}

bool TrackingProtectionOnboarding::ShouldShowOnboardingNotice() {
  auto onboarding_status = GetInternalOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return false;
    case TrackingProtectionOnboardingStatus::kEligible:
      return true;
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return !pref_service_->GetBoolean(
          prefs::kTrackingProtectionOnboardingAcked);
  }
}

TrackingProtectionOnboarding::OnboardingStatus
TrackingProtectionOnboarding::GetOnboardingStatus() const {
  auto onboarding_status = GetInternalOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return OnboardingStatus::kIneligible;
    case TrackingProtectionOnboardingStatus::kEligible:
      return OnboardingStatus::kEligible;
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return OnboardingStatus::kOnboarded;
  }
}

void TrackingProtectionOnboarding::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrackingProtectionOnboarding::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace privacy_sandbox
