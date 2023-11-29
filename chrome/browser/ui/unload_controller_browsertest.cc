// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/prevent_close_test_base.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
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

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kShouldPreventClose = true;
#else
constexpr bool kShouldPreventClose = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

using UnloadControllerPreventCloseTest = PreventCloseTestBase;

IN_PROC_BROWSER_TEST_F(UnloadControllerPreventCloseTest,
                       PreventCloseEnforedByPolicy) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetWebAppSettings(kPreventCloseEnabledForCalculator);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/true);
  ASSERT_TRUE(browser);

  UnloadController unload_controller(browser);
  EXPECT_NE(kShouldPreventClose, unload_controller.ShouldCloseWindow());

  if (kShouldPreventClose) {
    ClearWebAppSettings();
    EXPECT_EQ(true, unload_controller.ShouldCloseWindow());
  }
}

IN_PROC_BROWSER_TEST_F(UnloadControllerPreventCloseTest,
                       PreventCloseEnforedByPolicyTabbedAppShallBeClosable) {
  InstallPWA(GURL(kCalculatorAppUrl), web_app::kCalculatorAppId);
  SetWebAppSettings(kPreventCloseEnabledForCalculator);

  Browser* const browser =
      LaunchPWA(web_app::kCalculatorAppId, /*launch_in_window=*/false);
  ASSERT_TRUE(browser);

  UnloadController unload_controller(browser);
  EXPECT_TRUE(unload_controller.ShouldCloseWindow());
}
