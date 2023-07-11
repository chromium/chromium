// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/with_crosapi_param.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
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

using web_app::test::CrosapiParam;
using web_app::test::WithCrosapiParam;

namespace web_app {

class WebAppGuestSessionBrowserTest : public WebAppControllerBrowserTest,
                                      public WithCrosapiParam {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }

  void SetUpOnMainThread() override {
    if (browser() == nullptr) {
      // Create a new Ash browser window so test code using browser() can work
      // even when Lacros is the only browser.
      // TODO(crbug.com/1450158): Remove uses of browser() from such tests.
      chrome::NewEmptyWindow(ProfileManager::GetActiveUserProfile());
      SelectFirstBrowser();
    }
    WebAppControllerBrowserTest::SetUpOnMainThread();
    VerifyLacrosStatus();
  }
};

// Test that the OS Settings app launches successfully.
IN_PROC_BROWSER_TEST_P(WebAppGuestSessionBrowserTest, LaunchOsSettings) {
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  Profile* profile = browser()->profile();
  apps::AppLaunchParams params(
      web_app::kOsSettingsAppId, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, apps::LaunchSource::kFromTest);

  content::WebContents* contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParamsForTesting(std::move(params));
  EXPECT_EQ(GURL(chrome::kChromeUIOSSettingsURL), contents->GetVisibleURL());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppGuestSessionBrowserTest,
                         ::testing::Values(CrosapiParam::kDisabled,
                                           CrosapiParam::kEnabled),
                         WithCrosapiParam::ParamToString);

}  // namespace web_app
