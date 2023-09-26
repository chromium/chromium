// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include <memory>
#include <utility>
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

class MockTrackingProtectionObserver
    : public TrackingProtectionOnboarding::Observer {
 public:
  MOCK_METHOD(void, OnTrackingProtectionOnboarded, (), (override));
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
        std::make_unique<TrackingProtectionOnboarding>(prefs());
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
};

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingProfileTriggersOnboardingObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTrackingProtectionOnboarded());

  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       EligibleProfileDoesntTriggersOnboardingObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTrackingProtectionOnboarded()).Times(0);

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
  tracking_protection_onboarding()->NoticeActionTaken(
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
  tracking_protection_onboarding()->NoticeShown();

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
  tracking_protection_onboarding()->NoticeShown();

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
  tracking_protection_onboarding()->NoticeShown();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince),
            base::Time::Now());
}

TEST_F(TrackingProtectionOnboardingTest,
       UserNoticeActionTakenAcknowledgedCorrectly) {
  // Action
  tracking_protection_onboarding()->NoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kLearnMore);

  // Verification
  EXPECT_EQ(prefs()->GetBoolean(prefs::kTrackingProtectionOnboardingAcked),
            true);
}

TEST_F(TrackingProtectionOnboardingTest,
       NoUserNoticeActionTakenDoesntAcknowledge) {
  // Action
  tracking_protection_onboarding()->NoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);

  // Verification
  EXPECT_EQ(prefs()->GetBoolean(prefs::kTrackingProtectionOnboardingAcked),
            false);
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

TEST_F(TrackingProtectionOnboardingTest, UserActionMetrics) {
  base::UserActionTester user_action_tester;

  tracking_protection_onboarding()->NoticeShown();
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("TrackingProtection.Notice.Shown"));

  tracking_protection_onboarding()->NoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kOther);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.DismissedOther"));

  tracking_protection_onboarding()->NoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kGotIt);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.GotItClicked"));

  tracking_protection_onboarding()->NoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kSettings);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.SettingsClicked"));

  tracking_protection_onboarding()->NoticeActionTaken(
      TrackingProtectionOnboarding::NoticeAction::kLearnMore);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.LearnMoreClicked"));

  tracking_protection_onboarding()->NoticeActionTaken(
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

class TrackingProtectionOnboardingWithFeatureOverrideTest
    : public TrackingProtectionOnboardingTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        privacy_sandbox::kTrackingProtectionOnboardingForceEligibility);
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(prefs());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrackingProtectionOnboardingWithFeatureOverrideTest,
       StartsUpAsEligible) {
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingStatus(),
            TrackingProtectionOnboarding::OnboardingStatus::kEligible);
}

}  // namespace
}  // namespace privacy_sandbox
