// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"

namespace privacy_sandbox {

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
}

TrackingProtectionOnboarding::~TrackingProtectionOnboarding() = default;

void TrackingProtectionOnboarding::OnOnboardingPrefChanged() const {
  if (GetOnboardingStatus() != OnboardingStatus::kOnboarded) {
    return;
  }

  for (auto& observer : observers_) {
    observer.OnTrackingProtectionOnboarded();
  }
}

TrackingProtectionOnboarding::OnboardingStatus
TrackingProtectionOnboarding::GetOnboardingStatus() const {
  auto onboarding_status =
      static_cast<tracking_protection::TrackingProtectionOnboardingStatus>(
          pref_service_->GetInteger(
              prefs::kTrackingProtectionOnboardingStatus));

  switch (onboarding_status) {
    case tracking_protection::TrackingProtectionOnboardingStatus::kIneligible:
      return OnboardingStatus::kIneligible;
    case tracking_protection::TrackingProtectionOnboardingStatus::kEligible:
      return OnboardingStatus::kEligible;
    case tracking_protection::TrackingProtectionOnboardingStatus::kOnboarded:
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
