// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_survey_service.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/mock_tracking_protection_onboarding_delegate.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/privacy_sandbox/tracking_protection_reminder_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tracking_protection_prefs.h"
#include "tracking_protection_reminder_service.h"

namespace privacy_sandbox {
namespace {

using NoticeType = privacy_sandbox::TrackingProtectionOnboarding::NoticeType;
using SurfaceType = privacy_sandbox::TrackingProtectionOnboarding::SurfaceType;

class TrackingProtectionSurveyServiceTest : public testing::Test {
 public:
  TrackingProtectionSurveyServiceTest() {
    tracking_protection::RegisterProfilePrefs(prefs()->registry());
    // Dependency for TrackingProtectionReminderService
    PrivacySandboxNoticeStorage::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    auto delegate =
        std::make_unique<MockTrackingProtectionOnboardingDelegate>();

    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(), {});
    onboarding_service_ = std::make_unique<TrackingProtectionOnboarding>(
        std::move(delegate), prefs(), version_info::Channel::DEV);
    reminder_service_ = std::make_unique<TrackingProtectionReminderService>(
        prefs(), onboarding_service());
    survey_service_ = std::make_unique<TrackingProtectionSurveyService>(
        prefs(), onboarding_service(), reminder_service());
  }

  void TearDown() override {
    survey_service_ = nullptr;
    reminder_service_ = nullptr;
    onboarding_service_ = nullptr;
  }

  TrackingProtectionOnboarding* onboarding_service() {
    return onboarding_service_.get();
  }

  TrackingProtectionReminderService* reminder_service() {
    return reminder_service_.get();
  }

  TrackingProtectionSurveyService* survey_service() {
    return survey_service_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {};
  }

  void ShowOnboardingNotice(bool is_silent) {
    if (is_silent) {
      onboarding_service()->MaybeMarkModeBSilentEligible();
      onboarding_service()->NoticeShown(SurfaceType::kDesktop,
                                        NoticeType::kModeBSilentOnboarding);
    } else {
      onboarding_service()->MaybeMarkModeBEligible();
      onboarding_service()->NoticeShown(SurfaceType::kDesktop,
                                        NoticeType::kModeBOnboarding);
    }
  }

  void CallOnboardingObserver(bool is_silent) {
    if (is_silent) {
      reminder_service()->OnTrackingProtectionSilentOnboardingUpdated(
          TrackingProtectionOnboarding::SilentOnboardingStatus::kOnboarded);
    } else {
      reminder_service()->OnTrackingProtectionOnboardingUpdated(
          TrackingProtectionOnboarding::OnboardingStatus::kOnboarded);
    }
  }

  std::optional<base::Time> GetOnboardedTimestamp(bool is_silent) {
    if (is_silent) {
      return onboarding_service()->GetSilentOnboardingTimestamp();
    } else {
      return onboarding_service()->GetOnboardingTimestamp();
    }
  }

  base::test::TaskEnvironment task_env_;
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<TrackingProtectionSurveyService> survey_service_;
  std::unique_ptr<TrackingProtectionOnboarding> onboarding_service_;
  std::unique_ptr<TrackingProtectionReminderService> reminder_service_;
  base::test::ScopedFeatureList feature_list_;
};

class TrackingProtectionSurveyServiceSurveyWindowStartTimeTest
    : public TrackingProtectionSurveyServiceTest,
      public testing::WithParamInterface<bool> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    // Set the survey to be anchored on onboarding with a 28 day delay.
    return {{kTrackingProtectionSentimentSurvey,
             {
                 {"survey-anchor", /*kOnboarding*/ "1"},
                 {"time-to-survey", "28d"},
             }}};
  }
};

TEST_P(TrackingProtectionSurveyServiceSurveyWindowStartTimeTest,
       DoesNotOverrideExistingStartTime) {
  // Set explicit start time.
  auto time_value = base::Time() + base::Days(7);
  prefs()->SetTime(prefs::kTrackingProtectionSurveyWindowStartTime, time_value);

  // Confirm that the start time was not overwritten upon onboarding.
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            time_value);
}

TEST_P(TrackingProtectionSurveyServiceSurveyWindowStartTimeTest,
       DoesNotSetWindowStartTimeWhenOnboardingNoticeNotShown) {
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());
  // Don't call `ShowOnboardingNotice` which sets the onboarding timestamp.
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_FALSE(GetOnboardedTimestamp(/*is_silent=*/GetParam()).has_value());
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());
}

TEST_P(TrackingProtectionSurveyServiceSurveyWindowStartTimeTest,
       UpdatesSurveyWindowStartTime) {
  // Ensure the start time has default value.
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());

  // Confirm that the window start time was updated.
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            *GetOnboardedTimestamp(/*is_silent=*/GetParam()) + base::Days(28));
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionSurveyServiceSurveyWindowStartTimeTest,
    TrackingProtectionSurveyServiceSurveyWindowStartTimeTest,
    /*is_silently_onboarded=*/testing::Bool());

class TrackingProtectionSurveyServiceFullExperienceAnchorTest
    : public TrackingProtectionSurveyServiceTest,
      public testing::WithParamInterface<bool> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionSentimentSurvey,
             {{"survey-anchor", /*kFullExperience*/ "2"},
              {"time-to-survey", "7d"}}}};
  }
};

TEST_P(TrackingProtectionSurveyServiceFullExperienceAnchorTest,
       DoesNotUpdateSurveyWindowStartTime) {
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());

  // Confirm that the window start time was not updated since the survey is
  // anchored to the full experience.
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());
}

INSTANTIATE_TEST_SUITE_P(
    TrackingProtectionSurveyServiceFullExperienceAnchorTest,
    TrackingProtectionSurveyServiceFullExperienceAnchorTest,
    /*is_silently_onboarded=*/testing::Bool());

class TrackingProtectionSurveyServiceFeatureDisabledTest
    : public TrackingProtectionSurveyServiceTest,
      public testing::WithParamInterface<bool> {};

TEST_P(TrackingProtectionSurveyServiceFeatureDisabledTest,
       DoesNotUpdateSurveyWindowStartTime) {
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());

  // Confirm that the window start time was not updated since the survey feature
  // was not set.
  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionSurveyServiceFeatureDisabledTest,
                         TrackingProtectionSurveyServiceFeatureDisabledTest,
                         /*is_silently_onboarded=*/testing::Bool());

class TrackingProtectionSurveyServiceTimeToSurveyNotSetTest
    : public TrackingProtectionSurveyServiceTest,
      public testing::WithParamInterface<bool> {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{kTrackingProtectionSentimentSurvey,
             {{"survey-anchor", /*kOnboarding*/ "1"}}}};
  }
};

TEST_P(TrackingProtectionSurveyServiceTimeToSurveyNotSetTest,
       UpdatesSurveyWindowStartTimeToDefault) {
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time());

  ShowOnboardingNotice(/*is_silent=*/GetParam());
  CallOnboardingObserver(/*is_silent=*/GetParam());
  // Confirm that window start time was updated using the default value since
  // time to survey was not set.
  EXPECT_EQ(prefs()->GetTime(prefs::kTrackingProtectionSurveyWindowStartTime),
            base::Time() + base::TimeDelta::Max());
}

INSTANTIATE_TEST_SUITE_P(TrackingProtectionSurveyServiceTimeToSurveyNotSetTest,
                         TrackingProtectionSurveyServiceTimeToSurveyNotSetTest,
                         /*is_silently_onboarded=*/testing::Bool());

}  // namespace
}  // namespace privacy_sandbox
