// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

TEST(StartupBrowserCreatorTest, ShouldLoadProfileWithoutWindow) {
  {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Forcibly set ash-chrome as the primary browser.
    // This is the current default behavior.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {}, {ash::features::kLacrosSupport, ash::features::kLacrosPrimary});
#endif
    EXPECT_FALSE(StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
        base::CommandLine(base::CommandLine::NO_PROGRAM)));
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kNoStartupWindow);
    EXPECT_TRUE(
        StartupBrowserCreator::ShouldLoadProfileWithoutWindow(command_line));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  {
    // Check what happens if lacros-chrome becomes the primary browser.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
         ash::features::kLacrosOnly,
         ash::features::kLacrosProfileMigrationForceOff},
        {});
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    auto* primary_user =
        fake_user_manager->AddUser(AccountId::FromUserEmail("test@test"));
    fake_user_manager->UserLoggedIn(primary_user->GetAccountId(),
                                    primary_user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    auto scoped_user_manager =
        std::make_unique<user_manager::ScopedUserManager>(
            std::move(fake_user_manager));
    EXPECT_TRUE(StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
        base::CommandLine(base::CommandLine::NO_PROGRAM)));
  }
#endif
}
