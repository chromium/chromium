// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/version_info/channel.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingAckAction;
using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;

TrackingProtectionOnboardingStatus GetInternalOnboardingStatus(
    PrefService* pref_service) {
  return static_cast<TrackingProtectionOnboardingStatus>(
      pref_service->GetInteger(prefs::kTrackingProtectionOnboardingStatus));
}

TrackingProtectionOnboardingAckAction ToInternalAckAction(
    TrackingProtectionOnboarding::NoticeAction action) {
  switch (action) {
    case TrackingProtectionOnboarding::NoticeAction::kOther:
      return TrackingProtectionOnboardingAckAction::kOther;
    case TrackingProtectionOnboarding::NoticeAction::kGotIt:
      return TrackingProtectionOnboardingAckAction::kGotIt;
    case TrackingProtectionOnboarding::NoticeAction::kSettings:
      return TrackingProtectionOnboardingAckAction::kSettings;
    case TrackingProtectionOnboarding::NoticeAction::kLearnMore:
      return TrackingProtectionOnboardingAckAction::kLearnMore;
    case TrackingProtectionOnboarding::NoticeAction::kClosed:
      return TrackingProtectionOnboardingAckAction::kClosed;
  }
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

void CreateHistogramOnboardingStartupState(
    TrackingProtectionOnboarding::OnboardingStartupState state) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State", state);
}

void CreateTimingHistogramOnboardingStartup(const char* name,
                                            base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(1),
                                base::Days(10), 100);
}

void RecordOnboardedHistogramsOnStartup(PrefService* pref_service) {
  if (!pref_service->GetBoolean(prefs::kTrackingProtectionOnboardingAcked)) {
    CreateHistogramOnboardingStartupState(
        TrackingProtectionOnboarding::OnboardingStartupState::
            kOnboardedWaitingToAck);
    auto waiting_to_ack_since =
        base::Time::Now() -
        pref_service->GetTime(prefs::kTrackingProtectionOnboardedSince);
    auto eligible_to_onboarded_duration =
        pref_service->GetTime(prefs::kTrackingProtectionOnboardedSince) -
        pref_service->GetTime(prefs::kTrackingProtectionEligibleSince);
    CreateTimingHistogramOnboardingStartup(
        "PrivacySandbox.TrackingProtection.OnboardingStartup.WaitingToAckSince",
        waiting_to_ack_since);
    CreateTimingHistogramOnboardingStartup(
        "PrivacySandbox.TrackingProtection.OnboardingStartup."
        "EligibleToOnboardedDuration",
        eligible_to_onboarded_duration);
    return;
  }
  auto eligible_to_onboarded_duration =
      pref_service->GetTime(prefs::kTrackingProtectionOnboardedSince) -
      pref_service->GetTime(prefs::kTrackingProtectionEligibleSince);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.OnboardingStartup."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration);
  auto action = static_cast<TrackingProtectionOnboardingAckAction>(
      pref_service->GetInteger(prefs::kTrackingProtectionOnboardingAckAction));
  switch (action) {
    case tracking_protection::TrackingProtectionOnboardingAckAction::kNotSet:
      break;
    case tracking_protection::TrackingProtectionOnboardingAckAction::kGotIt:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::kAckedGotIt);
      break;
    case tracking_protection::TrackingProtectionOnboardingAckAction::kSettings:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::kAckedSettings);
      break;
    case tracking_protection::TrackingProtectionOnboardingAckAction::kClosed:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::kAckedClosed);
      break;
    case tracking_protection::TrackingProtectionOnboardingAckAction::kLearnMore:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::
              kAckedLearnMore);
      break;
    case tracking_protection::TrackingProtectionOnboardingAckAction::kOther:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::kAckedOther);
      break;
  }
}

void RecordHistogramsOnStartup(PrefService* pref_service) {
  auto status = GetInternalOnboardingStatus(pref_service);
  switch (status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::kIneligible);
      break;
    case TrackingProtectionOnboardingStatus::kEligible: {
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::
              kEligibleWaitingToOnboard);
      auto waiting_to_onboard_since =
          base::Time::Now() -
          pref_service->GetTime(prefs::kTrackingProtectionEligibleSince);
      CreateTimingHistogramOnboardingStartup(
          "PrivacySandbox.TrackingProtection.OnboardingStartup."
          "WaitingToOnboardSince",
          waiting_to_onboard_since);
      break;
    }
    case TrackingProtectionOnboardingStatus::kOnboarded:
      RecordOnboardedHistogramsOnStartup(pref_service);
      break;
  }
}

bool IsRollbackEnabled() {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kTrackingProtectionOnboardingRollback);
}

void OffboardingNoticeShown(PrefService* pref_service) {
  if (pref_service->GetBoolean(prefs::kTrackingProtectionOffboarded)) {
    return;
  }
  pref_service->SetBoolean(prefs::kTrackingProtectionOffboarded, true);
  pref_service->SetTime(prefs::kTrackingProtectionOffboardedSince,
                        base::Time::Now());
}

void OffboardingNoticeActionTaken(
    TrackingProtectionOnboarding::NoticeAction action,
    PrefService* pref_service) {
  pref_service->SetInteger(prefs::kTrackingProtectionOffboardingAckAction,
                           static_cast<int>(ToInternalAckAction(action)));
}

}  // namespace

TrackingProtectionOnboarding::TrackingProtectionOnboarding(
    PrefService* pref_service,
    version_info::Channel channel)
    : pref_service_(pref_service), channel_(channel) {
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
  pref_change_registrar_.Add(
      prefs::kTrackingProtectionOffboarded,
      base::BindRepeating(
          &TrackingProtectionOnboarding::OnOffboardingPrefChanged,
          base::Unretained(this)));

  RecordHistogramsOnStartup(pref_service_);
}

TrackingProtectionOnboarding::~TrackingProtectionOnboarding() = default;

void TrackingProtectionOnboarding::OnOnboardingPrefChanged() const {
  // We notify observers of all changes to the onboarding pref.
  auto onboarding_status = GetOnboardingStatus();
  for (auto& observer : observers_) {
    observer.OnTrackingProtectionOnboardingUpdated(onboarding_status);
  }

  switch (GetInternalOnboardingStatus(pref_service_)) {
    case tracking_protection::TrackingProtectionOnboardingStatus::kIneligible:
    case tracking_protection::TrackingProtectionOnboardingStatus::kEligible:
      for (auto& observer : observers_) {
        observer.OnShouldShowNoticeUpdated();
      }
      break;
    default:
      break;
  }
}

void TrackingProtectionOnboarding::OnOnboardingAckedChanged() const {
  for (auto& observer : observers_) {
    observer.OnShouldShowNoticeUpdated();
  }
}

void TrackingProtectionOnboarding::OnOffboardingPrefChanged() const {
  for (auto& observer : observers_) {
    observer.OnTrackingProtectionOnboardingUpdated(GetOnboardingStatus());
  }
}

void TrackingProtectionOnboarding::MaybeMarkEligible() {
  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kIneligible) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkEligible",
        false);
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionEligibleSince,
                         base::Time::Now());
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kEligible));
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkEligible", true);
}

void TrackingProtectionOnboarding::MaybeMarkIneligible() {
  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkIneligible",
        false);
    return;
  }
  pref_service_->ClearPref(prefs::kTrackingProtectionEligibleSince);
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kIneligible));
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkIneligible", true);
}

void TrackingProtectionOnboarding::MaybeResetOnboardingPrefs() {
  // Clearing the prefs is only allowed in Beta, Canary and Dev for testing.
  switch (channel_) {
    case version_info::Channel::BETA:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
      break;
    default:
      return;
  }

  // Clear all Onboarding Prefs. Excluding Ack prefs.
  pref_service_->ClearPref(prefs::kTrackingProtectionOnboardingStatus);
  pref_service_->ClearPref(prefs::kTrackingProtectionEligibleSince);
  pref_service_->ClearPref(prefs::kTrackingProtectionOnboardedSince);
  pref_service_->ClearPref(prefs::kTrackingProtectionNoticeLastShown);
}

void TrackingProtectionOnboarding::OnboardingNoticeShown() {
  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.Notice.Shown"));

  pref_service_->SetTime(prefs::kTrackingProtectionNoticeLastShown,
                         base::Time::Now());

  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
        false);
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionOnboardedSince,
                         base::Time::Now());
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));
  auto eligible_to_onboarded_duration =
      pref_service_->GetTime(prefs::kTrackingProtectionOnboardedSince) -
      pref_service_->GetTime(prefs::kTrackingProtectionEligibleSince);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.Onboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration);
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
      true);
}

void TrackingProtectionOnboarding::NoticeShown(NoticeType notice_type) {
  switch (notice_type) {
    case NoticeType::kNone:
      return;
    case NoticeType::kOnboarding:
      OnboardingNoticeShown();
      return;
    case NoticeType::kOffboarding:
      OffboardingNoticeShown(pref_service_);
      return;
  }
}

void TrackingProtectionOnboarding::OnboardingNoticeActionTaken(
    NoticeAction action) {
  RecordActionMetrics(action);

  if (pref_service_->GetBoolean(prefs::kTrackingProtectionOnboardingAcked)) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding."
        "DidNoticeActionAckowledge",
        false);
    return;
  }

  pref_service_->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);
  pref_service_->SetInteger(prefs::kTrackingProtectionOnboardingAckAction,
                            static_cast<int>(ToInternalAckAction(action)));
  auto onboarding_to_acked_duration =
      base::Time::Now() -
      pref_service_->GetTime(prefs::kTrackingProtectionOnboardedSince);
  auto last_shown_to_acked_duration =
      base::Time::Now() -
      pref_service_->GetTime(prefs::kTrackingProtectionNoticeLastShown);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.Onboarding.OnboardedToAckedDuration",
      onboarding_to_acked_duration);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.Onboarding.LastShownToAckedDuration",
      last_shown_to_acked_duration);
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.Onboarding."
      "DidNoticeActionAckowledge",
      true);
}

void TrackingProtectionOnboarding::NoticeActionTaken(NoticeType notice_type,
                                                     NoticeAction action) {
  switch (notice_type) {
    case NoticeType::kNone:
      return;
    case NoticeType::kOnboarding:
      OnboardingNoticeActionTaken(action);
      return;
    case NoticeType::kOffboarding:
      OffboardingNoticeActionTaken(action, pref_service_);
      return;
  }
}

bool TrackingProtectionOnboarding::ShouldShowOnboardingNotice() {
  return GetRequiredNotice() == NoticeType::kOnboarding;
}

NoticeType TrackingProtectionOnboarding::GetRequiredNotice() {
  auto onboarding_status = GetInternalOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return NoticeType::kNone;
    case TrackingProtectionOnboardingStatus::kEligible:
      // We haven't showed the user any notice yet. only shown them the
      // onboarding notice if we're not planning on offboarding them.
      return IsRollbackEnabled() ? NoticeType::kNone : NoticeType::kOnboarding;
    case TrackingProtectionOnboardingStatus::kOnboarded:
      // We've already showed the user the onboarding notice. We
      // offboard them if applicable. Otherwise, we keep showing the
      // Onboarding Notice until they Ack.
      if (IsRollbackEnabled()) {
        return pref_service_->GetBoolean(prefs::kTrackingProtectionOffboarded)
                   ? NoticeType::kNone
                   : NoticeType::kOffboarding;
      }
      return pref_service_->GetBoolean(
                 prefs::kTrackingProtectionOnboardingAcked)
                 ? NoticeType::kNone
                 : NoticeType::kOnboarding;
  }
}

bool TrackingProtectionOnboarding::IsOffboarded() const {
  return GetOnboardingStatus() == OnboardingStatus::kOffboarded;
}

TrackingProtectionOnboarding::OnboardingStatus
TrackingProtectionOnboarding::GetOnboardingStatus() const {
  if (IsRollbackEnabled() &&
      pref_service_->GetBoolean(prefs::kTrackingProtectionOffboarded)) {
    return OnboardingStatus::kOffboarded;
  }
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
