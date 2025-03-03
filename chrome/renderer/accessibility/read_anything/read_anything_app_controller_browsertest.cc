// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_app_controller.h"

#include <cstddef>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/read_anything/read_anything_util.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_test_utils.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_frame.h"
#include "read_anything_test_utils.h"
#include "services/strings/grit/services_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

base::File GetInvalidModelFile() {
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("model_file.tflite");
  base::File file(file_path, (base::File::FLAG_CREATE | base::File::FLAG_READ |
                              base::File::FLAG_WRITE |
                              base::File::FLAG_CAN_DELETE_ON_CLOSE));
  EXPECT_EQ(5u, file.WriteAtCurrentPos(base::byte_span_from_cstring("12345")));
  return file;
}

base::File GetValidModelFile() {
  base::FilePath source_root_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir);
  base::FilePath model_file_path = source_root_dir.AppendASCII("chrome")
                                       .AppendASCII("test")
                                       .AppendASCII("data")
                                       .AppendASCII("accessibility")
                                       .AppendASCII("phrase_segmentation")
                                       .AppendASCII("model.tflite");
  base::File file(model_file_path,
                  (base::File::FLAG_OPEN | base::File::FLAG_READ));
  return file;
}

int kSecondsElapsedSincePageLoadForDataCollection = 30;
int kSecondsElapsedSinceTreeChangedForDataCollection = 30;

}  // namespace

class MockAXTreeDistiller : public AXTreeDistiller {
 public:
  explicit MockAXTreeDistiller(content::RenderFrame* render_frame)
      : AXTreeDistiller(render_frame, base::NullCallback()) {}
  MOCK_METHOD(void,
              Distill,
              (const ui::AXTree& tree,
               const ui::AXTreeUpdate& snapshot,
               const ukm::SourceId ukm_source_id),
              (override));
};

class MockReadAnythingUntrustedPageHandler
    : public read_anything::mojom::UntrustedPageHandler {
 public:
  MockReadAnythingUntrustedPageHandler() = default;

  MOCK_METHOD(void,
              GetDependencyParserModel,
              (GetDependencyParserModelCallback mojo_callback),
              (override));
  MOCK_METHOD(void,
              GetVoicePackInfo,
              (const std::string& language),
              (override));
  MOCK_METHOD(void,
              InstallVoicePack,
              (const std::string& language),
              (override));
  MOCK_METHOD(void, UninstallVoice, (const std::string& language), (override));
  MOCK_METHOD(void,
              OnLinkClicked,
              (const ui::AXTreeID& target_tree_id, ui::AXNodeID target_node_id),
              (override));
  MOCK_METHOD(void,
              ScrollToTargetNode,
              (const ui::AXTreeID& target_tree_id, ui::AXNodeID target_node_id),
              (override));
  MOCK_METHOD(void,
              OnSelectionChange,
              (const ui::AXTreeID& target_tree_id,
               ui::AXNodeID anchor_node_id,
               int anchor_offset,
               ui::AXNodeID focus_node_id,
               int focus_offset),
              (override));
  MOCK_METHOD(void, OnCollapseSelection, (), (override));
  MOCK_METHOD(void, OnScreenshotRequested, (), (override));
  MOCK_METHOD(void, OnCopy, (), (override));
  MOCK_METHOD(void,
              OnLineSpaceChange,
              (read_anything::mojom::LineSpacing line_spacing),
              (override));
  MOCK_METHOD(void,
              OnLetterSpaceChange,
              (read_anything::mojom::LetterSpacing letter_spacing),
              (override));
  MOCK_METHOD(void, OnFontChange, (const std::string& font), (override));
  MOCK_METHOD(void, OnFontSizeChange, (double font_size), (override));
  MOCK_METHOD(void, OnLinksEnabledChanged, (bool enabled), (override));
  MOCK_METHOD(void, OnImagesEnabledChanged, (bool enabled), (override));
  MOCK_METHOD(void, OnSpeechRateChange, (double rate), (override));
  MOCK_METHOD(void,
              OnVoiceChange,
              (const std::string& voice, const std::string& lang),
              (override));
  MOCK_METHOD(void,
              OnLanguagePrefChange,
              (const std::string& lang, bool enabled),
              (override));
  MOCK_METHOD(void,
              OnColorChange,
              (read_anything::mojom::Colors color),
              (override));
  MOCK_METHOD(void,
              OnHighlightGranularityChanged,
              (read_anything::mojom::HighlightGranularity color),
              (override));
  MOCK_METHOD(void,
              OnImageDataRequested,
              (const ::ui::AXTreeID& target_tree_id, int32_t target_node_id),
              (override));

  mojo::PendingRemote<read_anything::mojom::UntrustedPageHandler>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<read_anything::mojom::UntrustedPageHandler> receiver_{this};
};

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Mock;

class ReadAnythingAppControllerTest : public ChromeRenderViewTest {
 public:
  ReadAnythingAppControllerTest() = default;
  ~ReadAnythingAppControllerTest() override = default;
  ReadAnythingAppControllerTest(const ReadAnythingAppControllerTest&) = delete;
  ReadAnythingAppControllerTest& operator=(
      const ReadAnythingAppControllerTest&) = delete;

  const std::string DOCS_URL =
      "https://docs.google.com/document/d/"
      "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
      "edit?ouid=103677288878638916900&usp=docs_home&ths=true";

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
    controller_ = ReadAnythingAppController::Install(render_frame);

    // Set the page handler for testing.
    controller_->page_handler_.reset();
    controller_->page_handler_.Bind(page_handler_.BindNewPipeAndPassRemote());

    // Set distiller for testing.
    std::unique_ptr<AXTreeDistiller> distiller =
        std::make_unique<MockAXTreeDistiller>(render_frame);
    controller_->distiller_ = std::move(distiller);
    distiller_ =
        static_cast<MockAXTreeDistiller*>(controller_->distiller_.get());

    // Create a tree id.
    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();

    // Create simple AXTreeUpdate with a root node and 3 children.
    std::unique_ptr<ui::AXTreeUpdate> snapshot = test::CreateInitialUpdate();
    test::SetUpdateTreeID(snapshot.get(), tree_id_);

    // Send the snapshot to the controller and set its tree ID to be the active
    // tree ID. When the accessibility event is received and unserialized, the
    // controller will call distiller_->Distill().
    EXPECT_CALL(*distiller_, Distill).Times(1);
    AccessibilityEventReceived({*snapshot});
    controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                         false);
    controller().OnAXTreeDistilled(tree_id_, {});
    Mock::VerifyAndClearExpectations(distiller_);
  }

  ReadAnythingAppController& controller() { return *controller_; }
  ReadAnythingAppModel& model() { return controller_->model_; }
  ReadAloudAppModel& read_aloud_model() {
    return controller_->read_aloud_model_;
  }

  void SendBatchUpdates() {
    std::vector<ui::AXTreeUpdate> batch_updates;
    for (int i = 2; i < 5; i++) {
      ui::AXTreeUpdate update;
      test::SetUpdateTreeID(&update, tree_id_);
      ui::AXNodeData node =
          test::TextNode(/* id= */ i, u"Node " + base::NumberToString16(i));
      update.nodes = {node};
      batch_updates.push_back(update);
    }

    AccessibilityEventReceived(batch_updates);
  }

  std::vector<int> SendSimpleUpdateAndGetChildIds() {
    ui::AXTreeUpdate initial_update;
    test::SetUpdateTreeID(&initial_update, tree_id_);
    initial_update.root_id = 1;
    initial_update.nodes.resize(3);
    std::vector<int> child_ids;
    for (int i = 0; i < 3; i++) {
      int id = i + 2;
      child_ids.push_back(id);
      initial_update.nodes[i] = test::TextNodeWithTextFromId(id);
    }
    // No events we care about come about, so there's no distillation.
    EXPECT_CALL(*distiller_, Distill).Times(0);
    AccessibilityEventReceived({initial_update});
    EXPECT_EQ(u"234", controller().GetTextContent(1));
    Mock::VerifyAndClearExpectations(distiller_);
    return child_ids;
  }

  void AccessibilityEventReceived(
      const std::vector<ui::AXTreeUpdate>& updates,
      const std::vector<ui::AXEvent>& events = std::vector<ui::AXEvent>()) {
    controller().AccessibilityEventReceived(updates[0].tree_data.tree_id,
                                            updates, events);
  }

  std::vector<ui::AXNodeID> MoveToNextGranularityAndGetText() {
    controller().MovePositionToNextGranularity();
    return controller().GetCurrentText();
  }

  std::vector<ui::AXNodeID> MoveToPreviousGranularityAndGetText() {
    controller().MovePositionToPreviousGranularity();
    return controller().GetCurrentText();
  }

  ui::AXNodePosition::AXPositionInstance GetNextNodePosition(
      a11y::ReadAloudCurrentGranularity granularity =
          a11y::ReadAloudCurrentGranularity()) {
    return read_aloud_model().GetNextValidPositionFromCurrentPosition(
        granularity, model().is_pdf(), model().IsDocs(),
        &model().display_node_ids());
  }

  a11y::ReadAloudCurrentGranularity GetNextNodes() {
    return read_aloud_model().GetNextNodes(model().is_pdf(), model().IsDocs(),
                                           &model().display_node_ids());
  }

  void ProcessDisplayNodes(const std::vector<ui::AXNodeID>& content_node_ids) {
    model().Reset(content_node_ids);
    model().ComputeDisplayNodeIdsForDistilledTree();
  }

  void SendUpdateWithNodes(std::vector<ui::AXNodeData> nodes) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_id_);
    update.nodes = nodes;
    AccessibilityEventReceived({update});
  }

  void SendUpdateAndDistillNodes(std::vector<ui::AXNodeData> nodes) {
    SendUpdateWithNodes(nodes);

    std::vector<int> node_ids;
    for (const ui::AXNodeData& node : nodes) {
      node_ids.push_back(node.id);
    }

    controller().OnAXTreeDistilled(tree_id_, node_ids);
    controller().InitAXPositionWithNode(nodes[0].id);
  }

  void InitializeWithAndProcessNodes(std::vector<ui::AXNodeData> nodes) {
    SendUpdateWithNodes(nodes);

    std::vector<int> node_ids;
    for (const ui::AXNodeData& node : nodes) {
      node_ids.push_back(node.id);
    }

    ProcessDisplayNodes(node_ids);
    controller().InitAXPositionWithNode(nodes[0].id);
  }

  ui::AXTreeID tree_id_;
  raw_ptr<MockAXTreeDistiller, DanglingUntriaged> distiller_ = nullptr;
  testing::StrictMock<MockReadAnythingUntrustedPageHandler> page_handler_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // ReadAnythingAppController constructor and destructor are protected so it's
  // not accessible by std::make_unique.
  raw_ptr<ReadAnythingAppController, DanglingUntriaged> controller_ = nullptr;
};

TEST_F(ReadAnythingAppControllerTest, IsReadAloudEnabled) {
// Read Aloud is currently only enabled by default on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(controller().IsReadAloudEnabled());

#else
  EXPECT_FALSE(controller().IsReadAloudEnabled());

  scoped_feature_list_.InitAndEnableFeature(features::kReadAnythingReadAloud);
  EXPECT_TRUE(controller().IsReadAloudEnabled());
#endif  // IS_CHROMEOS_ASH
}

TEST_F(ReadAnythingAppControllerTest, OnLetterSpacingChange_ValidChange) {
  controller().OnLetterSpacingChange(2);
  EXPECT_CALL(page_handler_,
              OnLetterSpaceChange(read_anything::mojom::LetterSpacing::kWide))
      .Times(1);
  ASSERT_EQ(controller().LetterSpacing(), 2);
}

TEST_F(ReadAnythingAppControllerTest, OnLetterSpacingChange_InvalidChange) {
  controller().OnLetterSpacingChange(10);
  EXPECT_CALL(page_handler_, OnLetterSpaceChange).Times(0);
}

TEST_F(ReadAnythingAppControllerTest, OnLineSpacingChange_ValidChange) {
  controller().OnLineSpacingChange(3);
  EXPECT_CALL(page_handler_,
              OnLineSpaceChange(read_anything::mojom::LineSpacing::kVeryLoose))
      .Times(1);
  ASSERT_EQ(controller().LineSpacing(), 3);
}

TEST_F(ReadAnythingAppControllerTest, OnLineSpacingChange_InvalidChange) {
  controller().OnLineSpacingChange(10);
  EXPECT_CALL(page_handler_, OnLineSpaceChange).Times(0);
}

TEST_F(ReadAnythingAppControllerTest, OnThemeChange_ValidChange) {
  controller().OnThemeChange(3);
  EXPECT_CALL(page_handler_,
              OnColorChange(read_anything::mojom::Colors::kYellow))
      .Times(1);
  ASSERT_EQ(controller().ColorTheme(), 3);
}

TEST_F(ReadAnythingAppControllerTest, OnThemeChange_InvalidChange) {
  controller().OnThemeChange(10);
  EXPECT_CALL(page_handler_, OnColorChange).Times(0);
}

TEST_F(ReadAnythingAppControllerTest, OnFontChange_UpdatesFont) {
  std::string expected_font = "Roboto";

  controller().OnFontChange(expected_font);

  EXPECT_CALL(page_handler_, OnFontChange(expected_font)).Times(1);
  ASSERT_EQ(controller().FontName(), expected_font);
}

TEST_F(ReadAnythingAppControllerTest, GetValidatedFontName_FontWithQuotes) {
  std::string expected_font = "\"Lexend Deca\"";
  std::string actual_font = controller().GetValidatedFontName("Lexend Deca");
  ASSERT_EQ(actual_font, expected_font);
}

TEST_F(ReadAnythingAppControllerTest, GetValidatedFontName_FontWithoutQuotes) {
  std::string expected_font = "serif";
  std::string actual_font = controller().GetValidatedFontName("Serif");
  ASSERT_EQ(actual_font, expected_font);
}

TEST_F(ReadAnythingAppControllerTest, GetValidatedFontName_InvalidFont) {
  // All languages have the same default font.
  std::string expected_font = GetSupportedFonts("en").front();
  std::string actual_font =
      controller().GetValidatedFontName("not a real font");
  ASSERT_EQ(actual_font, expected_font);
}

TEST_F(ReadAnythingAppControllerTest, GetValidatedFontName_UnsupportedFont) {
  // All languages have the same default font.
  std::string expected_font = GetSupportedFonts("en").front();
  std::string actual_font =
      controller().GetValidatedFontName("Times New Roman");
  ASSERT_EQ(actual_font, expected_font);
}

TEST_F(ReadAnythingAppControllerTest, OnSpeechRateChange) {
  double expected_rate = 1.5;

  controller().OnSpeechRateChange(expected_rate);

  EXPECT_CALL(page_handler_, OnSpeechRateChange(expected_rate)).Times(1);
  ASSERT_EQ(read_aloud_model().speech_rate(), expected_rate);
}

TEST_F(ReadAnythingAppControllerTest, OnLanguagePrefChange) {
  std::string enabled_lang = "ja-jp";
  std::string disabled_lang = "en-us";

  controller().OnLanguagePrefChange(enabled_lang, true);
  controller().OnLanguagePrefChange(disabled_lang, true);
  controller().OnLanguagePrefChange(disabled_lang, false);

  EXPECT_CALL(page_handler_, OnLanguagePrefChange).Times(3);
  ASSERT_TRUE(base::Contains(read_aloud_model().languages_enabled_in_pref(),
                             enabled_lang));
  ASSERT_FALSE(base::Contains(read_aloud_model().languages_enabled_in_pref(),
                              disabled_lang));
}

TEST_F(ReadAnythingAppControllerTest, GetStoredVoice_ReturnsLatestVoice) {
  std::string current_lang = "it-IT";
  std::string current_voice = "Italian voice 3";
  std::string previous_voice = "Dutch voice 1";

  controller().SetLanguageForTesting(current_lang);
  controller().OnVoiceChange(previous_voice, current_lang);
  controller().OnVoiceChange(current_voice, current_lang);

  EXPECT_CALL(page_handler_, OnVoiceChange).Times(2);
  ASSERT_EQ(controller().GetStoredVoice(), current_voice);
}

TEST_F(ReadAnythingAppControllerTest,
       GetStoredVoice_ReturnsPreferredVoiceForLang) {
  std::string current_lang = "it-IT";
  std::string other_lang = "de-DE";
  std::string current_voice = "Italian voice 3";
  std::string previous_voice = "Dutch voice 1";

  controller().SetLanguageForTesting(current_lang);
  controller().OnVoiceChange(previous_voice, current_lang);
  controller().OnVoiceChange(current_voice, other_lang);

  EXPECT_CALL(page_handler_, OnVoiceChange).Times(2);

  // Even though the current language is Italian, the preferred voice for
  // Italian was selected as the Dutch voice, so this is the voice that should
  // be used.
  ASSERT_EQ(controller().GetStoredVoice(), previous_voice);
}

TEST_F(ReadAnythingAppControllerTest, GetStoredVoice_NoVoices_ReturnsEmpty) {
  scoped_feature_list_.InitWithFeatures({features::kReadAnythingReadAloud}, {});
  ASSERT_EQ(controller().GetStoredVoice(), "");
}

TEST_F(ReadAnythingAppControllerTest,
       GetStoredVoice_CurrentBaseLangStored_ReturnsExpectedVoice) {
  scoped_feature_list_.InitWithFeatures({features::kReadAnythingReadAloud}, {});
  std::string base_lang = "fr";
  std::string expected_voice_name = "French voice 1";

  controller().OnVoiceChange(expected_voice_name, base_lang);
  controller().SetLanguageForTesting(base_lang);

  EXPECT_CALL(page_handler_, OnVoiceChange).Times(1);
  ASSERT_EQ(controller().GetStoredVoice(), expected_voice_name);
}

TEST_F(ReadAnythingAppControllerTest,
       GetStoredVoice_CurrentFullLangStored_ReturnsExpectedVoice) {
  scoped_feature_list_.InitWithFeatures({features::kReadAnythingReadAloud}, {});
  std::string full_lang = "en-UK";
  std::string expected_voice_name = "British voice 45";

  controller().OnVoiceChange(expected_voice_name, full_lang);
  controller().SetLanguageForTesting(full_lang);

  EXPECT_CALL(page_handler_, OnVoiceChange).Times(1);
  ASSERT_EQ(controller().GetStoredVoice(), expected_voice_name);
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetStoredVoice_BaseLangStoredButCurrentLangIsFull_ReturnsStoredBaseLang) {
  scoped_feature_list_.InitWithFeatures({features::kReadAnythingReadAloud}, {});
  std::string base_lang = "zh";
  std::string full_lang = "zh-TW";
  std::string expected_voice_name = "Chinese voice";

  controller().OnVoiceChange(expected_voice_name, base_lang);
  controller().SetLanguageForTesting(full_lang);

  EXPECT_CALL(page_handler_, OnVoiceChange).Times(1);
  ASSERT_EQ(controller().GetStoredVoice(), expected_voice_name);
}

TEST_F(ReadAnythingAppControllerTest,
       GetStoredVoice_CurrentLangNotStored_ReturnsEmpty) {
  scoped_feature_list_.InitWithFeatures({features::kReadAnythingReadAloud}, {});
  std::string current_lang = "de-DE";
  std::string stored_lang = "it-IT";

  controller().OnVoiceChange("Italian voice 3", stored_lang);
  controller().SetLanguageForTesting(current_lang);

  EXPECT_CALL(page_handler_, OnVoiceChange).Times(1);
  ASSERT_EQ(controller().GetStoredVoice(), "");
}

TEST_F(ReadAnythingAppControllerTest, OnSettingsRestoredFromPrefs) {
  auto line_spacing = read_anything::mojom::LineSpacing::kVeryLoose;
  auto letter_spacing = read_anything::mojom::LetterSpacing::kVeryWide;
  std::string font_name = "Roboto";
  double font_size = 3.0;
  bool links_enabled = false;
  bool images_enabled = true;
  auto color = read_anything::mojom::Colors::kDefaultValue;
  int color_value = 0;
  double speech_rate = 1.5;
  std::string voice_value = "Italian voice 3";
  std::string language_value = "it";
  base::Value::Dict voices = base::Value::Dict();
  voices.Set(language_value, voice_value);
  base::Value::List languages_enabled_in_pref = base::Value::List();
  languages_enabled_in_pref.Append(language_value);
  auto highlight_granularity =
      read_anything::mojom::HighlightGranularity::kDefaultValue;
  int highlight_granularity_value = 0;

  controller().SetLanguageForTesting(language_value);

  controller().OnSettingsRestoredFromPrefs(
      line_spacing, letter_spacing, font_name, font_size, links_enabled,
      images_enabled, color, speech_rate, std::move(voices),
      std::move(languages_enabled_in_pref), highlight_granularity);

  EXPECT_EQ(static_cast<int>(line_spacing), controller().LineSpacing());
  EXPECT_EQ(static_cast<int>(letter_spacing), controller().LetterSpacing());
  EXPECT_EQ(font_name, controller().FontName());
  EXPECT_EQ(font_size, controller().FontSize());
  EXPECT_EQ(links_enabled, controller().LinksEnabled());
  EXPECT_EQ(color_value, controller().ColorTheme());
  EXPECT_EQ(speech_rate, read_aloud_model().speech_rate());
  EXPECT_EQ(voice_value, controller().GetStoredVoice());
  EXPECT_EQ(1u, controller().GetLanguagesEnabledInPref().size());
  EXPECT_EQ(language_value, controller().GetLanguagesEnabledInPref()[0]);
  EXPECT_EQ(highlight_granularity_value,
            read_aloud_model().highlight_granularity());
}

TEST_F(ReadAnythingAppControllerTest, RootIdIsSnapshotRootId) {
  controller().OnAXTreeDistilled(tree_id_, {1});
  EXPECT_EQ(1, controller().RootId());
  controller().OnAXTreeDistilled(tree_id_, {2});
  EXPECT_EQ(1, controller().RootId());
  controller().OnAXTreeDistilled(tree_id_, {3});
  EXPECT_EQ(1, controller().RootId());
  controller().OnAXTreeDistilled(tree_id_, {4});
  EXPECT_EQ(1, controller().RootId());
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_NoSelectionOrContentNodes) {
  ui::AXNodeData node;
  node.id = 3;
  node.role = ax::mojom::Role::kNone;
  SendUpdateWithNodes({node});
  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(0u, controller().GetChildren(1).size());
  EXPECT_EQ(0u, controller().GetChildren(2).size());
  EXPECT_EQ(0u, controller().GetChildren(3).size());
  EXPECT_EQ(0u, controller().GetChildren(4).size());
}

TEST_F(ReadAnythingAppControllerTest, GetChildren_WithContentNodes) {
  ui::AXNodeData node;
  node.id = 3;
  node.role = ax::mojom::Role::kNone;
  SendUpdateWithNodes({node});
  controller().OnAXTreeDistilled(tree_id_, {1, 2, 3, 4});
  EXPECT_EQ(2u, controller().GetChildren(1).size());
  EXPECT_EQ(0u, controller().GetChildren(2).size());
  EXPECT_EQ(0u, controller().GetChildren(3).size());
  EXPECT_EQ(0u, controller().GetChildren(4).size());

  EXPECT_EQ(2, controller().GetChildren(1)[0]);
  EXPECT_EQ(4, controller().GetChildren(1)[1]);
}

TEST_F(ReadAnythingAppControllerTest,
       GetChildren_WithSelection_ContainsNearbyNodes) {
  // Create selection from node 3-4.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3u, controller().GetChildren(1).size());
  EXPECT_EQ(0u, controller().GetChildren(2).size());
  EXPECT_EQ(0u, controller().GetChildren(3).size());
  EXPECT_EQ(0u, controller().GetChildren(4).size());

  EXPECT_EQ(2, controller().GetChildren(1)[0]);
  EXPECT_EQ(3, controller().GetChildren(1)[1]);
  EXPECT_EQ(4, controller().GetChildren(1)[2]);
}

TEST_F(ReadAnythingAppControllerTest,
       GetChildren_WithBackwardSelection_ContainsNearbyNodes) {
  // Create backward selection from node 4-3.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3u, controller().GetChildren(1).size());
  EXPECT_EQ(0u, controller().GetChildren(2).size());
  EXPECT_EQ(0u, controller().GetChildren(3).size());
  EXPECT_EQ(0u, controller().GetChildren(4).size());

  EXPECT_EQ(2, controller().GetChildren(1)[0]);
  EXPECT_EQ(3, controller().GetChildren(1)[1]);
  EXPECT_EQ(4, controller().GetChildren(1)[2]);
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag) {
  std::string span = "span";
  std::string h1 = "h1";
  std::string ul = "ul";
  ui::AXNodeData span_node;
  span_node.id = 2;
  span_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, span);

  ui::AXNodeData h1_node;
  h1_node.id = 3;
  h1_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, h1);

  ui::AXNodeData ul_node;
  ul_node.id = 4;
  ul_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, ul);

  SendUpdateWithNodes({span_node, h1_node, ul_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(span, controller().GetHtmlTag(2));
  EXPECT_EQ(h1, controller().GetHtmlTag(3));
  EXPECT_EQ(ul, controller().GetHtmlTag(4));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_TextFieldReturnsDiv) {
  std::string span = "span";
  std::string h1 = "h1";
  std::string ul = "ul";
  std::string div = "div";
  ui::AXNodeData span_node;
  span_node.id = 2;
  span_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, span);

  ui::AXNodeData h1_node;
  h1_node.id = 3;
  h1_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, h1);
  h1_node.role = ax::mojom::Role::kTextField;

  ui::AXNodeData ul_node;
  ul_node.id = 4;
  ul_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, ul);
  ul_node.role = ax::mojom::Role::kTextFieldWithComboBox;

  SendUpdateWithNodes({span_node, h1_node, ul_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(span, controller().GetHtmlTag(2));
  EXPECT_EQ(div, controller().GetHtmlTag(3));
  EXPECT_EQ(div, controller().GetHtmlTag(4));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_SvgReturnsDivIfGoogleDocs) {
  std::string svg = "svg";
  std::string div = "div";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node;
  node.id = 2;
  node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, svg);

  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());
  EXPECT_EQ(div, controller().GetHtmlTag(2));
}

TEST_F(ReadAnythingAppControllerTest,
       GetHtmlTag_paragraphWithTagGReturnsPIfGoogleDocs) {
  std::string g = "g";
  std::string p = "p";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  ui::AXNodeData paragraph_node;
  paragraph_node.id = 2;
  paragraph_node.role = ax::mojom::Role::kParagraph;
  paragraph_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, g);

  ui::AXNodeData svg_node;
  svg_node.id = 3;
  svg_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, g);

  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  root.role = ax::mojom::Role::kParagraph;
  root.child_ids = {paragraph_node.id, svg_node.id};
  update.root_id = root.id;
  update.nodes = {root, paragraph_node, svg_node};
  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());
  EXPECT_EQ("", controller().GetHtmlTag(1));
  EXPECT_EQ(p, controller().GetHtmlTag(2));
  EXPECT_EQ(g, controller().GetHtmlTag(3));
}

TEST_F(ReadAnythingAppControllerTest,
       GetHtmlTag_DivWithHeadingAndAriaLevelReturnsH) {
  std::string h3 = "h3";
  std::string div = "div";
  ui::AXNodeData node1;
  node1.id = 2;

  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kHeading;
  node2.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 3);

  ui::AXNodeData node3;
  node3.id = 4;
  SendUpdateWithNodes({node1, node2, node3});
  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(h3, controller().GetHtmlTag(3));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_PDF) {
  // Send pdf iframe update with html tags to test.
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                       /*is_pdf=*/true);
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");
  ui::AXNodeData node2;
  node2.id = 3;
  node2.role = ax::mojom::Role::kHeading;
  node2.AddIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel, 2);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};
  AccessibilityEventReceived({update});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ("span", controller().GetHtmlTag(1));
  EXPECT_EQ("h1", controller().GetHtmlTag(2));
  EXPECT_EQ("h2", controller().GetHtmlTag(3));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_IncorrectlyFormattedPDF) {
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                       /*is_pdf=*/true);

  // Send pdf update with html tags to test. Two headings next to each
  // other should be spans. A heading that's too long should be turned into a
  // paragraph.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData heading_node1;
  heading_node1.id = 2;
  heading_node1.role = ax::mojom::Role::kHeading;
  heading_node1.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");
  ui::AXNodeData heading_node2;
  heading_node2.id = 3;
  heading_node2.role = ax::mojom::Role::kHeading;
  heading_node2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");

  ui::AXNodeData link_node;
  link_node.id = 4;
  link_node.role = ax::mojom::Role::kLink;

  ui::AXNodeData aria_node;
  aria_node.id = 5;
  aria_node.role = ax::mojom::Role::kHeading;
  aria_node.html_attributes.emplace_back("aria-level", "1");
  aria_node.SetNameChecked(
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
      "tempor incididunt ut labore et dolore magna aliqua.");
  aria_node.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {heading_node1.id, heading_node2.id, link_node.id,
                    aria_node.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = root.id;
  update.nodes = {root, heading_node1, heading_node2, link_node, aria_node};

  AccessibilityEventReceived({update});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ("span", controller().GetHtmlTag(2));
  EXPECT_EQ("span", controller().GetHtmlTag(3));
  EXPECT_EQ("a", controller().GetHtmlTag(4));
  EXPECT_EQ("p", controller().GetHtmlTag(5));
}

TEST_F(ReadAnythingAppControllerTest, GetHtmlTag_InaccessiblePDF) {
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId,
                                       /*is_pdf=*/true);

  // Send pdf iframe update with html tags to test.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node;
  node.id = 2;
  node.role = ax::mojom::Role::kContentInfo;
  node.SetNameChecked(l10n_util::GetStringUTF8(IDS_PDF_OCR_RESULT_END));
  node.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node.id};
  root.role = ax::mojom::Role::kPdfRoot;
  update.root_id = 1;
  update.nodes = {root, node};
  AccessibilityEventReceived({update});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ("br", controller().GetHtmlTag(2));
}

TEST_F(ReadAnythingAppControllerTest, GetAltText) {
  std::string img = "img";
  std::string sample_alt_text = "sample_alt_text";
  ui::AXNodeData img_node;
  img_node.id = 2;
  img_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, img);
  img_node.AddStringAttribute(ax::mojom::StringAttribute::kName,
                              sample_alt_text);

  SendUpdateWithNodes({img_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(img, controller().GetHtmlTag(2));
  EXPECT_EQ(sample_alt_text, controller().GetAltText(2));
}

TEST_F(ReadAnythingAppControllerTest, GetAltText_Unset) {
  std::string img = "img";
  ui::AXNodeData img_node;
  img_node.id = 2;
  img_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, img);

  SendUpdateWithNodes({img_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(img, controller().GetHtmlTag(2));
  EXPECT_EQ("", controller().GetAltText(2));
}

TEST_F(ReadAnythingAppControllerTest, GetImageDataUrl) {
  std::string img = "img";
  std::string img_data =
      "data:image/"
      "png;base64,"
      "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAADElEQVQImWNgoBMAAABpAAFE"
      "I8ARAAAAAElFTkSuQmCC";
  ui::AXNodeData img_node;
  img_node.id = 2;
  img_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, img);
  img_node.AddStringAttribute(ax::mojom::StringAttribute::kImageDataUrl,
                              img_data);

  SendUpdateWithNodes({img_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(img, controller().GetHtmlTag(2));
  EXPECT_EQ(img_data, controller().GetImageDataUrl(2));
}

TEST_F(ReadAnythingAppControllerTest, GetImageDataUrl_Unset) {
  std::string img = "img";
  ui::AXNodeData img_node;
  img_node.id = 2;
  img_node.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, img);

  SendUpdateWithNodes({img_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(img, controller().GetHtmlTag(2));
  EXPECT_EQ("", controller().GetImageDataUrl(2));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_NoSelection) {
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2, u"Hello");
  ui::AXNodeData node2 = test::ExplicitlyEmptyTextNode(/* id= */ 3);
  ui::AXNodeData node3 = test::TextNode(/* id = */ 4, u" world");

  SendUpdateWithNodes({node1, node2, node3});
  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(u"Hello world", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u" world", controller().GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, GetTextContent_WithSelection) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2, u"Hello");
  ui::AXNodeData node2 = test::TextNode(/* id= */ 3, u" world");
  ui::AXNodeData node3 = test::TextNode(/* id= */ 4, u" friend");
  update.nodes = {node1, node2, node3};

  // Create selection from node 2-3.
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 3;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(u"Hello world friend", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello", controller().GetTextContent(2));
  EXPECT_EQ(u" world", controller().GetTextContent(3));
  EXPECT_EQ(u" friend", controller().GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest,
       GetTextContent_IgoreStaticTextIfGoogleDocs) {
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2, u"Hello");
  ui::AXNodeData node2 = test::ExplicitlyEmptyTextNode(/* id= */ 3);

  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kParagraph;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};

  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());
  EXPECT_EQ(u"", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
}

TEST_F(ReadAnythingAppControllerTest,
       GetTextContent_UseNameAttributeTextIfGoogleDocs) {
  std::u16string text_content = u"Hello";
  std::u16string more_text_content = u"world";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kName, "Hello");

  ui::AXNodeData node2;
  node2.id = 3;
  node2.AddStringAttribute(ax::mojom::StringAttribute::kName, "world");
  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kParagraph;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};

  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());
  EXPECT_EQ(u"Hello world ", controller().GetTextContent(1));
  EXPECT_EQ(text_content + u" ", controller().GetTextContent(2));
  EXPECT_EQ(more_text_content + u" ", controller().GetTextContent(3));
}

TEST_F(ReadAnythingAppControllerTest,
       GetTextContent_DoNotUseNameAttributeTextIfNotGoogleDocs) {
  std::string text_content = "Hello";
  std::string more_text_content = "world";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node1;
  node1.id = 2;
  node1.AddStringAttribute(ax::mojom::StringAttribute::kName, text_content);

  ui::AXNodeData node2;
  node2.id = 3;
  node2.AddStringAttribute(ax::mojom::StringAttribute::kName,
                           more_text_content);

  ui::AXNodeData root = test::LinkNode(/* id= */ 1, "https://www.google.com");
  root.child_ids = {node1.id, node2.id};
  root.role = ax::mojom::Role::kParagraph;
  update.root_id = root.id;
  update.nodes = {root, node1, node2};

  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_FALSE(controller().IsGoogleDocs());
  EXPECT_EQ(u"", controller().GetTextContent(1));
  EXPECT_EQ(u"", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
}

TEST_F(ReadAnythingAppControllerTest, GetDisplayNameForLocale) {
  EXPECT_EQ(controller().GetDisplayNameForLocale("en-US", "en"),
            "English (United States)");
  EXPECT_EQ(controller().GetDisplayNameForLocale("en-US", "es"),
            "Inglés (Estados Unidos)");
  EXPECT_EQ(controller().GetDisplayNameForLocale("en-US", "en-US"),
            "English (United States)");
  EXPECT_EQ(controller().GetDisplayNameForLocale("en-UK", "en"),
            "English (United Kingdom)");
  EXPECT_EQ(controller().GetDisplayNameForLocale("en-UK", "foo5"), "");
  EXPECT_EQ(controller().GetDisplayNameForLocale("foo", "en"), "");
}

TEST_F(ReadAnythingAppControllerTest, GetUrl) {
  std::string http_url = "http://www.google.com";
  std::string https_url = "https://www.google.com";
  std::string invalid_url = "cats";
  std::string missing_url = "";
  std::string js = "javascript:alert(origin)";

  ui::AXNodeData node1 = test::LinkNode(/* id= */ 2, http_url);
  ui::AXNodeData node2 = test::LinkNode(/*id= */ 3, https_url);
  ui::AXNodeData node3 = test::LinkNode(/* id= */ 4, invalid_url);
  ui::AXNodeData node4 = test::LinkNode(/* id= */ 5, missing_url);
  ui::AXNodeData node5 = test::LinkNode(/* id= */ 6, js);

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node1.id, node2.id, node3.id, node4.id, node5.id};
  SendUpdateWithNodes({root, node1, node2, node3, node4, node5});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(http_url, controller().GetUrl(2));
  EXPECT_EQ(https_url, controller().GetUrl(3));
  EXPECT_EQ("", controller().GetUrl(4));
  EXPECT_EQ("", controller().GetUrl(5));
  EXPECT_EQ("", controller().GetUrl(6));
}

TEST_F(ReadAnythingAppControllerTest, ShouldBold) {
  ui::AXNodeData overline_node;
  overline_node.id = 2;
  overline_node.AddTextStyle(ax::mojom::TextStyle::kOverline);

  ui::AXNodeData underline_node;
  underline_node.id = 3;
  underline_node.AddTextStyle(ax::mojom::TextStyle::kUnderline);

  ui::AXNodeData italic_node;
  italic_node.id = 4;
  italic_node.AddTextStyle(ax::mojom::TextStyle::kItalic);
  SendUpdateWithNodes({overline_node, underline_node, italic_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(false, controller().ShouldBold(2));
  EXPECT_EQ(true, controller().ShouldBold(3));
  EXPECT_EQ(true, controller().ShouldBold(4));
}

TEST_F(ReadAnythingAppControllerTest, IsOverline) {
  ui::AXNodeData overline_node;
  overline_node.id = 2;
  overline_node.AddTextStyle(ax::mojom::TextStyle::kOverline);

  ui::AXNodeData underline_node;
  underline_node.id = 3;
  underline_node.AddTextStyle(ax::mojom::TextStyle::kUnderline);
  SendUpdateWithNodes({overline_node, underline_node});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(true, controller().IsOverline(2));
  EXPECT_EQ(false, controller().IsOverline(3));
}

TEST_F(ReadAnythingAppControllerTest, IsLeafNode) {
  ui::AXNodeData node1;
  node1.id = 2;

  ui::AXNodeData node2;
  node2.id = 3;

  ui::AXNodeData node3;
  node3.id = 4;

  ui::AXNodeData parent;
  parent.id = 1;
  parent.child_ids = {node1.id, node2.id, node3.id};
  SendUpdateWithNodes({parent, node1, node2, node3});

  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_EQ(false, controller().IsLeafNode(1));
  EXPECT_EQ(true, controller().IsLeafNode(2));
  EXPECT_EQ(true, controller().IsLeafNode(3));
  EXPECT_EQ(true, controller().IsLeafNode(4));
}

TEST_F(ReadAnythingAppControllerTest,
       SelectionNodeIdsContains_SelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = false;

  AccessibilityEventReceived({update});
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 1));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 3));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppControllerTest,
       SelectionNodeIdsContains_BackwardSelectionAndNearbyNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 1));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 3));
  EXPECT_TRUE(base::Contains(model().selection_node_ids(), 4));
}

TEST_F(ReadAnythingAppControllerTest, DisplayNodeIdsContains_ContentNodes) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData node;
  node.id = 3;
  update.nodes = {node};
  // This update says the page loaded. When the controller receives it in
  // AccessibilityEventReceived, it will re-distill the tree. This is an
  // example of a non-generated event.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({update}, {load_complete});
  controller().OnAXTreeDistilled(tree_id_, {3});
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 3));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       DisplayNodeIdsContains_NoSelectionOrContentNodes) {
  controller().OnAXTreeDistilled(tree_id_, {});
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 2));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 3));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 4));
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfContentNodeNotFoundInTree) {
  controller().OnAXTreeDistilled(tree_id_, {6});
}

TEST_F(ReadAnythingAppControllerTest, Draw_RecomputeDisplayNodes) {
  ui::AXNodeData node;
  node.id = 4;

  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  SendUpdateWithNodes({node});
  model().Reset({3, 4});
  controller().Draw(/* recompute_display_nodes= */ true);
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 3));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 4));
}

TEST_F(ReadAnythingAppControllerTest, Draw_DoNotRecomputeDisplayNodesForDocs) {
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  ui::AXNodeData node;
  node.id = 2;

  ui::AXNodeData root = test::LinkNode(/* id= */ 1, DOCS_URL);
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({update}, {load_complete});
  controller().OnAXTreeDistilled(tree_id_, {3});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 2));
  EXPECT_TRUE(base::Contains(model().display_node_ids(), 3));
  Mock::VerifyAndClearExpectations(distiller_);

  ui::AXNodeData node1;
  node1.id = 4;

  // This update changes the structure of the tree. When the controller receives
  // it in AccessibilityEventReceived, it will re-distill the tree.
  SendUpdateWithNodes({node1});
  model().Reset({3, 4});
  controller().Draw(/* recompute_display_nodes= */ true);
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 1));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 2));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 3));
  EXPECT_FALSE(base::Contains(model().display_node_ids(), 4));
}

TEST_F(ReadAnythingAppControllerTest, AccessibilityEventReceived) {
  // Tree starts off with no text content.
  EXPECT_EQ(u"", controller().GetTextContent(1));
  EXPECT_EQ(u"", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u"", controller().GetTextContent(4));

  // Send a new update which settings the text content of node 2.
  ui::AXNodeData node = test::TextNode(/* id= */ 2, u"Hello world");
  SendUpdateWithNodes({node});

  EXPECT_EQ(u"Hello world", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello world", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u"", controller().GetTextContent(4));

  // Send three updates which should be merged.
  SendBatchUpdates();
  EXPECT_EQ(u"Node 2Node 3Node 4", controller().GetTextContent(1));
  EXPECT_EQ(u"Node 2", controller().GetTextContent(2));
  EXPECT_EQ(u"Node 3", controller().GetTextContent(3));
  EXPECT_EQ(u"Node 4", controller().GetTextContent(4));

  // Clear node 1.
  ui::AXTreeUpdate clear_update;
  test::SetUpdateTreeID(&clear_update, tree_id_);
  clear_update.root_id = 1;
  clear_update.node_id_to_clear = 1;
  ui::AXNodeData clearNode;
  clearNode.id = 1;
  clear_update.nodes = {clearNode};
  AccessibilityEventReceived({clear_update});
  EXPECT_EQ(u"", controller().GetTextContent(1));
}

TEST_F(ReadAnythingAppControllerTest,
       AccessibilityEventReceivedWhileDistilling) {
  // Tree starts off with no text content.
  EXPECT_EQ(u"", controller().GetTextContent(1));
  EXPECT_EQ(u"", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u"", controller().GetTextContent(4));

  // Send a new update which settings the text content of node 2.
  ui::AXNodeData start_node = test::TextNode(/* id= */ 2, u"Hello world");
  SendUpdateWithNodes({start_node});

  EXPECT_EQ(u"Hello world", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello world", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u"", controller().GetTextContent(4));

  // Send three updates while distilling.
  model().set_distillation_in_progress(true);
  SendBatchUpdates();

  // The updates shouldn't be applied yet.
  EXPECT_EQ(u"Hello world", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello world", controller().GetTextContent(2));

  // Send another update after distillation finishes but before
  // OnAXTreeDistilled would unserialize the pending updates. Since a11y events
  // happen asynchronously, they can come between the time distillation finishes
  // and pending updates are unserialized.
  model().set_distillation_in_progress(false);
  ui::AXNodeData final_node = test::TextNode(/* id= */ 2, u"Final update");
  SendUpdateWithNodes({final_node});

  EXPECT_EQ(u"Final updateNode 3Node 4", controller().GetTextContent(1));
  EXPECT_EQ(u"Final update", controller().GetTextContent(2));
  EXPECT_EQ(u"Node 3", controller().GetTextContent(3));
  EXPECT_EQ(u"Node 4", controller().GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, AccessibilityEventReceivedWhileSpeaking) {
  // Tree starts off with no text content.
  EXPECT_EQ(u"", controller().GetTextContent(1));
  EXPECT_EQ(u"", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u"", controller().GetTextContent(4));

  // Send a new update which settings the text content of node 2.
  ui::AXNodeData start_node = test::TextNode(/* id= */ 2, u"Hello world");
  SendUpdateWithNodes({start_node});

  EXPECT_EQ(u"Hello world", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello world", controller().GetTextContent(2));
  EXPECT_EQ(u"", controller().GetTextContent(3));
  EXPECT_EQ(u"", controller().GetTextContent(4));

  // Send three updates while playing.
  controller().OnSpeechPlayingStateChanged(/* is_speech_active= */ true);
  SendBatchUpdates();

  // The updates shouldn't be applied yet.
  EXPECT_EQ(u"Hello world", controller().GetTextContent(1));
  EXPECT_EQ(u"Hello world", controller().GetTextContent(2));

  // Send another update after distillation finishes but before
  // OnAXTreeDistilled would unserialize the pending updates. Since a11y events
  // happen asynchronously, they can come between the time distillation finishes
  // and pending updates are unserialized.
  controller().OnSpeechPlayingStateChanged(/* is_speech_active= */ false);
  ui::AXNodeData final_node = test::TextNode(/* id= */ 2, u"Final update");
  SendUpdateWithNodes({final_node});

  EXPECT_EQ(u"Final updateNode 3Node 4", controller().GetTextContent(1));
  EXPECT_EQ(u"Final update", controller().GetTextContent(2));
  EXPECT_EQ(u"Node 3", controller().GetTextContent(3));
  EXPECT_EQ(u"Node 4", controller().GetTextContent(4));
}

TEST_F(ReadAnythingAppControllerTest, AccessibilityLocationChangesReceived) {
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  ui::AXRelativeBounds initial_bounds;
  initial_bounds.bounds = gfx::RectF(1, 1, 100, 100);
  initial_bounds.offset_container_id = 12345;
  ui::AXNodeData node;
  node.id = 2;
  node.relative_bounds = initial_bounds;

  ui::AXNodeData root;
  root.id = 1;
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(tree_id_, {1});
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);

  // Create a new bounding box that the node will update to have
  ui::AXRelativeBounds location_update;
  location_update.offset_container_id = 1;
  location_update.bounds = gfx::RectF(5, 5, 100, 100);
  ui::AXLocationAndScrollUpdates location_and_scroll_updates;
  location_and_scroll_updates.location_changes.emplace_back(2, location_update);

  // Test that the node data updates correctly
  controller().AccessibilityLocationChangesReceived(
      id_1, location_and_scroll_updates);
  node = model().GetAXNode(2)->data();
  EXPECT_EQ(node.relative_bounds, location_update);
}

TEST_F(ReadAnythingAppControllerTest, OnActiveAXTreeIDChanged) {
  // Create three AXTreeUpdates with three different tree IDs.
  std::vector<ui::AXTreeID> tree_ids = {ui::AXTreeID::CreateNewAXTreeID(),
                                        ui::AXTreeID::CreateNewAXTreeID(),
                                        tree_id_};
  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 3; i++) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_ids[i]);
    ui::AXNodeData node =
        test::TextNode(/* id= */ 1, u"Tree " + base::NumberToString16(i));
    update.root_id = node.id;
    update.nodes = {node};
    updates.push_back(update);
  }
  // Add the three updates separately since they have different tree IDs.
  // Check that changing the active tree ID changes the active tree which is
  // used when using a v8 getter.
  for (int i = 0; i < 3; i++) {
    AccessibilityEventReceived({updates[i]});
    controller().OnAXTreeDistilled(tree_id_, {1});
    EXPECT_CALL(*distiller_, Distill).Times(1);
    controller().OnActiveAXTreeIDChanged(tree_ids[i], ukm::kInvalidSourceId,
                                         false);
    EXPECT_EQ(u"Tree " + base::NumberToString16(i),
              controller().GetTextContent(1));
    Mock::VerifyAndClearExpectations(distiller_);
  }

  // Changing the active tree ID to the same ID does nothing.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  controller().OnActiveAXTreeIDChanged(tree_ids[2], ukm::kInvalidSourceId,
                                       false);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, IsGoogleDocs) {
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  update.root_id = 1;

  ui::AXNodeData node = test::LinkNode(/*id = */ 1, "www.google.com");
  update.nodes = {node};
  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {1});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_FALSE(controller().IsGoogleDocs());
  Mock::VerifyAndClearExpectations(distiller_);

  ui::AXTreeUpdate update_1;
  test::SetUpdateTreeID(&update_1, tree_id_);
  ui::AXNodeData root = test::LinkNode(/*id = */ 1, DOCS_URL);
  update_1.root_id = root.id;
  update_1.nodes = {root};
  AccessibilityEventReceived({update_1});
  EXPECT_TRUE(
      model().GetTreesForTesting()->at(tree_id_)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {1});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfActiveAXTreeIDUnknown) {
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ui::AXTreeID tree_id = ui::AXTreeIDUnknown();
  controller().OnActiveAXTreeIDChanged(tree_id, ukm::kInvalidSourceId, false);
  controller().OnAXTreeDestroyed(tree_id);
  controller().OnAXTreeDistilled(tree_id_, {1});
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, DoesNotCrashIfActiveAXTreeIDNotInTrees) {
  ui::AXTreeID tree_id = ui::AXTreeID::CreateNewAXTreeID();
  controller().OnActiveAXTreeIDChanged(tree_id, ukm::kInvalidSourceId, false);
  controller().OnAXTreeDestroyed(tree_id);
}

TEST_F(ReadAnythingAppControllerTest, AddAndRemoveTrees) {
  // Create two new trees with new tree IDs.
  std::vector<ui::AXTreeID> tree_ids = {ui::AXTreeID::CreateNewAXTreeID(),
                                        ui::AXTreeID::CreateNewAXTreeID()};
  std::vector<ui::AXTreeUpdate> updates;
  for (int i = 0; i < 2; i++) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_ids[i]);
    ui::AXNodeData node;
    node.id = 1;
    update.root_id = node.id;
    update.nodes = {node};
    updates.push_back(update);
  }

  // Start with 1 tree (the tree created in SetUp).
  ASSERT_TRUE(model().ContainsTree(tree_id_));

  // Add the two trees.
  AccessibilityEventReceived({updates[0]});
  ASSERT_TRUE(model().ContainsTree(tree_id_));
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  AccessibilityEventReceived({updates[1]});
  ASSERT_TRUE(model().ContainsTree(tree_id_));
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));

  // Remove all of the trees.
  controller().OnAXTreeDestroyed(tree_id_);
  ASSERT_FALSE(model().ContainsTree(tree_id_));
  ASSERT_TRUE(model().ContainsTree(tree_ids[0]));
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));
  controller().OnAXTreeDestroyed(tree_ids[0]);
  ASSERT_FALSE(model().ContainsTree(tree_ids[0]));
  ASSERT_TRUE(model().ContainsTree(tree_ids[1]));
  controller().OnAXTreeDestroyed(tree_ids[1]);
  ASSERT_FALSE(model().ContainsTree(tree_ids[1]));
}

TEST_F(ReadAnythingAppControllerTest, OnAXTreeDestroyed_EraseTreeCalled) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(u"2345", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[1]});
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Destroy the tree.
  ASSERT_TRUE(model().ContainsTree(tree_id_));
  controller().OnAXTreeDestroyed(tree_id_);
  ASSERT_FALSE(model().ContainsTree(tree_id_));
}

TEST_F(ReadAnythingAppControllerTest,
       DistillationInProgress_TreeUpdateReceivedOnActiveTree) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0. Data gets unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(u"2345", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1. This triggers distillation via a non-generated event. The
  // data is also unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete_1(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[1]}, {load_complete_1});
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 2. Distillation is still in progress; we get a non-generated
  // event. This does not result in distillation (yet). The data is not
  // unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ui::AXEvent load_complete_2(2, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[2]}, {load_complete_2});
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Complete distillation. The queued up tree update gets unserialized; we also
  // request distillation (deferred from above) with state
  // `requires_distillation_` from the model.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnAXTreeDistilled(tree_id_, {1});
  EXPECT_EQ(u"234567", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       SpeechPlaying_TreeUpdateReceivedOnActiveTree) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0. Data gets unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  EXPECT_EQ(u"2345", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1. This triggers distillation via a non-generated event. The
  // data is also unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete_1(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[1]}, {load_complete_1});
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 2. Distillation is still in progress; we get a non-generated
  // event. This does not result in distillation (yet). The data is not
  // unserialized. Speech starts playing
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ui::AXEvent load_complete_2(2, ax::mojom::Event::kLoadComplete);
  controller().OnSpeechPlayingStateChanged(/*is_speech_active=*/true);
  AccessibilityEventReceived({updates[2]}, {load_complete_2});
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Complete distillation with speech still playing. This does not result in
  // distillation (yet). The data is not unserialized
  EXPECT_CALL(*distiller_, Distill).Times(0);
  controller().OnAXTreeDistilled(tree_id_, {1});
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Speech stops. We request distillation (deferred from above)
  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnSpeechPlayingStateChanged(/*is_speech_active=*/false);
  EXPECT_EQ(u"23456", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Complete distillation. The queued up tree update gets unserialized.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  controller().OnAXTreeDistilled(tree_id_, {1});
  EXPECT_EQ(u"234567", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       AccessibilityReceivedAfterDistillingOnSameTree_DoesNotCrash) {
  std::vector<int> child_ids = SendSimpleUpdateAndGetChildIds();
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Send update 0, which starts distillation because of the load complete.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[0]}, {load_complete});
  Mock::VerifyAndClearExpectations(distiller_);

  // Send update 1. Since there's no event (generated or not) which triggers
  // distllation, we have no calls.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[1]});
  Mock::VerifyAndClearExpectations(distiller_);

  // Ensure that there are no crashes after an accessibility event is received
  // immediately after distilling.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  controller().OnAXTreeDistilled(tree_id_, {1});
  model().set_distillation_in_progress(true);
  AccessibilityEventReceived({updates[2]});
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       DistillationInProgress_ActiveTreeIDChanges) {
  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  Mock::VerifyAndClearExpectations(distiller_);

  EXPECT_CALL(*distiller_, Distill).Times(1);
  ui::AXEvent load_complete(1, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[1]}, {load_complete});
  Mock::VerifyAndClearExpectations(distiller_);

  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[2]});
  EXPECT_EQ(u"56", controller().GetTextContent(1));
  Mock::VerifyAndClearExpectations(distiller_);

  // Calling OnActiveAXTreeID updates the active AXTreeID.
  ui::AXTreeID tree_id_2 = ui::AXTreeID::CreateNewAXTreeID();
  EXPECT_CALL(*distiller_, Distill).Times(0);
  ASSERT_EQ(tree_id_, model().active_tree_id());
  controller().OnActiveAXTreeIDChanged(tree_id_2, ukm::kInvalidSourceId, false);
  ASSERT_EQ(tree_id_2, model().active_tree_id());
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithInactiveTreeId) {
  controller().OnActiveAXTreeIDChanged(ui::AXTreeID::CreateNewAXTreeID(),
                                       ukm::kInvalidSourceId, false);
  // Should not crash.
  controller().OnAXTreeDistilled(tree_id_, {});
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithDestroyedTreeId) {
  controller().OnAXTreeDestroyed(tree_id_);
  // Should not crash.
  controller().OnAXTreeDistilled(tree_id_, {});
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithUnknownActiveTreeId) {
  controller().OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(),
                                       ukm::kInvalidSourceId, false);
  // Should not crash.
  controller().OnAXTreeDistilled(tree_id_, {});
}

TEST_F(ReadAnythingAppControllerTest,
       OnAXTreeDistilledCalledWithUnknownTreeId) {
  // Should not crash.
  controller().OnAXTreeDistilled(ui::AXTreeIDUnknown(), {});
}

TEST_F(ReadAnythingAppControllerTest,
       ChangeActiveTreeWithPendingUpdates_UnknownID) {
  // Create a couple of updates which add additional nodes to the tree.
  std::vector<int> child_ids = {2, 3, 4};
  std::vector<ui::AXTreeUpdate> updates =
      test::CreateSimpleUpdateList(child_ids, tree_id_);

  // Create an update which has no tree id.
  ui::AXTreeUpdate update;
  ui::AXNodeData generic_container_node = test::GenericContainerNode(/*id =*/1);
  update.nodes = {generic_container_node};
  updates.push_back(update);

  // Add the three updates.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  AccessibilityEventReceived({updates[0]});
  controller().AccessibilityEventReceived(tree_id_, {updates[1], updates[2]},
                                          {});
  Mock::VerifyAndClearExpectations(distiller_);

  // Switch to a new active tree. Should not crash.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  controller().OnActiveAXTreeIDChanged(ui::AXTreeIDUnknown(),
                                       ukm::kInvalidSourceId, false);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, OnLinkClicked) {
  ui::AXNodeID ax_node_id = 2;
  EXPECT_CALL(page_handler_, OnLinkClicked(tree_id_, ax_node_id)).Times(1);
  controller().OnLinkClicked(ax_node_id);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, RequestImageDataUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kReadAnythingImagesViaAlgorithm,
       features::kReadAnythingReadAloud},
      {});
  ui::AXNodeID ax_node_id = 2;
  EXPECT_CALL(page_handler_, OnImageDataRequested(tree_id_, ax_node_id))
      .Times(1);

  auto line_spacing = read_anything::mojom::LineSpacing::kDefaultValue;
  auto letter_spacing = read_anything::mojom::LetterSpacing::kDefaultValue;
  std::string font_name = "Roboto";
  double font_size = 18.0;
  bool links_enabled = false;
  bool images_enabled = true;
  auto color = read_anything::mojom::Colors::kDefaultValue;
  double speech_rate = 1.5;
  std::string voice_value = "Italian voice 3";
  std::string language_value = "it-IT";
  base::Value::Dict voices = base::Value::Dict();
  voices.Set(language_value, voice_value);
  base::Value::List languages_enabled_in_pref = base::Value::List();
  languages_enabled_in_pref.Append(language_value);
  auto highlight_granularity =
      read_anything::mojom::HighlightGranularity::kDefaultValue;

  controller().OnSettingsRestoredFromPrefs(
      line_spacing, letter_spacing, font_name, font_size, links_enabled,
      images_enabled, color, speech_rate, std::move(voices),
      std::move(languages_enabled_in_pref), highlight_granularity);
  controller().RequestImageDataUrl(ax_node_id);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, OnLinkClicked_DistillationInProgress) {
  ui::AXTreeID new_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, new_tree_id);
  ui::AXNodeData node;
  node.id = 1;
  update.root_id = node.id;
  update.nodes = {node};
  AccessibilityEventReceived({update});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnActiveAXTreeIDChanged(new_tree_id, ukm::kInvalidSourceId,
                                       false);
  Mock::VerifyAndClearExpectations(distiller_);

  // If distillation is in progress, OnLinkClicked should not be called.
  EXPECT_CALL(page_handler_, OnLinkClicked).Times(0);
  controller().OnLinkClicked(2);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, ScrollToTargetNode_ScrollsIfGoogleDocs) {
  ui::AXNodeData root;
  ui::AXNodeData node;
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  root.id = 1;
  root.AddStringAttribute(
      ax::mojom::StringAttribute::kUrl,
      "https://docs.google.com/document/d/"
      "1t6x1PQaQWjE8wb9iyYmFaoK1XAEgsl8G1Hx3rzfpoKA/"
      "edit?ouid=103677288878638916900&usp=docs_home&ths=true");
  node.id = 2;
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {1});
  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_TRUE(controller().IsGoogleDocs());

  ui::AXNodeID ax_node_id = 4;
  EXPECT_CALL(page_handler_, ScrollToTargetNode(id_1, ax_node_id)).Times(1);
  controller().OnScrolledToBottom();
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       ScrollToTargetNode_DoesNotScrollIfNotGoogleDocs) {
  ui::AXNodeData root;
  ui::AXNodeData node;
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  root.id = 1;
  root.AddStringAttribute(ax::mojom::StringAttribute::kUrl,
                          "https://www.google.com/");
  node.id = 2;
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  AccessibilityEventReceived({update});
  EXPECT_TRUE(model().GetTreesForTesting()->at(id_1)->is_url_information_set);
  controller().OnAXTreeDistilled(tree_id_, {1});
  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  EXPECT_FALSE(controller().IsGoogleDocs());

  ui::AXNodeID ax_node_id = 4;
  EXPECT_CALL(page_handler_, ScrollToTargetNode(id_1, ax_node_id)).Times(0);
  controller().OnScrolledToBottom();
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, OnSelectionChange) {
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2);
  ui::AXNodeData node2 = test::TextNode(/* id= */ 3);
  ui::AXNodeData node3 = test::TextNode(/* id= */ 4);

  SendUpdateWithNodes({node1, node2, node3});
  ui::AXNodeID anchor_node_id = 2;
  int anchor_offset = 0;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 1;
  EXPECT_CALL(page_handler_,
              OnSelectionChange(tree_id_, anchor_node_id, anchor_offset,
                                focus_node_id, focus_offset))
      .Times(1);
  controller().OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id,
                                 focus_offset);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, OnCollapseSelection) {
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2);
  ui::AXNodeData node2 = test::TextNode(/* id= */ 3);
  ui::AXNodeData node3 = test::TextNode(/* id= */ 4);

  SendUpdateWithNodes({node1, node2, node3});
  EXPECT_CALL(page_handler_, OnCollapseSelection()).Times(1);
  controller().OnCollapseSelection();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_ClickAfterClickDoesNotUpdateSelection) {
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2);
  ui::AXNodeData node2 = test::TextNode(/* id= */ 3);
  SendUpdateWithNodes({node1, node2});

  ui::AXTreeUpdate selection;
  test::SetUpdateTreeID(&selection, tree_id_);
  selection.has_tree_data = true;
  selection.event_from = ax::mojom::EventFrom::kUser;
  selection.tree_data.sel_anchor_object_id = 2;
  selection.tree_data.sel_focus_object_id = 2;
  selection.tree_data.sel_anchor_offset = 0;
  selection.tree_data.sel_focus_offset = 0;
  AccessibilityEventReceived({selection});

  EXPECT_CALL(page_handler_, OnSelectionChange).Times(0);
  controller().OnSelectionChange(3, 5, 3, 5);
  page_handler_.FlushForTesting();
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_ClickAfterSelectionClearsSelection) {
  ui::AXNodeData node1 = test::TextNode(/* id= */ 2);
  ui::AXNodeData node2 = test::TextNode(/* id= */ 3);
  SendUpdateWithNodes({node1, node2});

  ui::AXTreeUpdate selection;
  test::SetUpdateTreeID(&selection, tree_id_);
  selection.has_tree_data = true;
  selection.event_from = ax::mojom::EventFrom::kUser;
  selection.tree_data.sel_anchor_object_id = 2;
  selection.tree_data.sel_focus_object_id = 3;
  selection.tree_data.sel_anchor_offset = 0;
  selection.tree_data.sel_focus_offset = 1;
  AccessibilityEventReceived({selection});

  ui::AXNodeID anchor_node_id = 3;
  int anchor_offset = 5;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 5;
  EXPECT_CALL(page_handler_, OnCollapseSelection()).Times(1);
  controller().OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id,
                                 focus_offset);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_DistillationInProgress) {
  ui::AXTreeID new_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, new_tree_id);
  ui::AXNodeData root = test::TextNode(/* id= */ 1);
  update.root_id = root.id;
  update.nodes = {root};
  AccessibilityEventReceived({update});
  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().OnActiveAXTreeIDChanged(new_tree_id, ukm::kInvalidSourceId,
                                       false);
  Mock::VerifyAndClearExpectations(distiller_);

  // If distillation is in progress, OnSelectionChange should not be called.
  EXPECT_CALL(page_handler_, OnSelectionChange).Times(0);
  controller().OnSelectionChange(2, 0, 3, 1);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest,
       OnSelectionChange_NonTextFieldDoesNotUpdateSelection) {
  ui::AXNodeData text_field_node1;
  text_field_node1.id = 2;
  text_field_node1.role = ax::mojom::Role::kTextField;

  ui::AXNodeData container_node = test::GenericContainerNode(/*id= */ 3);

  ui::AXNodeData text_field_node2;
  text_field_node2.id = 4;
  text_field_node2.role = ax::mojom::Role::kTextField;
  SendUpdateWithNodes({text_field_node1, container_node, text_field_node2});

  ui::AXNodeID anchor_node_id = 2;
  int anchor_offset = 0;
  ui::AXNodeID focus_node_id = 3;
  int focus_offset = 1;
  EXPECT_CALL(page_handler_,
              OnSelectionChange(tree_id_, anchor_node_id, anchor_offset,
                                focus_node_id, focus_offset))
      .Times(0);
  controller().OnSelectionChange(anchor_node_id, anchor_offset, focus_node_id,
                                 focus_offset);
  page_handler_.FlushForTesting();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerTest, Selection_Forward) {
  // Create selection from node 3-4.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 3;
  update.tree_data.sel_focus_object_id = 4;
  update.tree_data.sel_anchor_offset = 0;
  update.tree_data.sel_focus_offset = 1;
  update.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3, controller().StartNodeId());
  EXPECT_EQ(4, controller().EndNodeId());
  EXPECT_EQ(0, controller().StartOffset());
  EXPECT_EQ(1, controller().EndOffset());
}

TEST_F(ReadAnythingAppControllerTest, Selection_Backward) {
  // Create backward selection from node 4-3.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 4;
  update.tree_data.sel_focus_object_id = 3;
  update.tree_data.sel_anchor_offset = 1;
  update.tree_data.sel_focus_offset = 0;
  update.tree_data.sel_is_backward = true;
  AccessibilityEventReceived({update});
  EXPECT_EQ(3, controller().StartNodeId());
  EXPECT_EQ(4, controller().EndNodeId());
  EXPECT_EQ(0, controller().StartOffset());
  EXPECT_EQ(1, controller().EndOffset());
}

TEST_F(ReadAnythingAppControllerTest, Selection_IgnoredNode) {
  // Make 4 ignored and give 3 some text content.
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.root_id = 1;
  ui::AXNodeData text_node = test::TextNode(/* id= */ 3, u"Hello");

  ui::AXNodeData ignored_node;
  ignored_node.id = 4;
  ignored_node.role = ax::mojom::Role::kNone;  // This node is ignored.
  update.nodes = {text_node, ignored_node};
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(tree_id_, {});

  // Create selection from node 2-4, where 4 is ignored.
  ui::AXTreeUpdate update_2;
  test::SetUpdateTreeID(&update_2, tree_id_);
  update_2.tree_data.sel_anchor_object_id = 2;
  update_2.tree_data.sel_focus_object_id = 4;
  update_2.tree_data.sel_anchor_offset = 0;
  update_2.tree_data.sel_focus_offset = 0;
  update_2.tree_data.sel_is_backward = false;
  AccessibilityEventReceived({update_2});
  controller().OnAXTreeDistilled(tree_id_, {});

  EXPECT_EQ(0, controller().StartNodeId());
  EXPECT_EQ(0, controller().EndNodeId());
  EXPECT_EQ(-1, controller().StartOffset());
  EXPECT_EQ(-1, controller().EndOffset());
  EXPECT_EQ(false, model().has_selection());
}

TEST_F(ReadAnythingAppControllerTest, Selection_IsCollapsed) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  update.has_tree_data = true;
  update.event_from = ax::mojom::EventFrom::kUser;
  update.tree_data.sel_anchor_object_id = 2;
  update.tree_data.sel_focus_object_id = 2;
  update.tree_data.sel_anchor_offset = 3;
  update.tree_data.sel_focus_offset = 3;
  AccessibilityEventReceived({update});
  EXPECT_EQ(ui::kInvalidAXNodeID, controller().StartNodeId());
  EXPECT_EQ(ui::kInvalidAXNodeID, controller().EndNodeId());
  EXPECT_EQ(-1, controller().StartOffset());
  EXPECT_EQ(-1, controller().EndOffset());
  EXPECT_EQ(false, model().has_selection());
}

TEST_F(ReadAnythingAppControllerTest, OnFontSizeReset_SetsFontSizeToDefault) {
  model().ResetTextSize();
  const double default_font_size = model().font_size();
  EXPECT_CALL(page_handler_, OnFontSizeChange(default_font_size)).Times(1);
  controller().OnFontSizeReset();
}

TEST_F(ReadAnythingAppControllerTest,
       OnLinksEnabledChanged_SetsEnabledToFalse) {
  const bool links_enabled = model().links_enabled();
  EXPECT_CALL(page_handler_, OnLinksEnabledChanged(!links_enabled)).Times(1);
  controller().OnLinksEnabledToggled();
}

TEST_F(ReadAnythingAppControllerTest, TurnedHighlightOn_SavesHighlightState) {
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOn))
      .Times(1);
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOff))
      .Times(0);

  controller().OnHighlightGranularityChanged(
      static_cast<int>(read_anything::mojom::HighlightGranularity::kOn));

  EXPECT_TRUE(controller().IsHighlightOn());
}

TEST_F(ReadAnythingAppControllerTest, TurnedHighlightOff_SavesHighlightState) {
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOn))
      .Times(0);
  EXPECT_CALL(page_handler_,
              OnHighlightGranularityChanged(
                  read_anything::mojom::HighlightGranularity::kOff))
      .Times(1);

  controller().OnHighlightGranularityChanged(
      static_cast<int>(read_anything::mojom::HighlightGranularity::kOff));

  EXPECT_FALSE(controller().IsHighlightOn());
}

TEST_F(ReadAnythingAppControllerTest, SetLanguageCode_UpdatesModelLanguage) {
  controller().SetLanguageForTesting("es");
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "es");

  controller().SetLanguageForTesting("en-UK");
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "en");

  controller().SetLanguageForTesting("zh-CN");
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "zh");
}

TEST_F(ReadAnythingAppControllerTest,
       SetLanguageCode_EmptyCode_DoesNotUpdateModelLanguage) {
  controller().SetLanguageForTesting("es");
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "es");
  ASSERT_FALSE(model().requires_tree_lang());

  controller().SetLanguageForTesting("");
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "es");
  ASSERT_TRUE(model().requires_tree_lang());
}

TEST_F(ReadAnythingAppControllerTest,
       SetLanguageCode_EmptyCode_SetsRootLanguageOnceAvailable) {
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "en");

  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);
  update.root_id = 1;

  ui::AXNodeData node;
  node.id = 1;
  node.AddStringAttribute(ax::mojom::StringAttribute::kLanguage, "yue");
  update.nodes = {node};
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(tree_id_, {1});

  EXPECT_CALL(*distiller_, Distill).Times(1);
  controller().SetLanguageForTesting("");
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  ASSERT_EQ(controller().GetLanguageCodeForSpeech(), "yue");
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_WhenCalledManyTimes_ReturnsSameNode) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);

  SendUpdateAndDistillNodes({static_text1, static_text2});

  EXPECT_EQ((int)controller().GetCurrentText().size(), 1);
  // The returned id should be the first node id, 2
  EXPECT_EQ(controller().GetCurrentText()[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text1.id);
  // Confirm size is still 1.
  EXPECT_EQ((int)controller().GetCurrentText().size(), 1);

  // The returned id should be the second node id, 3
  controller().MovePositionToNextGranularity();
  EXPECT_EQ((int)controller().GetCurrentText().size(), 1);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentText()[0], static_text2.id);
  // Confirm size is still 1.
  EXPECT_EQ((int)controller().GetCurrentText().size(), 1);
}

TEST_F(ReadAnythingAppControllerTest, GetCurrentText_ReturnsExpectedNodes) {
  // TODO(crbug.com/40927698): Investigate if we can improve in scenarios when
  // there's not a space between sentences.
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
  std::u16string sentence3 = u"And this is yet another sentence. ";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  // The returned id should be the next node id, 2
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  // The returned int should be the beginning of the node's text.
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  // The returned int should be equivalent to the text in the node.
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Move to the next node
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Move to the last node
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Attempt to move to another node.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       PreprocessNodes_DoesNotImpactCurrentNodes) {
  std::u16string sentence1 = u"Life was a chore. ";
  std::u16string sentence2 = u"So she set sail. ";
  std::u16string sentence3 = u"Fifteen twenty-two, came straight to the UK. ";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);
  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});
  controller().PreprocessTextForSpeech();

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  // The returned id should be the next node id, 2
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  // The returned int should be the beginning of the node's text.
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  // The returned int should be equivalent to the text in the node.
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Move to the next node
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Move to the last node
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Move backwards
  next_node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Move to the last node again.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Attempt to move to another node.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       PreprocessNodes_CalledMultipleTimes_DoesNotImpactCurrentNodes) {
  std::u16string sentence1 = u"Keep a grip and take a deep breath. ";
  std::u16string sentence2 = u"And soon we'll know what's what. ";
  std::u16string sentence3 =
      u"Put on a show, rewards will flow, and we'll go from there. ";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});
  controller().PreprocessTextForSpeech();
  controller().PreprocessTextForSpeech();

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  // The returned id should be the next node id, 2
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  // The returned int should be the beginning of the node's text.
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  // The returned int should be equivalent to the text in the node.
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Preprocess is called again.
  controller().PreprocessTextForSpeech();
  controller().PreprocessTextForSpeech();

  // But nothing changes with what's returned by GetCurrentText
  next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  // The returned id should be the next node id, 2
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  // The returned int should be the beginning of the node's text.
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  // The returned int should be equivalent to the text in the node.
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Move to the next node
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Move to the last node
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Preprocess is called again.
  controller().PreprocessTextForSpeech();
  controller().PreprocessTextForSpeech();

  // And nothing has changed with the current text.
  next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Move backwards
  next_node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Preprocess is called again.
  controller().PreprocessTextForSpeech();
  controller().PreprocessTextForSpeech();

  // And nothing has changed with the current text.
  next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Move to the last node again.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Attempt to move to another node.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_AfterRestartReadAloud_StartsOver) {
  std::u16string sentence1 = u"I've got the wind in my hair. ";
  std::u16string sentence2 = u"And a gleam in my eyes. ";
  std::u16string sentence3 = u"And an endless horizon. ";

  ui::AXNodeData static_text1 = test::TextNode(/*id = */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/*id = */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/*id = */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);

  // Move to the next sentence.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);

  // If we init without restarting we should just go to the next sentence.
  controller().InitAXPositionWithNode(static_text1.id);
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);

  // After reset and before an init, the current text should be empty.
  read_aloud_model().ResetReadAloudState();
  std::vector<ui::AXNodeID> after_reset_ids = controller().GetCurrentText();
  EXPECT_EQ((int)after_reset_ids.size(), 0);

  // After an init, we should get the first sentence again.
  controller().InitAXPositionWithNode(static_text1.id);
  after_reset_ids = controller().GetCurrentText();
  EXPECT_EQ((int)after_reset_ids.size(), 1);
  EXPECT_EQ(after_reset_ids[0], static_text1.id);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_AfterResetGranularityIndex_StartsOver) {
  std::u16string sentence1 = u"I've got the wind in my hair. ";
  std::u16string sentence2 = u"And a gleam in my eyes. ";
  std::u16string sentence3 = u"And an endless horizon. ";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);
  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);

  // Move to the next sentence.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);

  // If we init without restarting we should just go to the next sentence.
  controller().InitAXPositionWithNode(static_text1.id);
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);

  // After reset, we should get the first sentence again.
  controller().ResetGranularityIndex();
  std::vector<ui::AXNodeID> after_reset_ids = controller().GetCurrentText();
  EXPECT_EQ((int)after_reset_ids.size(), 1);
  EXPECT_EQ(after_reset_ids[0], static_text1.id);
}

TEST_F(ReadAnythingAppControllerTest, GetCurrentText_AfterAXTreeRefresh) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(/* id = */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id = */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id = */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Simulate updating the page text.
  std::u16string new_sentence_1 =
      u"And so I read a book or maybe two or three. ";
  std::u16string new_sentence_2 =
      u"I will add a few new paitings to my gallery. ";
  std::u16string new_sentence_3 =
      u"I will play guitar and knit and cook and basically wonder when will my "
      u"life begin.";
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, id_1);
  ui::AXNodeData root;
  root.id = 1;

  ui::AXNodeData new_static_text1 =
      test::TextNode(/* id= */ 10, new_sentence_1);
  ui::AXNodeData new_static_text2 =
      test::TextNode(/* id= */ 12, new_sentence_2);
  ui::AXNodeData new_static_text3 =
      test::TextNode(/* id= */ 16, new_sentence_3);

  root.child_ids = {new_static_text1.id, new_static_text2.id,
                    new_static_text3.id};
  update2.root_id = root.id;
  update2.nodes = {root, new_static_text1, new_static_text2, new_static_text3};
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  controller().OnAXTreeDistilled(tree_id_, {});
  AccessibilityEventReceived({update2});
  controller().OnAXTreeDistilled(
      id_1, {new_static_text1.id, new_static_text2.id, new_static_text3.id});
  controller().InitAXPositionWithNode(update2.nodes[1].id);

  // The nodes from the new tree are used.
  next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], update2.nodes[1].id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)new_sentence_1.length());

  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], update2.nodes[2].id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)new_sentence_2.length());

  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], update2.nodes[3].id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)new_sentence_3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SentenceSplitAcrossMultipleNodes) {
  std::u16string sentence1 = u"The wind is howling like this ";
  std::u16string sentence2 = u"swirling storm ";
  std::u16string sentence3 = u"inside.";

  ui::AXNodeData static_text1 = test::TextNode(/*id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/*id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/*id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The second segment was returned correctly.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  // The third segment was returned correctly.
  EXPECT_EQ(next_node_ids[2], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[2]),
            (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SentenceSplitAcrossTwoNodes) {
  std::u16string sentence1 = u"And I am almost ";
  std::u16string sentence2 = u"there. ";
  std::u16string sentence3 = u"I am almost there.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 2);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The second segment was returned correctly.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  // The third segment was returned correctly after getting the next text.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Nodes are empty at the end of the tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_OpeningPunctuationIgnored) {
  std::u16string sentence1 = u"And I am almost there.";
  std::u16string sentence2 = u"[2]";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);

  SendUpdateAndDistillNodes({static_text1, static_text2});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The parenthetical expression is returned as a single separate segment.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_OpeningPunctuationIncludedWhenEntireNode) {
  // Simulate breaking up the brackets across a link.
  std::u16string sentence1 = u"And I am almost there.";
  std::u16string sentence2 = u"[";
  std::u16string sentence3 = u"2";
  std::u16string sentence4 = u"]";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);
  ui::AXNodeData static_text4 = test::TextNode(/* id= */ 12, sentence4);

  ui::AXNodeData superscript = test::GenericContainerNode(/* id= */ 13);
  superscript.child_ids = {static_text2.id, static_text3.id, static_text4.id};

  ui::AXNodeData root;
  root.id = 10;
  root.child_ids = {static_text1.id, superscript.id};
  update.root_id = root.id;

  update.nodes = {root,         static_text1, superscript,
                  static_text2, static_text3, static_text4};
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(
      id_1, {root.id, static_text1.id, superscript.id, static_text2.id,
             static_text3.id, static_text4.id});
  controller().InitAXPositionWithNode(static_text1.id);

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The next segment contains the entire bracketed statement '[2]' with both
  // opening and closing brackets so neither bracket is read out-of-context.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 3);

  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  EXPECT_EQ(next_node_ids[1], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence3.length());

  EXPECT_EQ(next_node_ids[2], static_text4.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[2]),
            (int)sentence4.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SuperscriptCombinedWithCurrentSegment) {
  std::u16string sentence1 = u"And I am almost there.";
  std::u16string sentence2 = u"2";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::SuperscriptNode(/* id= */ 3, sentence2);

  SendUpdateAndDistillNodes({static_text1, static_text2});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 2);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The superscript is attached to the first sentence.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SuperscriptWithBracketsCombinedWithCurrentSegment) {
  std::u16string sentence1 = u"And I am almost there.";
  std::u16string sentence2 = u"[2]";
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::SuperscriptNode(/* id= */ 3, sentence2);

  SendUpdateAndDistillNodes({static_text1, static_text2});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 2);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The superscript is attached to the first sentence.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SuperscriptIncludedWhenEntireNode) {
  // Simulate breaking up the brackets across a link.
  std::u16string sentence1 = u"And I am almost there.";
  std::u16string sentence2 = u"[";
  std::u16string sentence3 = u"2";
  std::u16string sentence4 = u"]";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::SuperscriptNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::SuperscriptNode(/* id= */ 4, sentence3);
  ui::AXNodeData static_text4 = test::SuperscriptNode(/* id= */ 12, sentence4);

  ui::AXNodeData superscript;
  superscript.id = 13;
  superscript.role = ax::mojom::Role::kSuperscript;
  superscript.child_ids = {static_text2.id, static_text3.id, static_text4.id};

  ui::AXNodeData root;
  root.id = 10;
  root.child_ids = {static_text1.id, superscript.id};
  update.root_id = root.id;

  update.nodes = {root,         static_text1, superscript,
                  static_text2, static_text3, static_text4};
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(
      id_1, {root.id, static_text1.id, superscript.id, static_text2.id,
             static_text3.id, static_text4.id});
  controller().InitAXPositionWithNode(static_text1.id);

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 4);

  // The first sentence and its superscript are returned as one segment.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  EXPECT_EQ(next_node_ids[2], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[2]),
            (int)sentence3.length());

  EXPECT_EQ(next_node_ids[3], static_text4.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[3]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[3]),
            (int)sentence4.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SuperscriptIncludedWhenEntireNodeAndMoreTextAfterScript) {
  // Simulate breaking up the brackets across a link.
  std::u16string sentence1 = u"And I am almost there.";
  std::u16string sentence2 = u"[";
  std::u16string sentence3 = u"2";
  std::u16string sentence4 = u"]";
  std::u16string sentence5 = u"People gon' come here from everywhere.";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::SuperscriptNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::SuperscriptNode(/* id= */ 4, sentence3);
  ui::AXNodeData static_text4 = test::SuperscriptNode(/* id= */ 12, sentence4);

  ui::AXNodeData superscript;
  superscript.id = 13;
  superscript.role = ax::mojom::Role::kSuperscript;
  superscript.child_ids = {static_text2.id, static_text3.id, static_text4.id};

  ui::AXNodeData static_text5 = test::TextNode(/* id= */ 100, sentence5);

  ui::AXNodeData root;
  root.id = 10;
  root.child_ids = {static_text1.id, superscript.id, static_text5.id};
  update.root_id = root.id;

  update.nodes = {root,         static_text1, superscript, static_text2,
                  static_text3, static_text4, static_text5};
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(
      id_1, {root.id, static_text1.id, superscript.id, static_text2.id,
             static_text3.id, static_text4.id, static_text5.id});
  controller().InitAXPositionWithNode(static_text1.id);

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 4);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // The superscript is returned as a segment.
  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  EXPECT_EQ(next_node_ids[2], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[2]),
            (int)sentence3.length());

  EXPECT_EQ(next_node_ids[3], static_text4.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[3]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[3]),
            (int)sentence4.length());

  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ(next_node_ids[0], static_text5.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence5.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetCurrentText_IncludesListMarkers) {
  // Simulate breaking up the brackets across a link.
  std::string marker_html_tag = "::marker";
  std::u16string bullet1 = u"1.";
  std::u16string sentence1 = u"Realize numbers are ignored in Read Aloud. ";
  std::u16string bullet2 = u"2.";
  std::u16string sentence2 = u"Fix it.";
  ui::AXTreeUpdate update;
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  test::SetUpdateTreeID(&update, id_1);

  ui::AXNodeData list_marker1;
  list_marker1.id = 2;
  list_marker1.role = ax::mojom::Role::kListMarker;
  list_marker1.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                  marker_html_tag);
  list_marker1.SetName(bullet1);
  list_marker1.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 3, sentence1);

  ui::AXNodeData list_marker2;
  list_marker2.id = 4;
  list_marker2.role = ax::mojom::Role::kListMarker;
  list_marker2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                  marker_html_tag);
  list_marker2.SetName(bullet2);
  list_marker2.SetNameFrom(ax::mojom::NameFrom::kContents);

  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 12, sentence2);

  ui::AXNodeData root;
  root.id = 10;
  root.child_ids = {list_marker1.id, static_text1.id, list_marker2.id,
                    static_text2.id};
  update.root_id = root.id;

  update.nodes = {root, list_marker1, static_text1, list_marker2, static_text2};
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(
      id_1, {root.id, list_marker1.id, static_text1.id, list_marker2.id,
             static_text2.id});
  controller().InitAXPositionWithNode(list_marker1.id);

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], list_marker1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)bullet1.length());

  // Move to the next segment.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Move to the next segment.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], list_marker2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)bullet2.length());

  // Move to the next segment.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SentenceSplitAcrossParagraphs) {
  std::u16string header_text = u"Header Text";
  std::u16string paragraph_text1 = u"Paragraph one.";
  std::u16string paragraph_text2 = u"Paragraph two.";
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, header_text);
  ui::AXNodeData static_text2 = test::TextNode(/*id= */ 3, paragraph_text1);
  ui::AXNodeData static_text3 = test::TextNode(/*id= */ 4, paragraph_text2);

  ui::AXNodeData header_node;
  header_node.id = 5;
  header_node.role = ax::mojom::Role::kHeader;
  header_node.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  header_node.child_ids = {static_text1.id};

  ui::AXNodeData paragraph_node1;
  paragraph_node1.id = 6;
  paragraph_node1.role = ax::mojom::Role::kParagraph;
  paragraph_node1.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  paragraph_node1.child_ids = {static_text2.id};

  ui::AXNodeData paragraph_node2;
  paragraph_node2.id = 7;
  paragraph_node2.role = ax::mojom::Role::kParagraph;
  paragraph_node2.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  paragraph_node2.child_ids = {static_text3.id};

  ui::AXNodeData root;
  root.id = 10;
  root.role = ax::mojom::Role::kParagraph;
  root.child_ids = {header_node.id, paragraph_node1.id, paragraph_node2.id};
  update.root_id = root.id;

  update.nodes = {root,         header_node,     static_text1, paragraph_node1,
                  static_text2, paragraph_node2, static_text3};
  AccessibilityEventReceived({update});
  controller().OnAXTreeDistilled(
      tree_id_, {root.id, header_node.id, static_text1.id, paragraph_node1.id,
                 static_text2.id, paragraph_node2.id, static_text3.id});
  controller().InitAXPositionWithNode(static_text1.id);

  // The header is returned alone.
  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)header_text.length());

  // Paragraph 1 is returned alone.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)paragraph_text1.length());

  // Paragraph 2 is returned alone.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)paragraph_text2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_SentenceSplitAcrossParagraphsWithoutParagraphRoles) {
  std::u16string header_text = u"Header Text\n";
  std::u16string paragraph_text1 = u"Paragraph one.\n";
  std::u16string paragraph_text2 = u"Paragraph two.";

  ui::AXNodeData header_node = test::TextNode(/* id= */ 2, header_text);
  ui::AXNodeData paragraph_node1 = test::TextNode(/* id= */ 3, paragraph_text1);
  ui::AXNodeData paragraph_node2 = test::TextNode(/* id= */ 4, paragraph_text2);

  SendUpdateAndDistillNodes({header_node, paragraph_node1, paragraph_node2});

  // The header is returned alone.
  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], header_node.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)header_text.length());

  // Paragraph 1 is returned alone.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], paragraph_node1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)paragraph_text1.length());

  // Paragraph 2 is returned alone.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], paragraph_node2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)paragraph_text2.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_MultipleSentencesInSameNode) {
  std::u16string sentence1 = u"But from up here. The ";
  std::u16string sentence2 = u"world ";
  std::u16string sentence3 =
      u"looks so small. And suddenly life seems so clear. And from up here. "
      u"You coast past it all. The obstacles just disappear.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id = */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  // The first segment was returned correctly.
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.find(u"The"));

  // The second segment was returned correctly, across 3 nodes.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 3);

  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]),
            (int)sentence1.find(u"The"));
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  EXPECT_EQ(next_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[1]),
            (int)sentence2.length());

  EXPECT_EQ(next_node_ids[2], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[2]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[2]),
            (int)sentence3.find(u"And"));

  // The next sentence "And suddenly life seems so clear" was returned correctly
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"And"));
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.find(u"And from"));

  // The next sentence "And from up here" was returned correctly
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"And from"));
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.find(u"You"));

  // The next sentence "You coast past it all" was returned correctly
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"You"));
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.find(u"The"));

  // The next sentence "The obstacles just disappear" was returned correctly
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);

  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]),
            (int)sentence3.find(u"The"));
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetCurrentText_EmptyTree) {
  // If InitAXPosition hasn't been called, GetCurrentText should return nothing.
  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 0);

  // GetCurrentTextStartIndex and GetCurrentTextEndIndex should return -1  on an
  // invalid id.
  EXPECT_EQ(controller().GetCurrentTextStartIndex(0), -1);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(0), -1);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_AfterAXTreeRefresh) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  std::vector<ui::AXNodeID> next_node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence1.length());

  // Simulate updating the page text.
  std::u16string new_sentence1 = u"Welcome to the show to the histo-remix. ";
  std::u16string new_sentence2 =
      u"Switching up the flow, as we add the prefix. ";
  std::u16string new_sentence3 =
      u"Everybody knows that we used to be six wives. ";
  ui::AXTreeID id_1 = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeUpdate update2;
  test::SetUpdateTreeID(&update2, id_1);
  ui::AXNodeData root;
  root.id = 1;

  ui::AXNodeData new_static_text1 = test::TextNode(/* id= */ 10, new_sentence1);
  ui::AXNodeData new_static_text2 = test::TextNode(/* id= */ 12, new_sentence2);
  ui::AXNodeData new_static_text3 = test::TextNode(/* id= */ 16, new_sentence3);

  root.child_ids = {new_static_text1.id, new_static_text2.id,
                    new_static_text3.id};
  update2.root_id = root.id;
  update2.nodes = {root, new_static_text1, new_static_text2, new_static_text3};
  controller().OnActiveAXTreeIDChanged(id_1, ukm::kInvalidSourceId, false);
  controller().OnAXTreeDistilled(tree_id_, {});
  AccessibilityEventReceived({update2});
  controller().OnAXTreeDistilled(
      id_1, {new_static_text1.id, new_static_text2.id, new_static_text3.id});
  controller().InitAXPositionWithNode(update2.nodes[1].id);

  // The nodes from the new tree are used.
  // Move to the last node of the content.
  controller().MovePositionToNextGranularity();
  controller().MovePositionToNextGranularity();

  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], new_static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)new_sentence2.length());

  previous_node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], new_static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)new_sentence1.length());

  // We're at the beginning of the content again, so the first sentence
  // should be retrieved next.
  previous_node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], new_static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)new_sentence1.length());

  // After navigating previous text, navigating forwards should continue
  // to work as expected.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], new_static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)new_sentence2.length());

  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], new_static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)new_sentence3.length());

  // Attempt to move to another node.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_ReturnsExpectedNodes) {
  std::u16string sentence1 = u"See the line where the sky meets the sea? ";
  std::u16string sentence2 = u"It calls me. ";
  std::u16string sentence3 = u"And no one knows how far it goes.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  // Move to the last granularity of the content.
  controller().MovePositionToNextGranularity();
  std::vector<ui::AXNodeID> next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);

  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)sentence2.length());

  previous_node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)sentence1.length());

  // We're at the beginning of the content again, so the first sentence
  // should be retrieved next.
  previous_node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)sentence1.length());

  // After navigating previous text, navigating forwards should continue
  // to work as expected.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence2.length());

  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Attempt to move to another node.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest, GetPreviousText_EmptyTree) {
  // If InitAXPosition hasn't been called, GetPreviousText should return
  // nothing.
  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 0);

  // GetCurrentTextStartIndex and GetCurrentTextEndIndex should return -1  on an
  // invalid id.
  EXPECT_EQ(controller().GetCurrentTextStartIndex(0), -1);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(0), -1);
}

TEST_F(
    ReadAnythingAppControllerTest,
    MoveToPreviousGranularityAndGetText_WhenFirstInitialized_StillReturnsFirstGranularity) {
  std::u16string sentence1 = u"This is a sentence. ";
  std::u16string sentence2 = u"This is another sentence. ";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);

  SendUpdateAndDistillNodes({static_text1, static_text2});

  // If we haven't called moveToNextGranularity, controller().GetCurrentText()
  // should still return the first granularity.
  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 1);
  EXPECT_EQ((int)previous_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)sentence1.length());
}

TEST_F(ReadAnythingAppControllerTest,
       GetCurrentText_WhenGranularityWasInitiallySkipped_ReturnsText) {
  std::u16string sentence1 = u"See the line where the sky meets the sea? ";
  std::u16string sentence2 = u"It calls me. ";
  std::u16string sentence3 = u"And no one knows how far it goes.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  // Move to third node
  controller().MovePositionToNextGranularity();
  controller().MovePositionToNextGranularity();
  EXPECT_EQ((int)controller().GetCurrentText()[0], static_text3.id);
  EXPECT_EQ((int)controller().GetCurrentText().size(), 1);

  // Move to second node which was initially skipped
  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();
  EXPECT_EQ(previous_node_ids[0], static_text2.id);
  EXPECT_EQ((int)previous_node_ids.size(), 1);
}

TEST_F(ReadAnythingAppControllerTest,
       GetPreviousText_SentenceSplitAcrossMultipleNodes) {
  std::u16string sentence1 = u"The wind is howling like this ";
  std::u16string sentence2 = u"swirling storm ";
  std::u16string sentence3 = u"inside.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  controller().GetCurrentText();
  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();

  // The first segment was returned correctly.
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)sentence1.length());

  // The second segment was returned correctly.
  EXPECT_EQ(previous_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[1]),
            (int)sentence2.length());

  // The third segment was returned correctly.
  EXPECT_EQ(previous_node_ids[2], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[2]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[2]),
            (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  std::vector<ui::AXNodeID> next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetPreviousText_SentenceSplitAcrossTwoNodes) {
  std::u16string sentence1 = u"And I am almost ";
  std::u16string sentence2 = u"there. ";
  std::u16string sentence3 = u"I am almost there.";

  ui::AXNodeData static_text1 = test::TextNode(/*id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/*id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/*id= */ 4, sentence3);

  SendUpdateAndDistillNodes({static_text1, static_text2, static_text3});

  // Move to last granularity.
  controller().MovePositionToNextGranularity();
  std::vector<ui::AXNodeID> previous_node_ids =
      MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)previous_node_ids.size(), 2);

  // Returns the 2nd segment correctly.
  EXPECT_EQ(previous_node_ids[1], static_text2.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[1]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[1]),
            (int)sentence2.length());

  // Returns the 1st segment correctly.
  EXPECT_EQ(previous_node_ids[0], static_text1.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(previous_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(previous_node_ids[0]),
            (int)sentence1.length());

  // After moving forward again, the third segment was returned correctly.
  // The third segment was returned correctly after getting the next text.
  std::vector<ui::AXNodeID> next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 1);
  EXPECT_EQ(next_node_ids[0], static_text3.id);
  EXPECT_EQ(controller().GetCurrentTextStartIndex(next_node_ids[0]), 0);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(next_node_ids[0]),
            (int)sentence3.length());

  // Nodes are empty at the end of the new tree.
  next_node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)next_node_ids.size(), 0);
}

TEST_F(ReadAnythingAppControllerTest,
       GetAccessibleBoundary_MaxLengthCutsOffSentence_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index =
      controller().GetAccessibleBoundary(sentence, first_sentence.length() - 3);
  EXPECT_TRUE(index < first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), u"This is a normal ");
}

TEST_F(ReadAnythingAppControllerTest,
       GetAccessibleBoundary_TextLongerThanMaxLength_ReturnsCorrectIndex) {
  const std::u16string first_sentence = u"This is a normal sentence. ";
  const std::u16string second_sentence = u"This is a second sentence.";

  const std::u16string sentence = first_sentence + second_sentence;
  size_t index = controller().GetAccessibleBoundary(
      sentence, first_sentence.length() + second_sentence.length() - 5);
  EXPECT_EQ(index, first_sentence.length());
  EXPECT_EQ(sentence.substr(0, index), first_sentence);
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetAccessibleBoundary_MaxLengthCutsOffSentence_OnlyOneSentence_ReturnsCorrectIndex) {
  const std::u16string sentence = u"Hello, this is a normal sentence.";

  size_t index = controller().GetAccessibleBoundary(sentence, 12);
  EXPECT_TRUE(index < sentence.length());
  EXPECT_EQ(sentence.substr(0, index), u"Hello, ");
}

TEST_F(ReadAnythingAppControllerTest, GetNextValidPosition) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(/*id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/*id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/*id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);

  // Getting the next node position shouldn't update the current AXPosition.
  new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingAppControllerTest, GetNextValidPosition_SkipsNonTextNode) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(/*id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/*id= */ 4, sentence2);

  ui::AXNodeData empty_node;
  empty_node.id = 3;

  InitializeWithAndProcessNodes({static_text1, empty_node, static_text2});

  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextValidPosition_SkipsNonDistilledNode) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  SendUpdateWithNodes({static_text1, static_text2, static_text3});
  // Don't distill the node with id 3.
  ProcessDisplayNodes({static_text1.id, static_text3.id});
  controller().InitAXPositionWithNode(static_text1.id);
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text3.id);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextValidPosition_SkipsNodeWithHTMLTag) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);

  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  static_text2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");

  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text3.id);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextValidPosition_ReturnsNullPositionAtEndOfTree) {
  std::u16string sentence1 = u"This is a sentence.";
  ui::AXNodeData static_text = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData empty_node1;
  empty_node1.id = 3;
  ui::AXNodeData empty_node2;
  empty_node2.id = 4;
  InitializeWithAndProcessNodes({static_text, empty_node1, empty_node2});

  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_TRUE(new_position->IsNullPosition());
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetNextValidPosition_AfterGetNextNodesButBeforeGetCurrentText_UsesCurrentGranularity) {
  std::u16string sentence1 = u"But from up here. The ";
  std::u16string sentence2 = u"world ";
  std::u16string sentence3 =
      u"looks so small. And suddenly life seems so clear. And from up here. "
      u"You coast past it all. The obstacles just disappear.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  a11y::ReadAloudCurrentGranularity current_granularity = GetNextNodes();
  // Expect that current_granularity contains static_text1
  // Expect that the indices aren't returned correctly
  // Expect that GetNextValidPosition fails without inserted the granularity.
  // The first segment was returned correctly.
  EXPECT_EQ((int)current_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(current_granularity.node_ids, static_text1.id));
  EXPECT_EQ(controller().GetCurrentTextStartIndex(static_text1.id), -1);
  EXPECT_EQ(controller().GetCurrentTextEndIndex(static_text1.id), -1);

  // Get the next position without using the current granularity. This
  // simulates getting the next node position from within GetNextNode if
  // the current granularity hasn't yet been added to the list processed
  // granularities. This should return the ID for static_text1, even though
  // it's already been used because the current granularity isn't being used.
  ui::AXNodePosition::AXPositionInstance new_position = GetNextNodePosition();
  EXPECT_EQ(new_position->anchor_id(), static_text1.id);

  // Now get the next position using the correct current granularity. This
  // simulates calling GetNextNodePosition from within GetNextNodes before
  // the nodes have been added to the list of processed granularities. This
  // should correctly return the next node in the tree.
  new_position = GetNextNodePosition(current_granularity);
  EXPECT_EQ(new_position->anchor_id(), static_text2.id);
}

TEST_F(ReadAnythingAppControllerTest,
       GetNextNodes_AfterResetReadAloudState_StartsOver) {
  std::u16string sentence1 = u"Where the north wind meets the sea. ";
  std::u16string sentence2 = u"There's a river full of memory. ";
  std::u16string sentence3 = u"Sleep my darling safe and sound. ";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  // Get first and second granularity.
  a11y::ReadAloudCurrentGranularity first_granularity = GetNextNodes();
  EXPECT_EQ((int)first_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(first_granularity.node_ids, static_text1.id));
  EXPECT_EQ(first_granularity.text, sentence1);
  a11y::ReadAloudCurrentGranularity next_granularity = GetNextNodes();
  EXPECT_EQ((int)next_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(next_granularity.node_ids, static_text2.id));
  EXPECT_EQ(next_granularity.text, sentence2);

  // If we init without resetting we should just go to the next sentence
  controller().InitAXPositionWithNode(static_text1.id);
  a11y::ReadAloudCurrentGranularity last_granularity = GetNextNodes();
  EXPECT_EQ((int)last_granularity.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(last_granularity.node_ids, static_text3.id));
  EXPECT_EQ(last_granularity.text, sentence3);

  // After reset and then init, we should get the first sentence again.
  read_aloud_model().ResetReadAloudState();
  controller().InitAXPositionWithNode(static_text1.id);
  a11y::ReadAloudCurrentGranularity after_reset = GetNextNodes();
  EXPECT_EQ((int)after_reset.node_ids.size(), 1);
  EXPECT_TRUE(base::Contains(after_reset.node_ids, static_text1.id));
  EXPECT_EQ(first_granularity.text, sentence1);
}

testing::Matcher<ReadAloudTextSegment> TextSegmentMatcher(ui::AXNodeID id,
                                                          int text_start,
                                                          int text_end) {
  return testing::AllOf(
      ::testing::Field(&ReadAloudTextSegment::id, ::testing::Eq(id)),
      ::testing::Field(&ReadAloudTextSegment::text_start,
                       ::testing::Eq(text_start)),
      ::testing::Field(&ReadAloudTextSegment::text_end,
                       ::testing::Eq(text_end)));
}

TEST_F(ReadAnythingAppControllerTest,
       GetHighlightForCurrentSegmentIndex_ReturnsCorrectNodes) {
  // Text indices             0 123456789012345678901
  std::u16string sentence = u"I\'m crossing the line!";
  ui::AXNodeData static_text = test::TextNode(/*id= */ 2, sentence);

  InitializeWithAndProcessNodes({static_text});

  // Before there are any processed granularities, GetHighlightStartIndex
  // should return an invalid id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Storing as a separate variable so we don't need to cast every time.
  int sentence_length = (int)sentence.length();

  // Since we just have one node with one text segment, the returned index
  // should equal the passed parameter.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text.id, 0, 4)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(3, false),
              ElementsAre(TextSegmentMatcher(static_text.id, 3, 4)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(7, false),
              ElementsAre(TextSegmentMatcher(static_text.id, 7, 13)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence_length - 1, false),
              ElementsAre(TextSegmentMatcher(static_text.id, 21, 22)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence_length, false),
              IsEmpty());
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetHighlightForCurrentSegmentIndex_SentenceSpansMultipleNodes_ReturnsCorrectNodes) {
  // Text indices:             0123456789012345678901234567890
  std::u16string sentence1 = u"Never feel heavy ";
  std::u16string sentence2 = u"or earthbound, ";
  std::u16string sentence3 = u"no worries or doubts interfere.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  // Before there are any processed granularities,
  // GetHighlightForCurrentSegmentIndex should return an empty array.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 3);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 6)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(7, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 7, 11)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 16, 17)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length(), false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 3)));

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + 1, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 1, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(26, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 9, 15)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 14, 15)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length(), false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 0, 3)));

  // Spot check that indices in sentence 3 map to the third node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() + 1, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 1, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(40, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 8, 11)));
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(
          sentence1.length() + sentence2.length() + sentence3.length() - 1,
          false),
      ElementsAre(TextSegmentMatcher(static_text3.id, 30, 31)));
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(
          sentence1.length() + sentence2.length() + sentence3.length(), false),
      IsEmpty());

  // Out-of-bounds nodes return an empty array.
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(
          sentence1.length() + sentence2.length() + sentence3.length() + 1,
          false),
      IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(535, false),
              IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(-10, false),
              IsEmpty());
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetHighlightForCurrentSegmentIndex_NodeSpansMultipleSentences_ReturnsCorrectNodes) {
  // Text indices:            0 12345678901234 5678901234
  std::u16string segment1 = u"I\'m taking what\'s mine! ";
  // Text indices:            012345678901234567890123456
  std::u16string segment2 = u"Every drop, every smidge. ";
  // Text indices:            0123 45678901234 5678901234567890123456
  std::u16string segment3 = u"If I\'m burning a bridge, let it burn. ";
  // Text indices:            01234 56789012345678901
  std::u16string segment4 = u"But I\'m crossing the ";

  std::u16string node1_text = segment1 + segment2 + segment3 + segment4;
  std::u16string node2_text = u"line.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, node1_text);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, node2_text);

  InitializeWithAndProcessNodes({static_text1, static_text2});

  // Before there are any processed granularities, GetHighlightStartIndex
  // should return an invalid id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Storing as a separate variable so we don't need to cast every time.
  int segment1_length = (int)segment1.length();
  int segment2_length = (int)segment2.length();
  int segment3_length = (int)segment3.length();
  int segment4_partial_length = (int)segment4.length();
  int segment4_full_length = (int)segment4.length() + (int)node2_text.length();

  // For the first node in the first segment, the returned index should equal
  // the passed parameter.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 4)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(6, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 6, 11)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(15, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 15, 16)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment1_length - 1, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id, segment1_length - 1, segment1_length)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment1_length, false),
              IsEmpty());

  // Move to segment 2.
  node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // For the second segment, the boundary index will have reset for the new
  // speech segment. The correct highlight start index is the index that the
  // boundary index within the segment corresponds to within the node.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, segment1_length,
                                             segment1_length + 6)));
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(10, false),
      ElementsAre(TextSegmentMatcher(static_text1.id, segment1_length + 10,
                                     segment1_length + 12)));
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(13, false),
      ElementsAre(TextSegmentMatcher(static_text1.id, segment1_length + 13,
                                     segment1_length + 18)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment2_length - 1, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id, segment1_length + segment2_length - 1,
                  segment1_length + segment2_length)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment1_length + segment2_length, false),
              IsEmpty());

  // Move to segment 3.
  node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // For the third segment, the boundary index will have reset for the new
  // speech segment. The correct highlight start index is the index that the
  // boundary index within the segment corresponds to within the node.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id, segment1_length + segment2_length,
                  segment1_length + segment2_length + 3)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(9, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id, segment1_length + segment2_length + 9,
                  segment1_length + segment2_length + 15)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(13, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id, segment1_length + segment2_length + 13,
                  segment1_length + segment2_length + 15)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment3_length - 1, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id,
                  segment1_length + segment2_length + segment3_length - 1,
                  segment1_length + segment2_length + segment3_length)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment1_length + segment2_length + segment3_length, false),
              IsEmpty());

  // Move to segment 4.
  node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)node_ids.size(), 2);
  EXPECT_EQ((int)node_ids[0], static_text1.id);
  EXPECT_EQ((int)node_ids[1], static_text2.id);

  // For the fourth segment, there are two nodes. For the first node,
  // the correct highlight start corresponds to the index within the first
  // node.
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
      ElementsAre(TextSegmentMatcher(
          static_text1.id, segment1_length + segment2_length + segment3_length,
          segment1_length + segment2_length + segment3_length + 4)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(2, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id,
                  segment1_length + segment2_length + segment3_length + 2,
                  segment1_length + segment2_length + segment3_length + 4)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(8, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id,
                  segment1_length + segment2_length + segment3_length + 8,
                  segment1_length + segment2_length + segment3_length + 17)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment4_partial_length - 1, false),
              ElementsAre(TextSegmentMatcher(
                  static_text1.id,
                  segment1_length + segment2_length + segment3_length +
                      segment4_partial_length - 1,
                  segment1_length + segment2_length + segment3_length +
                      segment4_partial_length)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment1_length + segment2_length + segment3_length +
                      segment4_partial_length,
                  false),
              IsEmpty());

  // For the second node, the highlight index corresponds to the position within
  // the second node.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment4_partial_length, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 5)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment4_partial_length + 2, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 2, 5)));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment4_full_length - 1, false),
              ElementsAre(TextSegmentMatcher(static_text2.id,
                                             (int)node2_text.length() - 1,
                                             (int)node2_text.length())));

  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  segment4_full_length, false),
              IsEmpty());
}

TEST_F(ReadAnythingAppControllerTest,
       GetHighlightForCurrentSegmentIndex_AfterNext_ReturnsCorrectNodes) {
  // Text indices:             012345678901234567890123456789012
  std::u16string sentence1 = u"Never feel heavy or earthbound. ";
  std::u16string sentence2 = u"No worries or doubts ";
  std::u16string sentence3 = u"interfere.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 6)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(7, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 7, 11)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 31, 32)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length(), false),
              IsEmpty());

  // Move to the next granularity.
  node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(7, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 7, 11)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence2.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 20, 21)));

  // Spot check that indices in sentence 3 map to the third node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence2.length() + 1, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 1, 10)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(27, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 6, 10)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence2.length() + sentence3.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 9, 10)));

  // Out-of-bounds nodes return invalid.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence2.length() + sentence3.length() + 1, false),
              IsEmpty());
}

TEST_F(ReadAnythingAppControllerTest,
       GetHighlightForCurrentSegmentIndex_AfterPrevious_ReturnsCorrectNodes) {
  // Text indices:             01234567890123456789012345678901234567890
  std::u16string sentence1 = u"There's nothing but you ";
  std::u16string sentence2 = u"looking down on the view from up here. ";
  std::u16string sentence3 = u"Stretch out with the wind behind you.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Move forward.
  node_ids = MoveToNextGranularityAndGetText();
  EXPECT_EQ((int)node_ids.size(), 1);

  // Spot check that indices 0->sentence3.length() map to the third node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 0, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(7, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 7, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence3.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text3.id, 36, 37)));

  // Move backwards.
  node_ids = MoveToPreviousGranularityAndGetText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Spot check that indices in sentence 1 map to the first node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(6, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 6, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 23, 24)));

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + 1, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 1, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(27, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 3, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() - 1, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 38, 39)));

  // Out-of-bounds nodes return invalid.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() + 1, false),
              IsEmpty());
}

TEST_F(ReadAnythingAppControllerTest,
       GetHighlightForCurrentSegmentIndex_MultinodeWords_ReturnsCorrectLength) {
  std::u16string word1 = u"Stretch ";
  std::u16string word2 = u"out ";
  std::u16string word3 = u"with ";
  std::u16string word4 = u"the ";
  std::u16string word5 = u"wind ";
  std::u16string word6 = u"beh";
  std::u16string word7 = u"ind ";
  std::u16string word8 = u"you.";
  std::u16string sentence1 = word1 + word2 + word3 + word4 + word5 + word6;
  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  std::u16string sentence2 = word7 + word8;
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);

  InitializeWithAndProcessNodes({static_text1, static_text2});

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(1, false),
              IsEmpty());

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 2);

  // Throughout first word.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(2, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 2, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  (int)word1.length() - 2, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 6, 8)));

  // Throughout third word.
  int third_word_index = sentence1.find(word3);
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  third_word_index, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 12, 17)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  third_word_index + 2, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 14, 17)));

  // Words split across node boundaries
  int sixth_word_index = sentence1.find(word6);
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sixth_word_index, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 26, 29),
                          TextSegmentMatcher(static_text2.id, 0, 4)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sixth_word_index + 2, false),
              ElementsAre(TextSegmentMatcher(static_text1.id, 28, 29),
                          TextSegmentMatcher(static_text2.id, 0, 4)));

  int seventh_word_index = sentence1.length();
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  seventh_word_index, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 4)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  seventh_word_index + 2, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 2, 4)));

  int last_word_index = sentence1.length() + sentence2.find(word8);
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  last_word_index, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 4, 8)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  last_word_index + 2, false),
              ElementsAre(TextSegmentMatcher(static_text2.id, 6, 8)));

  // Boundary testing.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(-5, false),
              IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length(), false),
              IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() + 1, false),
              IsEmpty());
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetHighlightForCurrentSegmentIndex_PhrasesEnabled_NoModel_SentenceSpansMultipleNodes_ReturnsCorrectNodes) {
  scoped_feature_list_.InitWithFeatures(
      {features::kReadAnythingReadAloud,
       features::kReadAnythingReadAloudPhraseHighlighting},
      {});

  EXPECT_TRUE(controller().IsPhraseHighlightingEnabled());
  // Text indices:             0123456789012345678901234567890
  std::u16string sentence1 = u"Never feel heavy ";
  std::u16string sentence2 = u"or earthbound, ";
  std::u16string sentence3 = u"no worries or doubts interfere.";

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});
  controller().PreprocessTextForSpeech();

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 3);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, true),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 17)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(7, true),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 17)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() - 1, true),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 17)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length(), true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 15),
                          TextSegmentMatcher(static_text3.id, 0, 3)));

  // Spot check that indices in sentence 2 map to the second node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + 1, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 15),
                          TextSegmentMatcher(static_text3.id, 0, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(26, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 15),
                          TextSegmentMatcher(static_text3.id, 0, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() - 1, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 15),
                          TextSegmentMatcher(static_text3.id, 0, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length(), true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 15),
                          TextSegmentMatcher(static_text3.id, 0, 3)));

  // Spot check that indices in sentence 3 map to the third node id.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(
                  sentence1.length() + sentence2.length() + 1, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 0, 15),
                          TextSegmentMatcher(static_text3.id, 0, 3)));
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(40, true),
              ElementsAre(TextSegmentMatcher(static_text3.id, 3, 21)));
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(
          sentence1.length() + sentence2.length() + sentence3.length() - 1,
          true),
      ElementsAre(TextSegmentMatcher(static_text3.id, 21, 31)));
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(
          sentence1.length() + sentence2.length() + sentence3.length(), true),
      IsEmpty());

  // Out-of-bounds nodes return an empty array.
  EXPECT_THAT(
      read_aloud_model().GetHighlightForCurrentSegmentIndex(
          sentence1.length() + sentence2.length() + sentence3.length() + 1,
          true),
      IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(535, true),
              IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(-10, true),
              IsEmpty());
}

TEST_F(
    ReadAnythingAppControllerTest,
    GetHighlightForCurrentSegmentIndex_PhrasesEnabled_ValidModel_SentenceSpansMultipleNodes_ReturnsCorrectNodes) {
  scoped_feature_list_.InitWithFeatures(
      {features::kReadAnythingReadAloud,
       features::kReadAnythingReadAloudPhraseHighlighting},
      {});

  controller().UpdateDependencyParserModel(GetValidModelFile());
  DependencyParserModel& model =
      controller().GetDependencyParserModelForTesting();

  EXPECT_TRUE(model.IsAvailable());

  EXPECT_TRUE(controller().IsPhraseHighlightingEnabled());

  // Text indices:             0123456789012345678901234567890
  std::u16string sentence1 = u"Never feel heavy or ";
  std::u16string sentence2 = u"earthbound, no ";
  std::u16string sentence3 = u"worries or doubts interfere.";

  // Expected phrases:
  // Never feel heavy or earthbound, /no worries or doubts interfere.
  // Expected phrase breaks: 0, 32

  ui::AXNodeData static_text1 = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(/* id= */ 3, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(/* id= */ 4, sentence3);

  InitializeWithAndProcessNodes({static_text1, static_text2, static_text3});
  controller().PreprocessTextForSpeech();

  // Wait till all async calculations complete.
  task_environment_.RunUntilIdle();

  std::vector<ui::AXNodeID> node_ids = controller().GetCurrentText();
  EXPECT_EQ((int)node_ids.size(), 3);

  // First character (N) => first phrase
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(0, true),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 20),
                          TextSegmentMatcher(static_text2.id, 0, 12)));

  // 20th character (e of earthbound) => first phrase
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(20, true),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 20),
                          TextSegmentMatcher(static_text2.id, 0, 12)));

  // 31st character (space before "no") => first phrase
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(31, true),
              ElementsAre(TextSegmentMatcher(static_text1.id, 0, 20),
                          TextSegmentMatcher(static_text2.id, 0, 12)));

  // 32nd character (n of no) => second phrase
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(32, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 12, 15),
                          TextSegmentMatcher(static_text3.id, 0, 28)));

  // 35th character (w of worries) => second phrase
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(35, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 12, 15),
                          TextSegmentMatcher(static_text3.id, 0, 28)));

  // 62nd character (final .) => second phrase
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(62, true),
              ElementsAre(TextSegmentMatcher(static_text2.id, 12, 15),
                          TextSegmentMatcher(static_text3.id, 0, 28)));

  // 63rd character (past the end of the sentence) => empty
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(63, true),
              IsEmpty());

  // Invalid indices.
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(535, true),
              IsEmpty());
  EXPECT_THAT(read_aloud_model().GetHighlightForCurrentSegmentIndex(-10, true),
              IsEmpty());
}

TEST_F(ReadAnythingAppControllerTest,
       GetDependencyParserModel_UnavailableWithoutModelFile) {
  DependencyParserModel& model =
      controller().GetDependencyParserModelForTesting();
  EXPECT_FALSE(model.IsAvailable());
}

TEST_F(ReadAnythingAppControllerTest,
       GetDependencyParserModel_AvailableWithValidModelFile) {
  controller().UpdateDependencyParserModel(GetValidModelFile());
  DependencyParserModel& model =
      controller().GetDependencyParserModelForTesting();

  EXPECT_TRUE(model.IsAvailable());
}

TEST_F(ReadAnythingAppControllerTest,
       GetDependencyParserModel_UnavailableWithInvalidModelFile) {
  controller().UpdateDependencyParserModel(GetInvalidModelFile());
  DependencyParserModel& model =
      controller().GetDependencyParserModelForTesting();

  EXPECT_FALSE(model.IsAvailable());
}

class ReadAnythingAppControllerScreen2xDataCollectionModeTest
    : public ReadAnythingAppControllerTest {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    scoped_feature_list_.InitWithFeatures(
        {features::kDataCollectionModeForScreen2x}, {});
    ChromeRenderViewTest::SetUp();

    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(GetMainFrame());
    controller_ = ReadAnythingAppController::Install(render_frame);

    // Set the page handler for testing.
    controller_->page_handler_.reset();
    controller_->page_handler_.Bind(page_handler_.BindNewPipeAndPassRemote());

    // Set distiller for testing.
    std::unique_ptr<AXTreeDistiller> distiller =
        std::make_unique<MockAXTreeDistiller>(render_frame);
    controller_->distiller_ = std::move(distiller);
    distiller_ =
        static_cast<MockAXTreeDistiller*>(controller_->distiller_.get());

    tree_id_ = ui::AXTreeID::CreateNewAXTreeID();
    ui::AXTreeUpdate snapshot;
    ui::AXNodeData root;
    root.id = 1;
    snapshot.root_id = root.id;
    snapshot.nodes = {root};
    test::SetUpdateTreeID(&snapshot, tree_id_);
    AccessibilityEventReceived({snapshot});
    controller().OnAXTreeDistilled(tree_id_, {});
  }

  void SetScreenAIServiceReady() { controller_->ScreenAIServiceReady(); }
};

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DoesNotDistillImmediately) {
  // When the AXTreeID changes, the controller usually will call
  // distiller_->Distill(). However, with the data collection mode enabled,
  // Distill() is not called immediately.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(0);
  SetScreenAIServiceReady();
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DistillsAfterDelay) {
  // When the AXTreeID changes, and 30s pass, the controller calls
  // distiller_->Distill().
  EXPECT_CALL(*distiller_, Distill).Times(1);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(1);
  SetScreenAIServiceReady();
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSincePageLoadForDataCollection + 1));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DistillsAfterDelayScreenAIServiceReady) {
  // When the AXTreeID changes, and 30s pass, the controller calls
  // distiller_->Distill() once the screenAI service is ready.
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSincePageLoadForDataCollection + 1));

  EXPECT_CALL(*distiller_, Distill).Times(1);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(1);
  SetScreenAIServiceReady();
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DoesNotDistillIfScreenAIServiceNotReady) {
  // When the AXTreeID changes, and 30s pass, the controller does not call
  // distiller_->Distill() as the screenAI service is not ready.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(0);
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSincePageLoadForDataCollection + 1));
  Mock::VerifyAndClearExpectations(distiller_);
}

// TODO(crbug.com/355925253): Update the test when time constants are finalized.
// This test is not meaningful now that the constants are equal.
TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DISABLED_DistillsAfterDelayWhenTreeIsStable) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData root;
  root.id = 1;
  ui::AXNodeData node;
  node.id = 2;
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  // TODO(crbug.com/355925253): Update all comments with time after time
  // constants are finalized.
  // When the tree is stable for 10s, the controller still waits for 30s after
  // page load completion.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(0);
  SetScreenAIServiceReady();
  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  AccessibilityEventReceived({update}, {load_complete});
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection + 1));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DistillsAfterDelayWhenTreeIsNotStable) {
  ui::AXTreeUpdate update;
  test::SetUpdateTreeID(&update, tree_id_);
  ui::AXNodeData root;
  root.id = 1;
  ui::AXNodeData node;
  node.id = 2;
  root.child_ids = {node.id};
  update.nodes = {root, node};
  update.root_id = root.id;

  // If the tree changes in the 30s after page load completion, distillation is
  // delayed for another 10s.
  EXPECT_CALL(*distiller_, Distill).Times(0);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(0);
  SetScreenAIServiceReady();
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSincePageLoadForDataCollection - 1));
  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({update}, {load_complete});
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection - 1));
  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DoesNotDistillAfterDelayIfTreeIsUnstable) {
  std::vector<ui::AXTreeUpdate> updates;
  std::vector<int> child_ids = {};
  for (int i = 0; i < 2; i++) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_id_);
    ui::AXNodeData root;
    root.id = 1;
    ui::AXNodeData node;
    node.id = i + 2;
    child_ids.push_back(node.id);
    root.child_ids = child_ids;
    update.nodes = {root, node};
    update.root_id = root.id;
    updates.push_back(update);
  }

  // When the load complete event is received, and the tree remains unstable,
  // the controller does not call distiller_->Distill().
  EXPECT_CALL(*distiller_, Distill).Times(0);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(0);
  SetScreenAIServiceReady();

  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[0]}, {load_complete});
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection - 1));

  AccessibilityEventReceived({updates[1]});
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection / 2));

  Mock::VerifyAndClearExpectations(distiller_);
}

TEST_F(ReadAnythingAppControllerScreen2xDataCollectionModeTest,
       DistillsAfter30sDelayEvenIfTreeIsUnstable) {
  std::vector<ui::AXTreeUpdate> updates;
  std::vector<int> child_ids = {};
  for (int i = 0; i < 4; i++) {
    ui::AXTreeUpdate update;
    test::SetUpdateTreeID(&update, tree_id_);
    ui::AXNodeData root;
    root.id = 1;
    ui::AXNodeData node;
    node.id = i + 2;
    child_ids.push_back(node.id);
    root.child_ids = child_ids;
    update.nodes = {root, node};
    update.root_id = root.id;
    updates.push_back(update);
  }

  // When the load complete event is received, even if the tree remains
  // unstable, the controller does not calls distiller_->Distill() after 30s.
  EXPECT_CALL(*distiller_, Distill).Times(1);
  EXPECT_CALL(page_handler_, OnScreenshotRequested).Times(1);
  SetScreenAIServiceReady();

  ui::AXEvent load_complete(0, ax::mojom::Event::kLoadComplete);
  AccessibilityEventReceived({updates[0]}, {load_complete});
  controller().OnActiveAXTreeIDChanged(tree_id_, ukm::kInvalidSourceId, false);
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection - 1));

  AccessibilityEventReceived({updates[1]});
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection - 1));

  AccessibilityEventReceived({updates[2]});
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection - 1));

  AccessibilityEventReceived({updates[3]});
  task_environment_.FastForwardBy(
      base::Seconds(kSecondsElapsedSinceTreeChangedForDataCollection + 1));

  Mock::VerifyAndClearExpectations(distiller_);
}
