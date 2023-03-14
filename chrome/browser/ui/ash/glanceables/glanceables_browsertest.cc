// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_util.h"
#include "ash/shell.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

constexpr char kTestUserName[] = "test@test.test";
constexpr char kTestUserGaiaId[] = "123456";

}  // namespace

// Tests for the glanceables feature, which adds a "welcome back" screen on
// some logins.
class GlanceablesBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    // The test harness adds --no-first-run. Remove it so glanceables show up.
    command_line->RemoveSwitch(switches::kNoFirstRun);

    // Don't open a browser window, because doing so would hide glanceables.
    // Note that InProcessBrowserTest::browser() will be null.
    command_line->AppendSwitch(switches::kNoStartupWindow);

    command_line->AppendSwitch(ash::switches::kLoginManager);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(ash::switches::kAllowFailedPolicyFetchForTest);
  }

  void CreateAndStartUserSession() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(kTestUserName, kTestUserGaiaId);
    auto* session_manager = session_manager::SessionManager::Get();
    session_manager->CreateSession(account_id, kTestUserName, false);

    profile_ = profiles::testing::CreateProfileSync(
        g_browser_process->profile_manager(),
        ash::ProfileHelper::GetProfilePathByUserIdHash(
            user_manager::UserManager::Get()
                ->FindUser(account_id)
                ->username_hash()));

    session_manager->NotifyUserProfileLoaded(account_id);
    session_manager->SessionStarted();
  }

  ash::GlanceablesController* glanceables_controller() {
    return ash::Shell::Get()->glanceables_controller();
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfileIfExists(profile_);
  }

 protected:
  base::test::ScopedFeatureList features_{ash::features::kGlanceables};
  Profile* profile_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(GlanceablesBrowserTest, ShowsAndHide) {
  // Not showing on the login screen.
  EXPECT_FALSE(glanceables_controller()->IsShowing());

  CreateAndStartUserSession();

  // Not showing right after login if there's no refresh token yet.
  EXPECT_FALSE(glanceables_controller()->IsShowing());

  // Makes the primary account available, generates a refresh token and runs
  // `IdentityManager` callbacks for signin success.
  signin::MakePrimaryAccountAvailable(identity_manager(), kTestUserName,
                                      signin::ConsentLevel::kSignin);

  // Showing once the refresh token is loaded.
  EXPECT_TRUE(glanceables_controller()->IsShowing());

  // Open a browser window.
  CreateBrowser(ProfileManager::GetLastUsedProfile());

  // Glanceables should close because a window opened.
  EXPECT_FALSE(glanceables_controller()->IsShowing());
}

class GlanceablesScreenshotDeletionBrowserTest
    : public GlanceablesBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool SetUpUserDataDirectory() override {
    if (!temp_dir_.CreateUniqueTempDir()) {
      return false;
    }
    home_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        base::DIR_HOME, temp_dir_.GetPath());

    if (!GetParam()) {
      return true;
    }

    SkBitmap bitmap;
    bitmap.allocN32Pixels(400, 300);
    bitmap.eraseColor(SK_ColorYELLOW);
    std::vector<unsigned char> png_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &png_data);
    return base::WriteFile(ash::glanceables_util::GetSignoutScreenshotPath(),
                           png_data);
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride> home_dir_override_;
};

IN_PROC_BROWSER_TEST_P(GlanceablesScreenshotDeletionBrowserTest,
                       DeletesScreenshotAfterLogin) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  CreateAndStartUserSession();
  signin::MakePrimaryAccountAvailable(identity_manager(), kTestUserName,
                                      signin::ConsentLevel::kSignin);

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(
      base::PathExists(ash::glanceables_util::GetSignoutScreenshotPath()));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlanceablesScreenshotDeletionBrowserTest,
                         testing::Bool());
