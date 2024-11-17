// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service_factory.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/ash/glanceables/glanceables_keyed_service.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class GlanceablesKeyedServiceFactoryTest : public BrowserWithTestWindowTest {
 public:
  TestingProfile* CreateProfile(const std::string& profile_name) override {
    auto* profile =
        profile_manager()->CreateTestingProfile(profile_name,
                                                /*is_main_profile=*/true);
    OnUserProfileCreated(profile_name, profile);
    return profile;
  }
};

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
