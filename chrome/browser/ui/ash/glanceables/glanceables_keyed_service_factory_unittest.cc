// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class GlanceablesKeyedServiceFactoryTest : public BrowserWithTestWindowTest {
 public:
  GlanceablesKeyedServiceFactoryTest()
      : scoped_user_manager_(std::make_unique<FakeChromeUserManager>()) {}

  TestingProfile* CreateProfile() override {
    const std::string profile_name = "primary_profile@example.com";
    const auto account_id = AccountId::FromUserEmail(profile_name);
    fake_chrome_user_manager()->AddUser(account_id);
    fake_chrome_user_manager()->LoginUser(account_id);
    session_controller_client()->AddUserSession(profile_name);
    session_controller_client()->SwitchActiveUser(account_id);
    return profile_manager()->CreateTestingProfile(profile_name,
                                                   /*is_main_profile=*/true);
  }

  FakeChromeUserManager* fake_chrome_user_manager() {
    return static_cast<FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  TestSessionControllerClient* session_controller_client() {
    return ash_test_helper()->test_session_controller_client();
  }

  TestingProfileManager* profile_manager() {
    return BrowserWithTestWindowTest::profile_manager();
  }

 protected:
  base::test::ScopedFeatureList feature_list_{features::kGlanceablesV2};
  user_manager::ScopedUserManager scoped_user_manager_;
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

}  // namespace ash
