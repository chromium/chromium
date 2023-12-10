// Copyright 2023 The Chromium Authors
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
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingAppReadAloudTest : public InProcessBrowserTest {
 public:
  ReadAnythingAppReadAloudTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingWebUIToolbar,
         features::kReadAnythingReadAloud},
        {});
  }
  ~ReadAnythingAppReadAloudTest() override = default;
  ReadAnythingAppReadAloudTest(const ReadAnythingAppReadAloudTest&) = delete;
  ReadAnythingAppReadAloudTest& operator=(const ReadAnythingAppReadAloudTest&) =
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
      return testing::AssertionFailure() << "Failed to navigate to WebUI";
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

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest, ReadAloud_Visible) {
  ASSERT_TRUE(RunTest("read_aloud_visible_with_flag.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       ReadAloud_HighlightWhileReading) {
  ASSERT_TRUE(RunTest("read_aloud_highlight_while_reading.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       ReadAloud_GranularityVisibleWhenPlaying) {
  ASSERT_TRUE(RunTest("granularity_visible_when_playing.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       ReadAloud_GranularityHiddenWhenPaused) {
  ASSERT_TRUE(RunTest("granularity_hidden_when_paused.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       RateCallback_ChangesSpeechRate) {
  ASSERT_TRUE(RunTest("rate_callback_changes_speech_rate.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       PlayPauseCallback_PlaysAndPausesSpeech) {
  ASSERT_TRUE(RunTest("play_pause_callback_play_pause_speech.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       ReadAloud_GranularityChangesUpdatesHighlight) {
  ASSERT_TRUE(RunTest("read_aloud_highlight_with_granularity_changes.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       HighlightCallback_TogglesHighlight) {
  ASSERT_TRUE(RunTest("highlight_callback_toggles_highlight.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest, ReadAloud_FontSizeMenu) {
  ASSERT_TRUE(RunTest("font_size_menu_with_read_aloud.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest, ReadAloud_FontMenu) {
  ASSERT_TRUE(RunTest("font_menu_with_read_aloud.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppReadAloudTest,
                       ReadAloud_KeyboardForPlayPause) {
  ASSERT_TRUE(RunTest("k_plays_and_pauses.js"));
}
