// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppGuestSessionBrowserTest : public WebAppBrowserTestBase {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
};

// Test that the OS Settings app launches successfully.
IN_PROC_BROWSER_TEST_F(WebAppGuestSessionBrowserTest, LaunchOsSettings) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  Profile* profile = browser()->profile();
  apps::AppLaunchParams params(
      ash::kOsSettingsAppId, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, apps::LaunchSource::kFromTest);

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(profile);
  base::test::TestFuture<base::WeakPtr<Browser>,
                         base::WeakPtr<content::WebContents>,
                         apps::LaunchContainer>
      future;
  provider->scheduler().LaunchAppWithCustomParams(std::move(params),
                                                  future.GetCallback());
  auto* web_contents = future.template Get<1>().get();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(GURL(chrome::kChromeUIOSSettingsURL),
            web_contents->GetVisibleURL());
}

}  // namespace web_app
