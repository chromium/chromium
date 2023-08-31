// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/search/ntp_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

class NewTabPageA11yBrowserTest : public WebUIMochaBrowserTest {
 protected:
  NewTabPageA11yBrowserTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
    // Always run with accessibility, in order to catch assertions and crashes.
    command_line->AppendSwitch(switches::kForceRendererAccessibility);
  }
};

using NewTabPageAppA11yTest = NewTabPageA11yBrowserTest;

// TODO(crbug.com/1476647/) Reenable this test after finding a proper fix.
IN_PROC_BROWSER_TEST_F(NewTabPageAppA11yTest, DISABLED_Clicks) {
  RunTest("new_tab_page/app_test.js",
          "runMochaSuite('NewTabPageAppTest Clicks')");
}
