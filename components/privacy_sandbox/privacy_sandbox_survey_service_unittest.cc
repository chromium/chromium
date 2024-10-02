// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_survey_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "privacy_sandbox_prefs.h"
#include "privacy_sandbox_survey_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ContainerEq;

namespace privacy_sandbox {
namespace {

class PrivacySandboxSurveyServiceTest : public testing::Test {
 public:
  PrivacySandboxSurveyServiceTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    privacy_sandbox::RegisterProfilePrefs(prefs()->registry());
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
  }

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    survey_service_ = std::make_unique<PrivacySandboxSurveyService>(
        prefs(), identity_test_env_->identity_manager());
  }
  void TearDown() override { survey_service_ = nullptr; }

 protected:
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_.get();
  }
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

  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
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

class PrivacySandboxSurveyServiceSentimentSurveyPsbTest
    : public PrivacySandboxSurveyServiceTest,
      public testing::WithParamInterface<
          testing::tuple<bool, bool, bool, bool>> {};

TEST_P(PrivacySandboxSurveyServiceSentimentSurveyPsbTest, FetchesValues) {
  prefs()->SetBoolean(prefs::kPrivacySandboxM1TopicsEnabled,
                      testing::get<0>(GetParam()));
  prefs()->SetBoolean(prefs::kPrivacySandboxM1FledgeEnabled,
                      testing::get<1>(GetParam()));
  prefs()->SetBoolean(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                      testing::get<2>(GetParam()));
  if (testing::get<3>(GetParam())) {
    signin::MakePrimaryAccountAvailable(identity_test_env()->identity_manager(),
                                        "test@gmail.com",
                                        signin::ConsentLevel::kSignin);
  }

  std::map<std::string, bool> expected_map = {
      {"Topics enabled", testing::get<0>(GetParam())},
      {"Protected audience enabled", testing::get<1>(GetParam())},
      {"Measurement enabled", testing::get<2>(GetParam())},
      {"Signed in", testing::get<3>(GetParam())},
  };

  EXPECT_THAT(survey_service()->GetSentimentSurveyPsb(),
              ContainerEq(expected_map));
}

INSTANTIATE_TEST_SUITE_P(PrivacySandboxSurveyServiceSentimentSurveyPsbTest,
                         PrivacySandboxSurveyServiceSentimentSurveyPsbTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace
}  // namespace privacy_sandbox
