// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingAckAction;
using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

TrackingProtectionOnboardingStatus GetInternalModeBOnboardingStatus(
    PrefService* pref_service) {
  return static_cast<TrackingProtectionOnboardingStatus>(
      pref_service->GetInteger(prefs::kTrackingProtectionOnboardingStatus));
}

TrackingProtectionOnboardingStatus GetInternalModeBSilentOnboardingStatus(
    PrefService* pref_service) {
  return static_cast<TrackingProtectionOnboardingStatus>(
      pref_service->GetInteger(
          prefs::kTrackingProtectionSilentOnboardingStatus));
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
  auto status = GetInternalModeBOnboardingStatus(pref_service);
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
  auto status = GetInternalModeBSilentOnboardingStatus(pref_service);
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
      NOTREACHED();
    }
  }
}

void RecordHistogramsOnStartup(PrefService* pref_service) {
  RecordHistogramsOnboardingOnStartup(pref_service);
  RecordHistogramsSilentOnboardingOnStartup(pref_service);
}

}  // namespace

TrackingProtectionOnboarding::TrackingProtectionOnboarding(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service_);

  RecordHistogramsOnStartup(pref_service_);
}

TrackingProtectionOnboarding::~TrackingProtectionOnboarding() = default;

void TrackingProtectionOnboarding::Shutdown() {
  pref_service_ = nullptr;
}

TrackingProtectionOnboarding::OnboardingStatus
TrackingProtectionOnboarding::GetOnboardingStatus() const {
  auto onboarding_status = GetInternalModeBOnboardingStatus(pref_service_);
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
  auto onboarding_status =
      GetInternalModeBSilentOnboardingStatus(pref_service_);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return SilentOnboardingStatus::kIneligible;
    case TrackingProtectionOnboardingStatus::kEligible:
      return SilentOnboardingStatus::kEligible;
    case TrackingProtectionOnboardingStatus::kRequested:
      NOTREACHED();
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return SilentOnboardingStatus::kOnboarded;
  }
}

}  // namespace privacy_sandbox
