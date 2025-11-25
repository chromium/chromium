// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_aloud_app_model.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/renderer/accessibility/read_anything/read_aloud_traversal_utils.h"
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
    model_->SetSpeechPlaying(speech_playing);
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

  void DisableReadAloud() {
    scoped_feature_list_.InitWithFeatures({},
                                          {features::kReadAnythingReadAloud});
  }

  std::vector<ui::AXNodeID> MoveToNextGranularityAndGetText(
      const std::set<ui::AXNodeID>* current_nodes) {
    model().MovePositionToNextGranularity();
    return model().GetCurrentText(false, false, current_nodes).node_ids;
  }

  std::vector<ui::AXNodeID> MoveToPreviousGranularityAndGetText(
      const std::set<ui::AXNodeID>* current_nodes) {
    model().MovePositionToPreviousGranularity();
    return model().GetCurrentText(false, false, current_nodes).node_ids;
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

  std::vector<ReadAloudTextSegment> GetHighlightsAtIndex(
      int index,
      bool is_phrase_highlight = false) {
    return model().GetHighlightForCurrentSegmentIndex(index,
                                                      is_phrase_highlight);
  }

  void ExpectHighlightAtIndexEmpty(int index,
                                   bool is_phrase_highlight = false) {
    EXPECT_THAT(GetHighlightsAtIndex(index, is_phrase_highlight),
                testing::IsEmpty());
  }

  void ExpectPhraseHighlightAtIndexEmpty(int index) {
    ExpectHighlightAtIndexEmpty(index, true);
  }

  void ExpectHighlightAtIndexMatches(int highlight_index,
                                     std::vector<test::TextRange> text_ranges,
                                     bool is_phrase_highlight = false) {
    std::vector<testing::Matcher<ReadAloudTextSegment>> matchers;
    for (const auto& range : text_ranges) {
      matchers.push_back(test::TextSegmentMatcher(range));
    }

    EXPECT_THAT(GetHighlightsAtIndex(highlight_index, is_phrase_highlight),
                ElementsAreArray(matchers));
  }

  void ExpectPhraseHighlightAtIndexMatches(
      int highlight_index,
      std::vector<test::TextRange> text_ranges) {
    ExpectHighlightAtIndexMatches(highlight_index, text_ranges, true);
  }

  std::vector<ReadAloudTextSegment> GetCurrentTextSegments(
      const std::set<ui::AXNodeID>* current_nodes,
      bool is_pdf = false,
      bool is_docs = false) {
    return model_->GetCurrentTextSegments(is_pdf, is_docs, current_nodes);
  }

  ReadAloudAppModel& model() { return *model_; }

  static constexpr ui::AXNodeID kId1 = 2;
  static constexpr ui::AXNodeID kId2 = 3;
  static constexpr ui::AXNodeID kId3 = 4;

 protected:
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
  DisableReadAloud();
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, 0);
}
#endif

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_WithReadAloud_AudioDidNotStart_LogsDelay) {
  EnableReadAloud();
  SetSpeechPlaying(true);
  const auto delay = base::Milliseconds(25);
  task_environment_.FastForwardBy(delay);
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
  histogram_tester.ExpectUniqueTimeSample(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, delay, 1);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, 0);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_WithReadAloud_AudioDidStart_DoesNotLogDelay) {
  EnableReadAloud();
  SetSpeechPlaying(true);
  const auto delay = base::Milliseconds(12);
  task_environment_.FastForwardBy(delay);
  model().SetAudioCurrentlyPlaying(true);
  auto source = ReadAloudAppModel::ReadAloudStopSource::kCloseReadingMode;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, 0);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_WithReadAloud_LogsStopSourceWithSpeechNotPlaying) {
  EnableReadAloud();
  auto source = ReadAloudAppModel::ReadAloudStopSource::kFinishContent;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
}

TEST_F(ReadAnythingReadAloudAppModelTest,
       LogSpeechStop_WithReadAloud_LogsStopSourceWithSpeechPlaying) {
  EnableReadAloud();
  SetSpeechPlaying(true);
  auto source = ReadAloudAppModel::ReadAloudStopSource::kFinishContent;
  base::HistogramTester histogram_tester;

  LogSpeechStop(source);

  histogram_tester.ExpectUniqueSample(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, source, 1);
}

TEST_F(ReadAnythingReadAloudAppModelTest, SetAudioCurrentlyPlaying_LogsDelay) {
  EnableReadAloud();
  SetSpeechPlaying(true);
  const auto delay = base::Milliseconds(27);
  task_environment_.FastForwardBy(delay);
  base::HistogramTester histogram_tester;

  model().SetAudioCurrentlyPlaying(true);

  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kSpeechStopSourceHistogramName, 0);
  histogram_tester.ExpectUniqueTimeSample(
      ReadAloudAppModel::kAudioStartTimeSuccessHistogramName, delay, 1);
  histogram_tester.ExpectTotalCount(
      ReadAloudAppModel::kAudioStartTimeFailureHistogramName, 0);
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

// TODO: crbug.com/440400392 - Ensure that phrase highlighting works with the
// TS text segmentation implementation.
class ReadAnythingReadAloudAppModelV8SegmentationTest
    : public ReadAnythingReadAloudAppModelTest {
 public:
  void SetUp() override {
    // Phrase highlighting currently doesn't work with the TS text segmentation
    // implementation, so we need to disable it to test phrase highlighting.
    scoped_feature_list_.InitWithFeatures(
        {features::kReadAnythingReadAloud,
         features::kReadAnythingReadAloudPhraseHighlighting},
        {features::kReadAnythingReadAloudTSTextSegmentation});
    ReadAnythingReadAloudAppModelTest::SetUp();
  }
};

TEST_F(
    ReadAnythingReadAloudAppModelV8SegmentationTest,
    GetHighlightForCurrentSegmentIndex_PhrasesEnabled_NoModel_SentenceSpansMultipleNodes_ReturnsCorrectNodes) {
  // Text indices:             0123456789012345678901234567890
  std::u16string sentence1 = u"Never feel heavy ";
  std::u16string sentence2 = u"or earthbound, ";
  std::u16string sentence3 = u"no worries or doubts interfere.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});
  model().PreprocessTextForSpeech(false, false, &current_nodes);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 3u);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  ExpectPhraseHighlightAtIndexMatches(0, {{kId1, 0, 17}});
  ExpectPhraseHighlightAtIndexMatches(7, {{kId1, 0, 17}});
  ExpectPhraseHighlightAtIndexMatches(sentence1.length() - 1, {{kId1, 0, 17}});

  int base_length = sentence1.length();
  ExpectPhraseHighlightAtIndexMatches(base_length,
                                      {{kId2, 0, 15}, {kId3, 0, 3}});

  // Spot check that indices in sentence 2 map to the second node id.
  ExpectPhraseHighlightAtIndexMatches(base_length + 1,
                                      {{kId2, 0, 15}, {kId3, 0, 3}});
  ExpectPhraseHighlightAtIndexMatches(26, {{kId2, 0, 15}, {kId3, 0, 3}});

  base_length += sentence2.length();
  ExpectPhraseHighlightAtIndexMatches(base_length - 1,
                                      {{kId2, 0, 15}, {kId3, 0, 3}});
  ExpectPhraseHighlightAtIndexMatches(base_length,
                                      {{kId2, 0, 15}, {kId3, 0, 3}});

  // Spot check that indices in sentence 3 map to the third node id.
  ExpectPhraseHighlightAtIndexMatches(base_length + 1,
                                      {{kId2, 0, 15}, {kId3, 0, 3}});
  ExpectPhraseHighlightAtIndexMatches(40, {{kId3, 3, 21}});

  ExpectPhraseHighlightAtIndexMatches(base_length + sentence3.length() - 1,
                                      {{kId3, 21, 31}});
  ExpectPhraseHighlightAtIndexEmpty(base_length + sentence3.length());

  // Out-of-bounds nodes return an empty array.
  ExpectPhraseHighlightAtIndexEmpty(base_length + sentence3.length() + 1);
  ExpectPhraseHighlightAtIndexEmpty(535);
  ExpectPhraseHighlightAtIndexEmpty(-10);
}

TEST_F(
    ReadAnythingReadAloudAppModelV8SegmentationTest,
    GetHighlightForCurrentSegmentIndex_PhrasesEnabled_ValidModel_SentenceSpansMultipleNodes_ReturnsCorrectNodes) {
  model().GetDependencyParserModel().UpdateWithFile(test::GetValidModelFile());
  DependencyParserModel& phrase_model = model().GetDependencyParserModel();

  EXPECT_TRUE(phrase_model.IsAvailable());

  // Text indices:             0123456789012345678901234567890
  std::u16string sentence1 = u"Never feel heavy or ";
  std::u16string sentence2 = u"earthbound, no ";
  std::u16string sentence3 = u"worries or doubts interfere.";

  // Expected phrases:
  // Never feel heavy or earthbound, /no worries or doubts interfere.
  // Expected phrase breaks: 0, 32

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});
  model().PreprocessTextForSpeech(false, false, &current_nodes);

  // Wait till all async calculations complete.
  task_environment_.RunUntilIdle();

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 3u);

  // First character (N) => first phrase
  ExpectPhraseHighlightAtIndexMatches(0, {{kId1, 0, 20}, {kId2, 0, 12}});

  // 20th character (e of earthbound) => first phrase
  ExpectPhraseHighlightAtIndexMatches(20, {{kId1, 0, 20}, {kId2, 0, 12}});

  // 31st character (space before "no") => first phrase
  ExpectPhraseHighlightAtIndexMatches(31, {{kId1, 0, 20}, {kId2, 0, 12}});

  // 32nd character (n of no) => second phrase
  ExpectPhraseHighlightAtIndexMatches(32, {{kId2, 12, 15}, {kId3, 0, 28}});

  // 35th character (w of worries) => second phrase
  ExpectPhraseHighlightAtIndexMatches(35, {{kId2, 12, 15}, {kId3, 0, 28}});

  // 62nd character (final .) => second phrase
  ExpectPhraseHighlightAtIndexMatches(62, {{kId2, 12, 15}, {kId3, 0, 28}});

  // 63rd character (past the end of the sentence) => empty
  ExpectPhraseHighlightAtIndexEmpty(63);

  // Invalid indices.
  ExpectPhraseHighlightAtIndexEmpty(535);
  ExpectPhraseHighlightAtIndexEmpty(-10);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetHighlightForCurrentSegmentIndex_ReturnsCorrectNodes) {
  // Text indices             0 123456789012345678901
  std::u16string sentence = u"I\'m crossing the line!";
  ui::AXNodeData static_text = test::TextNode(kId1, sentence);

  const std::set<ui::AXNodeID> current_nodes =
      InitializeWithAndProcessNodes({std::move(static_text)});

  // Before there are any processed granularities, GetHighlightStartIndex
  // should return an invalid id.
  ExpectHighlightAtIndexEmpty(1);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 1u);

  // Since we just have one node with one text segment, the returned index
  // should equal the passed parameter.
  ExpectHighlightAtIndexMatches(0, {{kId1, 0, 4}});
  ExpectHighlightAtIndexMatches(3, {{kId1, 3, 4}});
  ExpectHighlightAtIndexMatches(7, {{kId1, 7, 13}});
  ExpectHighlightAtIndexMatches(sentence.length() - 1, {{kId1, 21, 22}});
  ExpectHighlightAtIndexEmpty(static_cast<int>(sentence.length()));
}

TEST_F(
    ReadAnythingReadAloudAppModelV8SegmentationTest,
    GetHighlightForCurrentSegmentIndex_SentenceSpansMultipleNodes_ReturnsCorrectNodes) {
  // Text indices:             0123456789012345678901234567890
  std::u16string sentence1 = u"Never feel heavy ";
  std::u16string sentence2 = u"or earthbound, ";
  std::u16string sentence3 = u"no worries or doubts interfere.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  // Before there are any processed granularities,
  // GetHighlightForCurrentSegmentIndex should return an empty array.
  ExpectHighlightAtIndexEmpty(1);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 3u);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  int base_length = sentence1.length();
  ExpectHighlightAtIndexMatches(0, {{kId1, 0, 6}});
  ExpectHighlightAtIndexMatches(7, {{kId1, 7, 11}});
  ExpectHighlightAtIndexMatches(base_length - 1, {{kId1, 16, 17}});
  ExpectHighlightAtIndexMatches(base_length, {{kId2, 0, 3}});

  // Spot check that indices in sentence 2 map to the second node id.
  base_length += sentence2.length();
  ExpectHighlightAtIndexMatches(sentence1.length() + 1, {{kId2, 1, 3}});
  ExpectHighlightAtIndexMatches(26, {{kId2, 9, 15}});
  ExpectHighlightAtIndexMatches(base_length - 1, {{kId2, 14, 15}});
  ExpectHighlightAtIndexMatches(base_length, {{kId3, 0, 3}});

  // Spot check that indices in sentence 3 map to the third node id.
  base_length += sentence3.length();
  ExpectHighlightAtIndexMatches(sentence1.length() + sentence2.length() + 1,
                                {{kId3, 1, 3}});
  ExpectHighlightAtIndexMatches(40, {{kId3, 8, 11}});
  ExpectHighlightAtIndexMatches(base_length - 1, {{kId3, 30, 31}});
  ExpectHighlightAtIndexEmpty(base_length);

  // Out-of-bounds nodes return an empty array.
  ExpectHighlightAtIndexEmpty(base_length);
  ExpectHighlightAtIndexEmpty(535);
  ExpectHighlightAtIndexEmpty(-10);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetNextValidPosition_UsesCurrentGranularity) {
  std::u16string sentence1 = u"But from up here. The ";
  std::u16string sentence2 = u"world ";
  std::u16string sentence3 =
      u"looks so small. And suddenly life seems so clear. And from up here. "
      u"You coast past it all. The obstacles just disappear.";

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

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest, GetNextValidPosition) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  ui::AXNodePosition::AXPositionInstance new_position =
      GetNextNodePosition(&current_nodes);
  EXPECT_EQ(new_position->anchor_id(), kId2);
  EXPECT_EQ(new_position->GetText(), sentence2);

  // Getting the next node position shouldn't update the current AXPosition.
  new_position = GetNextNodePosition(&current_nodes);
  EXPECT_EQ(new_position->anchor_id(), kId2);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetNextValidPosition_SkipsNonTextNode) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId3, sentence2);

  ui::AXNodeData empty_node;
  empty_node.role = ax::mojom::Role::kNone;
  empty_node.id = kId2;

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(empty_node),
       std::move(static_text2)});

  ui::AXNodePosition::AXPositionInstance new_position =
      GetNextNodePosition(&current_nodes);
  EXPECT_EQ(new_position->anchor_id(), kId3);
  EXPECT_EQ(new_position->GetText(), sentence2);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetNextValidPosition_SkipsNonDistilledNode) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  InitializeWithAndProcessNodes({std::move(static_text1),
                                 std::move(static_text2),
                                 std::move(static_text3)});
  // Don't distill the node with id 3.
  std::set<ui::AXNodeID> current_nodes;
  current_nodes.insert(kId1);
  current_nodes.insert(kId3);
  InitAXPositionWithNode(kId1);
  ui::AXNodePosition::AXPositionInstance new_position =
      GetNextNodePosition(&current_nodes);
  EXPECT_EQ(new_position->anchor_id(), kId3);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetNextValidPosition_SkipsNodeWithHTMLTag) {
  std::u16string sentence1 = u"This is a sentence.";
  std::u16string sentence2 = u"This is another sentence.";
  std::u16string sentence3 = u"And this is yet another sentence.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);
  static_text2.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "h1");

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  ui::AXNodePosition::AXPositionInstance new_position =
      GetNextNodePosition(&current_nodes);
  EXPECT_EQ(new_position->anchor_id(), kId3);
  EXPECT_EQ(new_position->GetText(), sentence3);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetNextValidPosition_ReturnsNullPositionAtEndOfTree) {
  std::u16string sentence1 = u"This is a sentence.";
  ui::AXNodeData static_text = test::TextNode(/* id= */ 2, sentence1);
  ui::AXNodeData empty_node1;
  empty_node1.role = ax::mojom::Role::kNone;
  empty_node1.id = 3;
  ui::AXNodeData empty_node2;
  empty_node2.role = ax::mojom::Role::kNone;
  empty_node2.id = 4;
  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text), std::move(empty_node1), std::move(empty_node2)});

  a11y::ReadAloudCurrentGranularity current_granularity =
      a11y::ReadAloudCurrentGranularity();
  current_granularity.AddText(static_text.id, 0, sentence1.length(), sentence1);

  ui::AXNodePosition::AXPositionInstance new_position =
      GetNextNodePosition(&current_nodes, current_granularity);
  EXPECT_TRUE(new_position->IsNullPosition());
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetNextNodes_AfterResetReadAloudState_StartsOver) {
  std::u16string sentence1 = u"Where the north wind meets the sea. ";
  std::u16string sentence2 = u"There's a river full of memory. ";
  std::u16string sentence3 = u"Sleep my darling safe and sound. ";

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

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetCurrentTextSegments_ReturnsCorrectSegments) {
  std::u16string sentence = u"I broke into a million pieces";
  ui::AXNodeData static_text = test::TextNode(kId1, sentence);
  const std::set<ui::AXNodeID> current_nodes =
      InitializeWithAndProcessNodes({std::move(static_text)});

  std::vector<ReadAloudTextSegment> segments =
      model().GetCurrentTextSegments(false, false, &current_nodes);

  EXPECT_EQ(1u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(0u, segments.at(0).text_start);
  EXPECT_EQ(sentence.size(), segments.at(0).text_end);
}

TEST_F(
    ReadAnythingReadAloudAppModelV8SegmentationTest,
    GetCurrentTextSegments_SentenceSpansMultipleNodes_ReturnsCorrectSegments) {
  std::u16string sentence1 = u"and I can't go back, ";
  std::u16string sentence2 = u"But now I'm seeing all the beauty ";
  std::u16string sentence3 = u"in the broken glass.";
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);
  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  std::vector<ReadAloudTextSegment> segments =
      model().GetCurrentTextSegments(false, false, &current_nodes);

  EXPECT_EQ(3u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(0u, segments.at(0).text_start);
  EXPECT_EQ(sentence1.size(), segments.at(0).text_end);
  EXPECT_EQ(kId2, segments.at(1).id);
  EXPECT_EQ(0u, segments.at(1).text_start);
  EXPECT_EQ(sentence2.size(), segments.at(1).text_end);
  EXPECT_EQ(kId3, segments.at(2).id);
  EXPECT_EQ(0u, segments.at(2).text_start);
  EXPECT_EQ(sentence3.size(), segments.at(2).text_end);
}

TEST_F(
    ReadAnythingReadAloudAppModelV8SegmentationTest,
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

  ui::AXNodeData static_text1 = test::TextNode(kId1, node1_text);
  ui::AXNodeData static_text2 = test::TextNode(kId2, node2_text);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2)});
  // Before there are any processed granularities, GetHighlightStartIndex
  // should return an invalid id.
  ExpectHighlightAtIndexEmpty(1);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 1u);

  // Storing as a separate variable so we don't need to cast every time.
  int segment1_length = static_cast<int>(segment1.length());
  int segment2_length = static_cast<int>(segment2.length());
  int segment3_length = static_cast<int>(segment3.length());
  int segment4_partial_length = static_cast<int>(segment4.length());
  int segment4_full_length =
      static_cast<int>(segment4.length() + node2_text.length());

  // For the first node in the first segment, the returned index should equal
  // the passed parameter.
  ExpectHighlightAtIndexMatches(0, {{kId1, 0, 4}});
  ExpectHighlightAtIndexMatches(6, {{kId1, 6, 11}});
  ExpectHighlightAtIndexMatches(15, {{kId1, 15, 16}});
  ExpectHighlightAtIndexMatches(segment1.length() - 1,
                                {{kId1, segment1_length - 1, segment1_length}});
  ExpectHighlightAtIndexEmpty(segment1.length());

  // Move to segment 2.
  node_ids = MoveToNextGranularityAndGetText(&current_nodes);
  EXPECT_EQ(node_ids.size(), 1u);

  // For the second segment, the boundary index will have reset for the new
  // speech segment. The correct highlight start index is the index that the
  // boundary index within the segment corresponds to within the node.
  int base_length = segment1_length;
  ExpectHighlightAtIndexMatches(0, {{kId1, base_length, base_length + 6}});
  ExpectHighlightAtIndexMatches(10,
                                {{kId1, base_length + 10, base_length + 12}});
  ExpectHighlightAtIndexMatches(13,
                                {{kId1, base_length + 13, base_length + 18}});

  base_length += segment2_length;
  ExpectHighlightAtIndexMatches(segment2_length - 1,
                                {{kId1, base_length - 1, base_length}});
  ExpectHighlightAtIndexEmpty(segment1_length + segment2_length);

  // Move to segment 3.
  node_ids = MoveToNextGranularityAndGetText(&current_nodes);
  EXPECT_EQ(node_ids.size(), 1u);

  // For the third segment, the boundary index will have reset for the new
  // speech segment. The correct highlight start index is the index that the
  // boundary index within the segment corresponds to within the node.
  ExpectHighlightAtIndexMatches(0, {{kId1, base_length, base_length + 3}});
  ExpectHighlightAtIndexMatches(9, {{kId1, base_length + 9, base_length + 15}});
  ExpectHighlightAtIndexMatches(13,
                                {{kId1, base_length + 13, base_length + 15}});
  ExpectHighlightAtIndexMatches(segment3_length - 1,
                                {{kId1, base_length + segment3_length - 1,
                                  base_length + segment3_length}});

  ExpectHighlightAtIndexEmpty(base_length + segment3.length());

  // Move to segment 4.
  node_ids = MoveToNextGranularityAndGetText(&current_nodes);
  EXPECT_EQ(node_ids.size(), 2u);
  EXPECT_EQ(node_ids[0], kId1);
  EXPECT_EQ(node_ids[1], kId2);

  // For the fourth segment, there are two nodes. For the first node,
  // the correct highlight start corresponds to the index within the first
  // node.
  base_length += segment3_length;
  ExpectHighlightAtIndexMatches(0, {{kId1, base_length, base_length + 4}});
  ExpectHighlightAtIndexMatches(2, {{kId1, base_length + 2, base_length + 4}});
  ExpectHighlightAtIndexMatches(8, {{kId1, base_length + 8, base_length + 17}});
  ExpectHighlightAtIndexMatches(
      segment4_partial_length - 1,
      {{kId1, base_length + segment4_partial_length - 1,
        base_length + segment4_partial_length}});

  ExpectHighlightAtIndexEmpty(
      static_cast<int>(base_length + segment4.length()));

  // For the second node, the highlight index corresponds to the position
  // within the second node.
  ExpectHighlightAtIndexMatches(segment4_partial_length, {{kId2, 0, 5}});
  ExpectHighlightAtIndexMatches(segment4_partial_length + 2, {{kId2, 2, 5}});
  ExpectHighlightAtIndexMatches(
      segment4_full_length - 1,
      {{kId2, static_cast<int>(node2_text.length() - 1),
        static_cast<int>(node2_text.length())}});

  ExpectHighlightAtIndexEmpty(
      static_cast<int>(segment4.length() + node2_text.length()));
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetHighlightForCurrentSegmentIndex_AfterPrevious_ReturnsCorrectNodes) {
  // Text indices:             01234567890123456789012345678901234567890
  std::u16string sentence1 = u"There's nothing but you ";
  std::u16string sentence2 = u"looking down on the view from up here. ";
  std::u16string sentence3 = u"Stretch out with the wind behind you.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  ExpectHighlightAtIndexEmpty(1);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 2u);

  // Move forward.
  node_ids = MoveToNextGranularityAndGetText(&current_nodes);
  EXPECT_EQ(node_ids.size(), 1u);

  // Spot check that indices 0->sentence3.length() map to the third node id.
  ExpectHighlightAtIndexMatches(0, {{kId3, 0, 8}});
  ExpectHighlightAtIndexMatches(7, {{kId3, 7, 8}});
  ExpectHighlightAtIndexMatches(sentence3.length() - 1, {{kId3, 36, 37}});

  // Move backwards.
  node_ids = MoveToPreviousGranularityAndGetText(&current_nodes);
  EXPECT_EQ(node_ids.size(), 2u);

  // Spot check that indices in sentence 1 map to the first node id.
  ExpectHighlightAtIndexMatches(0, {{kId1, 0, 8}});
  ExpectHighlightAtIndexMatches(6, {{kId1, 6, 8}});
  ExpectHighlightAtIndexMatches(sentence1.length() - 1, {{kId1, 23, 24}});

  // Spot check that indices in sentence 2 map to the second node id.
  ExpectHighlightAtIndexMatches(sentence1.length() + 1, {{kId2, 1, 8}});
  ExpectHighlightAtIndexMatches(27, {{kId2, 3, 8}});
  ExpectHighlightAtIndexMatches(sentence1.length() + sentence2.length() - 1,
                                {{kId2, 38, 39}});

  // Out-of-bounds nodes return invalid.
  ExpectHighlightAtIndexEmpty(sentence1.length() + sentence2.length() + 1);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetHighlightForCurrentSegmentIndex_AfterNext_ReturnsCorrectNodes) {
  // Text indices:             012345678901234567890123456789012
  std::u16string sentence1 = u"Never feel heavy or earthbound. ";
  std::u16string sentence2 = u"No worries or doubts ";
  std::u16string sentence3 = u"interfere.";

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  ExpectHighlightAtIndexEmpty(1);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 1u);

  // Spot check that indices 0->sentence1.length() map to the first node id.
  ExpectHighlightAtIndexMatches(0, {{kId1, 0, 6}});
  ExpectHighlightAtIndexMatches(7, {{kId1, 7, 11}});
  ExpectHighlightAtIndexMatches(sentence1.length() - 1, {{kId1, 31, 32}});
  ExpectHighlightAtIndexEmpty(sentence1.length());

  // Move to the next granularity.
  node_ids = MoveToNextGranularityAndGetText(&current_nodes);
  EXPECT_EQ(node_ids.size(), 2u);

  // Spot check that indices in sentence 2 map to the second node id.
  ExpectHighlightAtIndexMatches(0, {{kId2, 0, 3}});
  ExpectHighlightAtIndexMatches(7, {{kId2, 7, 11}});
  ExpectHighlightAtIndexMatches(sentence2.length() - 1, {{kId2, 20, 21}});

  // Spot check that indices in sentence 3 map to the third node id.
  ExpectHighlightAtIndexMatches(sentence2.length() + 1, {{kId3, 1, 10}});
  ExpectHighlightAtIndexMatches(27, {{kId3, 6, 10}});
  ExpectHighlightAtIndexMatches(sentence2.length() + sentence3.length() - 1,
                                {{kId3, 9, 10}});

  // Out-of-bounds nodes return invalid.
  ExpectHighlightAtIndexEmpty(sentence2.length() + sentence3.length() + 1);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
       GetCurrentTextSegments_AfterPrevious_ReturnsCorrectNodes) {
  std::u16string sentence1 =
      u"Why did I cover up the colors stuck inside my head? ";
  std::u16string sentence2 = u"I should've let the jagged edges ";
  std::u16string sentence3 = u"meet the light instead.";
  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);
  ui::AXNodeData static_text3 = test::TextNode(kId3, sentence3);
  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2),
       std::move(static_text3)});

  std::vector<ReadAloudTextSegment> segments =
      model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(1u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(0u, segments.at(0).text_start);
  EXPECT_EQ(sentence1.size(), segments.at(0).text_end);

  MoveToNextGranularityAndGetText(&current_nodes);
  segments = model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(2u, segments.size());
  EXPECT_EQ(kId2, segments.at(0).id);
  EXPECT_EQ(0u, segments.at(0).text_start);
  EXPECT_EQ(sentence2.size(), segments.at(0).text_end);
  EXPECT_EQ(kId3, segments.at(1).id);
  EXPECT_EQ(0u, segments.at(1).text_start);
  EXPECT_EQ(sentence3.size(), segments.at(1).text_end);

  MoveToPreviousGranularityAndGetText(&current_nodes);
  segments = model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(1u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(0u, segments.at(0).text_start);
  EXPECT_EQ(sentence1.size(), segments.at(0).text_end);
}

TEST_F(ReadAnythingReadAloudAppModelV8SegmentationTest,
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
  std::u16string sentence2 = word7 + word8;

  ui::AXNodeData static_text1 = test::TextNode(kId1, sentence1);
  ui::AXNodeData static_text2 = test::TextNode(kId2, sentence2);

  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2)});

  // Before there are any processed granularities,
  // GetNodeIdForCurrentSegmentIndex should return an invalid id.
  ExpectHighlightAtIndexEmpty(1);

  std::vector<ui::AXNodeID> node_ids =
      model().GetCurrentText(false, false, &current_nodes).node_ids;
  EXPECT_EQ(node_ids.size(), 2u);

  // Throughout first word.
  ExpectHighlightAtIndexMatches(0, {{kId1, 0, 8}});
  ExpectHighlightAtIndexMatches(2, {{kId1, 2, 8}});
  ExpectHighlightAtIndexMatches(word1.length() - 2, {{kId1, 6, 8}});

  // Throughout third word.
  int third_word_index = sentence1.find(word3);
  ExpectHighlightAtIndexMatches(third_word_index, {{kId1, 12, 17}});
  ExpectHighlightAtIndexMatches(third_word_index + 2, {{kId1, 14, 17}});

  // Words split across node boundaries
  int sixth_word_index = sentence1.find(word6);
  ExpectHighlightAtIndexMatches(sixth_word_index,
                                {{kId1, 26, 29}, {kId2, 0, 4}});
  ExpectHighlightAtIndexMatches(sixth_word_index + 2,
                                {{kId1, 28, 29}, {kId2, 0, 4}});

  int seventh_word_index = sentence1.length();
  ExpectHighlightAtIndexMatches(seventh_word_index, {{kId2, 0, 4}});
  ExpectHighlightAtIndexMatches(seventh_word_index + 2, {{kId2, 2, 4}});

  int last_word_index = sentence1.length() + sentence2.find(word8);
  ExpectHighlightAtIndexMatches(last_word_index, {{kId2, 4, 8}});
  ExpectHighlightAtIndexMatches(last_word_index + 2, {{kId2, 6, 8}});

  // Boundary testing.
  ExpectHighlightAtIndexEmpty(-5);
  ExpectHighlightAtIndexEmpty(sentence1.length() + sentence2.length());
  ExpectHighlightAtIndexEmpty(sentence1.length() + sentence2.length() + 1);
}

TEST_F(
    ReadAnythingReadAloudAppModelV8SegmentationTest,
    GetCurrentTextSegments_NodeSpansMultipleSentences_ReturnsCorrectSegments) {
  std::u16string segment1 = u"The scars are part of me! ";
  std::u16string segment2 = u"Darkness and harmony. ";
  std::u16string segment3 = u"My voice without the lies. ";
  std::u16string segment4 = u"This is what it sounds ";
  std::u16string node1_text = segment1 + segment2 + segment3 + segment4;
  std::u16string node2_text = u"like.";
  int end1 = segment1.size();
  int end2 = end1 + segment2.size();
  int end3 = end2 + segment3.size();
  int end4 = end3 + segment4.size();
  ui::AXNodeData static_text1 = test::TextNode(kId1, node1_text);
  ui::AXNodeData static_text2 = test::TextNode(kId2, node2_text);
  const std::set<ui::AXNodeID> current_nodes = InitializeWithAndProcessNodes(
      {std::move(static_text1), std::move(static_text2)});

  // The first sentence is all of segment1.
  std::vector<ReadAloudTextSegment> segments =
      model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(1u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(0u, segments.at(0).text_start);
  EXPECT_EQ(end1, segments.at(0).text_end);

  // Next sentence is all of segment2.
  MoveToNextGranularityAndGetText(&current_nodes);
  segments = model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(1u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(end1, segments.at(0).text_start);
  EXPECT_EQ(end2, segments.at(0).text_end);

  // Next sentence is all of segment3.
  MoveToNextGranularityAndGetText(&current_nodes);
  segments = model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(1u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(end2, segments.at(0).text_start);
  EXPECT_EQ(end3, segments.at(0).text_end);

  // Final sentence is all of segment4 in node1 plus all of node2.
  MoveToNextGranularityAndGetText(&current_nodes);
  segments = model().GetCurrentTextSegments(false, false, &current_nodes);
  EXPECT_EQ(2u, segments.size());
  EXPECT_EQ(kId1, segments.at(0).id);
  EXPECT_EQ(end3, segments.at(0).text_start);
  EXPECT_EQ(end4, segments.at(0).text_end);
  EXPECT_EQ(kId2, segments.at(1).id);
  EXPECT_EQ(0, segments.at(1).text_start);
  EXPECT_EQ(node2_text.size(), segments.at(1).text_end);
}
