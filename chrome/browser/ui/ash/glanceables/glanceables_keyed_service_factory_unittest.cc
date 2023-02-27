// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class GlanceablesKeyedServiceFactoryTest : public BrowserWithTestWindowTest {
 public:
  GlanceablesKeyedServiceFactoryTest()
      : user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  TestingProfileManager* profile_manager() {
    return BrowserWithTestWindowTest::profile_manager();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  std::unique_ptr<FakeChromeUserManager> user_manager_;
};

TEST_F(GlanceablesKeyedServiceFactoryTest, NoSupportWhenFeatureIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlanceablesV2);

  EXPECT_FALSE(
      GlanceablesKeyedServiceFactory::GetInstance()->GetService(GetProfile()));
}

TEST_F(GlanceablesKeyedServiceFactoryTest, NoSupportForGuestProfile) {
  std::unique_ptr<TestingProfile> guest_profile =
      TestingProfile::Builder()
          .SetGuestSession()
          .SetProfileName("guest_profile")
          .AddTestingFactories({})
          .Build();
  ASSERT_TRUE(guest_profile);

  auto* service = GlanceablesKeyedServiceFactory::GetInstance()->GetService(
      guest_profile.get());
  EXPECT_FALSE(service);
}

TEST_F(GlanceablesKeyedServiceFactoryTest, NoSupportForOffTheRecordProfile) {
  auto* service_for_primary_profile =
      GlanceablesKeyedServiceFactory::GetInstance()->GetService(GetProfile());
  EXPECT_TRUE(service_for_primary_profile);

  TestingProfile* incognito_profile =
      TestingProfile::Builder()
          .SetProfileName(GetProfile()->GetProfileUserName())
          .BuildIncognito(GetProfile());
  ASSERT_TRUE(incognito_profile);
  ASSERT_TRUE(incognito_profile->IsOffTheRecord());

  auto* service_for_incognito_profile =
      GlanceablesKeyedServiceFactory::GetInstance()->GetService(
          incognito_profile);
  EXPECT_FALSE(service_for_incognito_profile);
}

TEST_F(GlanceablesKeyedServiceFactoryTest, SupportsMultipleUserProfiles) {
  auto* service_1 =
      GlanceablesKeyedServiceFactory::GetInstance()->GetService(GetProfile());

  // Returns the same instance for the same profile.
  EXPECT_EQ(
      service_1,
      GlanceablesKeyedServiceFactory::GetInstance()->GetService(GetProfile()));

  const std::string second_profile_name = "second_profile";
  const AccountId account_id(AccountId::FromUserEmail(second_profile_name));
  user_manager_->AddUser(account_id);
  user_manager_->LoginUser(account_id);
  TestingProfile* second_profile = profile_manager()->CreateTestingProfile(
      second_profile_name,
      std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
      u"Test profile 2",
      /*avatar_id=*/1,
      /*testing_factories=*/{});

  // Returns different instances for different profiles.
  EXPECT_NE(service_1,
            GlanceablesKeyedServiceFactory::GetInstance()->GetService(
                second_profile));
}

}  // namespace ash
