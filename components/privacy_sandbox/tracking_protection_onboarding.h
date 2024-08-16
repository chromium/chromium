// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/version_info/channel.h"

class PrefService;

namespace tpcd::experiment {
class EligibilityServiceTest;
}  // namespace tpcd::experiment

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

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class NoticeAction {
    // Other action taken - notice dismissed due to other actions.
    kOther = 0,
    // Using the GotIt button.
    kGotIt = 1,
    // Using the Settings button.
    kSettings = 2,
    // Using the LearnMore button - only on Clank.
    kLearnMore = 3,
    // The X button on desktop / swipe away on Clank.
    kClosed = 4,
    kMaxValue = kClosed,
  };

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class NoticeType {
    kNone,
    // The notice in question is a Mode B Onboarding Notice.
    kModeBOnboarding,
    // The notice in question is a silent Mode B Onboarding Notice.
    kModeBSilentOnboarding,
  };

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.privacy_sandbox
  enum class SurfaceType {
    kDesktop = 0,
    kBrApp = 1,
    kAGACCT = 2,
    kMaxValue = kAGACCT,
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

  class Observer {
   public:
    // Fired when a profile's tracking protection onboarding state is changed.
    virtual void OnTrackingProtectionOnboardingUpdated(
        OnboardingStatus onboarding_status) {}

    // Fired when a profile's tracking protection silent onboarding state is
    // changed.
    virtual void OnTrackingProtectionSilentOnboardingUpdated(
        SilentOnboardingStatus onboarding_status) {}
  };

  TrackingProtectionOnboarding(PrefService* pref_service,
                               version_info::Channel channel,
                               bool is_silent_onboarding_enabled = false);
  ~TrackingProtectionOnboarding() override;

  // KeyedService:
  void Shutdown() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // To be called by the Mode B experiment service to indicate that the profile
  // is eligible for onboarding.
  void MaybeMarkModeBEligible();

  // To be called by the Mode B experiment service to indicate that the profile
  // is no longer eligible for onboarding.
  void MaybeMarkModeBIneligible();

  // To be called by the experiment service to indicate that the profile is
  // eligible for silent onboarding.
  void MaybeMarkModeBSilentEligible();

  // To be called by the experiment service to indicate that the profile is no
  // longer eligible for silent onboarding.
  void MaybeMarkModeBSilentIneligible();

  // To be called by the Mode B experiment service in BETA, DEV and CANARY only
  // to reset the user's prefs for testing.
  void MaybeResetModeBOnboardingPrefs();

  // Indicates the onboarding status for the user. Return value is the enum
  // defined above.
  OnboardingStatus GetOnboardingStatus() const;

  // Indicates the silent onboarding status for the user. Return value is the
  // enum defined above.
  SilentOnboardingStatus GetSilentOnboardingStatus() const;

  // To be Called by UI code when the user has been shown the notice.
  void NoticeShown(SurfaceType surface, NoticeType notice_type);

  // To be called by UI code when the user has taken action on the notice.
  void NoticeActionTaken(SurfaceType surface,
                         NoticeType notice_type,
                         NoticeAction action);

 private:
  friend class tpcd::experiment::EligibilityServiceTest;

  // Called when the underlying onboarding pref is changed.
  virtual void OnOnboardingPrefChanged() const;
  // Called when the underlying silent onboarding pref is changed.
  virtual void OnSilentOnboardingPrefChanged() const;

  base::ObserverList<Observer>::Unchecked observers_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  version_info::Channel channel_;
  bool is_silent_onboarding_enabled_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
