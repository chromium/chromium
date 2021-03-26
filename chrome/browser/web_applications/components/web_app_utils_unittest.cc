// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_utils.h"

#include <memory>

#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

using ::testing::ElementsAre;

class WebAppUtilsTest : public WebAppTest,
                        public ::testing::WithParamInterface<bool> {
 public:
  WebAppUtilsTest() : is_ephemeral_guest_(GetParam()) {
    // Update for platforms which do not support ephemeral Guest profiles.
    is_ephemeral_guest_ &=
        TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
            scoped_feature_list_, is_ephemeral_guest_);
  }

  bool is_ephemeral_guest() const { return is_ephemeral_guest_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool is_ephemeral_guest_;
};

// Sanity check that iteration order of SortedSizesPx is ascending. The
// correctness of most usage of SortedSizesPx depends on this.
TEST(WebAppTest, SortedSizesPxIsAscending) {
  // Removal of duplicates is expected but not required for correctness.
  std::vector<SquareSizePx> in{512, 512, 16, 512, 64, 32, 256};
  SortedSizesPx sorted(in);
  ASSERT_THAT(sorted, ElementsAre(16, 32, 64, 256, 512));

  std::vector<SquareSizePx> out(sorted.begin(), sorted.end());
  ASSERT_THAT(out, ElementsAre(16, 32, 64, 256, 512));

  std::vector<SquareSizePx> reversed(sorted.rbegin(), sorted.rend());
  ASSERT_THAT(reversed, ElementsAre(512, 256, 64, 32, 16));

  std::vector<SquareSizePx> base_reversed(base::Reversed(sorted).begin(),
                                          base::Reversed(sorted).end());
  ASSERT_THAT(base_reversed, ElementsAre(512, 256, 64, 32, 16));
}

TEST_P(WebAppUtilsTest, AreWebAppsEnabled) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile->GetPrimaryOTRProfile()));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID("Test::WebAppUtils"))));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile));
  if (!is_ephemeral_guest())
    EXPECT_TRUE(AreWebAppsEnabled(guest_profile->GetPrimaryOTRProfile()));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsEnabled(system_profile));
  EXPECT_FALSE(AreWebAppsEnabled(system_profile->GetPrimaryOTRProfile()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* signin_profile =
      profile_manager.CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile));
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile->GetPrimaryOTRProfile()));

  Profile* lock_screen_profile = profile_manager.CreateTestingProfile(
      chromeos::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsEnabled(lock_screen_profile));
  EXPECT_FALSE(AreWebAppsEnabled(lock_screen_profile->GetPrimaryOTRProfile()));

  using MockUserManager = testing::NiceMock<ash::MockUserManager>;
  {
    auto user_manager = std::make_unique<MockUserManager>();
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  }
  {
    auto user_manager = std::make_unique<MockUserManager>();
    EXPECT_CALL(*user_manager, IsLoggedInAsKioskApp())
        .WillOnce(testing::Return(true));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_FALSE(AreWebAppsEnabled(regular_profile));
  }
  {
    auto user_manager = std::make_unique<MockUserManager>();
    EXPECT_CALL(*user_manager, IsLoggedInAsArcKioskApp())
        .WillOnce(testing::Return(true));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_FALSE(AreWebAppsEnabled(regular_profile));
  }
#endif
}

TEST_P(WebAppUtilsTest, AreWebAppsUserInstallable) {
  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsUserInstallable(regular_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(regular_profile->GetPrimaryOTRProfile()));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID("Test::WebAppUtils"))));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(guest_profile));
  if (!is_ephemeral_guest()) {
    EXPECT_FALSE(
        AreWebAppsUserInstallable(guest_profile->GetPrimaryOTRProfile()));
  }

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(system_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(system_profile->GetPrimaryOTRProfile()));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* signin_profile =
      profile_manager.CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsUserInstallable(signin_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(signin_profile->GetPrimaryOTRProfile()));

  Profile* lock_screen_profile = profile_manager.CreateTestingProfile(
      chromeos::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsUserInstallable(lock_screen_profile));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(lock_screen_profile->GetPrimaryOTRProfile()));
#endif
}

TEST_P(WebAppUtilsTest, GetBrowserContextForWebApps) {
  Profile* regular_profile = profile();

  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(regular_profile));
  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(
                                 regular_profile->GetPrimaryOTRProfile()));
  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebApps(regular_profile->GetOffTheRecordProfile(
                Profile::OTRProfileID("Test::WebAppUtils"))));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(guest_profile));
  if (!is_ephemeral_guest()) {
    EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(
                                 guest_profile->GetPrimaryOTRProfile()));
  }

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(system_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(
                         system_profile->GetPrimaryOTRProfile()));
}

TEST_P(WebAppUtilsTest, GetBrowserContextForWebAppMetrics) {
  Profile* regular_profile = profile();

  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebAppMetrics(regular_profile));
  EXPECT_EQ(regular_profile, GetBrowserContextForWebAppMetrics(
                                 regular_profile->GetPrimaryOTRProfile()));
  EXPECT_EQ(
      regular_profile,
      GetBrowserContextForWebAppMetrics(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID("Test::WebAppUtils"))));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_profile));
  if (!is_ephemeral_guest()) {
    EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(
                           guest_profile->GetPrimaryOTRProfile()));
  }

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(system_profile));
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(
                         system_profile->GetPrimaryOTRProfile()));
}

INSTANTIATE_TEST_SUITE_P(AllGuestTypes,
                         WebAppUtilsTest,
                         /*is_ephemeral_guest=*/testing::Bool());

}  // namespace web_app
