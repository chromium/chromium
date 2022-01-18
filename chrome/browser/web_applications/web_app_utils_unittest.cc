// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <memory>

#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
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

using WebAppUtilsTest = WebAppTest;
using ::testing::ElementsAre;

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

TEST_F(WebAppUtilsTest, AreWebAppsEnabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SkipMainProfileCheckForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  EXPECT_TRUE(AreWebAppsEnabled(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true)));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile));
  EXPECT_TRUE(AreWebAppsEnabled(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsEnabled(system_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* signin_profile =
      profile_manager.CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      signin_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* lock_screen_profile = profile_manager.CreateTestingProfile(
      ash::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsEnabled(lock_screen_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      lock_screen_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

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

TEST_F(WebAppUtilsTest, AreWebAppsUserInstallable) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SkipMainProfileCheckForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsUserInstallable(regular_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_FALSE(
      AreWebAppsUserInstallable(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(guest_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(system_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* signin_profile =
      profile_manager.CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsUserInstallable(signin_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      signin_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* lock_screen_profile = profile_manager.CreateTestingProfile(
      ash::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsUserInstallable(lock_screen_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      lock_screen_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebApps) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SkipMainProfileCheckForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  Profile* regular_profile = profile();

  EXPECT_EQ(regular_profile, GetBrowserContextForWebApps(regular_profile));
  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebApps(regular_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebApps(regular_profile->GetOffTheRecordProfile(
                Profile::OTRProfileID::CreateUniqueForTesting(),
                /*create_if_needed=*/true)));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(guest_profile));
  EXPECT_EQ(guest_profile,
            GetBrowserContextForWebApps(guest_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(system_profile));
  EXPECT_EQ(nullptr,
            GetBrowserContextForWebApps(system_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebAppMetrics) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SkipMainProfileCheckForTesting();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  Profile* regular_profile = profile();

  EXPECT_EQ(regular_profile,
            GetBrowserContextForWebAppMetrics(regular_profile));
  EXPECT_EQ(
      regular_profile,
      GetBrowserContextForWebAppMetrics(
          regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_EQ(
      regular_profile,
      GetBrowserContextForWebAppMetrics(regular_profile->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(),
          /*create_if_needed=*/true)));

  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());

  Profile* guest_profile = profile_manager.CreateGuestProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_profile));
  EXPECT_EQ(
      nullptr,
      GetBrowserContextForWebAppMetrics(
          guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* system_profile = profile_manager.CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(system_profile));
  EXPECT_EQ(
      nullptr,
      GetBrowserContextForWebAppMetrics(
          system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
}

}  // namespace web_app
