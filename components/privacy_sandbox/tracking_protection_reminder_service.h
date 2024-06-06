// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

namespace privacy_sandbox {

class TrackingProtectionReminderService
    : TrackingProtectionOnboarding::Observer,
      public KeyedService {
 public:
  explicit TrackingProtectionReminderService(
      PrefService* pref_service,
      TrackingProtectionOnboarding* onboarding_service);
  ~TrackingProtectionReminderService() override;
  TrackingProtectionReminderService(const TrackingProtectionReminderService&) =
      delete;
  TrackingProtectionReminderService& operator=(
      const TrackingProtectionReminderService&) = delete;

  // From TrackingProtectionOnboarding::Observer
  void OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus onboarding_status)
      override;
  void OnTrackingProtectionSilentOnboardingUpdated(
      TrackingProtectionOnboarding::SilentOnboardingStatus onboarding_status)
      override;

 private:
  raw_ptr<PrefService> pref_service_;
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};

  // TODO(b/342413229): Remove this when updating Mode B detection.
  // Default this to true to prevent reminder logic from running.
  bool is_mode_b_user_ = true;
  friend class TrackingProtectionReminderServiceModeBEnabledTest;
  friend class TrackingProtectionReminderServiceTest;
};

}  // namespace privacy_sandbox
#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_SERVICE_H_
