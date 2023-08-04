// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_settings/web_app_settings_navigation_throttle.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using AppSettingsTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(AppSettingsTest, App) {
  set_test_loader_host(chrome::kChromeUIWebAppSettingsHost);
  WebAppSettingsNavigationThrottle::DisableForTesting();
  RunTest("app_settings/app_test.js", "mocha.run()");
}
