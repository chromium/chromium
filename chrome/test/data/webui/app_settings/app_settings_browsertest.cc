// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class AppSettingsTest : public WebUIMochaBrowserTest {
 protected:
  AppSettingsTest() {
    set_test_loader_host(chrome::kChromeUIWebAppSettingsHost);
  }
};

IN_PROC_BROWSER_TEST_F(AppSettingsTest, App) {
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AppSettingsTest, PermissionItem) {
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/permission_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AppSettingsTest, FileHandlingItem) {
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/file_handling_item_test.js", "mocha.run()");
}

// TODO(crbug.com/348992509): Re-enable the test.
IN_PROC_BROWSER_TEST_F(AppSettingsTest, DISABLED_SupportedLinksItem) {
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/supported_links_item_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AppSettingsTest, UninstallButton) {
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/uninstall_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(AppSettingsTest, WindowModeItem) {
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/window_mode_item_test.js", "mocha.run()");
}
