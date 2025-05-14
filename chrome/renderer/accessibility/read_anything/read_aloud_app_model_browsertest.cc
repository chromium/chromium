// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_aloud_app_model.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "read_anything_test_utils.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree_manager.h"

class ReadAnythingReadAloudAppModelTest : public ChromeRenderViewTest {
 public:
  ReadAnythingReadAloudAppModelTest() = default;
  ~ReadAnythingReadAloudAppModelTest() override = default;
  ReadAnythingReadAloudAppModelTest(const ReadAnythingReadAloudAppModelTest&) =
      delete;
  ReadAnythingReadAloudAppModelTest& operator=(
      const ReadAnythingReadAloudAppModelTest&) = delete;

  void SetUp() override {
    ChromeRenderViewTest::SetUp();
    model_ = new ReadAloudAppModel();
  }

  bool SpeechPlaying() { return model_->speech_playing(); }

  void SetSpeechPlaying(bool speech_playing) {
    model_->set_speech_playing(speech_playing);
  }

  double SpeechRate() { return model_->speech_rate(); }

  void SetSpeechRate(double speech_rate) {
    model_->set_speech_rate(speech_rate);
  }

  const base::Value::List& EnabledLanguages() {
    return model_->languages_enabled_in_pref();
  }

  void SetLanguageEnabled(const std::string& lang, bool enabled) {
    model_->SetLanguageEnabled(lang, enabled);
  }

  const base::Value::Dict& Voices() { return model_->voices(); }

  void SetVoice(const std::string& voice, const std::string& lang) {
    model_->SetVoice(voice, lang);
  }

  int HighlightGranularity() { return model_->highlight_granularity(); }

  void SetHighlightGranularity(int granularity) {
    model_->set_highlight_granularity(granularity);
  }

  bool IsHighlightOn() { return model_->IsHighlightOn(); }

  std::string DefaultLanguage() { return model_->default_language_code(); }

  void SetDefaultLanguage(std::string lang) {
    model_->set_default_language_code(lang);
  }

  void LogSpeechStop(ReadAloudAppModel::ReadAloudStopSource source) {
    model_->LogSpeechStop(source);
  }

  void EnableReadAloud() {
    scoped_feature_list_.InitAndEnableFeature(features::kReadAnythingReadAloud);
  }

  a11y::ReadAloudCurrentGranularity GetNextNodes(
      const std::set<ui::AXNodeID>* current_nodes) {
    return model_->GetNextNodes(false, false, current_nodes);
  }

  ui::AXNodePosition::AXPositionInstance GetNextNodePosition(
      const std::set<ui::AXNodeID>* current_nodes,
      a11y::ReadAloudCurrentGranularity granularity =
          a11y::ReadAloudCurrentGranularity()) {
    return model().GetNextValidPositionFromCurrentPosition(
        granularity, false, false, current_nodes);
  }

  void InitAXPositionWithNode(ui::AXNodeID id) {
    model_->InitAXPositionWithNode(tree_manager_->GetNode(id),
                                   tree_manager_->GetTreeID());
  }

  std::set<ui::AXNodeID> InitializeWithAndProcessNodes(
      std::vector<ui::AXNodeData> nodes) {
    std::vector<ui::AXNodeID> node_ids;
    for (ui::AXNodeData data : nodes) {
      node_ids.push_back(data.id);
    }

    // Root that will be parent for all the nodes.
    ui::AXNodeData root_data;
    root_data.id = 1;
    root_data.child_ids = node_ids;

    // Make sure the root data is included in the nodes list.
    nodes.insert(nodes.begin(), root_data);

    ui::AXTreeData tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();

    ui::AXTreeUpdate update;
    update.root_id = root_data.id;
    update.nodes = nodes;
    update.tree_data = tree_data;
    update.has_tree_data = true;

    auto tree = std::make_unique<ui::AXTree>(update);
    ui::AXTreeID tree_id = tree->GetAXTreeID();
    tree_manager_ = std::make_unique<ui::AXTreeManager>(std::move(tree));

    InitAXPositionWithNode(node_ids[0]);
    std::set<ui::AXNodeID> current_nodes;
    for (ui::AXNodeData data : nodes) {
      current_nodes.insert(data.id);
    }
    return current_nodes;
  }

  ReadAloudAppModel& model() { return *model_; }

 private:
  // ReadAloudAppModel constructor and destructor are private so it's
  // not accessible by std::make_unique.
  raw_ptr<ReadAloudAppModel> model_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;

  // A reference to the tree manager is needed so that the AXTree can stay
  // alive throughout the entire test cycle.
  std::unique_ptr<ui::AXTreeManager> tree_manager_;
};

// Read Aloud is currently only enabled by default on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ReadAnythingReadAloudAppModelTest, LogSpeechStop_WithoutReadAloud) {
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);
  EXPECT_EQ(0, histogram_tester.GetTotalSum(
                   ReadAloudAppModel::kSpeechStopSourceHistogramName));
}
#endif

TEST_F(ReadAnythingReadAloudAppModelTest, LogSpeechStop_WithReadAloud) {
  EnableReadAloud();
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);
  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
}

TEST_F(ReadAnythingReadAloudAppModelTest, SpeechPlaying) {
  EXPECT_FALSE(SpeechPlaying());

  SetSpeechPlaying(true);
  EXPECT_TRUE(SpeechPlaying());

  SetSpeechPlaying(false);
  EXPECT_FALSE(SpeechPlaying());
}

TEST_F(ReadAnythingReadAloudAppModelTest, SpeechRate) {
  EXPECT_EQ(SpeechRate(), 1);

  const double speech_rate1 = 0.5;
  SetSpeechRate(speech_rate1);
  EXPECT_EQ(SpeechRate(), speech_rate1);

  const double speech_rate2 = 1.2;
  SetSpeechRate(speech_rate2);
  EXPECT_EQ(SpeechRate(), speech_rate2);
}

TEST_F(ReadAnythingReadAloudAppModelTest, EnabledLanguages) {
  EXPECT_TRUE(EnabledLanguages().empty());

  const std::string enabled_lang = "fr";
  SetLanguageEnabled(enabled_lang, true);
  EXPECT_TRUE(base::Contains(EnabledLanguages(), enabled_lang));

  SetLanguageEnabled(enabled_lang, false);
  EXPECT_FALSE(base::Contains(EnabledLanguages(), enabled_lang));
}

TEST_F(ReadAnythingReadAloudAppModelTest, Voices) {
  EXPECT_TRUE(Voices().empty());

  const char* lang1 = "pt-br";
  const char* voice1 = "Mulan";
  const char* lang2 = "yue";
  const char* voice2 = "Shang";
  SetVoice(voice1, lang1);
  SetVoice(voice2, lang2);
  EXPECT_TRUE(base::Contains(Voices(), lang1));
  EXPECT_TRUE(base::Contains(Voices(), lang2));
  EXPECT_STREQ(Voices().FindString(lang1)->c_str(), voice1);
  EXPECT_STREQ(Voices().FindString(lang2)->c_str(), voice2);

  const char* voice3 = "Mushu";
  SetVoice(voice3, lang2);
  EXPECT_TRUE(base::Contains(Voices(), lang1));
  EXPECT_TRUE(base::Contains(Voices(), lang2));
  EXPECT_STREQ(Voices().FindString(lang2)->c_str(), voice3);
}

TEST_F(ReadAnythingReadAloudAppModelTest, Highlight) {
  EXPECT_EQ(HighlightGranularity(), 0);

  const int off = 1;
  SetHighlightGranularity(off);
  EXPECT_EQ(HighlightGranularity(), off);
  EXPECT_FALSE(IsHighlightOn());

  const int on = 0;
  SetHighlightGranularity(on);
  EXPECT_EQ(HighlightGranularity(), on);
  EXPECT_TRUE(IsHighlightOn());
}

TEST_F(ReadAnythingReadAloudAppModelTest, DefaultLanguageCode) {
  EXPECT_EQ(DefaultLanguage(), "en");

  const char* lang1 = "tr";
  SetDefaultLanguage(lang1);
  EXPECT_EQ(DefaultLanguage(), lang1);

  const char* lang2 = "hi";
  SetDefaultLanguage(lang2);
  EXPECT_EQ(DefaultLanguage(), lang2);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       GetNextValidPosition_UsesCurrentGranularity) {
  std::u16string sentence1 = u"But from up here. The ";
  std::u16string sentence2 = u"world ";
  std::u16string sentence3 =
      u"looks so small. And suddenly life seems so clear. And from up here. "
      u"You coast past it all. The obstacles just disappear.";

  static constexpr ui::AXNodeID kId1 = 2;
  static constexpr ui::AXNodeID kId2 = 3;
  static constexpr ui::AXNodeID kId3 = 4;
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  a11y::ReadAloudCurrentGranularity current_granularity =
      GetNextNodes(&current_nodes);
  // Expect that current_granularity contains static_text1
  // Expect that the indices aren't returned correctly
  // Expect that GetNextValidPosition fails without inserted the granularity.
  // The first segment was returned correctly.
  EXPECT_EQ(current_granularity.node_ids.size(), 1u);
  EXPECT_TRUE(base::Contains(current_granularity.node_ids, kId1));
  EXPECT_EQ(model().GetCurrentTextStartIndex(kId1), -1);
  EXPECT_EQ(model().GetCurrentTextEndIndex(kId1), -1);

  ui::AXNodePosition::AXPositionInstance new_position =
      GetNextNodePosition(&current_nodes);
  EXPECT_EQ(new_position->anchor_id(), kId2);

  // Simulate adding to the current granularity.
  current_granularity.AddText(kId2, 0, sentence2.length(), sentence2);

  // Now get the next position using the correct current granularity. This
  // simulates calling GetNextNodePosition from within GetNextNodes before
  // the nodes have been added to the list of processed granularities. This
  // should correctly return the next node in the tree.
  new_position = GetNextNodePosition(&current_nodes, current_granularity);
  EXPECT_EQ(new_position->anchor_id(), kId3);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       GetNextNodes_AfterResetReadAloudState_StartsOver) {
  std::u16string sentence1 = u"Where the north wind meets the sea. ";
  std::u16string sentence2 = u"There's a river full of memory. ";
  std::u16string sentence3 = u"Sleep my darling safe and sound. ";

  static constexpr ui::AXNodeID kId1 = 2;
  static constexpr ui::AXNodeID kId2 = 3;
  static constexpr ui::AXNodeID kId3 = 4;
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  // Get first and second granularity.
  a11y::ReadAloudCurrentGranularity first_granularity =
      GetNextNodes(&current_nodes);
  EXPECT_EQ(first_granularity.node_ids.size(), 1u);
  EXPECT_TRUE(base::Contains(first_granularity.node_ids, kId1));
  EXPECT_EQ(first_granularity.text, sentence1);
  a11y::ReadAloudCurrentGranularity next_granularity =
      GetNextNodes(&current_nodes);
  EXPECT_EQ(next_granularity.node_ids.size(), 1u);
  EXPECT_TRUE(base::Contains(next_granularity.node_ids, kId2));
  EXPECT_EQ(next_granularity.text, sentence2);

  // If we init without resetting we should just go to the next sentence
  InitAXPositionWithNode(kId1);
  a11y::ReadAloudCurrentGranularity last_granularity =
      GetNextNodes(&current_nodes);
  EXPECT_EQ(last_granularity.node_ids.size(), 1u);
  EXPECT_TRUE(base::Contains(last_granularity.node_ids, kId3));
  EXPECT_EQ(last_granularity.text, sentence3);

  // After reset and then init, we should get the first sentence again.
  model().ResetReadAloudState();
  InitAXPositionWithNode(kId1);
  a11y::ReadAloudCurrentGranularity after_reset = GetNextNodes(&current_nodes);
  EXPECT_EQ(after_reset.node_ids.size(), 1u);
  EXPECT_TRUE(base::Contains(after_reset.node_ids, kId1));
  EXPECT_EQ(first_granularity.text, sentence1);
}
