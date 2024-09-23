// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_hats_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/safety_hub/safety_hub_hats_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SafetyHubHatsServiceTest : public testing::Test {
 public:
  SafetyHubHatsServiceTest() = default;

 protected:
  TestingProfile* profile() { return &profile_; }

  SafetyHubHatsService* service() {
    return SafetyHubHatsServiceFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(SafetyHubHatsServiceTest, SafetyHubInteractionState) {
  base::test::ScopedFeatureList scoped_feature;
  scoped_feature.InitAndEnableFeature(features::kSafetyHub);

  // A profile password store is required to launch the password status check
  // service, which is used to get the password card data, which is part of the
  // retrieved PSD.
  CreateAndUseTestPasswordStore(profile());

  EXPECT_FALSE(service()
                   ->GetSafetyHubProductSpecificData()
                   .find("User visited Safety Hub page")
                   ->second);
  EXPECT_FALSE(service()
                   ->GetSafetyHubProductSpecificData()
                   .find("User clicked Safety Hub notification")
                   ->second);
  EXPECT_FALSE(service()
                   ->GetSafetyHubProductSpecificData()
                   .find("User interacted with Safety Hub")
                   ->second);

  service()->SafetyHubVisited();
  EXPECT_TRUE(service()
                  ->GetSafetyHubProductSpecificData()
                  .find("User visited Safety Hub page")
                  ->second);
  EXPECT_FALSE(service()
                   ->GetSafetyHubProductSpecificData()
                   .find("User clicked Safety Hub notification")
                   ->second);
  EXPECT_FALSE(service()
                   ->GetSafetyHubProductSpecificData()
                   .find("User interacted with Safety Hub")
                   ->second);

  service()->SafetyHubNotificationClicked();
  EXPECT_TRUE(service()
                  ->GetSafetyHubProductSpecificData()
                  .find("User visited Safety Hub page")
                  ->second);
  EXPECT_TRUE(service()
                  ->GetSafetyHubProductSpecificData()
                  .find("User clicked Safety Hub notification")
                  ->second);
  EXPECT_FALSE(service()
                   ->GetSafetyHubProductSpecificData()
                   .find("User interacted with Safety Hub")
                   ->second);

  service()->SafetyHubModuleInteracted();
  EXPECT_TRUE(service()
                  ->GetSafetyHubProductSpecificData()
                  .find("User visited Safety Hub page")
                  ->second);
  EXPECT_TRUE(service()
                  ->GetSafetyHubProductSpecificData()
                  .find("User clicked Safety Hub notification")
                  ->second);
  EXPECT_TRUE(service()
                  ->GetSafetyHubProductSpecificData()
                  .find("User interacted with Safety Hub")
                  ->second);
}
