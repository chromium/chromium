// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_keyed_service_factory.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/birch/birch_keyed_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class BirchKeyedServiceFactoryTest : public BrowserWithTestWindowTest {
 protected:
  base::test::ScopedFeatureList feature_list_{features::kForestFeature};
};

TEST_F(BirchKeyedServiceFactoryTest, SupportWhenFeatureIsEnabled) {
  EXPECT_TRUE(
      BirchKeyedServiceFactory::GetInstance()->GetService(GetProfile()));
}

TEST_F(BirchKeyedServiceFactoryTest, NoSupportWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kForestFeature});

  EXPECT_FALSE(
      BirchKeyedServiceFactory::GetInstance()->GetService(GetProfile()));
}

TEST_F(BirchKeyedServiceFactoryTest, NoSupportForGuestProfile) {
  std::unique_ptr<TestingProfile> guest_profile =
      TestingProfile::Builder()
          .SetGuestSession()
          .SetProfileName("guest_profile")
          .AddTestingFactories({})
          .Build();
  ASSERT_TRUE(guest_profile);

  auto* service =
      BirchKeyedServiceFactory::GetInstance()->GetService(guest_profile.get());
  EXPECT_FALSE(service);
}

TEST_F(BirchKeyedServiceFactoryTest, NoSupportForOffTheRecordProfile) {
  auto* service_for_primary_profile =
      BirchKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  EXPECT_TRUE(service_for_primary_profile);

  TestingProfile* incognito_profile =
      TestingProfile::Builder()
          .SetProfileName(GetProfile()->GetProfileUserName())
          .BuildIncognito(GetProfile());
  ASSERT_TRUE(incognito_profile);
  ASSERT_TRUE(incognito_profile->IsOffTheRecord());

  auto* service_for_incognito_profile =
      BirchKeyedServiceFactory::GetInstance()->GetService(incognito_profile);
  EXPECT_FALSE(service_for_incognito_profile);
}

}  // namespace ash
