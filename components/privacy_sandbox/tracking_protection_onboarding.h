// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_
#define COMPONENTS_PRIVACY_SANDBOX_TRACKING_PROTECTION_ONBOARDING_H_

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
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
    kOffboarded = 3,
    kOnboardingRequested = 4,
    kMaxValue = kOnboardingRequested,
  };

  // Enum value interfacing with the TrackingProtectionOnboarding service
  // callers, to indicate the status the silent onboarding is at.
  enum class SilentOnboardingStatus {
    kIneligible = 0,
    kEligible = 1,
    kOnboarded = 2,
    kMaxValue = kOnboarded,
  };

  // Enum used for interfacing with the onboarding service to indicate the HaTS
  // group the profile belongs to.
  enum class SentimentSurveyGroup {
    kNotSet = 0,
    kControlImmediate = 1,
    kTreatmentImmediate = 2,
    kControlDelayed = 3,
    kTreatmentDelayed = 4,
  };

  // Enum used for emitting metrics during the process of the Sentiment Survey
  // given to users.
  enum class SentimentSurveyGroupMetrics {
    kControlImmediate = 0,
    kTreatmentImmediate = 1,
    kControlDelayed = 2,
    kTreatmentDelayed = 3,
    kMaxValue = kTreatmentDelayed,
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
    // The notice in question is an Onboarding Notice.
    kOnboarding,
    // The notice in question is an offboarding/rollback notice.
    kOffboarding,
    // The notice in question is a silent onboarding notice.
    kSilentOnboarding,
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

    // Fired when the ShouldShowNotice is updated (to True or False).
    virtual void OnShouldShowNoticeUpdated() {}

    // Fired when a profile's tracking protection silent onboarding state is
    // changed.
    virtual void OnTrackingProtectionSilentOnboardingUpdated(
        SilentOnboardingStatus onboarding_status) {}
  };

  TrackingProtectionOnboarding(PrefService* pref_service,
                               version_info::Channel channel,
                               bool is_silent_onboarding_enabled = false);
  ~TrackingProtectionOnboarding() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // To be called by the Mode B experiment service to indicate that the profile
  // is eligible for onboarding.
  void MaybeMarkEligible();

  // To be called by the Mode B experiment service to indicate that the profile
  // is no longer eligible for onboarding.
  void MaybeMarkIneligible();

  // To be called by the experiment service to indicate that the profile is
  // eligible for silent onboarding.
  void MaybeMarkSilentEligible();

  // To be called by the experiment service to indicate that the profile is no
  // longer eligible for silent onboarding.
  void MaybeMarkSilentIneligible();

  // To be called by the Mode B experiment service in BETA, DEV and CANARY only
  // to reset the user's prefs for testing.
  void MaybeResetOnboardingPrefs();

  // Indicates the onboarding status for the user. Return value is the enum
  // defined above.
  OnboardingStatus GetOnboardingStatus() const;

  // Indicates the silent onboarding status for the user. Return value is the
  // enum defined above.
  SilentOnboardingStatus GetSilentOnboardingStatus() const;

  // Returns whether the profile has been offboarded.
  bool IsOffboarded() const;

  // To be called by UI code when we've requested the onboarding notice.
  void OnboardingNoticeRequested();

  // To be called by UI code when we've requested the notice.
  void NoticeRequested(NoticeType notice_type);

  // To be Called by UI code when the user has been shown the notice.
  void NoticeShown(NoticeType notice_type);

  // To be called by UI code when the user has taken action on the notice.
  void NoticeActionTaken(NoticeType notice_type, NoticeAction action);

  // Called by UI code to determine what type of notice is required.
  NoticeType GetRequiredNotice();

  // To be called by UI code when the user has taken action on the onboarding
  // notice.
  void OnboardingNoticeActionTaken(NoticeAction action);

  // To be Called by UI code when the user has been shown the onboarding notice.
  void OnboardingNoticeShown();

  // To be Called by UI code when the user has been "shown" the silent
  // onboarding notice.
  void SilentOnboardingNoticeShown();

  // Called by UI code to determine if we should show the onboarding notice to
  // the user.
  bool ShouldShowOnboardingNotice();

  // HaTS
  // TODO(b:308320418) These should ideally live in a separate Tracking
  // Protection HaTS service, and not be tied to the onboarding one.

  // Returns whether or not the profile requires a fresh survey registration.
  bool RequiresSentimentSurveyGroup();

  // Registers the profile in the requested group, and optionally sets its start
  // and end survey time.
  void RegisterSentimentSurveyGroup(SentimentSurveyGroup group);

  // Computes HaTS eligibility for the profile. Will return kNotSet if the
  // profile isn't to be shown a survey.
  SentimentSurveyGroup GetEligibleSurveyGroup();

  // Returns the time delta from Onboarded to Acknowledged.
  std::optional<base::TimeDelta> OnboardedToAcknowledged();

 private:
  friend class tpcd::experiment::EligibilityServiceTest;
  FRIEND_TEST(TrackingProtectionOnboardingNoticeBrowserTest,
              TreatsAsShownIfPreviouslyDismissed);

  // Called when the underlying onboarding pref is changed.
  virtual void OnOnboardingPrefChanged() const;
  // Called when the notice has been acked.
  virtual void OnOnboardingAckedChanged() const;
  // Called when the underlying offboarding pref is changed.
  virtual void OnOffboardingPrefChanged() const;
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
