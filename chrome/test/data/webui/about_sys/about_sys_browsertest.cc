// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class SystemInfoBrowserTest : public WebUIMochaBrowserTest {
 protected:
  SystemInfoBrowserTest() {
    set_test_loader_host(chrome::kChromeUISystemInfoHost);
  }
};

IN_PROC_BROWSER_TEST_F(SystemInfoBrowserTest, AboutSystem) {
  RunTest("about_sys/about_sys_test.js", "runMochaSuite('AboutSystemTest')");
}

IN_PROC_BROWSER_TEST_F(SystemInfoBrowserTest, FeedbackSysInfo) {
  // Test feedback system info page (chrome://system?showFeedbackInfo=true).
  set_test_loader_host(std::string(chrome::kChromeUISystemInfoHost) +
                       "?showFeedbackInfo=true");
  RunTestWithoutTestLoader("about_sys/about_sys_test.js",
                           "runMochaSuite('FeedbackSysInfoTest')");
}

IN_PROC_BROWSER_TEST_F(SystemInfoBrowserTest, KeyValuePairViewer) {
  RunTest("about_sys/key_value_pair_viewer_test.js", "mocha.run()");
}
