// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
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
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud,
         features::kReadAloudLanguagePackDownloading},
        {});
  }

  void RunSidePanelTest(const std::string& file, const std::string& trigger) {
    auto* side_panel_ui = browser()->GetFeatures().side_panel_ui();
    side_panel_ui->Show(SidePanelEntryId::kReadAnything);
    auto* web_contents =
        side_panel_ui->GetWebContentsForTest(SidePanelEntryId::kReadAnything);
    ASSERT_TRUE(web_contents);

    content::WaitForLoadStop(web_contents);

    ASSERT_TRUE(RunTestOnWebContents(web_contents, file, trigger, true));
    side_panel_ui->Close();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using ReadAnythingMochaTest = ReadAnythingMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, CheckmarkVisibleOnSelected) {
  RunSidePanelTest(
      "side_panel/read_anything/checkmark_visible_on_selected_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Common) {
  RunSidePanelTest("side_panel/read_anything/common_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Logger) {
  RunSidePanelTest("side_panel/read_anything/read_anything_logger_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LoadingScreen) {
  RunSidePanelTest("side_panel/read_anything/loading_screen_test.js",
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

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceNotificationManager) {
  RunSidePanelTest(
      "side_panel/read_anything/voice_notification_manager_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ReadAloudFlag) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_flag_test.js",
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

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Toolbar) {
  RunSidePanelTest("side_panel/read_anything/toolbar_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, AppStyleUpdater) {
  RunSidePanelTest("side_panel/read_anything/app_style_updater_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LanguageMenu) {
  RunSidePanelTest("side_panel/read_anything/language_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LanguageToast) {
  RunSidePanelTest("side_panel/read_anything/language_toast_test.js",
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

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, NextPrevious) {
  RunSidePanelTest("side_panel/read_anything/next_previous_granularity_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, RateSelection) {
  RunSidePanelTest("side_panel/read_anything/rate_selection_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, FakeTreeBuilderTest) {
  RunSidePanelTest("side_panel/read_anything/fake_tree_builder_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest,
                       UpdateContentSelectionWithHighlights) {
  RunSidePanelTest(
      "side_panel/read_anything/"
      "update_content_selection_with_highlights_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LanguageChanged) {
  RunSidePanelTest("side_panel/read_anything/language_change_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceSelection) {
  RunSidePanelTest("side_panel/read_anything/voice_selection_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Prefs) {
  RunSidePanelTest("side_panel/read_anything/prefs_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_UpdateVoicePack DISABLED_UpdateVoicePack
#else
#define MAYBE_UpdateVoicePack UpdateVoicePack
#endif
IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, MAYBE_UpdateVoicePack) {
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

// TODO(crbug.com/368057422): This test is flaky on Mac. Fix and re-enable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_LinksToggledIntegration DISABLED_LinksToggledIntegration
#else
#define MAYBE_LinksToggledIntegration LinksToggledIntegration
#endif
IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       MAYBE_LinksToggledIntegration) {
  RunSidePanelTest("side_panel/read_anything/links_toggled_integration_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       SpeechUsesMaxTextLength) {
  RunSidePanelTest(
      "side_panel/read_anything/speech_uses_max_text_length_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       ReadAloud_UpdateContentSelection) {
  RunSidePanelTest(
      "side_panel/read_anything/read_aloud_update_content_selection_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       ReadAloud_UpdateContentSelectionPDF) {
  RunSidePanelTest(
      "side_panel/read_anything/"
      "read_aloud_update_content_selection_pdf_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest, ReadAloudHighlight) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_highlighting_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       WordBoundariesUsedForSpeech) {
  RunSidePanelTest("side_panel/read_anything/word_boundaries_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest, Speech) {
  RunSidePanelTest("side_panel/read_anything/speech_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest, UpdateContentSelection) {
  RunSidePanelTest("side_panel/read_anything/update_content_selection_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudMochaTest,
                       UpdateContentIntegration) {
  RunSidePanelTest(
      "side_panel/read_anything/update_content_integration_test.js",
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
        {features::kReadAnythingReadAloudPhraseHighlighting});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudWordHighlightingMochaTest,
                       WordHighlighting) {
  RunSidePanelTest("side_panel/read_anything/word_highlighting_test.js",
                   "mocha.run()");
}

// TODO(b/301131238): Remove this test once the word highlighting flag is
// removed.
// Integration tests that need the actual Read Aloud flag enabled and the word
// highlighting flag because they use the full C++ pipeline
class ReadAnythingReadAloudWordHighlightingDisabledMochaTest
    : public ReadAnythingMochaBrowserTest {
 protected:
  ReadAnythingReadAloudWordHighlightingDisabledMochaTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud},
        {features::kReadAnythingReadAloudAutomaticWordHighlighting});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudWordHighlightingDisabledMochaTest,
                       WordHighlighting) {
  RunSidePanelTest(
      "side_panel/read_anything/word_highlighting_disabled_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudWordHighlightingDisabledMochaTest,
                       HighlightToggle) {
  RunSidePanelTest("side_panel/read_anything/highlight_toggle_test.js",
                   "mocha.run()");
}

class ReadAnythingReadAloudPhraseHighlightingMochaTest
    : public ReadAnythingMochaBrowserTest {
 protected:
  ReadAnythingReadAloudPhraseHighlightingMochaTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud,
         features::kReadAnythingReadAloudAutomaticWordHighlighting,
         features::kReadAnythingReadAloudPhraseHighlighting},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudPhraseHighlightingMochaTest,
                       HighlightMenu) {
  RunSidePanelTest("side_panel/read_anything/highlight_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudPhraseHighlightingMochaTest,
                       PhraseHighlighting) {
  RunSidePanelTest("side_panel/read_anything/phrase_highlighting_test.js",
                   "mocha.run()");
}
