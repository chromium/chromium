// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "privacy_sandbox_prefs.h"
#include "privacy_sandbox_survey_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

class PrivacySandboxSurveyServiceTest : public testing::Test {
 public:
  PrivacySandboxSurveyServiceTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    survey_service_ = std::make_unique<PrivacySandboxSurveyService>(prefs());
  }
  void TearDown() override { survey_service_ = nullptr; }

 protected:
  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {{kPrivacySandboxSentimentSurvey, {}}};
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  PrivacySandboxSurveyService* survey_service() {
    return survey_service_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  TestingPrefServiceSimple prefs_;
  std::unique_ptr<PrivacySandboxSurveyService> survey_service_;
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_env_;
};

class PrivacySandboxSurveyServiceFeatureDisabledTest
    : public PrivacySandboxSurveyServiceTest {
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {};
  }
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {kPrivacySandboxSentimentSurvey};
  }
};

TEST_F(PrivacySandboxSurveyServiceFeatureDisabledTest, SurveyDoesNotShow) {
  EXPECT_FALSE(survey_service()->ShouldShowSentimentSurvey());
}

class PrivacySandboxSurveyServiceCooldownTest
    : public PrivacySandboxSurveyServiceTest {};

TEST_F(PrivacySandboxSurveyServiceCooldownTest, SurveyShownByDefault) {
  // By default the survey should be shown.
  EXPECT_TRUE(survey_service()->ShouldShowSentimentSurvey());
}

TEST_F(PrivacySandboxSurveyServiceCooldownTest,
       SurveyNotShownWithActiveCooldown) {
  EXPECT_TRUE(survey_service()->ShouldShowSentimentSurvey());
  survey_service()->OnSuccessfulSentimentSurvey();
  // We just showed the survey, so the cooldown prevents showing it again.
  EXPECT_FALSE(survey_service()->ShouldShowSentimentSurvey());
}

TEST_F(PrivacySandboxSurveyServiceCooldownTest,
       SurveyShownWhenCooldownExpires) {
  EXPECT_TRUE(survey_service()->ShouldShowSentimentSurvey());
  survey_service()->OnSuccessfulSentimentSurvey();
  // We just showed the survey, so the cooldown prevents showing it again.
  EXPECT_FALSE(survey_service()->ShouldShowSentimentSurvey());
  task_env_.FastForwardBy(base::Days(180));
  EXPECT_TRUE(survey_service()->ShouldShowSentimentSurvey());
}

class PrivacySandboxSurveyServiceOnSuccessfulSentimentSurveyTest
    : public PrivacySandboxSurveyServiceTest {};

TEST_F(PrivacySandboxSurveyServiceOnSuccessfulSentimentSurveyTest,
       SetsPrefToCurrentTime) {
  base::Time current_time = base::Time::Now();
  survey_service()->OnSuccessfulSentimentSurvey();
  EXPECT_EQ(prefs()->GetTime(prefs::kPrivacySandboxSentimentSurveyLastSeen),
            current_time);
}

}  // namespace
}  // namespace privacy_sandbox
