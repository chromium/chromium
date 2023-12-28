// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include <memory>
#include <optional>
#include <utility>
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/version_info/channel.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingAckAction;

using NoticeType = ::privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using NoticeAction =
    ::privacy_sandbox::TrackingProtectionOnboarding::NoticeAction;
using SentimentSurveyGroup =
    ::privacy_sandbox::TrackingProtectionOnboarding::SentimentSurveyGroup;

class MockTrackingProtectionObserver
    : public TrackingProtectionOnboarding::Observer {
 public:
  MOCK_METHOD(
      void,
      OnTrackingProtectionOnboardingUpdated,
      (TrackingProtectionOnboarding::OnboardingStatus onboarding_status),
      (override));
  MOCK_METHOD(void, OnShouldShowNoticeUpdated, (), (override));
  MOCK_METHOD(
      void,
      OnTrackingProtectionSilentOnboardingUpdated,
      (TrackingProtectionOnboarding::SilentOnboardingStatus onboarding_status),
      (override));
};

class TrackingProtectionOnboardingTest : public testing::Test {
 public:
  TrackingProtectionOnboardingTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            prefs(), version_info::Channel::UNKNOWN);
  }

  TrackingProtectionOnboarding* tracking_protection_onboarding() {
    return tracking_protection_onboarding_service_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionOnboarding>
      tracking_protection_onboarding_service_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingProfileTriggersOnboardingObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionOnboardingUpdated(
                  TrackingProtectionOnboarding::OnboardingStatus::kOnboarded));

  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       EligibleProfileTriggersOnboardingObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionOnboardingUpdated(
                  TrackingProtectionOnboarding::OnboardingStatus::kEligible));

  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       MarkingAsEligibleTriggersShouldShowNoticeObservers) {
  // Setup
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->MaybeMarkEligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       MarkingAsIneligibleTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->MaybeMarkIneligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       NoticeActionTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kSettings);

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       NoticeShownDoesNotTriggerShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(0);

  // Action
  tracking_protection_onboarding()->OnboardingNoticeShown();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       MaybeMarkEligibleDoesNothingIfProfileNotIneligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeMarkEligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionOnboardingTest,
       MaybeMarkEligibleMarksEligibleIfProfileIsIneligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding()->MaybeMarkEligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kEligible);
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionEligibleSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionOnboardingTest,
       MaybeMarkIneligibleDoesNothingIfProfileNotEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeMarkIneligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionOnboardingTest,
       MaybeMarkIneligibleMarksIneligibleIfProfileIsEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding()->MaybeMarkIneligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kIneligible);
  EXPECT_TRUE(prefs()
                  ->FindPreference(prefs::kTrackingProtectionEligibleSince)
                  ->IsDefaultValue());
}

TEST_F(TrackingProtectionOnboardingTest,
       NoticeShownDoesNothingIfProfileNotEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding()->OnboardingNoticeShown();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kIneligible);
}

TEST_F(TrackingProtectionOnboardingTest,
       NoticeShownMarksOnboardedIfProfileIsEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding()->OnboardingNoticeShown();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionOnboardingTest, UpdatesLastNoticeShownCorrectly) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding()->OnboardingNoticeShown();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  // Show the notice again.
  tracking_protection_onboarding()->OnboardingNoticeShown();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);

  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionNoticeLastShown),
            base::Time::Now());
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince),
            base::Time::Now() - delay);
}

TEST_F(TrackingProtectionOnboardingTest,
       PreviouslyAcknowledgedDoesntReacknowledge) {
  // Ack with GotIt
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kGotIt);
  // Action: Re Ack with Learnmore
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kLearnMore);

  // Verification: LearnMore doesn't persit.
  EXPECT_EQ(
      static_cast<TrackingProtectionOnboardingAckAction>(
          prefs()->GetInteger(prefs::kTrackingProtectionOnboardingAckAction)),
      TrackingProtectionOnboardingAckAction::kGotIt);
}

TEST_F(TrackingProtectionOnboardingTest, AckingNoticeSetsAckedSincePref) {
  // Ack the notice.
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kGotIt);

  // Verification
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionOnboardingAckedSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsFalseIfProfileIneligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->ShouldShowOnboardingNotice(),
            false);
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsTrueIfProfileEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->ShouldShowOnboardingNotice(),
            true);
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsTrueIfProfileOnboardedNotAcked) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, false);

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->ShouldShowOnboardingNotice(),
            true);
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsFalseIfProfileOnboardedAcked) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->ShouldShowOnboardingNotice(),
            false);
}

TEST_F(TrackingProtectionOnboardingTest, MaybeResetOnboardingPrefsInStable) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::STABLE);
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionOnboardingTest, MaybeResetOnboardingPrefsInCanary) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Verification
  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kTrackingProtectionOnboardingStatus)
                   ->HasUserSetting());
}

TEST_F(TrackingProtectionOnboardingTest,
       MaybeResetOnboardingPrefsInCanaryTriggersObserver) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionOnboardingUpdated(
                  TrackingProtectionOnboarding::OnboardingStatus::kIneligible));
  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Expectation
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardedToAckForNotOnboardedProfile) {
  tracking_protection_onboarding()->MaybeMarkEligible();
  EXPECT_EQ(tracking_protection_onboarding()->OnboardedToAcknowledged(),
            std::nullopt);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardedToAckForNotAckedProfile) {
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->OnboardingNoticeShown();
  EXPECT_EQ(tracking_protection_onboarding()->OnboardedToAcknowledged(),
            std::nullopt);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardedToAckForAckedProfile) {
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->OnboardingNoticeShown();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kGotIt);

  EXPECT_EQ(tracking_protection_onboarding()->OnboardedToAcknowledged(),
            std::make_optional(delay));
}

TEST_F(TrackingProtectionOnboardingTest, UserActionMetrics) {
  base::UserActionTester user_action_tester;

  tracking_protection_onboarding()->OnboardingNoticeShown();
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("TrackingProtection.Notice.Shown"));

  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.DismissedOther"));

  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kGotIt);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.GotItClicked"));

  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kSettings);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.SettingsClicked"));

  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kLearnMore);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.LearnMoreClicked"));

  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kClosed);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("TrackingProtection.Notice.Closed"));
}

class TrackingProtectionOnboardingAccessorTest
    : public TrackingProtectionOnboardingTest,
      public testing::WithParamInterface<
          std::pair<TrackingProtectionOnboardingStatus,
                    TrackingProtectionOnboarding::OnboardingStatus>> {};

TEST_P(TrackingProtectionOnboardingAccessorTest,
       ReturnsCorrectOnboardingValue) {
  prefs()->SetInteger(prefs::kTrackingProtectionOnboardingStatus,
                      static_cast<int>(std::get<0>(GetParam())));
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingStatus(),
            std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionOnboardingAccessorTest,
    TrackingProtectionOnboardingAccessorTest,
    testing::Values(
        std::pair(TrackingProtectionOnboardingStatus::kIneligible,
                  TrackingProtectionOnboarding::OnboardingStatus::kIneligible),
        std::pair(TrackingProtectionOnboardingStatus::kEligible,
                  TrackingProtectionOnboarding::OnboardingStatus::kEligible),
        std::pair(TrackingProtectionOnboardingStatus::kOnboarded,
                  TrackingProtectionOnboarding::OnboardingStatus::kOnboarded)));

class TrackingProtectionOnboardingAckActionTest
    : public TrackingProtectionOnboardingTest,
      public testing::WithParamInterface<std::pair<
          TrackingProtectionOnboarding::NoticeAction,
          tracking_protection::TrackingProtectionOnboardingAckAction>> {};

TEST_P(TrackingProtectionOnboardingAckActionTest,
       UserNoticeActionTakenAcknowledgedCorrectly) {
  // Action
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      std::get<0>(GetParam()));

  // Verification
  EXPECT_EQ(prefs()->GetBoolean(prefs::kTrackingProtectionOnboardingAcked),
            true);
  EXPECT_EQ(
      static_cast<TrackingProtectionOnboardingAckAction>(
          prefs()->GetInteger(prefs::kTrackingProtectionOnboardingAckAction)),
      std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionOnboardingAckActionTest,
    TrackingProtectionOnboardingAckActionTest,
    testing::Values(
        std::pair(TrackingProtectionOnboarding::NoticeAction::kOther,
                  TrackingProtectionOnboardingAckAction::kOther),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kGotIt,
                  TrackingProtectionOnboardingAckAction::kGotIt),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kSettings,
                  TrackingProtectionOnboardingAckAction::kSettings),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kLearnMore,
                  TrackingProtectionOnboardingAckAction::kLearnMore),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kClosed,
                  TrackingProtectionOnboardingAckAction::kClosed)));

class TrackingProtectionSentimentTracking
    : public TrackingProtectionOnboardingTest {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(TrackingProtectionSentimentTracking, RegistersProfileCorrectly) {
  // Group unset initially.
  EXPECT_TRUE(tracking_protection_onboarding()->RequiresSentimentSurveyGroup());
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kControlImmediate);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Registered group not yet returned
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered group returned once the profile is eligible for the survey:
  // After the survey start time, but before the survey end time.
  task_env_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kControlImmediate);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered",
      TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kControlImmediate,
      1);
}

TEST_F(TrackingProtectionSentimentTracking,
       ComputeSurveyEligibilityAfterEndTime) {
  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kControlDelayed);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Registered group not yet returned
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered",
      TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kControlDelayed,
      1);

  // Registered group returned once the profile is eligible for the survey:
  // After the survey start time, but before the survey end time.
  task_env_.FastForwardBy(base::Days(14));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kControlDelayed);

  // Afte the end date, no longer return the registered group.
  task_env_.FastForwardBy(base::Days(15));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);
}

TEST_F(TrackingProtectionSentimentTracking, RegistersTreatmentBeforeAck) {
  // Setup
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->OnboardingNoticeShown();

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kTreatmentImmediate);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Registered group not yet returned
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered group Still not returned even after the survey start time, and
  // before the survey end time.
  task_env_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);
}

TEST_F(TrackingProtectionSentimentTracking, RegistersTreatmentAfterAck) {
  // Setup
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->OnboardingNoticeShown();
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      NoticeAction::kGotIt);

  // Needs registration
  EXPECT_TRUE(tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kTreatmentDelayed);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered",
      TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kTreatmentDelayed,
      1);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Registered group not yet returned
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered returned after the survey start time, and before the survey end
  // time.
  task_env_.FastForwardBy(base::Days(14));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kTreatmentDelayed);
}

TEST_F(TrackingProtectionSentimentTracking, RegistersTreatmentAndAcksLater) {
  // Setup
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->OnboardingNoticeShown();

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kTreatmentImmediate);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Action: Ack the notice.
  tracking_protection_onboarding()->OnboardingNoticeActionTaken(
      NoticeAction::kGotIt);

  // Registered group still not returned after Acking the notice
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered group returned after the survey start time, and before the
  // survey end time.
  task_env_.FastForwardBy(base::Minutes(3));
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kTreatmentImmediate);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered",
      TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kTreatmentImmediate,
      1);
}

class TrackingProtectionSentimentTrackinWithSilentOnboarding
    : public TrackingProtectionSentimentTracking {
 public:
  void SetUp() override {
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            prefs(), version_info::Channel::UNKNOWN,
            /* is_silent_onboarding_enabled=*/true);
  }
};

TEST_F(TrackingProtectionSentimentTrackinWithSilentOnboarding,
       RegistersControlBeforeOnboarding) {
  // Setup
  tracking_protection_onboarding()->MaybeMarkSilentEligible();

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kControlImmediate);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Registered group not yet returned
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered group Still not returned even after the survey start time, and
  // before the survey end time.
  task_env_.FastForwardBy(base::Minutes(3));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);
}

TEST_F(TrackingProtectionSentimentTrackinWithSilentOnboarding,
       RegistersControlAfterSilentOnboarding) {
  // Setup
  tracking_protection_onboarding()->MaybeMarkSilentEligible();
  tracking_protection_onboarding()->SilentOnboardingNoticeShown();

  // Needs registration
  EXPECT_TRUE(tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kControlDelayed);

  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered",
      TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kControlDelayed,
      1);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Registered group not yet returned
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered returned after the survey start time, and before the survey end
  // time.
  task_env_.FastForwardBy(base::Days(14));

  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kControlDelayed);
}

TEST_F(TrackingProtectionSentimentTrackinWithSilentOnboarding,
       RegistersControlAndOnboardsLater) {
  // Setup
  tracking_protection_onboarding()->MaybeMarkSilentEligible();

  // Action: Register the group.
  tracking_protection_onboarding()->RegisterSentimentSurveyGroup(
      SentimentSurveyGroup::kControlImmediate);

  // Verification: Registration no longer required.
  EXPECT_FALSE(
      tracking_protection_onboarding()->RequiresSentimentSurveyGroup());

  // Action: Silently onboard.
  tracking_protection_onboarding()->SilentOnboardingNoticeShown();

  // Registered group still not returned after Acking the notice
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kNotSet);

  // Registered group returned after the survey start time, and before the
  // survey end time.
  task_env_.FastForwardBy(base::Minutes(3));
  EXPECT_EQ(tracking_protection_onboarding()->GetEligibleSurveyGroup(),
            SentimentSurveyGroup::kControlImmediate);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SentimentSurvey.Registered",
      TrackingProtectionOnboarding::SentimentSurveyGroupMetrics::
          kControlImmediate,
      1);
}

class TrackingProtectionOffboardingTest
    : public TrackingProtectionOnboardingTest {
 public:
  void RestartServiceWithRollbackFlag() {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kTrackingProtectionOnboardingRollback);
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            prefs(), version_info::Channel::UNKNOWN);
  }

  void RestartServiceWithoutRollbackFlag() {
    feature_list_.Reset();
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            prefs(), version_info::Channel::UNKNOWN);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrackingProtectionOffboardingTest, IneligibleProfileDoesntNeedNotice) {
  // Setup
  // We start with an ineligible profile (default)

  // Action
  RestartServiceWithRollbackFlag();

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOffboardingTest, NonOnboardedProfileDoesntNeedNoice) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkEligible();

  // Action
  RestartServiceWithRollbackFlag();

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOffboardingTest, OnboardedProfileNeedsNotice) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);

  // Action
  RestartServiceWithRollbackFlag();

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kOffboarding);
}

TEST_F(TrackingProtectionOffboardingTest, AckedProfileNeedsNotice) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  tracking_protection_onboarding()->NoticeActionTaken(
      NoticeType::kOnboarding,
      TrackingProtectionOnboarding::NoticeAction::kGotIt);

  // Action
  RestartServiceWithRollbackFlag();

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kOffboarding);
}

TEST_F(TrackingProtectionOffboardingTest, NoticeNotRequiredIfShownOnce) {
  // Setup
  // We start with an onboarded profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  RestartServiceWithRollbackFlag();

  // Action
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOffboarding);

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOffboardingTest, OffboardedNotifies) {
  // Setup
  // We start with an onboarded profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  RestartServiceWithRollbackFlag();

  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnTrackingProtectionOnboardingUpdated(
                  TrackingProtectionOnboarding::OnboardingStatus::kOffboarded));
  // Action
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOffboarding);

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOffboardingTest, NoticeShownDoesntNotify) {
  // Setup
  // We start with an onboarded profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  RestartServiceWithRollbackFlag();

  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(0);
  // Offborading notice is required before the action.
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kOffboarding);
  // Action
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOffboarding);

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kNone);
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOffboardingTest, NoticeShownPersists) {
  // Setup
  // We start with an onboarded profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  RestartServiceWithRollbackFlag();

  // Action
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOffboarding);

  // Verification
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kTrackingProtectionOffboarded));
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionOffboardedSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionOffboardingTest, NoticeActionTakenPersists) {
  // Setup
  // We start with an onboarded profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  RestartServiceWithRollbackFlag();

  // Action
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOffboarding);
  tracking_protection_onboarding()->NoticeActionTaken(
      NoticeType::kOffboarding,
      TrackingProtectionOnboarding::NoticeAction::kGotIt);

  // Verification
  EXPECT_EQ(
      static_cast<TrackingProtectionOnboardingAckAction>(
          prefs()->GetInteger(prefs::kTrackingProtectionOffboardingAckAction)),
      TrackingProtectionOnboardingAckAction::kGotIt);
}

TEST_F(TrackingProtectionOffboardingTest,
       GoesBackToPreviousStatusWhenOffboardingDisabled) {
  // Setup
  // We start with an onboarded profile
  tracking_protection_onboarding()->MaybeMarkEligible();
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOnboarding);
  RestartServiceWithRollbackFlag();

  // Action
  // Before showing the offboarding notice, the user is considered onboarded:
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingStatus(),
            TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
  tracking_protection_onboarding()->NoticeShown(NoticeType::kOffboarding);

  // Verification
  // User was offboarded successfully.
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingStatus(),
            TrackingProtectionOnboarding::OnboardingStatus::kOffboarded);
  // Restarting without the flag is equivalent to "disabling" offboarding.
  // User's status should go back to its value before the offboarding.
  RestartServiceWithoutRollbackFlag();
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingStatus(),
            TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
}

class TrackingProtectionOnboardingStartupStateTest
    : public TrackingProtectionOnboardingTest {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateIneligible) {
  // Onboarding startup state starts as ineligible
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      TrackingProtectionOnboarding::OnboardingStartupState::kIneligible, 1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateEligible) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      TrackingProtectionOnboarding::OnboardingStartupState::
          kEligibleWaitingToOnboard,
      1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateOnboardingWaitingToAck) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      TrackingProtectionOnboarding::OnboardingStartupState::
          kOnboardedWaitingToAck,
      1);
}

class TrackingProtectionOnboardingStartupStateAckedTest
    : public TrackingProtectionOnboardingTest,
      public testing::WithParamInterface<
          std::pair<TrackingProtectionOnboarding::NoticeAction,
                    TrackingProtectionOnboarding::OnboardingStartupState>> {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_P(TrackingProtectionOnboardingStartupStateAckedTest,
       OnboardingStartupStateAckedAction) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  tracking_protection_onboarding_service_->OnboardingNoticeActionTaken(
      std::get<0>(GetParam()));
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      std::get<1>(GetParam()), 1);
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionOnboardingStartupStateAckedTest,
    TrackingProtectionOnboardingStartupStateAckedTest,
    testing::Values(
        std::pair(
            TrackingProtectionOnboarding::NoticeAction::kGotIt,
            TrackingProtectionOnboarding::OnboardingStartupState::kAckedGotIt),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kSettings,
                  TrackingProtectionOnboarding::OnboardingStartupState::
                      kAckedSettings),
        std::pair(
            TrackingProtectionOnboarding::NoticeAction::kClosed,
            TrackingProtectionOnboarding::OnboardingStartupState::kAckedClosed),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kLearnMore,
                  TrackingProtectionOnboarding::OnboardingStartupState::
                      kAckedLearnMore),
        std::pair(TrackingProtectionOnboarding::NoticeAction::kOther,
                  TrackingProtectionOnboarding::OnboardingStartupState::
                      kAckedOther)));

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateEligibleWaitingToOnboardSince) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  // Verification
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup."
      "WaitingToOnboardSince",
      delay, 1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateOnboardedWaitingToAckTimings) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  // Verification
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.WaitingToAckSince",
      delay, 1);
  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateEligibleToOnboardingDuration) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  tracking_protection_onboarding_service_->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingEligibleToOnboardingDuration) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardingOnboardedToAckedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  tracking_protection_onboarding_service_->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  auto onboarding_to_acked_duration =
      base::Time::Now() -
      prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.OnboardedToAckedDuration",
      onboarding_to_acked_duration, 1);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardingLastShownToAckedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkEligible();
  tracking_protection_onboarding_service_->OnboardingNoticeShown();
  tracking_protection_onboarding_service_->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  auto last_shown_to_acked_duration =
      prefs()->GetTime(prefs::kTrackingProtectionNoticeLastShown) -
      base::Time::Now();
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.LastShownToAckedDuration",
      last_shown_to_acked_duration, 1);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardingMaybeMarkEligibleHistogram) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkEligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkEligible", false,
      1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkEligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkEligible", true,
      1);
}

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingMaybeMarkIneligibleHistogram) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkIneligible", false,
      1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkIneligible", true,
      1);
}

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingDidNoticeShownOnboardHistogram) {
  // Action
  tracking_protection_onboarding_service_->OnboardingNoticeShown();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
      false, 1);

  // Setup
  tracking_protection_onboarding_service_->MaybeMarkEligible();

  // Action
  tracking_protection_onboarding_service_->OnboardingNoticeShown();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
      true, 1);
}

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingDidNoticeActionAckowledgeHistogram) {
  // Setup
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

  // Action
  tracking_protection_onboarding_service_->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeActionAckowledge",
      false, 1);

  // Setup
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, false);

  // Action
  tracking_protection_onboarding_service_->OnboardingNoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeActionAckowledge",
      true, 1);
}

class TrackingProtectionSilentOnboardingTest
    : public TrackingProtectionOnboardingTest {};

TEST_F(TrackingProtectionSilentOnboardingTest,
       OnboardingProfileTriggersOnboardingObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded));

  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       EligibleProfileTriggersOnboardingObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kEligible));

  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MarkingAsEligibleTriggersShouldShowNoticeObservers) {
  // Setup
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->MaybeMarkSilentEligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MarkingAsIneligibleTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkSilentEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->MaybeMarkSilentIneligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       NoticeShownTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkSilentEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->SilentOnboardingNoticeShown();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeMarkEligibleDoesNothingIfProfileNotIneligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeMarkSilentEligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeMarkEligibleMarksEligibleIfProfileIsIneligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding()->MaybeMarkSilentEligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kEligible);
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSilentEligibleSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeMarkIneligibleDoesNothingIfProfileNotEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeMarkSilentIneligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeMarkSilentIneligibleMarksIneligibleIfProfileIsEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding()->MaybeMarkSilentIneligible();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kIneligible);
  EXPECT_TRUE(
      prefs()
          ->FindPreference(prefs::kTrackingProtectionSilentEligibleSince)
          ->IsDefaultValue());
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       NoticeShownDoesNothingIfProfileNotEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding()->SilentOnboardingNoticeShown();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kIneligible);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       NoticeShownMarksOnboardedIfProfileIsEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding()->SilentOnboardingNoticeShown();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSilentOnboardedSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       ShouldNotShowNoticeIfProfileIneligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       ShouldShowNoticeIfProfileEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kSilentOnboarding);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       ShouldNotShowNoticeIfProfileOnboarded) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       OnboardingEligibleToOnboardedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();
  tracking_protection_onboarding_service_->SilentOnboardingNoticeShown();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionSilentOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

TEST_F(TrackingProtectionSilentOnboardingTest, MaybeMarkEligibleHistogram) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkEligible",
      false, 1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkEligible",
      true, 1);
}

TEST_F(TrackingProtectionSilentOnboardingTest, MaybeMarkIneligibleHistogram) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkSilentIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkIneligible",
      false, 1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkSilentIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkIneligible",
      true, 1);
}

TEST_F(TrackingProtectionSilentOnboardingTest, DidNoticeShownOnboardHistogram) {
  // Action
  tracking_protection_onboarding_service_->SilentOnboardingNoticeShown();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "DidNoticeShownOnboard",
      false, 1);

  // Setup
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();

  // Action
  tracking_protection_onboarding_service_->SilentOnboardingNoticeShown();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "DidNoticeShownOnboard",
      true, 1);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeResetOnboardingPrefsInStable) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::STABLE);
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeResetOnboardingPrefsInCanary) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Verification
  EXPECT_FALSE(
      prefs()
          ->FindPreference(prefs::kTrackingProtectionSilentOnboardingStatus)
          ->HasUserSetting());
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeResetOnboardingPrefsInCanaryTriggersObserver) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kIneligible));
  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Expectation
  testing::Mock::VerifyAndClearExpectations(&observer);
}

class TrackingProtectionSilentOnboardingAccessorTest
    : public TrackingProtectionSilentOnboardingTest,
      public testing::WithParamInterface<
          std::pair<TrackingProtectionOnboardingStatus,
                    TrackingProtectionOnboarding::SilentOnboardingStatus>> {};

TEST_P(TrackingProtectionSilentOnboardingAccessorTest,
       ReturnsCorrectOnboardingValue) {
  prefs()->SetInteger(prefs::kTrackingProtectionSilentOnboardingStatus,
                      static_cast<int>(std::get<0>(GetParam())));
  EXPECT_EQ(tracking_protection_onboarding()->GetSilentOnboardingStatus(),
            std::get<1>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionSilentOnboardingAccessorTest,
    TrackingProtectionSilentOnboardingAccessorTest,
    testing::Values(
        std::pair(
            TrackingProtectionOnboardingStatus::kIneligible,
            TrackingProtectionOnboarding::SilentOnboardingStatus::kIneligible),
        std::pair(
            TrackingProtectionOnboardingStatus::kEligible,
            TrackingProtectionOnboarding::SilentOnboardingStatus::kEligible),
        std::pair(
            TrackingProtectionOnboardingStatus::kOnboarded,
            TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded)));

class TrackingProtectionSilentOnboardingStartupStateTest
    : public TrackingProtectionSilentOnboardingTest {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateIneligible) {
  // Silent onboarding startup state starts as ineligible
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup.State",
      TrackingProtectionOnboarding::OnboardingStartupState::kIneligible, 1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateEligible) {
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup.State",
      TrackingProtectionOnboarding::SilentOnboardingStartupState::
          kEligibleWaitingToOnboard,
      1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateOnboarded) {
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();
  tracking_protection_onboarding_service_->SilentOnboardingNoticeShown();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup.State",
      TrackingProtectionOnboarding::SilentOnboardingStartupState::kOnboarded,
      1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateEligibleWaitingToOnboardSince) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);

  // Verification
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup."
      "WaitingToOnboardSince",
      delay, 1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateEligibleToOnboardedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkSilentEligible();
  tracking_protection_onboarding_service_->SilentOnboardingNoticeShown();
  tracking_protection_onboarding_service_.reset();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::UNKNOWN);
  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionSilentOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

}  // namespace
}  // namespace privacy_sandbox
