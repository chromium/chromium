// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/unload_controller.h"

#include "ash/constants/web_app_id_constants.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {
constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

constexpr char kPreventCloseEnabledForCalculator[] = R"([
  {
    "manifest_id": "https://calculator.apps.chrome/",
    "run_on_os_login": "run_windowed",
    "prevent_close_after_run_on_os_login": true
  }
])";

constexpr char kCalculatorForceInstalled[] = R"([
  {
    "url": "https://calculator.apps.chrome/",
    "default_launch_container": "window"
  }
])";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

using UnloadControllerPreventCloseTest = PreventCloseTestBase;

IN_PROC_BROWSER_TEST_F(UnloadControllerPreventCloseTest,
                       PreventCloseEnforcedByPolicy) {
  const absl::Cleanup policy_cleanup = [this] {
    SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
  };

  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  UnloadController unload_controller(browser);
  EXPECT_EQ(kShouldPreventClose ? BrowserClosingStatus::kDeniedByPolicy
                                : BrowserClosingStatus::kPermitted,
            unload_controller.GetBrowserClosingStatus());
}

IN_PROC_BROWSER_TEST_F(UnloadControllerPreventCloseTest,
                       PreventCloseEnforcedByPolicyTabbedAppShallBeClosable) {
  const absl::Cleanup policy_cleanup = [this] {
    SetPolicies(/*web_app_settings=*/"[]", /*web_app_install_force_list=*/"[]");
  };

  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetPoliciesAndWaitUntilInstalled(web_app::kCalculatorAppId,
                                   kPreventCloseEnabledForCalculator,
                                   kCalculatorForceInstalled);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  UnloadController unload_controller(browser);
  EXPECT_EQ(BrowserClosingStatus::kPermitted,
            unload_controller.GetBrowserClosingStatus());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Browser tests for verifying `UnloadController` behavior for apps when locked
// (and not locked) for OnTask. Only relevant for non-web browser scenarios.
class UnloadControllerWithOnTaskTest : public InProcessBrowserTest {
 protected:
  webapps::AppId InstallMockApp() {
    return web_app::test::InstallDummyWebApp(
        browser()->profile(), /*app_name=*/"Mock app",
        /*app_url=*/GURL("https://www.example.com/"));
  }
};

IN_PROC_BROWSER_TEST_F(UnloadControllerWithOnTaskTest,
                       PreventCloseWhenLockedForOnTask) {
  // Install and launch app.
  webapps::AppId app_id = InstallMockApp();
  Browser* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  app_browser->SetLockedForOnTask(true);

  // Verify tab cannot be closed.
  content::WebContents* const active_web_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);
  UnloadController unload_controller(app_browser);
  EXPECT_FALSE(unload_controller.CanCloseContents(active_web_contents));
}

IN_PROC_BROWSER_TEST_F(UnloadControllerWithOnTaskTest,
                       AllowCloseWhenNotLockedForOnTask) {
  // Install and launch app.
  webapps::AppId app_id = InstallMockApp();
  Browser* const app_browser =
      web_app::LaunchWebAppBrowser(browser()->profile(), app_id);
  app_browser->SetLockedForOnTask(false);

  // Verify tab can be closed.
  content::WebContents* const active_web_contents =
      app_browser->tab_strip_model()->GetWebContentsAt(0);
  UnloadController unload_controller(app_browser);
  EXPECT_TRUE(unload_controller.CanCloseContents(active_web_contents));
}

#endif
