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
#include "components/privacy_sandbox/mock_tracking_protection_onboarding_delegate.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/version_info/channel.h"
#include "privacy_sandbox_notice_constants.h"
#include "privacy_sandbox_notice_storage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tracking_protection_onboarding.h"

namespace privacy_sandbox {

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingStatus;

using ::privacy_sandbox::tracking_protection::
    TrackingProtectionOnboardingAckAction;

using NoticeType = ::privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using NoticeAction =
    ::privacy_sandbox::TrackingProtectionOnboarding::NoticeAction;
using SurfaceType =
    ::privacy_sandbox::TrackingProtectionOnboarding::SurfaceType;

using ::testing::Combine;
using ::testing::Values;

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
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
  }

  void RecreateOnboardingService(
      version_info::Channel channel = version_info::Channel::UNKNOWN) {
    delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(std::move(delegate_),
                                                       prefs(), channel);
  }

  MockTrackingProtectionOnboardingDelegate* GetMockDelegate() {
    return (MockTrackingProtectionOnboardingDelegate*)
        tracking_protection_onboarding()
            ->delegate_.get();
  }

  void SetUp() override { RecreateOnboardingService(); }

  TrackingProtectionOnboarding* tracking_protection_onboarding() {
    return tracking_protection_onboarding_service_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionOnboarding>
      tracking_protection_onboarding_service_;
  std::unique_ptr<MockTrackingProtectionOnboardingDelegate> delegate_;
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrackingProtectionOnboardingTest,
       IsEnterpriseManagedReturnsValueProvidedByDelegate) {
  GetMockDelegate()->SetUpIsEnterpriseManaged(/*managed=*/false);
  EXPECT_FALSE(tracking_protection_onboarding()->IsEnterpriseManaged());

  GetMockDelegate()->SetUpIsEnterpriseManaged(/*managed=*/true);
  EXPECT_TRUE(tracking_protection_onboarding()->IsEnterpriseManaged());
}

TEST_F(TrackingProtectionOnboardingTest,
       IsNewProfileReturnsValueProvidedByDelegate) {
  GetMockDelegate()->SetUpIsNewProfile(/*new_profile=*/true);
  EXPECT_TRUE(tracking_protection_onboarding()->IsNewProfile());

  GetMockDelegate()->SetUpIsNewProfile(/*new_profile=*/false);
  EXPECT_FALSE(tracking_protection_onboarding()->IsNewProfile());
}

TEST_F(TrackingProtectionOnboardingTest,
       AreThirdPartyCookiesBlockedReturnsValueProvidedByDelegate) {
  GetMockDelegate()->SetUpAreThirdPartyCookiesBlocked(/*blocked=*/false);
  EXPECT_FALSE(tracking_protection_onboarding()->AreThirdPartyCookiesBlocked());

  GetMockDelegate()->SetUpAreThirdPartyCookiesBlocked(/*blocked=*/true);
  EXPECT_TRUE(tracking_protection_onboarding()->AreThirdPartyCookiesBlocked());
}

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
  tracking_protection_onboarding()->MaybeMarkModeBEligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       MarkingAsIneligibleTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->MaybeMarkModeBIneligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       NoticeActionTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kSettings);

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest,
       NoticeShownDoesNotTriggerShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(0);

  // Action
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);

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
  tracking_protection_onboarding()->MaybeMarkModeBEligible();

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
  tracking_protection_onboarding()->MaybeMarkModeBEligible();

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
  tracking_protection_onboarding()->MaybeMarkModeBIneligible();

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
  tracking_protection_onboarding()->MaybeMarkModeBIneligible();

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
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);

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
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);

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
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  // Show the notice again.
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);

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
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kGotIt);
  // Action: Re Ack with Learnmore
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kLearnMore);

  // Verification: LearnMore doesn't persit.
  EXPECT_EQ(
      static_cast<TrackingProtectionOnboardingAckAction>(
          prefs()->GetInteger(prefs::kTrackingProtectionOnboardingAckAction)),
      TrackingProtectionOnboardingAckAction::kGotIt);
}

TEST_F(TrackingProtectionOnboardingTest, AckingNoticeSetsAckedSincePref) {
  // Ack the notice.
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kGotIt);

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
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsTrueIfProfileEligible) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kModeBOnboarding);
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsTrueIfProfileOnboardedNotAcked) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, false);

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kModeBOnboarding);
}

TEST_F(TrackingProtectionOnboardingTest,
       ShouldShowNoticeReturnsIsFalseIfProfileOnboardedAcked) {
  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, true);

  // Verification
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOnboardingTest, GetRequiredNotice_Full3PCDDisabled) {
  feature_list_.InitAndDisableFeature(
      privacy_sandbox::kTrackingProtectionOnboarding);

  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOnboardingTest, GetRequiredNotice_Full3PCDEnabled) {
  feature_list_.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kTrackingProtectionOnboarding,
      {{privacy_sandbox::kTrackingProtectionBlock3PC.name, "true"}});

  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kFull3PCDOnboarding);
}

TEST_F(TrackingProtectionOnboardingTest,
       GetRequiredNotice_Full3PCDSilentOnboarding) {
  feature_list_.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kTrackingProtectionOnboarding,
      {{privacy_sandbox::kTrackingProtectionBlock3PC.name, "false"}});

  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kFull3PCDSilentOnboarding);
}

TEST_F(TrackingProtectionOnboardingTest,
       GetRequiredNotice_Full3PCDEnabledWithIPP) {
  feature_list_.InitWithFeaturesAndParameters(
      {{privacy_sandbox::kTrackingProtectionOnboarding,
        {{privacy_sandbox::kTrackingProtectionBlock3PC.name, "true"}}},
       {privacy_sandbox::kIpProtectionUx, {}}},
      {});

  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kFull3PCDOnboardingWithIPP);
}

TEST_F(TrackingProtectionOnboardingTest, GetRequiredNotice_ModeBAlreadyAcked) {
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kGotIt);

  feature_list_.InitAndEnableFeatureWithParameters(
      privacy_sandbox::kTrackingProtectionOnboarding,
      {{privacy_sandbox::kTrackingProtectionBlock3PC.name, "true"}});

  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionOnboardingTest, MaybeResetOnboardingPrefsInStable) {
  // Setup
  delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          std::move(delegate_), prefs(), version_info::Channel::STABLE);
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionOnboardingTest, MaybeResetOnboardingPrefsInCanary) {
  // Setup
  delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          std::move(delegate_), prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Verification
  EXPECT_FALSE(prefs()
                   ->FindPreference(prefs::kTrackingProtectionOnboardingStatus)
                   ->HasUserSetting());
}

TEST_F(TrackingProtectionOnboardingTest,
       MaybeResetOnboardingPrefsInCanaryTriggersObserver) {
  // Setup
  delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          std::move(delegate_), prefs(), version_info::Channel::CANARY);
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
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Expectation
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardedToAckForNotOnboardedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  EXPECT_EQ(tracking_protection_onboarding()->OnboardedToAcknowledged(),
            std::nullopt);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardedToAckForNotAckedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);
  EXPECT_EQ(tracking_protection_onboarding()->OnboardedToAcknowledged(),
            std::nullopt);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardedToAckForAckedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kGotIt);

  EXPECT_EQ(tracking_protection_onboarding()->OnboardedToAcknowledged(),
            std::make_optional(delay));
}

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingTimestampIsNullForNotOnboardedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingTimestamp(),
            std::nullopt);
}

TEST_F(TrackingProtectionOnboardingTest,
       ReturnsOnboardingTimestampForOnboardedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBEligible();
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);

  EXPECT_EQ(tracking_protection_onboarding()->GetOnboardingTimestamp(),
            std::make_optional(base::Time::Now()));
}

TEST_F(TrackingProtectionOnboardingTest, UserActionMetrics) {
  base::UserActionTester user_action_tester;

  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                NoticeType::kModeBOnboarding);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("TrackingProtection.Notice.Shown"));

  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kOther);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.DismissedOther"));

  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kGotIt);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.GotItClicked"));

  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kSettings);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.SettingsClicked"));

  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kLearnMore);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "TrackingProtection.Notice.LearnMoreClicked"));

  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kClosed);
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
          NoticeAction,
          tracking_protection::TrackingProtectionOnboardingAckAction>> {};

TEST_P(TrackingProtectionOnboardingAckActionTest,
       UserNoticeActionTakenAcknowledgedCorrectly) {
  // Action
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
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
        std::pair(NoticeAction::kOther,
                  TrackingProtectionOnboardingAckAction::kOther),
        std::pair(NoticeAction::kGotIt,
                  TrackingProtectionOnboardingAckAction::kGotIt),
        std::pair(NoticeAction::kSettings,
                  TrackingProtectionOnboardingAckAction::kSettings),
        std::pair(NoticeAction::kLearnMore,
                  TrackingProtectionOnboardingAckAction::kLearnMore),
        std::pair(NoticeAction::kClosed,
                  TrackingProtectionOnboardingAckAction::kClosed)));

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
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      TrackingProtectionOnboarding::OnboardingStartupState::
          kEligibleWaitingToOnboard,
      1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateOnboardingWaitingToAck) {
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      TrackingProtectionOnboarding::OnboardingStartupState::
          kOnboardedWaitingToAck,
      1);
}

class TrackingProtectionOnboardingStartupStateAckedTest
    : public TrackingProtectionOnboardingTest,
      public testing::WithParamInterface<
          std::pair<NoticeAction,
                    TrackingProtectionOnboarding::OnboardingStartupState>> {
 protected:
  base::HistogramTester histogram_tester_;
};

TEST_P(TrackingProtectionOnboardingStartupStateAckedTest,
       OnboardingStartupStateAckedAction) {
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      std::get<0>(GetParam()));
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup.State",
      std::get<1>(GetParam()), 1);
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionOnboardingStartupStateAckedTest,
    TrackingProtectionOnboardingStartupStateAckedTest,
    testing::Values(
        std::pair(
            NoticeAction::kGotIt,
            TrackingProtectionOnboarding::OnboardingStartupState::kAckedGotIt),
        std::pair(NoticeAction::kSettings,
                  TrackingProtectionOnboarding::OnboardingStartupState::
                      kAckedSettings),
        std::pair(
            NoticeAction::kClosed,
            TrackingProtectionOnboarding::OnboardingStartupState::kAckedClosed),
        std::pair(NoticeAction::kLearnMore,
                  TrackingProtectionOnboarding::OnboardingStartupState::
                      kAckedLearnMore),
        std::pair(NoticeAction::kOther,
                  TrackingProtectionOnboarding::OnboardingStartupState::
                      kAckedOther)));

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateEligibleWaitingToOnboardSince) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

  // Verification
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup."
      "WaitingToOnboardSince",
      delay, 1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupStateOnboardedWaitingToAckTimings) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

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
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kOther);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
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
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardingOnboardedToAckedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kOther);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

  auto onboarding_to_acked_duration =
      base::Time::Now() -
      prefs()->GetTime(prefs::kTrackingProtectionOnboardedSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.OnboardedToAckedDuration",
      onboarding_to_acked_duration, 1);
}

TEST_F(TrackingProtectionOnboardingTest, OnboardingLastShownToAckedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kOther);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

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
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkEligible", false,
      1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();

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
  tracking_protection_onboarding_service_->MaybeMarkModeBIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkIneligible", false,
      1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkModeBIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.MaybeMarkIneligible", true,
      1);
}

TEST_F(TrackingProtectionOnboardingTest,
       OnboardingDidNoticeShownOnboardHistogram) {
  // Action
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeShownOnboard",
      false, 1);

  // Setup
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();

  // Action
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);

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
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kOther);

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.Onboarding.DidNoticeActionAckowledge",
      false, 1);

  // Setup
  prefs()->SetBoolean(prefs::kTrackingProtectionOnboardingAcked, false);

  // Action
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kOther);

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
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MarkingAsIneligibleTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->MaybeMarkModeBSilentIneligible();

  // Verification
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       NoticeShownTriggersShouldShowNoticeObservers) {
  // Setup
  // We start with an eligible profile
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnShouldShowNoticeUpdated()).Times(1);

  // Action
  tracking_protection_onboarding()->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);

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
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();

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
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();

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
  tracking_protection_onboarding()->MaybeMarkModeBSilentIneligible();

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
  tracking_protection_onboarding()->MaybeMarkModeBSilentIneligible();

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
  tracking_protection_onboarding()->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);

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
  tracking_protection_onboarding()->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);

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
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
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
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kModeBSilentOnboarding);
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
  EXPECT_EQ(tracking_protection_onboarding()->GetRequiredNotice(
                SurfaceType::kDesktop),
            NoticeType::kNone);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       OnboardingEligibleToOnboardedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

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
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkEligible",
      false, 1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kIneligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();

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
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkIneligible",
      false, 1);

  // Setup
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kEligible));

  // Action
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentIneligible();

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding.MaybeMarkIneligible",
      true, 1);
}

TEST_F(TrackingProtectionSilentOnboardingTest, DidNoticeShownOnboardHistogram) {
  // Action
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "DidNoticeShownOnboard",
      false, 1);

  // Setup
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();

  // Action
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);

  // Verification
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboarding."
      "DidNoticeShownOnboard",
      true, 1);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeResetOnboardingPrefsInStable) {
  // Setup
  delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();

  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          std::move(delegate_), prefs(), version_info::Channel::STABLE);
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Verification
  EXPECT_EQ(static_cast<TrackingProtectionOnboardingStatus>(prefs()->GetInteger(
                prefs::kTrackingProtectionSilentOnboardingStatus)),
            TrackingProtectionOnboardingStatus::kOnboarded);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeResetOnboardingPrefsInCanary) {
  // Setup
  delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();

  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          std::move(delegate_), prefs(), version_info::Channel::CANARY);
  prefs()->SetInteger(
      prefs::kTrackingProtectionSilentOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));

  // Action
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Verification
  EXPECT_FALSE(
      prefs()
          ->FindPreference(prefs::kTrackingProtectionSilentOnboardingStatus)
          ->HasUserSetting());
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       MaybeResetOnboardingPrefsInCanaryTriggersObserver) {
  // Setup
  delegate_ = std::make_unique<MockTrackingProtectionOnboardingDelegate>();

  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          std::move(delegate_), prefs(), version_info::Channel::CANARY);
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
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Expectation
  testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       SilentOnboardingTimestampIsNullForNotOnboardedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();
  EXPECT_EQ(tracking_protection_onboarding()->GetSilentOnboardingTimestamp(),
            std::nullopt);
}

TEST_F(TrackingProtectionSilentOnboardingTest,
       ReturnsSilentOnboardingTimestampForSilentlyOnboardedProfile) {
  tracking_protection_onboarding()->MaybeMarkModeBSilentEligible();
  tracking_protection_onboarding()->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);

  EXPECT_EQ(tracking_protection_onboarding()->GetSilentOnboardingTimestamp(),
            std::make_optional(base::Time::Now()));
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
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup.State",
      TrackingProtectionOnboarding::SilentOnboardingStartupState::
          kEligibleWaitingToOnboard,
      1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateOnboarded) {
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
  histogram_tester_.ExpectBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup.State",
      TrackingProtectionOnboarding::SilentOnboardingStartupState::kOnboarded,
      1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateEligibleWaitingToOnboardSince) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

  // Verification
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup."
      "WaitingToOnboardSince",
      delay, 1);
}

TEST_F(TrackingProtectionSilentOnboardingStartupStateTest,
       StartupStateEligibleToOnboardedDuration) {
  tracking_protection_onboarding_service_->MaybeMarkModeBSilentEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBSilentOnboarding);
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();
  auto eligible_to_onboarded_duration =
      prefs()->GetTime(prefs::kTrackingProtectionSilentOnboardedSince) -
      prefs()->GetTime(prefs::kTrackingProtectionSilentEligibleSince);
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.SilentOnboardingStartup."
      "EligibleToOnboardedDuration",
      eligible_to_onboarded_duration, 1);
}

TEST_F(TrackingProtectionOnboardingStartupStateTest,
       OnboardingStartupAckedSinceHistogram) {
  // Setup
  tracking_protection_onboarding_service_->MaybeMarkModeBEligible();
  tracking_protection_onboarding_service_->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding);
  tracking_protection_onboarding_service_->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kModeBOnboarding,
      NoticeAction::kGotIt);
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);

  // Action
  tracking_protection_onboarding_service_.reset();
  RecreateOnboardingService();

  // Verification
  histogram_tester_.ExpectTimeBucketCount(
      "PrivacySandbox.TrackingProtection.OnboardingStartup."
      "AckedSince",
      delay, 1);
}

class TrackingProtectionFull3PCDTest
    : public TrackingProtectionOnboardingTest,
      public testing::WithParamInterface<
          std::tuple<TrackingProtectionOnboarding::SurfaceType,
                     TrackingProtectionOnboarding::NoticeType,
                     const char*>> {
 protected:
  PrivacySandboxNoticeStorage notice_storage_;
};

TEST_P(TrackingProtectionFull3PCDTest, NoticeShownMarksPrefShown) {
  // Action
  tracking_protection_onboarding()->NoticeShown(std::get<0>(GetParam()),
                                                std::get<1>(GetParam()));

  // Verification
  const auto notice_data =
      notice_storage_.ReadNoticeData(prefs(), std::get<2>(GetParam()));
  EXPECT_EQ(notice_data->notice_first_shown, base::Time::Now());
  EXPECT_EQ(notice_data->notice_last_shown, base::Time::Now());
}

TEST_P(TrackingProtectionFull3PCDTest, UpdatesLastNoticeShownCorrectly) {
  // Action
  auto first_shown_time = base::Time::Now();
  tracking_protection_onboarding()->NoticeShown(std::get<0>(GetParam()),
                                                std::get<1>(GetParam()));
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  // Show the notice again.
  tracking_protection_onboarding()->NoticeShown(std::get<0>(GetParam()),
                                                std::get<1>(GetParam()));

  // Verification
  const auto notice_data =
      notice_storage_.ReadNoticeData(prefs(), std::get<2>(GetParam()));
  EXPECT_EQ(notice_data->notice_first_shown, first_shown_time);
  EXPECT_EQ(notice_data->notice_last_shown, base::Time::Now());
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionFull3PCDTest,
    TrackingProtectionFull3PCDTest,
    testing::Values(  // Full 3PCD.
        std::make_tuple(SurfaceType::kDesktop,
                        NoticeType::kFull3PCDOnboarding,
                        kFull3PCDIPH),
        std::make_tuple(SurfaceType::kBrApp,
                        NoticeType::kFull3PCDOnboarding,
                        kFull3PCDClankBrApp),
        std::make_tuple(SurfaceType::kAGACCT,
                        NoticeType::kFull3PCDOnboarding,
                        kFull3PCDClankCCT),
        // Full 3PCD with IPP.
        std::make_tuple(SurfaceType::kDesktop,
                        NoticeType::kFull3PCDOnboardingWithIPP,
                        kFull3PCDWithIPPIPH),
        std::make_tuple(SurfaceType::kBrApp,
                        NoticeType::kFull3PCDOnboardingWithIPP,
                        kFull3PCDWithIPPClankBrApp),
        std::make_tuple(SurfaceType::kAGACCT,
                        NoticeType::kFull3PCDOnboardingWithIPP,
                        kFull3PCDWithIPPClankCCT),
        // Full 3PCD silent.
        std::make_tuple(SurfaceType::kDesktop,
                        NoticeType::kFull3PCDSilentOnboarding,
                        kFull3PCDSilentIPH),
        std::make_tuple(SurfaceType::kBrApp,
                        NoticeType::kFull3PCDSilentOnboarding,
                        kFull3PCDSilentClankBrApp),
        std::make_tuple(SurfaceType::kAGACCT,
                        NoticeType::kFull3PCDSilentOnboarding,
                        kFull3PCDSilentClankCCT)));

class TrackingProtectionFull3PCDActionTest
    : public TrackingProtectionOnboardingTest {
 protected:
  std::string ToIPHNoticeName(NoticeType notice_type) {
    switch (notice_type) {
      case NoticeType::kFull3PCDOnboarding:
        return kFull3PCDIPH;
      case NoticeType::kFull3PCDOnboardingWithIPP:
        return kFull3PCDWithIPPIPH;
      case NoticeType::kFull3PCDSilentOnboarding:
        return kFull3PCDSilentIPH;
      default:
        // Other cases aren't part of 3PCD.
        NOTREACHED_NORETURN();
    }
  }
  PrivacySandboxNoticeStorage notice_storage_;
};

class TrackingProtectionVisibleFull3PCDActionTest
    : public TrackingProtectionFull3PCDActionTest,
      public testing::WithParamInterface<
          std::tuple<NoticeType,
                     std::pair<TrackingProtectionOnboarding::NoticeAction,
                               NoticeActionTaken>>> {};

TEST_P(TrackingProtectionVisibleFull3PCDActionTest,
       UserNoticeActionTakenAcknowledgedCorrectly) {
  NoticeType notice_type = std::get<0>(GetParam());
  TrackingProtectionOnboarding::NoticeAction notice_action =
      std::get<1>(GetParam()).first;
  // Action
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                notice_type);
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, notice_type, notice_action);

  // Verification
  const auto notice_data =
      notice_storage_.ReadNoticeData(prefs(), ToIPHNoticeName(notice_type));
  EXPECT_EQ(notice_data->notice_action_taken, std::get<1>(GetParam()).second);
  EXPECT_EQ(notice_data->notice_action_taken_time, base::Time::Now());
  EXPECT_EQ(
      notice_data->notice_shown_duration,
      notice_data->notice_action_taken_time - notice_data->notice_first_shown);
}

TEST_P(TrackingProtectionVisibleFull3PCDActionTest,
       PreviouslyAcknowledgedDoesntReacknowledge) {
  NoticeType notice_type = std::get<0>(GetParam());
  TrackingProtectionOnboarding::NoticeAction notice_action =
      std::get<1>(GetParam()).first;
  // Set up notice shown first.
  tracking_protection_onboarding()->NoticeShown(SurfaceType::kDesktop,
                                                notice_type);
  // Initial action.
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, notice_type, notice_action);
  // Action: Re-Ack with 'LearnMore'
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, notice_type,
      TrackingProtectionOnboarding::NoticeAction::kLearnMore);

  // Verification: LearnMore doesn't persist.
  const auto notice_data =
      notice_storage_.ReadNoticeData(prefs(), ToIPHNoticeName(notice_type));
  EXPECT_EQ(notice_data->notice_action_taken, std::get<1>(GetParam()).second);
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionVisibleFull3PCDActionTest,
    TrackingProtectionVisibleFull3PCDActionTest,
    Combine(
        Values(NoticeType::kFull3PCDOnboarding,
               NoticeType::kFull3PCDOnboardingWithIPP),
        Values(
            std::make_pair(TrackingProtectionOnboarding::NoticeAction::kGotIt,
                           NoticeActionTaken::kAck),
            std::make_pair(
                TrackingProtectionOnboarding::NoticeAction::kSettings,
                NoticeActionTaken::kSettings),
            std::make_pair(TrackingProtectionOnboarding::NoticeAction::kOther,
                           NoticeActionTaken::kOther),
            std::make_pair(
                TrackingProtectionOnboarding::NoticeAction::kLearnMore,
                NoticeActionTaken::kLearnMore),
            std::make_pair(TrackingProtectionOnboarding::NoticeAction::kClosed,
                           NoticeActionTaken::kClosed))));

class TrackingProtectionSilentFull3PCDActionTest
    : public TrackingProtectionFull3PCDActionTest {};

TEST_F(TrackingProtectionSilentFull3PCDActionTest,
       NoticeDoesntTrackNoticeAction) {
  // Action
  tracking_protection_onboarding()->NoticeShown(
      SurfaceType::kDesktop, NoticeType::kFull3PCDSilentOnboarding);
  auto delay = base::Seconds(15);
  task_env_.FastForwardBy(delay);
  tracking_protection_onboarding()->NoticeActionTaken(
      SurfaceType::kDesktop, NoticeType::kFull3PCDSilentOnboarding,
      TrackingProtectionOnboarding::NoticeAction::kGotIt);

  // Verification
  const auto notice_data = notice_storage_.ReadNoticeData(
      prefs(), ToIPHNoticeName(NoticeType::kFull3PCDSilentOnboarding));
  EXPECT_EQ(notice_data->notice_action_taken, NoticeActionTaken::kNotSet);
}

}  // namespace privacy_sandbox
