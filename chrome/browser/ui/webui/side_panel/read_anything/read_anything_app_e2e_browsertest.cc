// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingAppE2ETest : public InProcessBrowserTest {
 public:
  ReadAnythingAppE2ETest() = default;
  ~ReadAnythingAppE2ETest() override = default;
  ReadAnythingAppE2ETest(const ReadAnythingAppE2ETest&) = delete;
  ReadAnythingAppE2ETest& operator=(const ReadAnythingAppE2ETest&) = delete;

  testing::AssertionResult RunTest(const char* main_content_name,
                                   const char* expect_ra_content_name) {
    std::string executor_script;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      // Tests are located in
      // chrome/test/data/webui/side_panel/read_anything/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("webui")
                 .AppendASCII("side_panel")
                 .AppendASCII("read_anything")
                 .AppendASCII("e2e_test_executor.js");

      // Read the test.
      if (!base::PathExists(path)) {
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      }
      base::ReadFileToString(path, &executor_script);
    }

    std::string main_content_html;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      // HTML Test files are located in
      // chrome/test/data/webui/side_panel/read_anything/html/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("webui")
                 .AppendASCII("side_panel")
                 .AppendASCII("read_anything")
                 .AppendASCII("html")
                 .AppendASCII(main_content_name);

      // Read the test.
      if (!base::PathExists(path)) {
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      }
      base::ReadFileToString(path, &main_content_html);
      main_content_html = "data:text/html, " + main_content_html;
    }

    std::string expected_ra_content_html;
    {
      std::string loaded;
      base::ScopedAllowBlockingForTesting allow_blocking;
      // HTML Test files are located in
      // chrome/test/data/webui/side_panel/read_anything/html/$(name).
      base::FilePath path;
      base::PathService::Get(chrome::DIR_TEST_DATA, &path);
      path = path.AppendASCII("webui")
                 .AppendASCII("side_panel")
                 .AppendASCII("read_anything")
                 .AppendASCII("html")
                 .AppendASCII(expect_ra_content_name);

      // Read the test.
      if (!base::PathExists(path)) {
        return testing::AssertionFailure() << "Couldn't find " << path.value();
      }
      base::ReadFileToString(path, &expected_ra_content_html);
      // Remove newlines and indentation for comparison with innerHTML.
      expected_ra_content_html = base::JoinString(
          base::SplitString(expected_ra_content_html, "\n",
                            base::WhitespaceHandling::TRIM_WHITESPACE,
                            base::SplitResult::SPLIT_WANT_NONEMPTY),
          "");
    }

    EXPECT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(main_content_html)));

    // Run the test. Navigating to the URL will trigger the read anything
    // navigation throttle and open the side panel instead of loading read
    // anything in the main content area.
    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIUntrustedReadAnythingSidePanelURL)));

    // Get the side panel entry registry.
    auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
    auto* side_panel_web_contents =
        side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadAnything);
    EXPECT_TRUE(side_panel_web_contents);
    // Wait for the view to load before trying to run the test. This ensures
    // that chrome.readingMode is set.
    content::WaitForLoadStop(side_panel_web_contents);

    std::string actual =
        content::EvalJs(side_panel_web_contents, executor_script)
            .ExtractString();

    EXPECT_EQ(actual, expected_ra_content_html);

    return testing::AssertionSuccess();
  }
};

IN_PROC_BROWSER_TEST_F(ReadAnythingAppE2ETest, DISABLED_Sample) {
  ASSERT_TRUE(RunTest("simple.html", "simple_expected.html"));
}
