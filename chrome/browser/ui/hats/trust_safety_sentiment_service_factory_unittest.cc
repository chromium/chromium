// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TrustSafetySentimentServiceFactoryTest : public testing::Test {
 public:
  TrustSafetySentimentServiceFactoryTest() {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(), base::BindRepeating(&BuildMockHatsService)));
    feature_list()->InitAndEnableFeature(features::kTrustSafetySentimentSurvey);
  }

 protected:
  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }
  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockHatsService> mock_hats_service_;
};

TEST_F(TrustSafetySentimentServiceFactoryTest, ServiceAvailable) {
  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(/*user_prompted=*/false))
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(TrustSafetySentimentServiceFactory::GetForProfile(profile()));
}

TEST_F(TrustSafetySentimentServiceFactoryTest, NoServiceWithoutHats) {
  // Check that when HaTS reports that the user is ineligible to receive any
  // survey, that the service is not created.
  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(/*user_prompted=*/false))
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(TrustSafetySentimentServiceFactory::GetForProfile(profile()));
  testing::Mock::VerifyAndClearExpectations(mock_hats_service());

  // Once the service has not been created, further attempts to create it should
  // be no-ops (the existing null service should be returned). This behavior is
  // defined the KeyedServiceFactory class, but this service depends on it for
  // optimisations (as the call to check is a user is eligible for HaTS is
  // somewhat heavy), and hence the behavior is confirmed here.
  EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(testing::_)).Times(0);
  EXPECT_FALSE(TrustSafetySentimentServiceFactory::GetForProfile(profile()));
}

TEST_F(TrustSafetySentimentServiceFactoryTest, NoServiceFeatureDisabled) {
  feature_list()->Reset();
  feature_list()->InitAndDisableFeature(features::kTrustSafetySentimentSurvey);
  EXPECT_FALSE(TrustSafetySentimentServiceFactory::GetForProfile(profile()));
}

TEST_F(TrustSafetySentimentServiceFactoryTest, VersionTwoEnabled) {
  feature_list()->Reset();
  feature_list()->InitWithFeatures({features::kTrustSafetySentimentSurveyV2},
                                   {features::kTrustSafetySentimentSurvey});
  EXPECT_FALSE(TrustSafetySentimentServiceFactory::GetForProfile(profile()));
}
