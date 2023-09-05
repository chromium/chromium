// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test chrome/browser/resources/side_panel/read_anything/app.ts here. Add a new
// test script to chrome/test/data/webui/side_panel/read_anything and add a new
// pass the file name to RunTest in this file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingAppToolbarTest : public InProcessBrowserTest {
 public:
  ReadAnythingAppToolbarTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingWebUIToolbar}, {});
  }
  ~ReadAnythingAppToolbarTest() override = default;
  ReadAnythingAppToolbarTest(const ReadAnythingAppToolbarTest&) = delete;
  ReadAnythingAppToolbarTest& operator=(const ReadAnythingAppToolbarTest&) =
      delete;

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

    // Run the test.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)));
    content::RenderFrameHost* webui = browser()
                                          ->tab_strip_model()
                                          ->GetActiveWebContents()
                                          ->GetPrimaryMainFrame();
    if (!webui) {
      return testing::AssertionFailure() << "Failed to navigate to WebUI";
    }

    bool result = content::EvalJs(webui, script).ExtractBool();
    return result ? testing::AssertionSuccess()
                  : (testing::AssertionFailure() << "Check console output");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingAppToolbarTest,
                       ReadAnythingToolbar_Visible) {
  ASSERT_TRUE(RunTest("toolbar_visible_with_flag.js"));
}

// TODO(crbug.com/1474951): Remove this test once Read Aloud flag is removed.
IN_PROC_BROWSER_TEST_F(ReadAnythingAppToolbarTest, ReadAloud_Hidden) {
  ASSERT_TRUE(RunTest("toolbar_without_flag_hides_read_aloud.js"));
}
