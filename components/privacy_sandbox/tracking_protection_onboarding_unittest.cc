// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_onboarding.h"
#include <memory>
#include <utility>
#include "components/prefs/testing_pref_service.h"
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
};

class TrackingProtectionOnboardingTest : public testing::Test {
 public:
  TrackingProtectionOnboardingTest() {
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

 private:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionOnboarding>
      tracking_protection_onboarding_service_;
};

TEST_F(TrackingProtectionOnboardingTest, OnboardingProfileCallsObservers) {
  MockTrackingProtectionObserver observer;
  tracking_protection_onboarding()->AddObserver(&observer);
  EXPECT_CALL(observer, OnTrackingProtectionOnboarded());

  prefs()->SetInteger(
      prefs::kTrackingProtectionOnboardingStatus,
      static_cast<int>(TrackingProtectionOnboardingStatus::kOnboarded));
  testing::Mock::VerifyAndClearExpectations(&observer);
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
}  // namespace
}  // namespace privacy_sandbox
