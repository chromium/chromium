// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/prefs/pref_change_registrar.h"
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

TrackingProtectionOnboardingStatus GetInternalSilentOnboardingStatus(
    PrefService* pref_service) {
  return static_cast<TrackingProtectionOnboardingStatus>(
      pref_service->GetInteger(
          prefs::kTrackingProtectionSilentOnboardingStatus));
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

void CreateHistogramSilentOnboardingStartupState(
    TrackingProtectionOnboarding::SilentOnboardingStartupState state) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup.State", state);
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
  if (pref_service->HasPrefPath(
          prefs::kTrackingProtectionOnboardingAckedSince)) {
    auto acked_since =
        base::Time::Now() -
        pref_service->GetTime(prefs::kTrackingProtectionOnboardingAckedSince);
    CreateTimingHistogramOnboardingStartup(
        "PrivacySandbox.TrackingProtection.OnboardingStartup.AckedSince",
        acked_since);
  }
}

void RecordEligibleWaitingToOnboardHistogramsOnStartup(
    PrefService* pref_service) {
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
}

void RecordHistogramsOnboardingOnStartup(PrefService* pref_service) {
  auto status = GetInternalOnboardingStatus(pref_service);
  switch (status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      CreateHistogramOnboardingStartupState(
          TrackingProtectionOnboarding::OnboardingStartupState::kIneligible);
      break;
    case TrackingProtectionOnboardingStatus::kEligible:
    case TrackingProtectionOnboardingStatus::kRequested: {
      RecordEligibleWaitingToOnboardHistogramsOnStartup(pref_service);
      break;
    }
    case TrackingProtectionOnboardingStatus::kOnboarded:
      RecordOnboardedHistogramsOnStartup(pref_service);
      break;
  }
}

void RecordHistogramsSilentOnboardingOnStartup(PrefService* pref_service) {
  auto status = GetInternalSilentOnboardingStatus(pref_service);
  switch (status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      CreateHistogramSilentOnboardingStartupState(
          TrackingProtectionOnboarding::SilentOnboardingStartupState::
              kIneligible);
      break;
    case TrackingProtectionOnboardingStatus::kEligible: {
      CreateHistogramSilentOnboardingStartupState(
          TrackingProtectionOnboarding::SilentOnboardingStartupState::
              kEligibleWaitingToOnboard);
      auto waiting_to_onboard_since =
          base::Time::Now() -
          pref_service->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
      CreateTimingHistogramOnboardingStartup(
          "PrivacySandbox.TrackingProtection.SilentOnboardingStartup."
          "WaitingToOnboardSince",
          waiting_to_onboard_since);
      break;
    }
    case TrackingProtectionOnboardingStatus::kOnboarded: {
      CreateHistogramSilentOnboardingStartupState(
          TrackingProtectionOnboarding::SilentOnboardingStartupState::
              kOnboarded);
      auto eligible_to_onboarded_duration =
          pref_service->GetTime(
              prefs::kTrackingProtectionSilentOnboardedSince) -
          pref_service->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
      CreateTimingHistogramOnboardingStartup(
          "PrivacySandbox.TrackingProtection.SilentOnboardingStartup."
          "EligibleToOnboardedDuration",
          eligible_to_onboarded_duration);
      break;
    }
    case TrackingProtectionOnboardingStatus::kRequested: {
      // kRequested isn't applicable when silent onboarding.
      NOTREACHED_NORETURN();
    }
  }
}

void RecordHistogramsOnStartup(PrefService* pref_service) {
  RecordHistogramsOnboardingOnStartup(pref_service);
  RecordHistogramsSilentOnboardingOnStartup(pref_service);
}


TrackingProtectionOnboarding::NoticeType GetRequiredSilentOnboardingNotice(
    PrefService* pref_service) {
  auto onboarding_status = GetInternalSilentOnboardingStatus(pref_service);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return TrackingProtectionOnboarding::NoticeType::kNone;
    case TrackingProtectionOnboardingStatus::kEligible:
      return TrackingProtectionOnboarding::NoticeType::kSilentOnboarding;
    case TrackingProtectionOnboardingStatus::kRequested:
      NOTREACHED_NORETURN();
  }
  NOTREACHED_NORETURN();
}

void RecordSilentOnboardingMarkEligibleHistogram(bool result) {
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkEligible",
      result);
}

void RecordSilentOnboardingMarkIneligibleHistogram(bool result) {
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkIneligible",
      result);
}

void RecordSilentOnboardingDidNoticeShownOnboard(bool result) {
  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "DidNoticeShownOnboard",
      result);
}

}  // namespace

TrackingProtectionOnboarding::TrackingProtectionOnboarding(
    PrefService* pref_service,
    version_info::Channel channel,
    bool is_silent_onboarding_enabled)
    : pref_service_(pref_service),
      channel_(channel),
      is_silent_onboarding_enabled_(is_silent_onboarding_enabled) {
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
      prefs::kTrackingProtectionSilentOnboardingStatus,
      base::BindRepeating(
          &TrackingProtectionOnboarding::OnSilentOnboardingPrefChanged,
          base::Unretained(this)));

  RecordHistogramsOnStartup(pref_service_);
}

TrackingProtectionOnboarding::~TrackingProtectionOnboarding() = default;

void TrackingProtectionOnboarding::Shutdown() {
  observers_.Clear();
  pref_service_ = nullptr;
  pref_change_registrar_.Reset();
}

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

void TrackingProtectionOnboarding::OnSilentOnboardingPrefChanged() const {
  auto onboarding_status = GetSilentOnboardingStatus();
  for (auto& observer : observers_) {
    observer.OnTrackingProtectionSilentOnboardingUpdated(onboarding_status);
    observer.OnShouldShowNoticeUpdated();
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

void TrackingProtectionOnboarding::MaybeMarkSilentEligible() {
  auto status = GetInternalSilentOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kIneligible) {
    RecordSilentOnboardingMarkEligibleHistogram(false);
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionSilentEligibleSince,
                         base::Time::Now());
  pref_service_->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kEligible));
  RecordSilentOnboardingMarkEligibleHistogram(true);
}

void TrackingProtectionOnboarding::MaybeMarkSilentIneligible() {
  auto status = GetInternalSilentOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    RecordSilentOnboardingMarkIneligibleHistogram(false);
    return;
  }
  pref_service_->ClearPref(prefs::kTrackingProtectionSilentEligibleSince);
  pref_service_->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kIneligible));
  RecordSilentOnboardingMarkIneligibleHistogram(true);
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
  pref_service_->ClearPref(prefs::kTrackingProtectionSilentOnboardingStatus);
  pref_service_->ClearPref(prefs::kTrackingProtectionSilentEligibleSince);
  pref_service_->ClearPref(prefs::kTrackingProtectionSilentOnboardedSince);
}

void TrackingProtectionOnboarding::OnboardingNoticeShown() {
  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.Notice.Shown"));
  base::Time now = base::Time::Now();
  pref_service_->SetTime(prefs::kTrackingProtectionNoticeLastShown, now);
  auto status = GetInternalOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible &&
      status != TrackingProtectionOnboardingStatus::kRequested) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
        false);
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionOnboardedSince, now);
  pref_service_->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));
  auto eligible_to_onboarded_duration =
      now - pref_service_->GetTime(prefs::kTrackingProtectionEligibleSince);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.Onboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration);

  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
      true);
}

void TrackingProtectionOnboarding::SilentOnboardingNoticeShown() {
  auto status = GetInternalSilentOnboardingStatus(pref_service_);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    RecordSilentOnboardingDidNoticeShownOnboard(false);
    return;
  }
  pref_service_->SetTime(prefs::kTrackingProtectionSilentOnboardedSince,
                         base::Time::Now());
  auto eligible_to_onboarded_duration =
      pref_service_->GetTime(prefs::kTrackingProtectionSilentOnboardedSince) -
      pref_service_->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration);
  pref_service_->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));
  RecordSilentOnboardingDidNoticeShownOnboard(true);
}

void TrackingProtectionOnboarding::NoticeShown(NoticeType notice_type) {
  switch (notice_type) {
    case NoticeType::kNone:
      return;
    case NoticeType::kOnboarding:
      OnboardingNoticeShown();
      return;
    case NoticeType::kOffboarding:
      return;
    case NoticeType::kSilentOnboarding:
      SilentOnboardingNoticeShown();
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

  pref_service_->SetTime(prefs::kTrackingProtectionOnboardingAckedSince,
                         base::Time::Now());
  pref_service_->SetInteger(prefs::kTrackingProtectionOnboardingAckAction,
                            static_cast<int>(ToInternalAckAction(action)));
  pref_service_->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

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
      return;
    case NoticeType::kSilentOnboarding:
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
      return GetRequiredSilentOnboardingNotice(pref_service_);
    case TrackingProtectionOnboardingStatus::kEligible:
    case TrackingProtectionOnboardingStatus::kRequested: {
      return NoticeType::kOnboarding;
    }
    case TrackingProtectionOnboardingStatus::kOnboarded: {
      // We've already showed the user the onboarding notice. We keep showing
      // the Onboarding Notice until they Ack.
      return pref_service_->GetBoolean(
                 prefs::kTrackingProtectionOnboardingAcked)
                 ? NoticeType::kNone
                 : NoticeType::kOnboarding;
    }
  }
}

std::optional<base::TimeDelta>
TrackingProtectionOnboarding::OnboardedToAcknowledged() {
  if (!pref_service_->HasPrefPath(
          prefs::kTrackingProtectionOnboardingAckedSince)) {
    return std::nullopt;
  }
  if (!pref_service_->HasPrefPath(prefs::kTrackingProtectionOnboardedSince)) {
    return std::nullopt;
  }
  return pref_service_->GetTime(
             prefs::kTrackingProtectionOnboardingAckedSince) -
         pref_service_->GetTime(prefs::kTrackingProtectionOnboardedSince);
}

TrackingProtectionOnboarding::OnboardingStatus
TrackingProtectionOnboarding::GetOnboardingStatus() const {
  auto onboarding_status = GetInternalOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return OnboardingStatus::kIneligible;
    case TrackingProtectionOnboardingStatus::kEligible:
    case TrackingProtectionOnboardingStatus::kRequested:
      return OnboardingStatus::kEligible;
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return OnboardingStatus::kOnboarded;
  }
}

TrackingProtectionOnboarding::SilentOnboardingStatus
TrackingProtectionOnboarding::GetSilentOnboardingStatus() const {
  auto onboarding_status = GetInternalSilentOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return SilentOnboardingStatus::kIneligible;
    case TrackingProtectionOnboardingStatus::kEligible:
      return SilentOnboardingStatus::kEligible;
    case TrackingProtectionOnboardingStatus::kRequested:
      NOTREACHED_NORETURN();
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return SilentOnboardingStatus::kOnboarded;
  }
}

void TrackingProtectionOnboarding::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrackingProtectionOnboarding::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace privacy_sandbox
