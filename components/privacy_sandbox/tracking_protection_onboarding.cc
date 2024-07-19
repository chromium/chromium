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
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/version_info/channel.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingAckAction;
using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using SurfaceType = privacy_sandbox::TrackingProtectionOnboarding::SurfaceType;

constexpr std::string_view Full3PCDNoticeNames[] = {
    kFull3PCDIPH,        kFull3PCDClankBrApp,        kFull3PCDClankCCT,
    kFull3PCDSilentIPH,  kFull3PCDSilentClankBrApp,  kFull3PCDSilentClankCCT,
    kFull3PCDWithIPPIPH, kFull3PCDWithIPPClankBrApp, kFull3PCDWithIPPClankCCT};

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
      NOTREACHED_NORETURN();
    }
  }
}

void RecordHistogramsOnStartup(PrefService* pref_service) {
  RecordHistogramsOnboardingOnStartup(pref_service);
  RecordHistogramsSilentOnboardingOnStartup(pref_service);
}

TrackingProtectionOnboarding::NoticeType GetRequiredModeBSilentOnboardingNotice(
    PrefService* pref_service) {
  auto onboarding_status = GetInternalModeBSilentOnboardingStatus(pref_service);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
    case TrackingProtectionOnboardingStatus::kOnboarded:
      return TrackingProtectionOnboarding::NoticeType::kNone;
    case TrackingProtectionOnboardingStatus::kEligible:
      return TrackingProtectionOnboarding::NoticeType::kModeBSilentOnboarding;
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

bool HasOnboardedAndAckedModeB(PrefService* pref_service) {
  return GetInternalModeBOnboardingStatus(pref_service) ==
             tracking_protection::TrackingProtectionOnboardingStatus::
                 kOnboarded &&
         pref_service->GetBoolean(prefs::kTrackingProtectionOnboardingAcked);
}

bool HasAcked3PCDNotice(PrefService* pref_service) {
  // TODO(crbug.com/351835842) Returned Ack status based on NoticeStorage.
  return false;
}

NoticeType GetRequiredModeBNotice(SurfaceType surface,
                                  PrefService* pref_service) {
  if (surface != SurfaceType::kDesktop && surface != SurfaceType::kBrApp) {
    return NoticeType::kNone;
  }

  auto onboarding_status = GetInternalModeBOnboardingStatus(pref_service);
  switch (onboarding_status) {
    case TrackingProtectionOnboardingStatus::kIneligible:
      return GetRequiredModeBSilentOnboardingNotice(pref_service);
    case TrackingProtectionOnboardingStatus::kEligible:
    case TrackingProtectionOnboardingStatus::kRequested: {
      return NoticeType::kModeBOnboarding;
    }
    case TrackingProtectionOnboardingStatus::kOnboarded: {
      // We've already showed the user the onboarding notice. We keep showing
      // the Onboarding Notice until they Ack.
      return pref_service->GetBoolean(prefs::kTrackingProtectionOnboardingAcked)
                 ? NoticeType::kNone
                 : NoticeType::kModeBOnboarding;
    }
  }
}

void ModeBNoticeActionTaken(TrackingProtectionOnboarding::NoticeAction action,
                            PrefService* pref_service) {
  RecordActionMetrics(action);

  if (pref_service->GetBoolean(prefs::kTrackingProtectionOnboardingAcked)) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding."
        "DidNoticeActionAckowledge",
        false);
    return;
  }

  pref_service->SetTime(prefs::kTrackingProtectionOnboardingAckedSince,
                        base::Time::Now());
  pref_service->SetInteger(prefs::kTrackingProtectionOnboardingAckAction,
                           static_cast<int>(ToInternalAckAction(action)));
  pref_service->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

  auto onboarding_to_acked_duration =
      base::Time::Now() -
      pref_service->GetTime(prefs::kTrackingProtectionOnboardedSince);
  auto last_shown_to_acked_duration =
      base::Time::Now() -
      pref_service->GetTime(prefs::kTrackingProtectionNoticeLastShown);
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

void ModeBNoticeShown(PrefService* pref_service) {
  base::RecordAction(
      base::UserMetricsAction("TrackingProtection.Notice.Shown"));
  base::Time now = base::Time::Now();
  pref_service->SetTime(prefs::kTrackingProtectionNoticeLastShown, now);
  auto status = GetInternalModeBOnboardingStatus(pref_service);
  if (status != TrackingProtectionOnboardingStatus::kEligible &&
      status != TrackingProtectionOnboardingStatus::kRequested) {
    base::UmaHistogramBoolean(
        "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
        false);
    return;
  }
  pref_service->SetTime(prefs::kTrackingProtectionOnboardedSince, now);
  pref_service->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));
  auto eligible_to_onboarded_duration =
      now - pref_service->GetTime(prefs::kTrackingProtectionEligibleSince);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.Onboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration);

  base::UmaHistogramBoolean(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
      true);
}

void ModeBSilentNoticeShown(PrefService* pref_service) {
  auto status = GetInternalModeBSilentOnboardingStatus(pref_service);
  if (status != TrackingProtectionOnboardingStatus::kEligible) {
    RecordSilentOnboardingDidNoticeShownOnboard(false);
    return;
  }
  pref_service->SetTime(prefs::kTrackingProtectionSilentOnboardedSince,
                        base::Time::Now());
  auto eligible_to_onboarded_duration =
      pref_service->GetTime(prefs::kTrackingProtectionSilentOnboardedSince) -
      pref_service->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
  CreateTimingHistogramOnboardingStartup(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration);
  pref_service->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));
  RecordSilentOnboardingDidNoticeShownOnboard(true);
}

std::string Get3PCDNoticeName(TrackingProtectionOnboarding::SurfaceType surface,
                              NoticeType notice_type) {
  switch (notice_type) {
    case NoticeType::kFull3PCDOnboarding:
      switch (surface) {
        case SurfaceType::kDesktop:
          return kFull3PCDIPH;
        case SurfaceType::kBrApp:
          return kFull3PCDClankBrApp;
        case SurfaceType::kAGACCT:
          return kFull3PCDClankCCT;
      }
      break;
    case NoticeType::kFull3PCDSilentOnboarding:
      switch (surface) {
        case SurfaceType::kDesktop:
          return kFull3PCDSilentIPH;
        case SurfaceType::kBrApp:
          return kFull3PCDSilentClankBrApp;
        case SurfaceType::kAGACCT:
          return kFull3PCDSilentClankCCT;
      }
      break;
    case NoticeType::kFull3PCDOnboardingWithIPP:
      switch (surface) {
        case SurfaceType::kDesktop:
          return kFull3PCDWithIPPIPH;
        case SurfaceType::kBrApp:
          return kFull3PCDWithIPPClankBrApp;
        case SurfaceType::kAGACCT:
          return kFull3PCDWithIPPClankCCT;
      }
      break;
    default:
      NOTREACHED_NORETURN();
  }
}

NoticeActionTaken ToNoticeActionTaken(
    TrackingProtectionOnboarding::NoticeAction action) {
  switch (action) {
    case TrackingProtectionOnboarding::NoticeAction::kOther:
      return NoticeActionTaken::kOther;
    case TrackingProtectionOnboarding::NoticeAction::kGotIt:
      return NoticeActionTaken::kAck;
    case TrackingProtectionOnboarding::NoticeAction::kSettings:
      return NoticeActionTaken::kSettings;
    case TrackingProtectionOnboarding::NoticeAction::kLearnMore:
      return NoticeActionTaken::kLearnMore;
    case TrackingProtectionOnboarding::NoticeAction::kClosed:
      return NoticeActionTaken::kClosed;
  }
}

}  // namespace

TrackingProtectionOnboarding::TrackingProtectionOnboarding(
    std::unique_ptr<Delegate> delegate,
    PrefService* pref_service,
    version_info::Channel channel,
    bool is_silent_onboarding_enabled)
    : delegate_(std::move(delegate)),
      pref_service_(pref_service),
      channel_(channel),
      is_silent_onboarding_enabled_(is_silent_onboarding_enabled) {
  CHECK(pref_service_);
  CHECK(delegate_);
  notice_storage_ =
      std::make_unique<privacy_sandbox::PrivacySandboxNoticeStorage>();

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
  // TODO(crbug.com/333406690): After migration, move this portion to the
  // chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h constructor
  // and emit ALL startup histograms instead of just TP related histograms.
  for (const std::string_view name_3pcd : Full3PCDNoticeNames) {
    notice_storage_->RecordHistogramsOnStartup(pref_service_, name_3pcd);
  }
}

TrackingProtectionOnboarding::~TrackingProtectionOnboarding() = default;

void TrackingProtectionOnboarding::Shutdown() {
  delegate_.reset();
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

  switch (GetInternalModeBOnboardingStatus(pref_service_)) {
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

bool TrackingProtectionOnboarding::IsEnterpriseManaged() const {
  return delegate_->IsEnterpriseManaged();
}

bool TrackingProtectionOnboarding::IsNewProfile() const {
  return delegate_->IsNewProfile();
}

bool TrackingProtectionOnboarding::AreThirdPartyCookiesBlocked() const {
  return delegate_->AreThirdPartyCookiesBlocked();
}

void TrackingProtectionOnboarding::MaybeMarkModeBEligible() {
  auto status = GetInternalModeBOnboardingStatus(pref_service_);
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

void TrackingProtectionOnboarding::MaybeMarkModeBIneligible() {
  auto status = GetInternalModeBOnboardingStatus(pref_service_);
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

void TrackingProtectionOnboarding::MaybeMarkModeBSilentEligible() {
  auto status = GetInternalModeBSilentOnboardingStatus(pref_service_);
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

void TrackingProtectionOnboarding::MaybeMarkModeBSilentIneligible() {
  auto status = GetInternalModeBSilentOnboardingStatus(pref_service_);
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

void TrackingProtectionOnboarding::MaybeResetModeBOnboardingPrefs() {
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

void TrackingProtectionOnboarding::NoticeShown(SurfaceType surface,
                                               NoticeType notice_type) {
  switch (notice_type) {
    case NoticeType::kNone:
      return;
    case NoticeType::kModeBOnboarding:
      ModeBNoticeShown(pref_service_);
      return;
    case NoticeType::kModeBSilentOnboarding:
      ModeBSilentNoticeShown(pref_service_);
      return;
    case NoticeType::kFull3PCDOnboarding:
    case NoticeType::kFull3PCDSilentOnboarding:
    case NoticeType::kFull3PCDOnboardingWithIPP:
      // TODO(crbug.com/353396271): Set the 3pcd Onboarded pref (excluding the
      // silent onboarding case)
      notice_storage_->SetNoticeShown(pref_service_,
                                      Get3PCDNoticeName(surface, notice_type),
                                      base::Time::Now());
  }
}

void TrackingProtectionOnboarding::NoticeActionTaken(SurfaceType surface,
                                                     NoticeType notice_type,
                                                     NoticeAction action) {
  switch (notice_type) {
    case NoticeType::kNone:
      return;
    case NoticeType::kModeBOnboarding:
      ModeBNoticeActionTaken(action, pref_service_);
      return;
    case NoticeType::kModeBSilentOnboarding:
      return;
    case NoticeType::kFull3PCDSilentOnboarding:
      return;
    case NoticeType::kFull3PCDOnboarding:
    case NoticeType::kFull3PCDOnboardingWithIPP:
      // TODO(crbug.com/353396271): Set the 3pcd ack bit.
      notice_storage_->SetNoticeActionTaken(
          pref_service_, Get3PCDNoticeName(surface, notice_type),
          ToNoticeActionTaken(action), base::Time::Now());
  }
}

bool TrackingProtectionOnboarding::ShouldRunUILogic(SurfaceType surface) {
  // TODO(crbug.com/341975190) Remove dependency on GetRequiredNotice for when
  // Full 3PCD logic is implemented.
  return GetRequiredNotice(surface) != NoticeType::kNone;
}

NoticeType Get3PCDNoticeFromFeature() {
  if (!base::FeatureList::IsEnabled(
          privacy_sandbox::kTrackingProtectionOnboarding)) {
    return NoticeType::kNone;
  }

  if (!kTrackingProtectionBlock3PC.Get()) {
    return NoticeType::kFull3PCDSilentOnboarding;
  }

  if (base::FeatureList::IsEnabled(privacy_sandbox::kIpProtectionUx)) {
    return NoticeType::kFull3PCDOnboardingWithIPP;
  }

  return NoticeType::kFull3PCDOnboarding;
}

NoticeType Get3PCDNoticeFromFeatureForSurface(SurfaceType surface) {
  switch (surface) {
    case SurfaceType::kDesktop:
    case SurfaceType::kBrApp:
      return Get3PCDNoticeFromFeature();
    case SurfaceType::kAGACCT:
      // TODO(crbug.com/353266883) Use app open heuristics to only show the
      // notice if the user doesn't use a better suited surface (ie BrAPp). Pay
      // close attention to what happens if they're not a BrApp user, but also
      // not necessarily a AGSA CCT user (if we don't have enough data for
      // example)
      NOTREACHED_NORETURN();
  }
  NOTREACHED_NORETURN();
}

NoticeType TrackingProtectionOnboarding::GetRequiredNotice(
    SurfaceType surface) {
  // If we're already acked 3pcd then no need to show anything else.
  if (HasAcked3PCDNotice(pref_service_)) {
    return NoticeType::kNone;
  }

  // The groups that was added to Mode B, and then later added to
  // 3PCD Silent treatment will also be excluded.
  // This check should only catch some edge cases, as Clients (clank and
  // Desktop) shouldn't call this function if ShouldRunUiLogic returns false
  // (which it will dor this group of users)
  if (HasOnboardedAndAckedModeB(pref_service_)) {
    // TODO(crbug.com/353380550) Add histograms to track how oftern these edge
    // cases happen.
    return NoticeType::kNone;
  }

  // Here means we're NOT already Full 3PCD Acked. Are we in the 3PCD
  // experiment at all?
  NoticeType notice_type = Get3PCDNoticeFromFeatureForSurface(surface);

  // TODO(crbug.com/349787413) Verify Eligibility Conditions before proceeding
  // further.

  switch (notice_type) {
    case NoticeType::kNone:
      // No Full 3PCD required means we need to continue on with the rest of the
      // logic (Mode B).
      break;
    case NoticeType::kFull3PCDSilentOnboarding:
      // TODO(crbug.com/351835842)
      // Check if we were previously silently onboarded, using the notice
      // Storage. No need to re silent onboard if the answer is yes.
      return notice_type;
    case NoticeType::kFull3PCDOnboarding:
    case NoticeType::kFull3PCDOnboardingWithIPP:
      // There are real notices to be shown. return them.
      return notice_type;
    case NoticeType::kModeBOnboarding:
    case NoticeType::kModeBSilentOnboarding:
      // Mode B notices should never be returned from the 3PCD notice function.
      NOTREACHED_NORETURN();
  }

  // Now continue with the Mode B logic.
  return GetRequiredModeBNotice(surface, pref_service_);
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

std::optional<base::Time>
TrackingProtectionOnboarding::GetOnboardingTimestamp() {
  if (!pref_service_->HasPrefPath(prefs::kTrackingProtectionOnboardedSince) ||
      GetOnboardingStatus() != OnboardingStatus::kOnboarded) {
    return std::nullopt;
  }
  return pref_service_->GetTime(prefs::kTrackingProtectionOnboardedSince);
}

std::optional<base::Time>
TrackingProtectionOnboarding::GetSilentOnboardingTimestamp() {
  if (!pref_service_->HasPrefPath(
          prefs::kTrackingProtectionSilentOnboardedSince) ||
      GetSilentOnboardingStatus() != SilentOnboardingStatus::kOnboarded) {
    return std::nullopt;
  }
  return pref_service_->GetTime(prefs::kTrackingProtectionSilentOnboardedSince);
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
