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

class ReadAnythingAppTest : public InProcessBrowserTest {
 public:
  ReadAnythingAppTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnything, features::kReadAnythingLocalSidePanel}, {});
  }
  ~ReadAnythingAppTest() override = default;
  ReadAnythingAppTest(const ReadAnythingAppTest&) = delete;
  ReadAnythingAppTest& operator=(const ReadAnythingAppTest&) = delete;

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

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateTheme_FontName) {
  ASSERT_TRUE(RunTest("update_theme_font_name.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, DISABLED_UpdateTheme_FontSize) {
  ASSERT_TRUE(RunTest("update_theme_font_size.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateTheme_ForegroundColor) {
  ASSERT_TRUE(RunTest("update_theme_foreground_color.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       DISABLED_UpdateTheme_BackgroundColor) {
  ASSERT_TRUE(RunTest("update_theme_background_color.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, DISABLED_UpdateTheme_LineSpacing) {
  ASSERT_TRUE(RunTest("update_theme_line_spacing.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       DISABLED_UpdateTheme_LetterSpacing) {
  ASSERT_TRUE(RunTest("update_theme_letter_spacing.js"));
}

// TODO(crbug.com/1442570): unflake and re-enable
IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       DISABLED_ConnectedCallback_ShowLoadingScreen) {
  ASSERT_TRUE(RunTest("connected_callback_show_loading_screen.js"));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingAppTest,
    OnSelectionChange_NothingSelectedOnLoadingScreenSelection) {
  ASSERT_TRUE(RunTest(
      "on_selection_change_nothing_selected_on_loading_screen_selection.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_HidesLoadingScreen) {
  ASSERT_TRUE(RunTest("update_content_hides_loading_screen.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_Paragraph) {
  ASSERT_TRUE(RunTest("update_content_paragraph.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_Language_ChildNodeDiffLang) {
  ASSERT_TRUE(RunTest("update_content_language_child_node_diff_lang.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_Language_ParentLangSet) {
  ASSERT_TRUE(RunTest("update_content_language_parent_lang_set.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_Heading) {
  ASSERT_TRUE(RunTest("update_content_heading.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_Link) {
  ASSERT_TRUE(RunTest("update_content_link.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_Link_BadInput) {
  ASSERT_TRUE(RunTest("update_content_link_bad_input.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_StaticText) {
  ASSERT_TRUE(RunTest("update_content_static_text.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_StaticText_BadInput) {
  ASSERT_TRUE(RunTest("update_content_static_text_bad_input.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_ClearContainer) {
  ASSERT_TRUE(RunTest("update_content_clear_container.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_Selection) {
  ASSERT_TRUE(RunTest("update_content_selection.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_Selection_Backwards) {
  ASSERT_TRUE(RunTest("update_content_selection_backwards.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_Selection_OutsideDistilledContent) {
  ASSERT_TRUE(RunTest("update_content_selection_outside_distilled_content.js"));
}

IN_PROC_BROWSER_TEST_F(
    ReadAnythingAppTest,
    UpdateContent_Selection_PartiallyOutsideDistilledContent) {
  ASSERT_TRUE(RunTest(
      "update_content_selection_partially_outside_distilled_content.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_Selection_Backwards_WithInlineText) {
  ASSERT_TRUE(
      RunTest("update_content_selection_backwards_with_inline_text.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_Selection_WithInlineText) {
  ASSERT_TRUE(RunTest("update_content_selection_with_inline_text.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_SetSelectedText) {
  ASSERT_TRUE(RunTest("update_content_set_selected_text.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_TextDirection) {
  ASSERT_TRUE(RunTest("update_content_text_direction.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_TextDirection_ParentNodeDiffDir) {
  ASSERT_TRUE(RunTest("update_content_text_direction_parent_node_diff_dir.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest,
                       UpdateContent_TextDirection_VerticalDir) {
  ASSERT_TRUE(RunTest("update_content_text_direction_vertical_dir.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_TextStyle_Overline) {
  ASSERT_TRUE(RunTest("update_content_text_style_overline.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_TextStyle_Bold) {
  ASSERT_TRUE(RunTest("update_content_text_style_bold.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_NoContentNodes) {
  ASSERT_TRUE(RunTest("update_content_no_content_nodes.js"));
}

IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, UpdateContent_InteractiveElement) {
  ASSERT_TRUE(RunTest("update_content_interactive_element.js"));
}

// TODO(crbug.com/1474951): Remove this test once the ReadAnythingWebUiToolbar
// flag is removed.
IN_PROC_BROWSER_TEST_F(ReadAnythingAppTest, ReadAnythingToolbar_Hidden) {
  ASSERT_TRUE(RunTest("toolbar_hidden_without_flag.js"));
}
