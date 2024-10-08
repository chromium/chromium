// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_

#include "base/metrics/single_sample_metrics.h"
#include "base/values.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"
#include "chrome/renderer/accessibility/read_aloud_traversal_utils.h"
#include "ui/accessibility/ax_node_position.h"

// A class that holds state related to Read Aloud for the
// ReadAnythingAppController for the Read Anything WebUI app.
class ReadAloudAppModel {
 public:
  ReadAloudAppModel();
  ~ReadAloudAppModel();
  ReadAloudAppModel(const ReadAloudAppModel& other) = delete;
  ReadAloudAppModel& operator=(const ReadAloudAppModel&) = delete;

  bool speech_playing() { return speech_playing_; }
  void set_speech_playing(bool is_playing) { speech_playing_ = is_playing; }
  double speech_rate() const { return speech_rate_; }
  void set_speech_rate(double rate) { speech_rate_ = rate; }
  const base::Value::List& languages_enabled_in_pref() const {
    return languages_enabled_in_pref_;
  }
  void SetLanguageEnabled(const std::string& lang, bool enabled);
  const base::Value::Dict& voices() const { return voices_; }
  void SetVoice(const std::string& voice, const std::string& lang) {
    voices_.Set(lang, voice);
  }
  int highlight_granularity() const { return highlight_granularity_; }
  void set_highlight_granularity(int granularity) {
    highlight_granularity_ = granularity;
  }
  const std::string& default_language_code() const {
    return default_language_code_;
  }
  void set_default_language_code(const std::string& code) {
    default_language_code_ = code;
  }

  bool IsHighlightOn();
  void OnSettingsRestoredFromPrefs(
      double speech_rate,
      base::Value::List* languages_enabled_in_pref,
      base::Value::Dict* voices,
      read_anything::mojom::HighlightGranularity granularity);

  // Returns the next valid AXNodePosition.
  ui::AXNodePosition::AXPositionInstance
  GetNextValidPositionFromCurrentPosition(
      const a11y::ReadAloudCurrentGranularity& current_granularity,
      bool is_pdf,
      bool is_docs,
      const std::set<ui::AXNodeID>* current_nodes);

  // Inits the AXPosition with a starting node.
  // TODO(crbug.com/40927698): We should be able to use AXPosition in a way
  // where this isn't needed.
  void InitAXPositionWithNode(ui::AXNode* ax_node);

  void ResetGranularityIndex();

  // Returns a list of AXNodeIds representing the next nodes that should be
  // spoken and highlighted with Read Aloud.
  // This defaults to returning the first granularity until
  // MovePositionTo<Next,Previous>Granularity() moves the position.
  // If the the current processed_granularity_index_ has not been calculated
  // yet, GetNextNodes() is called which updates the AXPosition.
  // GetCurrentTextStartIndex and GetCurrentTextEndIndex called with an AXNodeID
  // return by GetCurrentText will return the starting text and ending text
  // indices for specific text that should be referenced within the node.
  std::vector<ui::AXNodeID> GetCurrentText(
      bool is_pdf,
      bool is_docs,
      const std::set<ui::AXNodeID>* current_nodes);

  // Asynchronously preprocess the text on the current page that will be
  // used for Read Aloud.
  void PreprocessTextForSpeech(bool is_pdf,
                               bool is_docs,
                               const std::set<ui::AXNodeID>* current_nodes);

  void PreprocessPhrasesForText(DependencyParserModel& dependency_parser_model);

  // Increments the processed_granularity_index_, updating ReadAloud's state of
  // the current granularity to refer to the next granularity. The current
  // behavior allows the client to increment past the end of the page's content.
  void MovePositionToNextGranularity();

  // Decrements the processed_granularity_index_,updating ReadAloud's state of
  // the current granularity to refer to the previous granularity. Cannot be
  // decremented less than 0.
  void MovePositionToPreviousGranularity();

  // Helper method for GetCurrentText.
  a11y::ReadAloudCurrentGranularity GetNextNodes(
      bool is_pdf,
      bool is_docs,
      const std::set<ui::AXNodeID>* current_nodes);

  // Returns the Read Aloud starting text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return 0. Returns -1 if the node isn't in the current
  // segment.
  int GetCurrentTextStartIndex(const ui::AXNodeID& node_id);

  // Returns the Read Aloud ending text index for a node. For example,
  // if the entire text of the node should be read by Read Aloud at a particular
  // moment, this will return the length of the node's text. Returns -1 if the
  // node isn't in the current segment.
  int GetCurrentTextEndIndex(const ui::AXNodeID& node_id);

  void ResetReadAloudState();

  // Given a text index for the current granularity, return the nodes and the
  // corresponding text ranges for that part of the text. The text ranges
  // consist of start and end offsets within each node. If the `phrases`
  // argument is `true`, the text ranges for the containing phrase are returned,
  // otherwise the text ranges for the word are returned.
  //
  // For example, if a current granularity segment has text:
  // "Hello darkness, my old friend."
  // Composed of nodes:
  // Node: {id: 113, text: "Hello dark"}
  // Node: {id: 207, text: "ness, my old friend."}
  // Then GetHighlightForCurrentSegmentIndex for index=6 will return the
  // following nodes, which correspond to the word "darkness, ":
  //    [{"113", 6, 10}, {"207", 0, 6}]
  // For index=17, which corresponds to the word "my ", will return:
  //    [{"207", 6, 9}].
  std::vector<ReadAloudTextSegment> GetHighlightForCurrentSegmentIndex(
      int index,
      bool phrases) const;

  // Updates the session count for the given metric name using
  // SingleSampleMetric. These are then logged once on destruction.
  void IncrementMetric(const std::string& metric_name);

 private:
  // Returns true if the node was previously spoken or we expect to speak it
  // to be spoken once the current run of #GetCurrentText which called
  // #NodeBeenOrWillBeSpoken finishes executing. Because AXPosition
  // sometimes returns leaf nodes, we sometimes need to use the parent of a
  // node returned by AXPosition instead of the node itself. Because of this,
  // we need to double-check that the node has not been used or currently
  // in use.
  // Example:
  // parent node: id=5
  //    child node: id=6
  //    child node: id =7
  // node: id = 10
  // Where AXPosition will return nodes in order of 6, 7, 10, but Reading Mode
  // process them as 5, 10. Without checking for previously spoken nodes,
  // id 5 will be spoken twice.
  bool NodeBeenOrWillBeSpoken(
      const a11y::ReadAloudCurrentGranularity& current_granularity,
      const ui::AXNodeID& id) const;

  bool IsValidAXPosition(
      const ui::AXNodePosition::AXPositionInstance& position,
      const a11y::ReadAloudCurrentGranularity& current_granularity,
      bool is_pdf,
      bool is_docs,
      const std::set<ui::AXNodeID>* current_nodes) const;

  void AddTextToCurrentGranularity(
      ui::AXNode* anchor_node,
      int start_index,
      int end_index,
      a11y::ReadAloudCurrentGranularity& current_granularity,
      bool is_docs);

  // Returns if we should end text traversal from the current position, due
  // to reaching the end of content or reaching a point, such as a paragraph,
  // where a segment should be split.
  bool ShouldEndTextTraversal(
      a11y::ReadAloudCurrentGranularity current_granularity);

  // Helper method for GetNextNodes.
  // During text traversal for Read Aloud, adds text to the current Read Aloud
  // segment from the start of the current node.
  // for example, if:
  //   node 1: This is sentence 1.
  //   node 2: This is sentence 2.
  //   ax_position_ points to node 2,
  //   AddTextFromStartOfNode will add the text in node 2 to the current
  //   segment
  // Returns a TraversalState enum used to indicate if traversal should end,
  // continue to the next node, or continue within the same node.
  a11y::TraversalState AddTextFromStartOfNode(
      bool is_pdf,
      bool is_docs,
      a11y::ReadAloudCurrentGranularity& current_granularity);

  // Helper method for GetNextNodes.
  // During text traversal for Read Aloud, adds text to the current Read Aloud
  // segment from the middle of the current node.
  // for example, if:
  //   node 1: This is sentence 1.
  //   node 2: Hello! This is sentence 2.
  //   ax_position_ points to node 2 and current_text_index_ is 7.
  //   AddTextFromMiddleOfNode will add the text in node 2 starting from the
  //   current_text_index_ to the current speech segment
  // Returns a TraversalState enum used to indicate if traversal should end,
  // continue to the next node, or continue within the same node.
  a11y::TraversalState AddTextFromMiddleOfNode(
      bool is_pdf,
      bool is_docs,
      a11y::ReadAloudCurrentGranularity& current_granularity);

  bool PositionEndsWithOpeningPunctuation(
      bool is_superscript,
      int combined_sentence_index,
      const std::u16string& combined_text,
      a11y::ReadAloudCurrentGranularity current_granularity);

  // Helper for GetNextNodes.
  // Moves the current AXPosition to the next valid position.
  void MoveToNextAXPosition(
      a11y::ReadAloudCurrentGranularity& current_granularity,
      bool is_pdf,
      bool is_docs,
      const std::set<ui::AXNodeID>* current_nodes);

  ui::AXNodePosition::AXPositionInstance GetNextSentencePosition() const;

  // Helper for GetNextNodes.
  // Returns true if the node at the current AXPosition has no more text
  // remaining.
  // e.g. If the current node's text is "You need to not care. You need to not
  //      stare." and Read Aloud has read out loud both sentences, this will
  //      return true. However, if Read Aloud has only read out the first
  //      sentence, this will return false because "You need to not stare."
  //      still needs to be read.
  bool NoValidTextRemainingInCurrentNode(bool is_pdf, bool is_docs) const;

  // Segment the given granularity into phrases with the given model.
  void CalculatePhrases(DependencyParserModel& dependency_parser_model,
                        a11y::ReadAloudCurrentGranularity& granularity);

  // Whether Read Aloud speech is currently playing or not.
  bool speech_playing_ = false;

  // The current speech rate for reading aloud.
  double speech_rate_ = kReadAnythingDefaultSpeechRate;

  // The languages that the user has enabled for reading aloud.
  base::Value::List languages_enabled_in_pref_;

  // The user's preferred voices. Maps from a language to the last chosen
  // voice for that language.
  base::Value::Dict voices_;

  // The current granularity being used for the reading highlight.
  int highlight_granularity_ =
      (int)read_anything::mojom::HighlightGranularity::kDefaultValue;

  // The default language code, used as a fallback in case the page language
  // is invalid. It's not guaranteed that default_language_code_ will always
  // be valid, but as it is tied to the browser language, it is likely more
  // stable.
  std::string default_language_code_ = "en";

  // Metrics for logging. Any metric that we want to track 0-counts of should
  // be initialized here.
  const int min_sample = 0;
  const int max_sample = 1000;
  const uint32_t buckets = 50;
  std::map<std::string, int64_t> metric_to_count_map_ = {
      {"Accessibility.ReadAnything.ReadAloudNextButtonSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPauseSessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPlaySessionCount", 0},
      {"Accessibility.ReadAnything.ReadAloudPreviousButtonSessionCount", 0},
  };
  std::map<std::string, std::unique_ptr<base::SingleSampleMetric>>
      metric_to_single_sample_;

  // Traversal state

  ui::AXNodePosition::AXPositionInstance ax_position_;

  // Our current index within processed_granularities_on_current_page_.
  size_t processed_granularity_index_ = 0;

  // The current text index within the given node.
  int current_text_index_ = 0;

  // TODO(crbug.com/40927698): Clear this when granularity changes.
  // TODO(crbug.com/40927698): Use this to assist in navigating forwards /
  // backwards.
  // Previously processed granularities on the current page.
  std::vector<a11y::ReadAloudCurrentGranularity>
      processed_granularities_on_current_page_;

  const ui::AXMovementOptions sentence_movement_options_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_APP_MODEL_H_
