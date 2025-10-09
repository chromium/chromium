// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_aloud_app_model.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_parser_model.h"
#include "chrome/renderer/accessibility/phrase_segmentation/dependency_tree.h"
#include "chrome/renderer/accessibility/phrase_segmentation/phrase_segmenter.h"
#include "chrome/renderer/accessibility/phrase_segmentation/token_boundaries.h"
#include "chrome/renderer/accessibility/phrase_segmentation/tokenized_sentence.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"
#include "ui/accessibility/accessibility_features.h"

namespace {

// Returns the dependency parser model for this renderer process.
DependencyParserModel& GetDependencyParserModel_() {
  static base::NoDestructor<DependencyParserModel> instance;
  return *instance;
}

std::vector<size_t> GetDependencyHeads(base::span<const std::string> input) {
  DependencyParserModel& dependency_parser_model = GetDependencyParserModel_();
  return dependency_parser_model.IsAvailable()
             ? dependency_parser_model.GetDependencyHeads(input)
             : std::vector<size_t>();
}

}  // namespace

ReadAloudAppModel::ReadAloudAppModel() {
  for (const auto& [metric, count] : metric_to_count_map_) {
    metric_to_single_sample_[metric] =
        base::SingleSampleMetricsFactory::Get()->CreateCustomCountsMetric(
            metric, min_sample, max_sample, buckets);
    // We want to know if the counts are never incremented, so set the minimum
    // sample in case IncrementMetric is never called.
    metric_to_single_sample_[metric]->SetSample(min_sample);
  }
}

ReadAloudAppModel::~ReadAloudAppModel() = default;

void ReadAloudAppModel::OnSettingsRestoredFromPrefs(
    double speech_rate,
    base::Value::List* languages_enabled_in_pref,
    base::Value::Dict* voices,
    read_anything::mojom::HighlightGranularity granularity) {
  speech_rate_ = speech_rate;
  languages_enabled_in_pref_ = languages_enabled_in_pref->Clone();
  voices_ = voices->Clone();
  highlight_granularity_ = static_cast<size_t>(granularity);
}

void ReadAloudAppModel::SetLanguageEnabled(const std::string& lang,
                                           bool enabled) {
  if (enabled) {
    languages_enabled_in_pref_.Append(lang);
  } else {
    languages_enabled_in_pref_.EraseValue(base::Value(lang));
  }
}

bool ReadAloudAppModel::IsHighlightOn() {
  return highlight_granularity_ !=
         static_cast<int>(read_anything::mojom::HighlightGranularity::kOff);
}

void ReadAloudAppModel::ResetGranularityIndex() {
  processed_granularity_index_ = 0;
}

void ReadAloudAppModel::InitAXPositionWithNode(
    ui::AXNode* ax_node,
    const ui::AXTreeID& active_tree_id) {
  if (IsTsTextSegmentationEnabled()) {
    return;
  }

  // If instance is Null or Empty, create the next AxPosition. Don't create a
  // new position if the node's manager is missing, as that means we've
  // received incorrect data somewhere.
  if (ax_node != nullptr && (!ax_position_ || ax_position_->IsNullPosition()) &&
      ax_node->GetManager() && !speech_tree_initialized_) {
    ax_position_ =
        ui::AXNodePosition::CreateTreePositionAtStartOfAnchor(*ax_node);
    current_text_index_ = 0;
    processed_granularity_index_ = 0;
    processed_granularities_on_current_page_.clear();
    active_tree_id_ = active_tree_id;
    speech_tree_initialized_ = true;
  }
}
void ReadAloudAppModel::MovePositionToNextGranularity() {
  processed_granularity_index_++;
}

void ReadAloudAppModel::MovePositionToPreviousGranularity() {
  if (processed_granularity_index_ > 0) {
    processed_granularity_index_--;
  }
}

a11y::ReadAloudCurrentGranularity ReadAloudAppModel::GetCurrentText(
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  if (IsTsTextSegmentationEnabled()) {
    return a11y::ReadAloudCurrentGranularity();
  }
  while (processed_granularities_on_current_page_.size() <=
         processed_granularity_index_) {
    a11y::ReadAloudCurrentGranularity next_granularity =
        GetNextNodes(is_pdf, is_docs, current_nodes);

    if (next_granularity.node_ids.size() == 0) {
      // TODO(crbug.com/40927698) think about behavior when increment happened
      // out of the content- should we reset the state?
      return next_granularity;
    }
    if (features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
      // TODO(crbug.com/330749762): initiate phrase calculation here, with some
      // way to access the dependency parser model.
    }
    processed_granularities_on_current_page_.push_back(next_granularity);
  }

  return processed_granularities_on_current_page_[processed_granularity_index_];
}

void ReadAloudAppModel::PreprocessTextForSpeech(
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  if (IsTsTextSegmentationEnabled()) {
    return;
  }
  a11y::ReadAloudCurrentGranularity current_granularity =
      GetNextNodes(is_pdf, is_docs, current_nodes);

  while (current_granularity.node_ids.size() > 0) {
    processed_granularities_on_current_page_.push_back(current_granularity);
    current_granularity = GetNextNodes(is_pdf, is_docs, current_nodes);
  }

  // Initiate phrase computation.
  if (features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
    StartPhraseCalculation();
  }
}

DependencyParserModel& ReadAloudAppModel::GetDependencyParserModel() {
  return GetDependencyParserModel_();
}

void ReadAloudAppModel::CalculatePhrases(
    a11y::ReadAloudCurrentGranularity& granularity) {
  if (!features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
    return;
  }

  if (granularity.are_phrases_calculated) {
    // Phrase calculation has already been done using the dependency heads
    // generated by the model.
    return;
  }

  if (granularity.text.size() == 0) {
    // Empty.
    return;
  }

  // By default, initialize with  3-word phrases, which will be overwritten with
  // actual phrase boundaries. This can take effect if Play is pressed before
  // the first phrase is calculated; it also takes effect in some tests.
  // TODO(crbug.com/330749762): replace with a proper workaround.
  granularity.CalculatePlaceholderPhrases();

  if (is_calculating_phrases) {
    // This happens if multiple PreprocessTextForSpeech calls arrive for the
    // same page, which is quite common on some pages. If that happens, there
    // will be two or more parallel calls to this function: one corresponding to
    // the old Preprocess, and another corresponding to the new one.
    // is_calculating_phrases will be false for the old call and true for the
    // new calls. In this case, it is OK to return early for the new call, since
    // at the end of the old call, another call to the next sentence will
    // automatically be performed.
    LOG(WARNING) << "WARNING: Already calculating phrases, returning";
    return;
  }

  is_calculating_phrases = true;

  const TokenizedSentence tokenized_sentence =
      TokenizedSentence(granularity.text);
  granularity.tokens = tokenized_sentence.tokens();

  // Need to convert because model inference only takes std::string array.
  std::vector<std::string> phrase_tokens;
  phrase_tokens.reserve(tokenized_sentence.tokens().size());
  std::ranges::transform(
      tokenized_sentence.tokens(), std::back_inserter(phrase_tokens),
      static_cast<std::string (*)(std::u16string_view)>(&base::UTF16ToUTF8));

  // Perform computation of dependency heads asynchronously.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetDependencyHeads, phrase_tokens),
      base::BindOnce(&ReadAloudAppModel::UpdatePhraseBoundaries,
                     weak_ptr_factory_.GetWeakPtr(), phrase_tokens));
}

static const Strategy kPhraseStrategy = Strategy::kWords;
static const int kPhraseStrategyParameter = 5;

void ReadAloudAppModel::StartPhraseCalculation() {
  if (processed_granularities_on_current_page_.size() > 0) {
    current_phrase_calculation_index_ = 0;
    CalculatePhrases(processed_granularities_on_current_page_[0]);
  }
}

void ReadAloudAppModel::UpdatePhraseBoundaries(std::vector<std::string> tokens,
                                               std::vector<size_t> heads) {
  // Reset the phrase calculation flag, so that the next phrase calculation can
  // be scheduled, if needed.
  is_calculating_phrases = false;

  if (heads.empty()) {
    // Empty output.
    return;
  }

  if ((current_phrase_calculation_index_ < 0) ||
      (current_phrase_calculation_index_ >=
       static_cast<int>(processed_granularities_on_current_page_.size()))) {
    // Likely that the granularities were overwritten after phrase calculation
    // was initiated (e.g. for dynamic page content). Reset the calculation.
    StartPhraseCalculation();
    return;
  }

  a11y::ReadAloudCurrentGranularity& granularity =
      processed_granularities_on_current_page_
          [current_phrase_calculation_index_];

  if (granularity.tokens.size() != tokens.size()) {
    // Likely that the granularities were overwritten after phrase calculation
    // was initiated (e.g. for dynamic page content). Reset the calculation.
    StartPhraseCalculation();
    return;
  }

  // Reconstruct the tokenized sentence using the tokens.
  std::vector<std::u16string> u16string_tokens;
  for (const auto& token : tokens) {
    u16string_tokens.emplace_back(base::UTF8ToUTF16(token));
  }

  const TokenizedSentence tokenized_sentence(granularity.text,
                                             u16string_tokens);

  // Determine the token boundaries.
  std::vector<int> dependency_heads(
      heads.begin(), heads.end());  // Needed to cast from unsigned to int

  const DependencyTree dependency_tree(tokenized_sentence, dependency_heads);
  const TokenBoundaries token_boundaries(dependency_tree);

  // Segment the sentence and update phrase boundaries.
  PhraseSegmenter smart_highlight;
  granularity.phrase_boundaries = CalculatePhraseBoundaries(
      smart_highlight, tokenized_sentence, token_boundaries, kPhraseStrategy,
      kPhraseStrategyParameter);
  granularity.are_phrases_calculated = true;

  // Kick off the next sentence, if available.
  if (++current_phrase_calculation_index_ <
      static_cast<int>(processed_granularities_on_current_page_.size())) {
    CalculatePhrases(processed_granularities_on_current_page_
                         [current_phrase_calculation_index_]);
  } else {
    current_phrase_calculation_index_ = -1;
    LOG(WARNING) << "All phrases calculated!";
  }
}

// TODO(crbug.com/40927698): Update to use AXRange to better handle multiple
// nodes. This may require updating GetText in ax_range.h to return AXNodeIds.
// AXRangeType#ExpandToEnclosingTextBoundary may also be useful.
a11y::ReadAloudCurrentGranularity ReadAloudAppModel::GetNextNodes(
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  a11y::ReadAloudCurrentGranularity current_granularity =
      a11y::ReadAloudCurrentGranularity();

  // Make sure we're adequately returning at the end of content.
  if (!ax_position_ || ax_position_->AtEndOfAXTree() ||
      ax_position_->IsNullPosition()) {
    return current_granularity;
  }

  // Loop through the tree in order to group nodes together into the same
  // granularity segment until there are no more pieces that can be added
  // to the current segment or we've reached the end of the tree.
  // e.g. if the following two nodes are next to one another in the tree:
  //  AXNode: id=1, text = "This is a "
  //  AXNode: id=2, text = "link. "
  // both AXNodes should be added to the current granularity, as the
  // combined text across the two nodes forms a complete sentence with sentence
  // granularity.
  // This allows text to be spoken smoothly across nodes with broken sentences,
  // such as links and formatted text.
  // TODO(crbug.com/40927698): Investigate how much of this can be pulled into
  // AXPosition to simplify Read Aloud-specific code and allow improvements
  // to be used by other places where AXPosition is used.
  while (!ax_position_->IsNullPosition() && !ax_position_->AtEndOfAXTree()) {
    if (NoValidTextRemainingInCurrentNode(is_pdf, is_docs)) {
      MoveToNextAXPosition(current_granularity, is_pdf, is_docs, current_nodes);

      // Return the current granularity if the position is invalid.
      if (ShouldEndTextTraversal(current_granularity)) {
        return current_granularity;
      }

      a11y::TraversalState traversal_state =
          AddTextFromStartOfNode(is_pdf, is_docs, current_granularity);

      switch (traversal_state) {
        case a11y::TraversalState::EndOfSegment:
          return current_granularity;
        case a11y::TraversalState::ContinueToNextNode:
          continue;
        case a11y::TraversalState::ContinueInCurrentNode:
          // Fall-through;
        default:
          break;
      }
    }

    if (AddTextFromMiddleOfNode(is_pdf, is_docs, current_granularity) ==
        a11y::TraversalState::EndOfSegment) {
      return current_granularity;
    }
  }
  return current_granularity;
}

bool ReadAloudAppModel::NoValidTextRemainingInCurrentNode(bool is_pdf,
                                                          bool is_docs) const {
  ui::AXNode* anchor_node = GetAnchorNode(ax_position_);
  std::u16string text = a11y::GetTextContent(anchor_node, is_pdf, is_docs);
  std::u16string text_substr = text.substr(current_text_index_);
  int prev_index = current_text_index_;
  // Gets the starting index for the next sentence in the current node.
  int next_sentence_index = GetNextSentence(text_substr) + prev_index;
  // If our current index within the current node is greater than that node's
  // text, look at the next node. If the starting index of the next sentence
  // in the node is the same the current index within the node, this means
  // that we've reached the end of all possible sentences within the current
  // node, and should move to the next node.
  return ((size_t)current_text_index_ >= text.size() ||
          (current_text_index_ == next_sentence_index));
}

void ReadAloudAppModel::MoveToNextAXPosition(
    a11y::ReadAloudCurrentGranularity& current_granularity,
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  ax_position_ = GetNextValidPositionFromCurrentPosition(
      current_granularity, is_pdf, is_docs, current_nodes);
  // Reset the current text index within the current node since we just
  // moved to a new node.
  current_text_index_ = 0;
}

bool ReadAloudAppModel::PositionEndsWithOpeningPunctuation(
    bool is_superscript,
    int combined_sentence_index,
    const std::u16string& combined_text,
    a11y::ReadAloudCurrentGranularity current_granularity) {
  // The code that checks for accessible text boundaries sometimes
  // incorrectly includes opening punctuation (i.e. '(', '<', etc.) as part
  // of the prior sentence.
  // e.g. "This is a sentence.[2]" will return a sentence boundary for
  // "This is a sentence.[", splitting the opening and closing punctuation.
  // When opening punctuation is split like this in Read Aloud, text will
  // be read out for the punctuation e.g. "opening square bracket," which
  // we want to avoid.
  // Therefore, this is a workaround that prevents adding text from the
  // next node to the current segment if that text is a single character
  // and also opening punctuation. The opening punctuation will then be
  // read out as part of the next segment. If the opening punctuation is
  // followed by text and closing punctuation, the punctuation will not be
  // read out directly- just the text content.
  // This workaround is not needed for superscripts because with a
  // superscript, the entire superscript is added to the utterance of the
  // superscript's associated sentence.
  // TODO(crbug.com/40927698): See if it's possible to fix the code
  // in FindAccessibleTextBoundary instead so that this workaround isn't
  // needed.
  if (!is_superscript &&
      combined_sentence_index == (int)current_granularity.text.length() + 1) {
    char c = combined_text[combined_sentence_index - 1];
    return IsOpeningPunctuation(c);
  }

  return false;
}

bool ReadAloudAppModel::ShouldEndTextTraversal(
    a11y::ReadAloudCurrentGranularity current_granularity) {
  // We should end text traversal early if we:
  // - Have reached the end of the content because there are no more nodes
  //   to look through
  // - Have move to the start of a paragraph and we've already gotten nodes
  //   to return because we don't want to cross paragraph boundaries in a
  //   speech segment
  // If we've reached the end of the content, go ahead and return the
  // current list of nodes because there are no more nodes to look through.
  return (ax_position_->IsNullPosition() || ax_position_->AtEndOfAXTree() ||
          !ax_position_->GetAnchor()) ||
         ShouldSplitAtParagraph(ax_position_, current_granularity);
}

a11y::TraversalState ReadAloudAppModel::AddTextFromStartOfNode(
    bool is_pdf,
    bool is_docs,
    a11y::ReadAloudCurrentGranularity& current_granularity) {
  ui::AXNode* anchor_node = GetAnchorNode(ax_position_);

  std::u16string base_text = a11y::GetTextContent(anchor_node, is_pdf, is_docs);

  bool is_superscript = a11y::IsSuperscript(anchor_node);

  // Look at the text of the items we've already added to the
  // current sentence (current_text) combined with the text of the next
  // node (base_text).
  const std::u16string& combined_text = current_granularity.text + base_text;
  // Get the index of the next sentence if we're looking at the combined
  // previous and current node text. If we're currently in a superscript,
  // no need to check for a combined sentence, as we want to add the
  // entire superscript to the current text segment.
  int combined_sentence_index =
      is_superscript ? combined_text.length() : GetNextSentence(combined_text);

  bool is_opening_punctuation = PositionEndsWithOpeningPunctuation(
      is_superscript, combined_sentence_index, combined_text,
      current_granularity);

  // If the combined_sentence_index is the same as the current_text length,
  // the new node should not be considered part of the current sentence.
  // If these values differ, add the current node's text to the list of
  // nodes in the current sentence.
  // Consider these two examples:
  // Example 1:
  //  current text: Hello
  //  current node's text: , how are you?
  //    The current text length is 5, but the index of the next sentence of
  //    the combined text is 19, so the current node should be added to
  //    the current sentence.
  // Example 2:
  //  current text: Hello.
  //  current node: Goodbye.
  //    The current text length is 6, and the next sentence index of
  //    "Hello. Goodbye." is still 6, so the current node's text shouldn't
  //    be added to the current sentence.
  if (((int)current_granularity.text.length() < combined_sentence_index) &&
      !is_opening_punctuation) {
    anchor_node = GetAnchorNode(ax_position_);
    // Calculate the new sentence index.
    int index_in_new_node =
        combined_sentence_index - current_granularity.text.length();
    // Add the current node to the list of nodes to be returned, with a
    // text range from 0 to the start of the next sentence
    // (index_in_new_node);
    AddTextToCurrentGranularity(anchor_node, /* startIndex= */ 0,
                                /* end_index= */ index_in_new_node,
                                current_granularity, is_pdf, is_docs);
    current_text_index_ = index_in_new_node;
    if (current_text_index_ != (int)base_text.length()) {
      // If we're in the middle of the node, there's no need to attempt
      // to find another segment, as we're at the end of the current
      // segment.
      return a11y::TraversalState::EndOfSegment;
    }
    return a11y::TraversalState::ContinueToNextNode;
  } else if (current_granularity.node_ids.size() > 0) {
    // If nothing has been added to the list of current nodes, we should
    // look at the next sentence within the current node. However, if
    // there have already been nodes added to the list of nodes to return
    // and we determine that the next node shouldn't be added to the
    // current sentence, we've completed the current sentence, so we can
    // return the current list.
    return a11y::TraversalState::EndOfSegment;
  }

  return a11y::TraversalState::ContinueInCurrentNode;
}

a11y::TraversalState ReadAloudAppModel::AddTextFromMiddleOfNode(
    bool is_pdf,
    bool is_docs,
    a11y::ReadAloudCurrentGranularity& current_granularity) {
  // Add the next granularity piece within the current node.
  ui::AXNode* anchor_node = GetAnchorNode(ax_position_);
  std::u16string text = a11y::GetTextContent(anchor_node, is_pdf, is_docs);
  int prev_index = current_text_index_;
  std::u16string text_substr = text.substr(current_text_index_);
  // Find the next sentence within the current node.
  int new_current_text_index = GetNextSentence(text_substr) + prev_index;
  int start_index = current_text_index_;
  current_text_index_ = new_current_text_index;

  // Add the current node to the list of nodes to be returned, with a
  // text range from the starting index (the end of the previous piece of
  // the sentence) to the start of the next sentence.
  AddTextToCurrentGranularity(anchor_node, start_index,
                              /* end_index= */ current_text_index_,
                              current_granularity, is_pdf, is_docs);

  // After adding the most recent granularity segment, if we're not at the
  //  end of the node, the current nodes can be returned, as we know there's
  // no further segments remaining.
  if ((size_t)current_text_index_ != text.length()) {
    return a11y::TraversalState::EndOfSegment;
  }

  return a11y::TraversalState::ContinueToNextNode;
}

void ReadAloudAppModel::AddTextToCurrentGranularity(
    ui::AXNode* anchor_node,
    int start_index,
    int end_index,
    a11y::ReadAloudCurrentGranularity& current_granularity,
    bool is_pdf,
    bool is_docs) {
  current_granularity.AddText(
      anchor_node->id(), start_index, end_index,
      a11y::GetTextContent(anchor_node, is_pdf, is_docs)
          .substr(start_index, end_index - start_index));
}

// Gets the next valid position from our current position within AXPosition
// AXPosition returns nodes that aren't supported by Reading Mode, so we
// need to have a bit of extra logic to ensure we're only passing along valid
// nodes.
// Some of the checks here right now are probably unneeded.
ui::AXNodePosition::AXPositionInstance
ReadAloudAppModel::GetNextValidPositionFromCurrentPosition(
    const a11y::ReadAloudCurrentGranularity& current_granularity,
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  ui::AXNodePosition::AXPositionInstance new_position =
      ui::AXNodePosition::CreateNullPosition();

  // Traverse the available nodes in the tree using depth-first search.
  // TODO(crbug.com/411198154): If it's determined that reading mode and read
  // aloud should both be using AXPosition, revisit using sentence start
  // positions. If reading mode is populating the panel with nodes and read
  // aloud is populating via text-based positions from AXPosition, there can
  // be inconsistencies between the two.
  new_position = ax_position_->CreateNextAnchorPosition();

  if (new_position->IsNullPosition() || new_position->AtEndOfAXTree() ||
      !new_position->GetAnchor()) {
    return new_position;
  }

  // Ensure the current position is valid. If it's not, move to the next
  // position.
  while (!IsValidAXPosition(new_position, current_granularity, is_pdf, is_docs,
                            current_nodes)) {
    new_position = new_position->CreateNextAnchorPosition();
    if (new_position->IsNullPosition() || new_position->AtEndOfAXTree() ||
        !new_position->GetAnchor()) {
      return new_position;
    }
  }

  return new_position;
}

int ReadAloudAppModel::GetCurrentTextStartIndex(const ui::AXNodeID& node_id) {
  if (IsTsTextSegmentationEnabled() ||
      processed_granularities_on_current_page_.size() < 1 ||
      processed_granularity_index_ >=
          processed_granularities_on_current_page_.size()) {
    return -1;
  }

  a11y::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }
  ReadAloudTextSegment segment = current_granularity.segments[node_id];

  return segment.text_start;
}

int ReadAloudAppModel::GetCurrentTextEndIndex(const ui::AXNodeID& node_id) {
  if (IsTsTextSegmentationEnabled() ||
      processed_granularities_on_current_page_.size() < 1 ||
      processed_granularity_index_ >=
          processed_granularities_on_current_page_.size()) {
    return -1;
  }

  a11y::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }
  ReadAloudTextSegment segment = current_granularity.segments[node_id];

  return segment.text_end;
}

bool ReadAloudAppModel::NodeBeenOrWillBeSpoken(
    const a11y::ReadAloudCurrentGranularity& current_granularity,
    const ui::AXNodeID& id) const {
  if (base::Contains(current_granularity.segments, id)) {
    return true;
  }
  for (const auto& granularity : processed_granularities_on_current_page_) {
    if (base::Contains(granularity.segments, id)) {
      return true;
    }
  }

  return false;
}

void ReadAloudAppModel::ResetReadAloudState() {
  if (IsTsTextSegmentationEnabled()) {
    return;
  }

  ax_position_ = ui::AXNodePosition::AXPosition::CreateNullPosition();
  current_text_index_ = 0;
  processed_granularity_index_ = 0;
  processed_granularities_on_current_page_.clear();
  speech_tree_initialized_ = false;
}

bool ReadAloudAppModel::IsValidAXPosition(
    const ui::AXNodePosition::AXPositionInstance& position,
    const a11y::ReadAloudCurrentGranularity& current_granularity,
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) const {
  ui::AXNode* anchor_node = GetAnchorNode(position);
  if (!anchor_node) {
    return false;
  }

  // AXPosition returns nodes that aren't part of the active tree, but
  // reading mode only displays nodes that are part of the active tree.
  bool on_active_tree = anchor_node->tree()->GetAXTreeID() == active_tree_id_;
  bool was_previously_spoken =
      NodeBeenOrWillBeSpoken(current_granularity, anchor_node->id());
  bool is_text_node = a11y::IsTextForReadAnything(anchor_node, is_pdf, is_docs);
  bool contains_node = base::Contains(*current_nodes, anchor_node->id());
  bool is_ignored = a11y::IsIgnored(anchor_node, is_pdf);

  return !is_ignored && !was_previously_spoken && is_text_node &&
         contains_node && on_active_tree;
}

std::vector<ReadAloudTextSegment> ReadAloudAppModel::GetCurrentTextSegments(
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  a11y::ReadAloudCurrentGranularity current_granularity =
      GetCurrentText(is_pdf, is_docs, current_nodes);

  if (current_granularity.node_ids.empty()) {
    return {};
  }

  return current_granularity.GetSegmentsForRange(
      0, current_granularity.text.length());
}

std::vector<ReadAloudTextSegment>
ReadAloudAppModel::GetHighlightForCurrentSegmentIndex(int index,
                                                      bool phrases) const {
  // If the granularity index isn't valid, return an empty array.
  if (processed_granularity_index_ >=
      processed_granularities_on_current_page_.size()) {
    return {};
  }

  a11y::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];

  // If the index is outside the current text, return an empty array.
  if ((index < 0) || (index >= (int)current_granularity.text.length())) {
    return {};
  }

  if (phrases && features::IsReadAnythingReadAloudPhraseHighlightingEnabled()) {
    // Phrase highlighting. Find the previous and next boundaries, and get all
    // segments between them.
    int start = current_granularity.GetPhraseIndex(index);
    if (start < 0) {
      // Index is not valid, or phrase boundaries are not valid.
      return {};
    }
    int start_index = current_granularity.phrase_boundaries[start];
    // The phrase ends either at the start of the next phrase, or if it is the
    // last phrase of the sentence, at the end of the sentence.
    int end_index =
        (start < (int)current_granularity.phrase_boundaries.size() - 1)
            ? current_granularity.phrase_boundaries[start + 1]
            : current_granularity.text.size();
    return current_granularity.GetSegmentsForRange(start_index, end_index);
  } else {
    // Word highlighting. Get the remaining text in the current granularity that
    // occurs after the starting index.
    std::u16string current_text = current_granularity.text.substr(index);

    // Get the word length of the next word following the index.
    int word_length = GetNextWord(current_text);
    int end_index = index + word_length;

    return current_granularity.GetSegmentsForRange(index, end_index);
  }
}

void ReadAloudAppModel::SetSpeechPlaying(bool is_playing) {
  if (is_playing) {
    speech_active_time_ms_ = base::TimeTicks::Now();
  }
  speech_playing_ = is_playing;
}

void ReadAloudAppModel::SetAudioCurrentlyPlaying(bool is_playing) {
  if (is_playing) {
    LogAudioDelay(/*success=*/true);
  }
  audio_currently_playing_ = is_playing;
}

void ReadAloudAppModel::IncrementMetric(const std::string& metric_name) {
  metric_to_count_map_[metric_name]++;
  // Update the count that will be logged on destruction.
  if (metric_to_single_sample_[metric_name]) {
    metric_to_single_sample_[metric_name]->SetSample(
        metric_to_count_map_[metric_name]);
  }
}

void ReadAloudAppModel::LogSpeechStop(ReadAloudStopSource source) {
  if (!features::IsReadAnythingReadAloudEnabled()) {
    return;
  }

  base::UmaHistogramEnumeration(kSpeechStopSourceHistogramName, source);
  // If speech started but audio is not playing yet when speech is stopped, log
  // the audio delay indicating that the user may have stopped speech because
  // audio wasn't starting.
  if (speech_playing_ && !audio_currently_playing_) {
    LogAudioDelay(/*success=*/false);
  }
}

void ReadAloudAppModel::LogAudioDelay(bool success) {
  if (!features::IsReadAnythingReadAloudEnabled()) {
    return;
  }

  const base::TimeDelta delay = base::TimeTicks::Now() - speech_active_time_ms_;
  if (success) {
    base::UmaHistogramLongTimes(kAudioStartTimeSuccessHistogramName, delay);
  } else {
    base::UmaHistogramLongTimes(kAudioStartTimeFailureHistogramName, delay);
  }
}

bool ReadAloudAppModel::IsTsTextSegmentationEnabled() const {
  return features::IsReadAnythingReadAloudTSTextSegmentationEnabled();
}
