// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using ChromeTimeTicksBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ChromeTimeTicksBrowserTest, All) {
  // Spawn the test from chrome://settings (or any WebUI surface) so that
  // the chrome.timeTicks API exists.
  set_test_loader_host(chrome::kChromeUISettingsHost);
  RunTest("chrome_timeticks_test.js", "mocha.run()");
}
