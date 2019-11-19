// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/chromeos/extensions/default_web_app_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user_names.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

class WebAppGuestSessionBrowserTest : public InProcessBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
    command_line->AppendSwitchASCII(
        chromeos::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test that the OS Settings app launches successfully.
IN_PROC_BROWSER_TEST_F(WebAppGuestSessionBrowserTest, LaunchOsSettings) {
  auto& system_web_app_manager =
      WebAppProvider::Get(browser()->profile())->system_web_app_manager();
  system_web_app_manager.InstallSystemAppsForTesting();

  Profile* profile = browser()->profile();
  apps::AppLaunchParams params(
      chromeos::default_web_apps::kOsSettingsAppId,
      apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::mojom::AppLaunchSource::kSourceTest);

  content::WebContents* contents =
      apps::LaunchService::Get(profile)->OpenApplication(params);
  EXPECT_EQ(GURL(chrome::kChromeUIOSSettingsURL), contents->GetVisibleURL());
}

}  // namespace web_app
