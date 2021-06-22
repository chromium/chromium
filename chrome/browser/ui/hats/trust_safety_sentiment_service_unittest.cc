// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service.h"

#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TrustSafetySentimentServiceTest : public testing::Test {
 public:
  TrustSafetySentimentServiceTest() {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(testing::_))
        .WillRepeatedly(testing::Return(true));
  }

 protected:
  struct FeatureParams {
    std::string privacy_settings_time = "20s";
    std::string min_time_to_prompt = "2m";
    std::string max_time_to_prompt = "60m";
    std::string ntp_visits_min_range = "2";
    std::string ntp_visits_max_range = "4";
    std::string privacy_settings_probability = "0.6";
    std::string trusted_surface_probability = "0.4";
    std::string transactions_probability = "0.05";
    std::string privacy_settings_trigger_id = "privacy-settings-test";
    std::string trusted_surface_trigger_id = "trusted-surface-test";
    std::string transactions_trigger_id = "transactions-test";
  };

  void SetupFeatureParameters(FeatureParams params) {
    feature_list()->InitAndEnableFeatureWithParameters(
        features::kTrustSafetySentimentSurvey,
        {
            {"privacy-settings-time", params.privacy_settings_time},
            {"min-time-to-prompt", params.min_time_to_prompt},
            {"max-time-to-prompt", params.max_time_to_prompt},
            {"ntp-visits-min-range", params.ntp_visits_min_range},
            {"ntp-visits-max-range", params.ntp_visits_max_range},
            {"privacy-settings-probability",
             params.privacy_settings_probability},
            {"trusted-surface-probability", params.trusted_surface_probability},
            {"transactions-probability", params.transactions_probability},
            {"privacy-settings-trigger-id", params.privacy_settings_trigger_id},
            {"trusted-surface-trigger-id", params.trusted_surface_trigger_id},
            {"transactions-trigger-id", params.transactions_trigger_id},
        });
  }

  TrustSafetySentimentService* service() {
    return TrustSafetySentimentServiceFactory::GetForProfile(profile());
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  MockHatsService* mock_hats_service_;
};

TEST_F(TrustSafetySentimentServiceTest, Eligibility_NtpOpens) {
  // A survey should not be shown if not enough NTPs have been opened since
  // the most recent trigger action.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "2";
  params.ntp_visits_max_range = "2";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  // The Trusted Surface trigger should prevent a survey from being shown, as
  // it still has 1 NTP to open.
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // The next NTP should be eligible for a survey.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(testing::_, testing::_, testing::_, testing::_));
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, Eligibility_Time) {
  // A survey should not be shown if any trigger action occurred to recently, or
  // if all trigger actions occurred too long ago.
  FeatureParams params;
  params.privacy_settings_probability = "1.0";
  params.trusted_surface_probability = "1.0";
  params.min_time_to_prompt = "1m";
  params.max_time_to_prompt = "10m";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->OpenedNewTabPage();

  task_environment()->AdvanceClock(base::TimeDelta::FromMinutes(2));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Moving the clock forward such that only the trusted surface trigger is
  // within the window should guarantee it is the survey shown.
  task_environment()->AdvanceClock(base::TimeDelta::FromMinutes(9));
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface,
                           testing::_, testing::_, testing::_));
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, TriggerProbability) {
  // Triggers which fail the probability check should not be considered.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  FeatureParams params;
  params.trusted_surface_probability = "0.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  SetupFeatureParameters(params);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
}

TEST_F(TrustSafetySentimentServiceTest, TriggersClearOnLaunch) {
  // Check that all active triggers are cleared when a survey is launched.
  FeatureParams params;
  params.trusted_surface_probability = "1.0";
  params.privacy_settings_probability = "1.0";
  params.min_time_to_prompt = "0s";
  params.ntp_visits_min_range = "0";
  params.ntp_visits_max_range = "0";
  SetupFeatureParameters(params);

  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kPrivacySettings, {});
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});

  // The launched survey will be randomly selected from the two triggers.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(testing::_, testing::_, testing::_, testing::_));
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // The trigger which did not result in a survey should no longer be
  // considered.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(testing::_, testing::_, testing::_, testing::_))
      .Times(0);
  service()->OpenedNewTabPage();
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Repeated triggers post survey launch should however be considered.
  EXPECT_CALL(*mock_hats_service(),
              LaunchSurvey(kHatsSurveyTriggerTrustSafetyTrustedSurface,
                           testing::_, testing::_, testing::_));
  service()->TriggerOccurred(
      TrustSafetySentimentService::FeatureArea::kTrustedSurface, {});
  service()->OpenedNewTabPage();
}
