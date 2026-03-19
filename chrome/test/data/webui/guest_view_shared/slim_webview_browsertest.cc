// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_test.h"

class SlimWebviewBrowserTest : public WebUIMochaBrowserTest {
 public:
  // While these tests are independent of Glic, we use the Glic host as a
  // convenient way to get a WebUI instance that has all the sources and
  // bindings set up correctly.
  SlimWebviewBrowserTest() { set_test_loader_host(chrome::kChromeUIGlicHost); }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpDefaultCommandLine(command_line);
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    auto base_url = embedded_test_server()->base_url();
    // Reusing the GlicGuestURL switch to communicate the test server root URL
    // to the test via loadTimeData.
    command_line->AppendSwitchASCII(switches::kGlicGuestURL, base_url.spec());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kGlic};
};

IN_PROC_BROWSER_TEST_F(SlimWebviewBrowserTest, All) {
  RunTest("guest_view_shared/slim_webview_test.js", "mocha.run();");
}
