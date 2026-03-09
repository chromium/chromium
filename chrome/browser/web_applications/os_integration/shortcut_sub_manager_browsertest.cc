// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/profile_destruction_waiter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

namespace web_app {

class ShortcutSubManagerBrowserTest : public WebAppBrowserTestBase {
 public:
  ShortcutSubManagerBrowserTest() = default;
  ~ShortcutSubManagerBrowserTest() override = default;
};

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC));

void FlushShortcutTasks() {
  {
    base::RunLoop loop;
    internals::GetShortcutIOTaskRunner()->PostTask(FROM_HERE,
                                                   loop.QuitClosure());
    loop.Run();
  }
  {
    base::RunLoop loop;
    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, loop.QuitClosure());
    loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(ShortcutSubManagerBrowserTest,
                       CleanUpEphemeralProfileDeletesShortcuts) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  ProfileManager* profile_manager = g_browser_process->profile_manager();

  base::test::TestFuture<Profile*> profile_future;
  ProfileManager::CreateMultiProfileAsync(
      u"A Profile", /*icon_index=*/0, /*is_hidden=*/true,
      profile_future.GetCallback(), base::DoNothing());
  Profile* secondary = profile_future.Get();
  ASSERT_TRUE(secondary);

  web_app::test::WaitUntilWebAppProviderAndSubsystemsReady(
      WebAppProvider::GetForTest(secondary));

  // Install a web app in the secondary profile.
  auto web_app_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://example.com/"));
  web_app_info->title = u"A Web App";
  web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
  webapps::AppId app_id =
      web_app::test::InstallWebApp(secondary, std::move(web_app_info));

  FlushShortcutTasks();

  // Enable run-on-OS-login so we can verify it is also cleaned up.
  base::test::TestFuture<void> rool_future;
  WebAppProvider::GetForTest(secondary)->scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, rool_future.GetCallback());
  ASSERT_TRUE(rool_future.Wait());

  // Verify the shortcut was created via the real installation path.
  EXPECT_TRUE(os_integration_override().IsShortcutCreated(secondary, app_id,
                                                          "A Web App"));
  EXPECT_TRUE(os_integration_override().IsRunOnOsLoginEnabled(secondary, app_id,
                                                              "A Web App"));

#if BUILDFLAG(IS_WIN)
  base::FilePath desktop_shortcut_path =
      os_integration_override().GetShortcutPath(
          secondary, os_integration_override().desktop(), app_id, "A Web App");
  base::FilePath shortcut_path = os_integration_override().GetShortcutPath(
      secondary, os_integration_override().application_menu(), app_id,
      "A Web App");
  base::FilePath startup_shortcut_path =
      os_integration_override().GetShortcutPath(
          secondary, os_integration_override().startup(), app_id, "A Web App");
  EXPECT_FALSE(desktop_shortcut_path.empty());
  EXPECT_TRUE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(shortcut_path.empty());
  EXPECT_TRUE(base::PathExists(shortcut_path));
  EXPECT_FALSE(startup_shortcut_path.empty());
  EXPECT_TRUE(base::PathExists(startup_shortcut_path));
#elif BUILDFLAG(IS_MAC)
  base::FilePath shortcut_path = os_integration_override().GetShortcutPath(
      secondary, os_integration_override().chrome_apps_folder(), app_id,
      "A Web App");
  EXPECT_FALSE(shortcut_path.empty());
  EXPECT_TRUE(base::PathExists(shortcut_path));
#elif BUILDFLAG(IS_LINUX)
  base::FilePath shortcut_path = os_integration_override().GetShortcutPath(
      secondary, os_integration_override().desktop(), app_id, "A Web App");
  EXPECT_FALSE(shortcut_path.empty());
  EXPECT_TRUE(base::PathExists(shortcut_path));
#endif

  ProfileDestructionWaiter destruction_waiter(secondary);
  profile_manager->ClearFirstBrowserWindowKeepAlive(secondary);
  destruction_waiter.Wait();

  profile_manager->GetDeleteProfileHelper().CleanUpEphemeralProfiles();

  FlushShortcutTasks();

  // Verify the shortcut(s) and run-on-OS-login entry were deleted.
  EXPECT_FALSE(base::PathExists(shortcut_path));
#if BUILDFLAG(IS_WIN)
  EXPECT_FALSE(base::PathExists(desktop_shortcut_path));
  EXPECT_FALSE(base::PathExists(startup_shortcut_path));
#elif BUILDFLAG(IS_MAC)
  EXPECT_FALSE(os_integration_override().IsRunOnOsLoginEnabled(
      secondary, app_id, "A Web App"));
#endif
}

}  // namespace web_app
