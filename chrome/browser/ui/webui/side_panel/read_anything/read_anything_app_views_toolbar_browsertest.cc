// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test chrome/browser/resources/side_panel/read_anything/app.ts here. Add a new
// test script to chrome/test/data/webui/side_panel/read_anything and add a new
// pass the file name to RunTest in this file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/accessibility_features.h"

// TODO(crbug.com/1474951): Remove this file once the ReadAnythingWebUiToolbar
// flag is removed.
class ReadAnythingAppViewsToolbarTest : public InProcessBrowserTest {
 public:
  ReadAnythingAppViewsToolbarTest() {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kReadAnything, true},
         {features::kReadAnythingLocalSidePanel, true},
         {features::kReadAnythingWebUIToolbar, false}});
  }
  ~ReadAnythingAppViewsToolbarTest() override = default;
  ReadAnythingAppViewsToolbarTest(const ReadAnythingAppViewsToolbarTest&) =
      delete;
  ReadAnythingAppViewsToolbarTest& operator=(
      const ReadAnythingAppViewsToolbarTest&) = delete;

  testing::AssertionResult RunTest(const char* name) {
    std::string script;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      // Tests are located in
      // chrome/test/data/webui/side_panel/read_anything/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("webui")
                 .AppendASCII("side_panel")
                 .AppendASCII("read_anything")
                 .AppendASCII(name);

      // Read the test.
      if (!base::PathExists(path)) {
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      }
      base::ReadFileToString(path, &script);
      script = "'use strict';" + script;
    }

    // Run the test. Navigating to the URL will trigger the read anything
    // navigation throttle and open the side panel instead of loading read
    // anything in the main content area.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)));
    // Get the side panel entry registry.
    auto* side_panel_ui = SidePanelUI::GetSidePanelUIForBrowser(browser());
    auto* side_panel_web_contents =
        side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadAnything);
    if (!side_panel_web_contents) {
      return testing::AssertionFailure()
             << "Failed to navigate to get WebContents";
    }
    // Wait for the view to load before trying to run the test. This ensures
    // that chrome.readingMode is set.
    content::WaitForLoadStop(side_panel_web_contents);
    // Eval the JS test.
    bool result =
        content::EvalJs(side_panel_web_contents, script).ExtractBool();
    return result ? testing::AssertionSuccess()
                  : (testing::AssertionFailure() << "Check console output");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the WebUI toolbar is hidden when the feature flag is disabled.
IN_PROC_BROWSER_TEST_F(ReadAnythingAppViewsToolbarTest,
                       ReadAnythingToolbar_Hidden) {
  ASSERT_TRUE(RunTest("toolbar_hidden_without_flag.js"));
}
