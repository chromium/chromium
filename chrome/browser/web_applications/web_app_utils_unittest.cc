// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include <memory>

#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
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

TEST(WebAppTest, ResolveEffectiveDisplayMode) {
  // When user_display_mode indicates a user preference for opening in
  // a browser tab, we open in a browser tab.
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, std::vector<DisplayMode>(),
                UserDisplayMode::kBrowser, /*is_isolated=*/false));

  // When user_display_mode indicates a user preference for opening in
  // a standalone window, we open in a minimal-ui window (for app_display_mode
  // 'browser' or 'minimal-ui') or a standalone window (for app_display_mode
  // 'standalone' or 'fullscreen').
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, std::vector<DisplayMode>(),
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
}

TEST(WebAppTest,
     ResolveEffectiveDisplayModeWithDisplayOverridesPreferUserMode) {
  // When user_display_mode indicates a user preference for opening in
  // a browser tab, we open in a browser tab even if display_overrides
  // are specified
  std::vector<DisplayMode> app_display_mode_overrides;
  app_display_mode_overrides.push_back(DisplayMode::kStandalone);

  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kBrowser,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, app_display_mode_overrides,
                UserDisplayMode::kBrowser, /*is_isolated=*/false));
}

TEST(WebAppTest,
     ResolveEffectiveDisplayModeWithDisplayOverridesFallbackToDisplayMode) {
  // When user_display_mode indicates a user preference for opening in
  // a standalone window, and the only display modes provided for
  // display_overrides contain only 'fullscreen' or 'browser',  open in a
  // minimal-ui window (for app_display_mode 'browser' or 'minimal-ui') or a
  // standalone window (for app_display_mode 'standalone' or 'fullscreen').
  std::vector<DisplayMode> app_display_mode_overrides;
  app_display_mode_overrides.push_back(DisplayMode::kFullscreen);

  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kMinimalUi,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
}

TEST(WebAppTest, ResolveEffectiveDisplayModeWithDisplayOverrides) {
  // When user_display_mode indicates a user preference for opening in
  // a standalone window, and return the first entry that is either
  // 'standalone' or 'minimal-ui' in display_override
  std::vector<DisplayMode> app_display_mode_overrides;
  app_display_mode_overrides.push_back(DisplayMode::kFullscreen);
  app_display_mode_overrides.push_back(DisplayMode::kBrowser);
  app_display_mode_overrides.push_back(DisplayMode::kStandalone);

  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kBrowser, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kMinimalUi, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kStandalone, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                DisplayMode::kFullscreen, app_display_mode_overrides,
                UserDisplayMode::kStandalone, /*is_isolated=*/false));
}

TEST(WebAppTest, ResolveEffectiveDisplayModeWithIsolatedWebApp) {
  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                /*app_display_mode=*/DisplayMode::kBrowser,
                /*app_display_mode_overrides=*/{DisplayMode::kBrowser},
                /*user_display_mode=*/UserDisplayMode::kBrowser,
                /*is_isolated=*/true));

  EXPECT_EQ(DisplayMode::kStandalone,
            ResolveEffectiveDisplayMode(
                /*app_display_mode=*/DisplayMode::kMinimalUi,
                /*app_display_mode_overrides=*/{},
                /*user_display_mode=*/UserDisplayMode::kBrowser,
                /*is_isolated=*/true));
}

TEST_F(WebAppUtilsTest, AreWebAppsEnabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  Profile* regular_profile = profile();

  EXPECT_FALSE(AreWebAppsEnabled(nullptr));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  EXPECT_TRUE(AreWebAppsEnabled(
      regular_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
  EXPECT_TRUE(AreWebAppsEnabled(regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true)));

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  EXPECT_TRUE(AreWebAppsEnabled(guest_profile));
  EXPECT_TRUE(AreWebAppsEnabled(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsEnabled(system_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* signin_profile =
      profile_manager().CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsEnabled(signin_profile));
  EXPECT_FALSE(AreWebAppsEnabled(
      signin_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* lock_screen_profile = profile_manager().CreateTestingProfile(
      ash::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_TRUE(AreWebAppsEnabled(lock_screen_profile));
  EXPECT_TRUE(AreWebAppsEnabled(
      lock_screen_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  using MockUserManager = testing::NiceMock<ash::MockUserManager>;
  {
    auto user_manager = std::make_unique<MockUserManager>();
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kKioskEnableAppService);
    auto user_manager = std::make_unique<MockUserManager>();
    EXPECT_CALL(*user_manager, IsLoggedInAsKioskApp())
        .WillOnce(testing::Return(true));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_FALSE(AreWebAppsEnabled(regular_profile));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kKioskEnableAppService);
    auto user_manager = std::make_unique<MockUserManager>();
    EXPECT_CALL(*user_manager, IsLoggedInAsWebKioskApp())
        .WillRepeatedly(testing::Return(true));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_FALSE(AreWebAppsEnabled(regular_profile));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kKioskEnableAppService);
    auto user_manager = std::make_unique<MockUserManager>();
    EXPECT_CALL(*user_manager, IsLoggedInAsArcKioskApp())
        .WillOnce(testing::Return(true));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_FALSE(AreWebAppsEnabled(regular_profile));
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
    EXPECT_CALL(*user_manager, IsLoggedInAsWebKioskApp())
        .WillRepeatedly(testing::Return(true));
    user_manager::ScopedUserManager enabler(std::move(user_manager));
    EXPECT_TRUE(AreWebAppsEnabled(regular_profile));
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
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
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

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(guest_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_FALSE(AreWebAppsUserInstallable(system_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* signin_profile =
      profile_manager().CreateTestingProfile(chrome::kInitialProfile);
  EXPECT_FALSE(AreWebAppsUserInstallable(signin_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      signin_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

  Profile* lock_screen_profile = profile_manager().CreateTestingProfile(
      ash::ProfileHelper::GetLockScreenAppProfileName());
  EXPECT_FALSE(AreWebAppsUserInstallable(lock_screen_profile));
  EXPECT_FALSE(AreWebAppsUserInstallable(
      lock_screen_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebApps) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
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

  Profile* guest_profile = profile_manager().CreateGuestProfile();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  guest_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(guest_profile, GetBrowserContextForWebApps(guest_profile));
  EXPECT_EQ(guest_profile,
            GetBrowserContextForWebApps(guest_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebApps(system_profile));
  EXPECT_EQ(nullptr,
            GetBrowserContextForWebApps(system_profile->GetPrimaryOTRProfile(
                /*create_if_needed=*/true)));
#endif
}

TEST_F(WebAppUtilsTest, GetBrowserContextForWebAppMetrics) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check;
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

  Profile* guest_profile = profile_manager().CreateGuestProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(guest_profile));
  EXPECT_EQ(
      nullptr,
      GetBrowserContextForWebAppMetrics(
          guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* system_profile = profile_manager().CreateSystemProfile();
  EXPECT_EQ(nullptr, GetBrowserContextForWebAppMetrics(system_profile));
  EXPECT_EQ(
      nullptr,
      GetBrowserContextForWebAppMetrics(
          system_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true)));
#endif
}

}  // namespace web_app
