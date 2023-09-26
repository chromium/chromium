// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

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

  class Observer {
   public:
    // Fired when a profile is onboarded (shown the TrackingProtection
    // onboarding notice)
    virtual void OnTrackingProtectionOnboarded() {}
    // Fired when the ShouldSHowNotice is updated (to True or False).
    virtual void OnShouldShowNoticeUpdated() {}
  };

  explicit TrackingProtectionOnboarding(PrefService* pref_service);
  ~TrackingProtectionOnboarding() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // To be called by the Mode B experiment service to indicate that the profile
  // is eligible for onboarding.
  void MaybeMarkEligible();

  // To be called by the Mode B experiment service to indicate that the profile
  // is no longer eligible for onboarding.
  void MaybeMarkIneligible();

  // Indicates the onboarding status for the user. Return value is the enum
  // defined above.
  OnboardingStatus GetOnboardingStatus() const;

  // To be Called by UI code when the user has been shown the notice.
  void NoticeShown();

  // To be called by UI code when the user has taken action on the notice.
  void NoticeActionTaken(NoticeAction action);

  // Called by UI code to determine if we should show the onboarding notice to
  // the user.
  bool ShouldShowOnboardingNotice();

 private:
  // Called when the underlying onboarding pref is changed.
  virtual void OnOnboardingPrefChanged() const;
  // Called when the notice has been acked.
  virtual void OnOnboardingAckedChanged() const;
  base::ObserverList<Observer>::Unchecked observers_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
