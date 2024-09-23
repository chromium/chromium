// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace privacy_sandbox {

// A Service which controls the onboarding onto tracking protection - namely
// Third Party Cookie Deprecation. It is meant to be called from the Mode B/B'
// Experiment service, as well as the Cookie Settings service.
class TrackingProtectionOnboarding : public KeyedService {
 public:
  // Enum value interfacing with the TrackingProtectionOnboarding service
  // callers, to indicate the status the onboarding is at.
  enum class OnboardingStatus {
    kIneligible = 0,
    kEligible = 1,
    kOnboarded = 2,
    kMaxValue = kOnboarded,
  };

  // Enum value interfacing with the TrackingProtectionOnboarding service
  // callers, to indicate the status the silent onboarding is at.
  enum class SilentOnboardingStatus {
    kIneligible = 0,
    kEligible = 1,
    kOnboarded = 2,
    kMaxValue = kOnboarded,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Enum value to indicate the state of onboarding on startup.
  enum class OnboardingStartupState {
    // User was ineligible on startup.
    kIneligible = 0,
    // User was eligible on startup but hasn't been onboarded yet on startup.
    kEligibleWaitingToOnboard = 1,
    // User was onboarded but has not yet acknowledged the notice on startup.
    kOnboardedWaitingToAck = 2,
    // User acknowledged with the GotIt button on startup.
    kAckedGotIt = 3,
    // User acknowledged with the Settings button on startup
    kAckedSettings = 4,
    // User acknowledged with the closed button on startup.
    kAckedClosed = 5,
    // User acknowledged with the learn more button (only on Clank) on startup.
    kAckedLearnMore = 6,
    // User acknowledged the notice by dismissing due to other actions on
    // startup.
    kAckedOther = 7,
    kMaxValue = kAckedOther,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // Enum value to indicate the state of silent onboarding on startup.
  enum class SilentOnboardingStartupState {
    // User was ineligible on startup.
    kIneligible = 0,
    // User was eligible on startup but hasn't been onboarded yet on startup.
    kEligibleWaitingToOnboard = 1,
    // User was onboarded on startup.
    kOnboarded = 2,
    kMaxValue = kOnboarded,
  };

  explicit TrackingProtectionOnboarding(PrefService* pref_service);
  ~TrackingProtectionOnboarding() override;

  // KeyedService:
  void Shutdown() override;

  // Indicates the onboarding status for the user. Return value is the enum
  // defined above.
  OnboardingStatus GetOnboardingStatus() const;

  // Indicates the silent onboarding status for the user. Return value is the
  // enum defined above.
  SilentOnboardingStatus GetSilentOnboardingStatus() const;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
