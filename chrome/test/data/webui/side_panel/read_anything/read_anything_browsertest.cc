// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/accessibility/accessibility_features.h"

class ReadAnythingMochaBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ReadAnythingMochaBrowserTest() {
    set_test_loader_host(chrome::kChromeUIUntrustedReadAnythingSidePanelHost);
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
  }

  void RunSidePanelTest(const std::string& file, const std::string& trigger) {
    auto* side_panel_ui = SidePanelUI::GetSidePanelUIForBrowser(browser());
    side_panel_ui->Show(SidePanelEntryId::kReadAnything);
    auto* web_contents =
        side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadAnything);
    ASSERT_TRUE(web_contents);

    content::WaitForLoadStop(web_contents);

    ASSERT_TRUE(RunTestOnWebContents(web_contents, file, trigger, true));
    side_panel_ui->Close();
  }

  base::test::ScopedFeatureList scoped_feature_list_{features::kReadAnything};
};

using ReadAnythingMochaTest = ReadAnythingMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, CheckmarkVisibleOnSelected) {
  RunSidePanelTest("side_panel/read_anything/checkmark_visible_on_selected.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceSelectionMenu) {
  RunSidePanelTest("side_panel/read_anything/voice_selection_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceLanguageUtil) {
  RunSidePanelTest("side_panel/read_anything/voice_language_util_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ReadAloudFlag) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_flag_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, WebUiToolbarFlag) {
  RunSidePanelTest("side_panel/read_anything/toolbar_flag_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, FontSize) {
  RunSidePanelTest("side_panel/read_anything/font_size_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, FontMenu) {
  RunSidePanelTest("side_panel/read_anything/font_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ColorMenu) {
  RunSidePanelTest("side_panel/read_anything/color_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LetterSpacing) {
  RunSidePanelTest("side_panel/read_anything/letter_spacing_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LineSpacing) {
  RunSidePanelTest("side_panel/read_anything/line_spacing_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, UpdateContent) {
  RunSidePanelTest("side_panel/read_anything/update_content_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, AppReceivesToolbarChanges) {
  RunSidePanelTest(
      "side_panel/read_anything/app_receives_toolbar_changes_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LanguageMenu) {
  RunSidePanelTest("side_panel/read_anything/language_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LinksToggle) {
  RunSidePanelTest("side_panel/read_anything/links_toggle_button_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, PlayPause) {
  RunSidePanelTest("side_panel/read_anything/play_pause_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, HighlightToggle) {
  RunSidePanelTest("side_panel/read_anything/highlight_toggle_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, NextPrevious) {
  RunSidePanelTest("side_panel/read_anything/next_previous_granularity_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, RateSelection) {
  RunSidePanelTest("side_panel/read_anything/rate_selection_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, UpdateContentSelection) {
  RunSidePanelTest("side_panel/read_anything/update_content_selection.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, FakeTreeBuilderTest) {
  RunSidePanelTest("side_panel/read_anything/fake_tree_builder_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest,
                       UpdateContentSelectionWithHighlights) {
  RunSidePanelTest(
      "side_panel/read_anything/update_content_selection_with_highlights.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LanguageChanged) {
  RunSidePanelTest("side_panel/read_anything/language_change_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Prefs) {
  RunSidePanelTest("side_panel/read_anything/prefs_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Speech) {
  RunSidePanelTest("side_panel/read_anything/speech_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, UpdateVoicePack) {
  RunSidePanelTest("side_panel/read_anything/update_voice_pack_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ToolbarOverflow) {
  RunSidePanelTest("side_panel/read_anything/toolbar_overflow_test.js",
                   "mocha.run()");
}

// Integration tests that need the actual Read Aloud flag enabled because they
// use the full C++ pipeline
class ReadAnythingReadAloudMochaTest : public ReadAnythingMochaBrowserTest {
 protected:
  ReadAnythingReadAloudMochaTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kReadAnythingReadAloud);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       LinksToggledIntegration) {
  RunSidePanelTest("side_panel/read_anything/links_toggled_integration.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       SpeechUsesMaxTextLength) {
  RunSidePanelTest("side_panel/read_anything/speech_uses_max_text_length.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       ReadAloud_UpdateContentSelection) {
  RunSidePanelTest(
      "side_panel/read_anything/read_aloud_update_content_selection.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       ReadAloud_UpdateContentSelectionPDF) {
  RunSidePanelTest(
      "side_panel/read_anything/read_aloud_update_content_selection_pdf.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest, ReadAloudHighlight) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_highlighting_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       WordBoundariesUsedForSpeech) {
  RunSidePanelTest(
      "side_panel/read_anything/word_boundaries_used_for_speech.js",
      "mocha.run()");
}

// Integration tests that need the actual Read Aloud flag enabled and the word
// highlighting flag because they use the full C++ pipeline
class ReadAnythingReadAloudWordHighlightingMochaTest
    : public ReadAnythingMochaBrowserTest {
 protected:
  ReadAnythingReadAloudWordHighlightingMochaTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud,
         features::kReadAnythingReadAloudAutomaticWordHighlighting},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudWordHighlightingMochaTest,
                       WordHighlighting) {
  RunSidePanelTest("side_panel/read_anything/word_highlighting.js",
                   "mocha.run()");
}
