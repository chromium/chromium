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

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionSentimentSurveyGroup;

using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using SentimentSurveyGroup =
    privacy_sandbox::TrackingProtectionOnboarding::SentimentSurveyGroup;

constexpr base::TimeDelta kHatsImmediateStartTimeDelta = base::Minutes(2);
constexpr base::TimeDelta kHatsImmediateEndTimeDelta = base::Hours(1);
constexpr base::TimeDelta kHatsDelayedStartTimeDelta = base::Days(14);
constexpr base::TimeDelta kHatsDelayedEndTimeDelta = base::Days(15);

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

SentimentSurveyGroup GetSurveyGroup(PrefService* pref_service) {
  auto internal_group = static_cast<TrackingProtectionSentimentSurveyGroup>(
      pref_service->GetInteger(prefs::kTrackingProtectionSentimentSurveyGroup));
  switch (internal_group) {
    case tracking_protection::TrackingProtectionSentimentSurveyGroup::kNotSet:
      return SentimentSurveyGroup::kNotSet;
    case tracking_protection::TrackingProtectionSentimentSurveyGroup::
        kControlImmediate:
      return SentimentSurveyGroup::kControlImmediate;
    case tracking_protection::TrackingProtectionSentimentSurveyGroup::
        kTreatmentImmediate:
      return SentimentSurveyGroup::kTreatmentImmediate;
    case tracking_protection::TrackingProtectionSentimentSurveyGroup::
        kControlDelayed:
      return SentimentSurveyGroup::kControlDelayed;
    case tracking_protection::TrackingProtectionSentimentSurveyGroup::
        kTreatmentDelayed:
      return SentimentSurveyGroup::kTreatmentDelayed;
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

void CreateHistogramSentimentSurveyRegistration(
    TrackingProtectionOnboarding::SentimentSurveyGroupMetrics group) {
  base::UmaHistogramEnumeration(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered", group);
}

void EmitRegistrationHistogram(SentimentSurveyGroup group) {
  switch (group) {
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kNotSet:
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kControlImmediate:
      CreateHistogramSentimentSurveyRegistration(
          TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
              kControlImmediate);
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kControlDelayed:
      CreateHistogramSentimentSurveyRegistration(
          TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
              kControlDelayed);
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::
        kTreatmentImmediate:
      CreateHistogramSentimentSurveyRegistration(
          TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
              kTreatmentImmediate);
      break;
    case TrackingProtectionOnboarding::SentimentSurveyGroup::kTreatmentDelayed:
      CreateHistogramSentimentSurveyRegistration(
          TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
              kTreatmentDelayed);
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
}

void RecordHistogramsOnboardingOnStartup(PrefService* pref_service) {
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
  }
}

void RecordHistogramsOnStartup(PrefService* pref_service) {
  RecordHistogramsOnboardingOnStartup(pref_service);
  RecordHistogramsSilentOnboardingOnStartup(pref_service);
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

void MaybeSetStartAndEndSurveyTime(PrefService* pref_service,
                                   bool is_silent_onboarding_enabled) {
  if (pref_service->HasPrefPath(
          prefs::kTrackingProtectionSentimentSurveyStartTime)) {
    return;
  }

  auto group = GetSurveyGroup(pref_service);
  // Setting the start and end time when applicable.
  // First, start by determining the anchor time. Control should be the first
  // time this function runs (unless silent onbaording is enabled, in which
  // case, we'd wait for the profile to get silentl Onboarded). Treatment should
  // be when the notice was acked. If the anchor time isn't year ready, return
  // early.
  base::Time anchor_time;
  switch (group) {
    case SentimentSurveyGroup::kNotSet:
      return;
    case SentimentSurveyGroup::kControlImmediate:
    case SentimentSurveyGroup::kControlDelayed:
      // If Silent Onboarding is enabled, we use the Onboarding time to
      // determine when the survey anchor time (Used to calculate the survey
      // start and end time). If it is not enabled, we use "Now" as the anchor
      // time.
      if (is_silent_onboarding_enabled) {
        if (!pref_service->HasPrefPath(
                prefs::kTrackingProtectionSilentOnboardedSince)) {
          return;
        }
        anchor_time = pref_service->GetTime(
            prefs::kTrackingProtectionSilentOnboardedSince);
      } else {
        anchor_time = base::Time::Now();
      }
      break;
    case SentimentSurveyGroup::kTreatmentImmediate:
    case SentimentSurveyGroup::kTreatmentDelayed:
      if (!pref_service->HasPrefPath(
              prefs::kTrackingProtectionOnboardingAckedSince)) {
        return;
      }
      anchor_time =
          pref_service->GetTime(prefs::kTrackingProtectionOnboardingAckedSince);
      break;
  }

  // Anchor time must be ready. Use it to set the start and end survey time on
  // the profile.
  switch (group) {
    case SentimentSurveyGroup::kNotSet:
      NOTREACHED_NORETURN();
    case SentimentSurveyGroup::kControlImmediate:
    case SentimentSurveyGroup::kTreatmentImmediate:
      pref_service->SetTime(prefs::kTrackingProtectionSentimentSurveyStartTime,
                            anchor_time + kHatsImmediateStartTimeDelta);
      pref_service->SetTime(prefs::kTrackingProtectionSentimentSurveyEndTime,
                            anchor_time + kHatsImmediateEndTimeDelta);
      EmitRegistrationHistogram(group);
      return;
    case SentimentSurveyGroup::kTreatmentDelayed:
    case SentimentSurveyGroup::kControlDelayed:
      pref_service->SetTime(prefs::kTrackingProtectionSentimentSurveyStartTime,
                            anchor_time + kHatsDelayedStartTimeDelta);
      pref_service->SetTime(prefs::kTrackingProtectionSentimentSurveyEndTime,
                            anchor_time + kHatsDelayedEndTimeDelta);
      EmitRegistrationHistogram(group);
      return;
  }
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
  }
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
      prefs::kTrackingProtectionOffboarded,
      base::BindRepeating(
          &TrackingProtectionOnboarding::OnOffboardingPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      base::BindRepeating(
          &TrackingProtectionOnboarding::OnSilentOnboardingPrefChanged,
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
  // Maybe set the Hats start and end time now that the profile is onboarded.
  MaybeSetStartAndEndSurveyTime(pref_service_, is_silent_onboarding_enabled_);
  for (auto& observer : observers_) {
    observer.OnShouldShowNoticeUpdated();
  }
}

void TrackingProtectionOnboarding::OnOffboardingPrefChanged() const {
  for (auto& observer : observers_) {
    observer.OnTrackingProtectionOnboardingUpdated(GetOnboardingStatus());
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

bool TrackingProtectionOnboarding::RequiresSentimentSurveyGroup() {
  // If the pref is in the default value, it means we haven't added this profile
  // to any of the groups yet.
  return !pref_service_->HasPrefPath(
      prefs::kTrackingProtectionSentimentSurveyGroup);
}

void TrackingProtectionOnboarding::RegisterSentimentSurveyGroup(
    SentimentSurveyGroup group) {
  // Setting the group first.
  tracking_protection::TrackingProtectionSentimentSurveyGroup internal_group;
  switch (group) {
    case SentimentSurveyGroup::kNotSet:
      return;
    case SentimentSurveyGroup::kControlImmediate:
      internal_group =
          TrackingProtectionSentimentSurveyGroup::kControlImmediate;
      break;
    case SentimentSurveyGroup::kTreatmentImmediate:
      internal_group =
          TrackingProtectionSentimentSurveyGroup::kTreatmentImmediate;
      break;
    case SentimentSurveyGroup::kControlDelayed:
      internal_group = TrackingProtectionSentimentSurveyGroup::kControlDelayed;
      break;
    case SentimentSurveyGroup::kTreatmentDelayed:
      internal_group =
          TrackingProtectionSentimentSurveyGroup::kTreatmentDelayed;
      break;
  }

  pref_service_->SetInteger(prefs::kTrackingProtectionSentimentSurveyGroup,
                            static_cast<int>(internal_group));

  MaybeSetStartAndEndSurveyTime(pref_service_, is_silent_onboarding_enabled_);
}

TrackingProtectionOnboarding::SentimentSurveyGroup
TrackingProtectionOnboarding::GetEligibleSurveyGroup() {
  SentimentSurveyGroup group = GetSurveyGroup(pref_service_);

  if (group == SentimentSurveyGroup::kNotSet) {
    return group;
  }

  // If Start time isn't set, it means the profile isn't yet eligible. This is
  // because all surveys MUST have a start time.
  if (!pref_service_->HasPrefPath(
          prefs::kTrackingProtectionSentimentSurveyStartTime)) {
    return SentimentSurveyGroup::kNotSet;
  }

  if (base::Time::Now() <
      pref_service_->GetTime(
          prefs::kTrackingProtectionSentimentSurveyStartTime)) {
    return SentimentSurveyGroup::kNotSet;
  }

  // No additional filtering if the end time isn't set. This is to support open
  // ended surveys.
  if (!pref_service_->HasPrefPath(
          prefs::kTrackingProtectionSentimentSurveyEndTime)) {
    return group;
  }

  // At this point, an end date is set, so we need to use it for filtering.
  if (base::Time::Now() >
      pref_service_->GetTime(
          prefs::kTrackingProtectionSentimentSurveyEndTime)) {
    return SentimentSurveyGroup::kNotSet;
  }

  return group;
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

  MaybeSetStartAndEndSurveyTime(pref_service_, is_silent_onboarding_enabled_);
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
      OffboardingNoticeActionTaken(action, pref_service_);
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

TrackingProtectionOnboarding::SilentOnboardingStatus
TrackingProtectionOnboarding::GetSilentOnboardingStatus() const {
  auto onboarding_status = GetInternalSilentOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return SilentOnboardingStatus::kIneligible;
    case TrackingProtectionOnboardingStatus::kEligible:
      return SilentOnboardingStatus::kEligible;
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
