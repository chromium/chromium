// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include <memory>
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

class MockTrackingProtectionObserver
    : public TrackingProtectionOnboarding::Observer {
 public:
  MOCK_METHOD(
      void,
      OnTrackingProtectionOnboardingUpdated,
      (TrackingProtectionOnboarding::OnboardingStatus onboarding_status),
      (override));
  MOCK_METHOD(void, OnShouldShowNoticeUpdated, (), (override));
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
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
  EXPECT_TRUE(prefs()->GetBoolean(prefs::kTrackingProtectionOnboardingAcked));
}

TEST_F(TrackingProtectionOnboardingTest, MaybeResetOnboardingPrefsInCanary) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

  // Action
  tracking_protection_onboarding()->MaybeResetOnboardingPrefs();

  // Verification
  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kTrackingProtectionOnboardingStatus)
                   ->HasUserSetting());

  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kTrackingProtectionOnboardingAcked)
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

class TrackingProtectionOnboardingWithFeatureOverrideTest
    : public TrackingProtectionOnboardingTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kTrackingProtectionOnboardingForceEligibility);
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(
            prefs(), version_info::Channel::UNKNOWN);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrackingProtectionOnboardingWithFeatureOverrideTest,
       StartsUpAsEligible) {
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingStatus(),
            TrackingProtectionOnboarding::OnboardingStatus::kEligible);
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
}  // namespace
}  // namespace privacy_sandbox
