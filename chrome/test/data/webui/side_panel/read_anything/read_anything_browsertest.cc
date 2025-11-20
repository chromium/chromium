// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

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
         features::kReadAnythingImagesViaAlgorithm},
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
    side_panel_ui->Close(SidePanelEntry::PanelType::kContent);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using ReadAnythingMochaTest = ReadAnythingMochaBrowserTest;

class ReadAnythingMochaParameterizedTest
    : public ReadAnythingMochaBrowserTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ReadAnythingMochaParameterizedTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        features::kReadAnythingReadAloud,
        features::kReadAnythingImagesViaAlgorithm};
    if (IsTsSegmentationEnabled()) {
      enabled_features.push_back(
          features::kReadAnythingReadAloudTSTextSegmentation);
    }
    scoped_feature_list_.InitWithFeatures(
        enabled_features, {features::kReadAnythingReadAloudPhraseHighlighting,
                           features::kReadAnythingDocsIntegration});
  }

  bool IsTsSegmentationEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, Speech) {
  RunSidePanelTest("side_panel/read_anything/speech_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       SpeechPresentationRules) {
  RunSidePanelTest("side_panel/read_anything/speech_presentation_rules_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, NodeStore) {
  RunSidePanelTest("side_panel/read_anything/node_store_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       SpeechControllerContent) {
  RunSidePanelTest("side_panel/read_anything/speech_controller_content_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, HighlightMenu) {
  RunSidePanelTest("side_panel/read_anything/highlight_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, TextSegmenter) {
  RunSidePanelTest("side_panel/read_anything/text_segmenter_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, SpeechModel) {
  RunSidePanelTest("side_panel/read_anything/speech_model_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, ContentController) {
  RunSidePanelTest("side_panel/read_anything/content_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, WordBoundaries) {
  RunSidePanelTest("side_panel/read_anything/word_boundaries_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       WordBoundariesUsedForSpeech) {
  RunSidePanelTest("side_panel/read_anything/word_boundaries_speech_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       UpdateContentIntegration) {
  RunSidePanelTest(
      "side_panel/read_anything/update_content_integration_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, Highlighter) {
  RunSidePanelTest("side_panel/read_anything/highlighter_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       VoiceLanguageController) {
  RunSidePanelTest("side_panel/read_anything/voice_language_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, VoiceLanguageModel) {
  RunSidePanelTest("side_panel/read_anything/voice_language_model_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       SelectionController) {
  RunSidePanelTest("side_panel/read_anything/selection_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, SpeechController) {
  RunSidePanelTest("side_panel/read_anything/speech_controller_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, Common) {
  RunSidePanelTest("side_panel/read_anything/common_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, Logger) {
  RunSidePanelTest("side_panel/read_anything/read_anything_logger_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, AppContent) {
  RunSidePanelTest("side_panel/read_anything/app_content_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, VoiceSelectionMenu) {
  RunSidePanelTest("side_panel/read_anything/voice_selection_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, VoiceLanguageUtil) {
  RunSidePanelTest(
      "side_panel/read_anything/voice_language_conversions_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, KeyboardUtil) {
  RunSidePanelTest("side_panel/read_anything/keyboard_util_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       VoiceNotificationManager) {
  RunSidePanelTest(
      "side_panel/read_anything/voice_notification_manager_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, FontSize) {
  RunSidePanelTest("side_panel/read_anything/font_size_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, FontMenu) {
  RunSidePanelTest("side_panel/read_anything/font_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, FontSelect) {
  RunSidePanelTest("side_panel/read_anything/font_select_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, ColorMenu) {
  RunSidePanelTest("side_panel/read_anything/color_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, LetterSpacing) {
  RunSidePanelTest("side_panel/read_anything/letter_spacing_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, LineSpacing) {
  RunSidePanelTest("side_panel/read_anything/line_spacing_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, Movement) {
  RunSidePanelTest("side_panel/read_anything/movement_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, Toolbar) {
  RunSidePanelTest("side_panel/read_anything/toolbar_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       AppReceivesToolbarChanges) {
  RunSidePanelTest(
      "side_panel/read_anything/app_receives_toolbar_changes_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, AppStyleUpdater) {
  RunSidePanelTest("side_panel/read_anything/app_style_updater_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, LanguageMenu) {
  RunSidePanelTest("side_panel/read_anything/language_menu_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, LanguageToast) {
  RunSidePanelTest("side_panel/read_anything/language_toast_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, RateMenu) {
  RunSidePanelTest("side_panel/read_anything/rate_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, ToolbarOverflow) {
  RunSidePanelTest("side_panel/read_anything/toolbar_overflow_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest,
                       SpeechUsesMaxTextLength) {
  RunSidePanelTest(
      "side_panel/read_anything/speech_uses_max_text_length_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ReadAnythingMochaParameterizedTest, ReadAloudHighlight) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_highlighting_test.js",
                   "mocha.run()");
}

INSTANTIATE_TEST_SUITE_P(
    ReadAnythingMochaParameterized,
    ReadAnythingMochaParameterizedTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<
        ReadAnythingMochaParameterizedTest::ParamType>& info) {
      return info.param ? "WithTsSegmentation" : "WithoutTsSegmentation";
    });

class ReadAnythingReadAloudTsSegmentationMochaTest
    : public ReadAnythingMochaBrowserTest {
 protected:
  ReadAnythingReadAloudTsSegmentationMochaTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud,
         features::kReadAnythingReadAloudTSTextSegmentation},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudTsSegmentationMochaTest,
                       ReadAloudNodeStore) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_node_store_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudTsSegmentationMochaTest,
                       DomReadAloudNode) {
  RunSidePanelTest("side_panel/read_anything/dom_read_aloud_node_test.js",
                   "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ReadAnythingReadAloudTsSegmentationMochaTest,
                       ReadAloudModel) {
  RunSidePanelTest("side_panel/read_anything/read_aloud_model_test.js",
                   "mocha.run()");
}
