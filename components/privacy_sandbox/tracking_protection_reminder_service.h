// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "tracking_protection_prefs.h"

namespace privacy_sandbox {

enum class ReminderType {
  kNone,    // Don't show a reminder.
  kSilent,  // Check if a reminder would be shown.
  kActive,  // Show a visual reminder.
};

class TrackingProtectionReminderService
    : TrackingProtectionOnboarding::Observer,
      public KeyedService {
 public:
  class Observer {
   public:
    // Fired when the reminder status is updated.
    virtual void OnTrackingProtectionReminderStatusChanged(
        tracking_protection::TrackingProtectionReminderStatus reminder_status) {
    }
  };

  explicit TrackingProtectionReminderService(
      PrefService* pref_service,
      TrackingProtectionOnboarding* onboarding_service);
  ~TrackingProtectionReminderService() override;
  TrackingProtectionReminderService(const TrackingProtectionReminderService&) =
      delete;
  TrackingProtectionReminderService& operator=(
      const TrackingProtectionReminderService&) = delete;

  // Determines the type of reminder that should be experienced.
  ReminderType GetReminderType();

  // Returns if the profile is pending a reminder.
  bool IsPendingReminder();

  // Returns the reminder status for the user.
  tracking_protection::TrackingProtectionReminderStatus GetReminderStatus();

  // Called after a reminder was experienced.
  void OnReminderExperienced(
      TrackingProtectionOnboarding::SurfaceType surface_type);

  // Called when a reminder was shown and an action was taken.
  void OnReminderActionTaken(
      privacy_sandbox::NoticeActionTaken action_taken,
      base::Time action_taken_time,
      TrackingProtectionOnboarding::SurfaceType surface_type);

  // Returns notice data for the reminder.
  std::optional<PrivacySandboxNoticeData> GetReminderNoticeData(
      TrackingProtectionOnboarding::SurfaceType surface_type);

  // KeyedService:
  void Shutdown() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // From TrackingProtectionOnboarding::Observer
  void OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus onboarding_status)
      override;
  void OnTrackingProtectionSilentOnboardingUpdated(
      TrackingProtectionOnboarding::SilentOnboardingStatus onboarding_status)
      override;

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
  std::unique_ptr<PrivacySandboxNoticeStorage> notice_storage_;
  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};

  void OnReminderStatusChanged();

  // TODO(b/342413229): Remove this when updating Mode B detection.
  // Default this to true to prevent reminder logic from running.
  bool is_mode_b_user_ = true;
  friend class TrackingProtectionReminderServiceModeBEnabledTest;
  friend class TrackingProtectionReminderServiceTest;
  friend class TrackingProtectionReminderDesktopUiControllerTest;
};

}  // namespace privacy_sandbox
#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_
