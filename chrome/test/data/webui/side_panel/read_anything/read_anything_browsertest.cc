// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
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
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingImagesViaAlgorithm},
        {features::kReadAnythingReadAloudPhraseHighlighting,
         features::kReadAnythingDocsIntegration});
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

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Speech) {
  RunSidePanelTest("side_panel/read_anything/speech_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SpeechPresentationRules) {
  RunSidePanelTest("side_panel/read_anything/speech_presentation_rules_test.js",
                   "mocha.run()");
}

// TODO(crbug.com/502069860): Re-enable after fixing flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_NodeStore DISABLED_NodeStore
#else
#define MAYBE_NodeStore NodeStore
#endif
IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, MAYBE_NodeStore) {
  RunSidePanelTest("side_panel/read_anything/node_store_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SpeechControllerContent) {
  RunSidePanelTest("side_panel/read_anything/speech_controller_content_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, HighlightMenu) {
  RunSidePanelTest("side_panel/read_anything/highlight_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, TextSegmenter) {
  RunSidePanelTest("side_panel/read_anything/text_segmenter_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SpeechModel) {
  RunSidePanelTest("side_panel/read_anything/speech_model_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ContentController) {
  RunSidePanelTest("side_panel/read_anything/content_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, WordBoundaries) {
  RunSidePanelTest("side_panel/read_anything/word_boundaries_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, WordBoundariesUsedForSpeech) {
  RunSidePanelTest("side_panel/read_anything/word_boundaries_speech_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, UpdateContentIntegration) {
  RunSidePanelTest(
      "side_panel/read_anything/update_content_integration_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Highlighter) {
  RunSidePanelTest("side_panel/read_anything/highlighter_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceLanguageController) {
  RunSidePanelTest("side_panel/read_anything/voice_language_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceLanguageModel) {
  RunSidePanelTest("side_panel/read_anything/voice_language_model_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SelectionController) {
  RunSidePanelTest("side_panel/read_anything/selection_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SpeechController) {
  RunSidePanelTest("side_panel/read_anything/speech_controller_test.js",
                   "mocha.run()");
}

// TODO(crbug.com/502069860): Re-enable after fixing flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Common DISABLED_Common
#else
#define MAYBE_Common Common
#endif
IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, MAYBE_Common) {
  RunSidePanelTest("side_panel/read_anything/common_test.js", "mocha.run()");
}

// TODO(crbug.com/502069860): Re-enable after fixing flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_RectCalculations DISABLED_RectCalculations
#else
#define MAYBE_RectCalculations RectCalculations
#endif
IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, MAYBE_RectCalculations) {
  RunSidePanelTest("side_panel/read_anything/rect_calculations_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Logger) {
  RunSidePanelTest("side_panel/read_anything/read_anything_logger_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, AppContent) {
  RunSidePanelTest("side_panel/read_anything/app_content_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceSelectionMenu) {
  RunSidePanelTest("side_panel/read_anything/voice_selection_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceLanguageUtil) {
  RunSidePanelTest(
      "side_panel/read_anything/voice_language_conversions_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, KeyboardUtil) {
  RunSidePanelTest("side_panel/read_anything/keyboard_util_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, VoiceNotificationManager) {
  RunSidePanelTest(
      "side_panel/read_anything/voice_notification_manager_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, FontSize) {
  RunSidePanelTest("side_panel/read_anything/font_size_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, FontMenu) {
  RunSidePanelTest("side_panel/read_anything/font_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SimpleActionMenu) {
  RunSidePanelTest("side_panel/read_anything/simple_action_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, GroupedActionMenu) {
  RunSidePanelTest("side_panel/read_anything/grouped_action_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ColorMenu) {
  RunSidePanelTest("side_panel/read_anything/color_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LineFocusMenu) {
  RunSidePanelTest("side_panel/read_anything/line_focus_menu_test.js",
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

// TODO(crbug.com/501840500): It is flaky on all platforms.
IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, DISABLED_Movement) {
  RunSidePanelTest("side_panel/read_anything/movement_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, Toolbar) {
  RunSidePanelTest("side_panel/read_anything/toolbar_test.js", "mocha.run()");
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

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, RateMenu) {
  RunSidePanelTest("side_panel/read_anything/rate_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ToolbarOverflow) {
  RunSidePanelTest("side_panel/read_anything/toolbar_overflow_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, SpeechUsesMaxTextLength) {
  RunSidePanelTest(
      "side_panel/read_anything/speech_uses_max_text_length_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ReadAloudHighlight) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_highlighting_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LineFocusController) {
  RunSidePanelTest("side_panel/read_anything/line_focus_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, LineFocusStyleMode) {
  RunSidePanelTest("side_panel/read_anything/line_focus_style_mode_test.js",
                   "mocha.run()");
}

// TODO(crbug.com/502069860): Re-enable after fixing flakiness.
#if BUILDFLAG(IS_WIN)
#define MAYBE_LineFocusMoveMode DISABLED_LineFocusMoveMode
#else
#define MAYBE_LineFocusMoveMode LineFocusMoveMode
#endif
IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, MAYBE_LineFocusMoveMode) {
  RunSidePanelTest("side_panel/read_anything/line_focus_move_mode_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, DomQueries) {
  RunSidePanelTest("side_panel/read_anything/dom_queries_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ReadAloudNodeStore) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_node_store_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, DomReadAloudNode) {
  RunSidePanelTest("side_panel/read_anything/dom_read_aloud_node_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, ReadAloudModel) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_model_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, TtsVoiceFiltering) {
  RunSidePanelTest("side_panel/read_anything/tts_voice_filtering_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingMochaTest, WebSpeechTtsClient) {
  RunSidePanelTest("side_panel/read_anything/webspeech_tts_client_test.js",
                   "mocha.run()");
}

class ImmersiveReadAnythingMochaTest : public ReadAnythingMochaBrowserTest {
 protected:
  ImmersiveReadAnythingMochaTest() {
    scoped_feature_list_.InitWithFeatures({features::kImmersiveReadAnything},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ImmersiveReadAnythingMochaTest, PresentationMenu) {
  RunSidePanelTest("side_panel/read_anything/presentation_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ImmersiveReadAnythingMochaTest, SettingsMenu) {
  RunSidePanelTest("side_panel/read_anything/settings_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ImmersiveReadAnythingMochaTest, ToolbarSettingsMenu) {
  RunSidePanelTest("side_panel/read_anything/toolbar_settings_menu_test.js",
                   "mocha.run()");
}

class ImmersiveReadAnythingWithReadabilityMochaTest
    : public ReadAnythingMochaBrowserTest {
 protected:
  ImmersiveReadAnythingWithReadabilityMochaTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kImmersiveReadAnything,
         features::kReadAnythingWithReadability},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug.com/502274118): Flaky on some windows builders.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ReadabilityImageClassifier DISABLED_ReadabilityImageClassifier
#else
#define MAYBE_ReadabilityImageClassifier ReadabilityImageClassifier
#endif
IN_PROC_BROWSER_TEST_F(ImmersiveReadAnythingWithReadabilityMochaTest,
                       MAYBE_ReadabilityImageClassifier) {
  RunSidePanelTest(
      "side_panel/read_anything/readability_image_classifier_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ImmersiveReadAnythingWithReadabilityMochaTest,
                       ReadabilityContentProcessing) {
  RunSidePanelTest(
      "side_panel/read_anything/readability_content_processing_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ImmersiveReadAnythingWithReadabilityMochaTest,
                       ReadabilityAnchorsIntegration) {
  RunSidePanelTest(
      "side_panel/read_anything/readability_anchors_integration_test.js",
      "mocha.run()");
}
