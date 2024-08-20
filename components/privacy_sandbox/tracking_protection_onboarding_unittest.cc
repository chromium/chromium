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
#include "privacy_sandbox_notice_constants.h"
#include "privacy_sandbox_notice_storage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tracking_protection_onboarding.h"

namespace privacy_sandbox {

namespace {

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

class TrackingProtectionOnboardingTest : public testing::Test {
 public:
  TrackingProtectionOnboardingTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
  }

  void RecreateOnboardingService(
      version_info::Channel channel = version_info::Channel::UNKNOWN) {
    tracking_protection_onboarding_service_ =
        std::make_unique<TrackingProtectionOnboarding>(prefs(), channel);
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
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(TrackingProtectionOnboardingTest, MaybeResetOnboardingPrefsInStable) {
  // Setup
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::STABLE);
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
  tracking_protection_onboarding_service_ =
      std::make_unique<TrackingProtectionOnboarding>(
          prefs(), version_info::Channel::CANARY);
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

class TrackingProtectionSilentOnboardingTest
    : public TrackingProtectionOnboardingTest {};

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
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

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
  tracking_protection_onboarding()->MaybeResetModeBOnboardingPrefs();

  // Verification
  EXPECT_FALSE(
      prefs()
          ->FindPreference(prefs::kTrackingProtectionSilentOnboardingStatus)
          ->HasUserSetting());
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

}  // namespace
}  // namespace privacy_sandbox
