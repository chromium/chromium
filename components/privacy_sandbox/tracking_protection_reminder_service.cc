// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_reminder_service.h"

#include "base/feature_list.h"
#include "base/time/time_delta_from_string.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "tracking_protection_prefs.h"
#include "tracking_protection_reminder_service.h"

namespace privacy_sandbox {

namespace {

bool IsReminderEnabled() {
  return base::FeatureList::IsEnabled(
      privacy_sandbox::kTrackingProtectionReminder);
}

bool ShouldReminderBeSilent() {
  return privacy_sandbox::kTrackingProtectionIsSilentReminder.Get();
}

const std::string GetNoticeName(
    TrackingProtectionOnboarding::SurfaceType surface_type) {
  switch (surface_type) {
    case TrackingProtectionOnboarding::SurfaceType::kBrApp:
      return ShouldReminderBeSilent() ? kTrackingProtectionSilentReminderClank
                                      : kTrackingProtectionReminderClank;
    case TrackingProtectionOnboarding::SurfaceType::kDesktop:
      return ShouldReminderBeSilent()
                 ? kTrackingProtectionSilentReminderDesktopIPH
                 : kTrackingProtectionReminderDesktopIPH;
    case TrackingProtectionOnboarding::SurfaceType::kAGACCT:
      NOTREACHED_NORETURN();
  }
}

void RecordNoticeHistogramsOnStartup(
    PrefService* pref_service,
    const PrivacySandboxNoticeStorage& notice_storage) {
  // TODO(crbug.com/333406690): After migration, move this portion to the
  // chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h
  // constructor and emit ALL startup histograms instead of just TP related
  // histograms.
  notice_storage.RecordHistogramsOnStartup(
      pref_service, kTrackingProtectionSilentReminderClank);
  notice_storage.RecordHistogramsOnStartup(pref_service,
                                           kTrackingProtectionReminderClank);
  notice_storage.RecordHistogramsOnStartup(
      pref_service, kTrackingProtectionSilentReminderDesktopIPH);
  notice_storage.RecordHistogramsOnStartup(
      pref_service, kTrackingProtectionReminderDesktopIPH);
}

void SetReminderStatus(
    PrefService* pref_service,
    tracking_protection::TrackingProtectionReminderStatus status) {
  pref_service->SetInteger(prefs::kTrackingProtectionReminderStatus,
                           static_cast<int>(status));
}

std::optional<base::Time> MaybeGetOnboardedTimestamp(
    TrackingProtectionOnboarding* onboarding_service) {
  std::optional<base::Time> onboarded_timestamp =
      onboarding_service->GetOnboardingTimestamp();
  if (onboarded_timestamp.has_value()) {
    return onboarded_timestamp;
  }
  return onboarding_service->GetSilentOnboardingTimestamp();
}

base::TimeDelta GetReminderDelay() {
  return privacy_sandbox::kTrackingProtectionReminderDelay.Get();
}

bool HasEnoughTimePassed(base::Time onboarded_timestamp) {
  base::TimeDelta reminder_delay = GetReminderDelay();
  return base::Time::Now() >= onboarded_timestamp + reminder_delay;
}

tracking_protection::TrackingProtectionReminderStatus GetReminderStatusInternal(
    PrefService* pref_service) {
  return static_cast<tracking_protection::TrackingProtectionReminderStatus>(
      pref_service->GetInteger(prefs::kTrackingProtectionReminderStatus));
}

void MaybeUpdateReminderStatus(PrefService* pref_service,
                               bool was_silently_onboarded = false) {
  // Do not overwrite the current reminder status if it's already set.
  if (GetReminderStatusInternal(pref_service) !=
      tracking_protection::TrackingProtectionReminderStatus::kUnset) {
    return;
  }

  if (!IsReminderEnabled()) {
    // Mark profiles that have had the reminder feature disabled and will not
    // experience any reminder logic. We will need to track this group to
    // ensure they do not receive a reminder in the future if feature
    // parameters change.
    SetReminderStatus(pref_service,
                      tracking_protection::TrackingProtectionReminderStatus::
                          kFeatureDisabledSkipped);
    return;
  }

  if (was_silently_onboarded && !ShouldReminderBeSilent()) {
    // We shouldn't show a reminder for silent onboardings unless it's a
    // silent reminder.
    // TODO(crbug.com/332764120): Emit a event to track this case.
    SetReminderStatus(
        pref_service,
        tracking_protection::TrackingProtectionReminderStatus::kInvalid);
    return;
  }

  SetReminderStatus(
      pref_service,
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder);
}

}  // namespace

TrackingProtectionReminderService::TrackingProtectionReminderService(
    PrefService* pref_service,
    TrackingProtectionOnboarding* onboarding_service)
    : pref_service_(pref_service), onboarding_service_(onboarding_service) {
  if (onboarding_service_) {
    onboarding_observation_.Observe(onboarding_service_);
  }
  CHECK(pref_service_);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kTrackingProtectionReminderStatus,
      base::BindRepeating(
          &TrackingProtectionReminderService::OnReminderStatusChanged,
          base::Unretained(this)));
  notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  RecordNoticeHistogramsOnStartup(pref_service_, *notice_storage_.get());
}

TrackingProtectionReminderService::~TrackingProtectionReminderService() =
    default;

ReminderType TrackingProtectionReminderService::GetReminderType() {
  if (GetReminderStatusInternal(pref_service_) !=
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder) {
    return ReminderType::kNone;
  }

  auto onboarded_timestamp = MaybeGetOnboardedTimestamp(onboarding_service_);
  if (!onboarded_timestamp.has_value()) {
    // This condition should only fail if the profile has not been onboarded.
    // TODO(crbug.com/332764120): Emit a metric detailing that we tried
    // checking if we should show a reminder for a profile that was not
    // onboarded.
    return ReminderType::kNone;
  }

  if (!HasEnoughTimePassed(*onboarded_timestamp)) {
    // Not enough time has passed to show the reminder.
    return ReminderType::kNone;
  }

  return ShouldReminderBeSilent() ? ReminderType::kSilent
                                  : ReminderType::kActive;
}

void TrackingProtectionReminderService::OnReminderExperienced(
    TrackingProtectionOnboarding::SurfaceType surface_type) {
  notice_storage_->SetNoticeShown(pref_service_, GetNoticeName(surface_type),
                                  base::Time::Now());
  if (GetReminderStatus() ==
      tracking_protection::TrackingProtectionReminderStatus::kPendingReminder) {
    SetReminderStatus(pref_service_,
                      tracking_protection::TrackingProtectionReminderStatus::
                          kExperiencedReminder);
  }
}

bool TrackingProtectionReminderService::IsPendingReminder() {
  return GetReminderStatus() ==
         tracking_protection::TrackingProtectionReminderStatus::
             kPendingReminder;
}

tracking_protection::TrackingProtectionReminderStatus
TrackingProtectionReminderService::GetReminderStatus() {
  return GetReminderStatusInternal(pref_service_);
}

void TrackingProtectionReminderService::OnReminderActionTaken(
    NoticeActionTaken action_taken,
    base::Time action_taken_time,
    TrackingProtectionOnboarding::SurfaceType surface_type) {
  notice_storage_->SetNoticeActionTaken(pref_service_,
                                        GetNoticeName(surface_type),
                                        action_taken, action_taken_time);
}

std::optional<PrivacySandboxNoticeData>
TrackingProtectionReminderService::GetReminderNoticeData(
    TrackingProtectionOnboarding::SurfaceType surface_type) {
  return notice_storage_->ReadNoticeData(pref_service_,
                                         GetNoticeName(surface_type));
}

void TrackingProtectionReminderService::Shutdown() {
  observers_.Clear();
  pref_change_registrar_.Reset();
}

void TrackingProtectionReminderService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void TrackingProtectionReminderService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void TrackingProtectionReminderService::OnReminderStatusChanged() {
  for (auto& observer : observers_) {
    observer.OnTrackingProtectionReminderStatusChanged(
        GetReminderStatusInternal(pref_service_));
  }
}

void TrackingProtectionReminderService::OnTrackingProtectionOnboardingUpdated(
    TrackingProtectionOnboarding::OnboardingStatus onboarding_status) {
  if (onboarding_status ==
      TrackingProtectionOnboarding::OnboardingStatus::kOnboarded) {
    // Exclude Mode B users from receiving reminders and surveys.
    if (is_mode_b_user_) {
      SetReminderStatus(pref_service_,
                        tracking_protection::TrackingProtectionReminderStatus::
                            kModeBUserSkipped);
      return;
    }
    MaybeUpdateReminderStatus(pref_service_);
  }
}

void TrackingProtectionReminderService::
    OnTrackingProtectionSilentOnboardingUpdated(
        TrackingProtectionOnboarding::SilentOnboardingStatus
            onboarding_status) {
  if (onboarding_status ==
      TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded) {
    // Exclude Mode B users from receiving reminders and surveys.
    if (is_mode_b_user_) {
      SetReminderStatus(pref_service_,
                        tracking_protection::TrackingProtectionReminderStatus::
                            kModeBUserSkipped);
      return;
    }
    MaybeUpdateReminderStatus(pref_service_, true);
  }
}

}  // namespace privacy_sandbox
