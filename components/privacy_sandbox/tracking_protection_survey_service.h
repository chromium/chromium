// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SURVEY_SERVICE_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SURVEY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_onboarding.h"

namespace privacy_sandbox {

// Service to handle eligiblity for tracking protection surveys. This service
// will not directly surface a survey since that logic is platform dependent and
// will instead help determine if one should be shown.
class TrackingProtectionSurveyService : TrackingProtectionOnboarding::Observer,
                                        public KeyedService {
 public:
  explicit TrackingProtectionSurveyService(
      PrefService* pref_service,
      TrackingProtectionOnboarding* onboarding_service);
  ~TrackingProtectionSurveyService() override;
  TrackingProtectionSurveyService(const TrackingProtectionSurveyService&) =
      delete;
  TrackingProtectionSurveyService& operator=(
      const TrackingProtectionSurveyService&) = delete;

  // TODO(crbug.com/357883287): Remove references to onboarding.
  // From TrackingProtectionOnboarding::Observer
  void OnTrackingProtectionOnboardingUpdated(
      TrackingProtectionOnboarding::OnboardingStatus onboarding_status)
      override;
  void OnTrackingProtectionSilentOnboardingUpdated(
      TrackingProtectionOnboarding::SilentOnboardingStatus onboarding_status)
      override;

 private:
  raw_ptr<PrefService> pref_service_;
  // TODO(crbug.com/357883287): Remove references to onboarding.
  raw_ptr<TrackingProtectionOnboarding> onboarding_service_;
  base::ScopedObservation<TrackingProtectionOnboarding,
                          TrackingProtectionOnboarding::Observer>
      onboarding_observation_{this};
};

}  // namespace privacy_sandbox
#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_SURVEY_SERVICE_H_
