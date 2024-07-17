// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_aloud_app_model.h"

#include "base/values.h"
#include "chrome/renderer/accessibility/read_anything_node_utils.h"

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
  return highlight_granularity_ ==
         static_cast<int>(read_anything::mojom::HighlightGranularity::kOn);
}

void ReadAloudAppModel::InitAXPositionWithNode(ui::AXNode* ax_node) {
  // If instance is Null or Empty, create the next AxPosition
  if (ax_node != nullptr && (!ax_position_ || ax_position_->IsNullPosition())) {
    ax_position_ =
        ui::AXNodePosition::CreateTreePositionAtStartOfAnchor(*ax_node);
    current_text_index_ = 0;
    processed_granularity_index_ = 0;
    processed_granularities_on_current_page_.clear();
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

std::vector<ui::AXNodeID> ReadAloudAppModel::GetCurrentText(
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) {
  while (processed_granularities_on_current_page_.size() <=
         processed_granularity_index_) {
    a11y::ReadAloudCurrentGranularity next_granularity =
        GetNextNodes(is_pdf, is_docs, current_nodes);

    if (next_granularity.node_ids.size() == 0) {
      // TODO(crbug.com/40927698) think about behavior when increment happened
      // out of the content- should we reset the state?
      return next_granularity.node_ids;
    }

    processed_granularities_on_current_page_.push_back(next_granularity);
  }

  return processed_granularities_on_current_page_[processed_granularity_index_]
      .node_ids;
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
    if (NoValidTextRemainingInCurrentNode(is_pdf)) {
      MoveToNextAXPosition(current_granularity, is_pdf, is_docs, current_nodes);

      // Return the current granularity if the position is invalid.
      if (ShouldEndTextTraversal(current_granularity)) {
        return current_granularity;
      }

      a11y::TraversalState traversal_state =
          AddTextFromStartOfNode(is_pdf, current_granularity);

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

    if (AddTextFromMiddleOfNode(is_pdf, current_granularity) ==
        a11y::TraversalState::EndOfSegment) {
      return current_granularity;
    }
  }
  return current_granularity;
}

bool ReadAloudAppModel::NoValidTextRemainingInCurrentNode(bool is_pdf) const {
  ui::AXNode* anchor_node = GetNextNodeFromPosition(ax_position_);
  std::u16string text = anchor_node->GetTextContentUTF16();
  std::u16string text_substr = text.substr(current_text_index_);
  int prev_index = current_text_index_;
  // Gets the starting index for the next sentence in the current node.
  int next_sentence_index = GetNextSentence(text_substr, is_pdf) + prev_index;
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
    a11y::ReadAloudCurrentGranularity& current_granularity) {
  ui::AXNode* anchor_node = GetNextNodeFromPosition(ax_position_);

  std::u16string base_text = anchor_node->GetTextContentUTF16();

  bool is_superscript = a11y::IsSuperscript(anchor_node);

  // Look at the text of the items we've already added to the
  // current sentence (current_text) combined with the text of the next
  // node (base_text).
  const std::u16string& combined_text = current_granularity.text + base_text;
  // Get the index of the next sentence if we're looking at the combined
  // previous and current node text. If we're currently in a superscript,
  // no need to check for a combined sentence, as we want to add the
  // entire superscript to the current text segment.
  int combined_sentence_index = is_superscript
                                    ? combined_text.length()
                                    : GetNextSentence(combined_text, is_pdf);

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
    anchor_node = GetNextNodeFromPosition(ax_position_);
    // Calculate the new sentence index.
    int index_in_new_node =
        combined_sentence_index - current_granularity.text.length();
    // Add the current node to the list of nodes to be returned, with a
    // text range from 0 to the start of the next sentence
    // (index_in_new_node);
    AddTextToCurrentGranularity(anchor_node, /* startIndex= */ 0,
                                /* end_index= */ index_in_new_node,
                                current_granularity);
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
    a11y::ReadAloudCurrentGranularity& current_granularity) {
  // Add the next granularity piece within the current node.
  ui::AXNode* anchor_node = GetNextNodeFromPosition(ax_position_);
  std::u16string text = anchor_node->GetTextContentUTF16();
  int prev_index = current_text_index_;
  std::u16string text_substr = text.substr(current_text_index_);
  // Find the next sentence within the current node.
  int new_current_text_index =
      GetNextSentence(text_substr, is_pdf) + prev_index;
  int start_index = current_text_index_;
  current_text_index_ = new_current_text_index;

  // Add the current node to the list of nodes to be returned, with a
  // text range from the starting index (the end of the previous piece of
  // the sentence) to the start of the next sentence.
  AddTextToCurrentGranularity(anchor_node, start_index,
                              /* end_index= */ current_text_index_,
                              current_granularity);

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
    a11y::ReadAloudCurrentGranularity& current_granularity) {
  ReadAloudTextSegment segment;
  segment.id = anchor_node->id();
  segment.text_start = start_index;
  segment.text_end = end_index;
  current_granularity.AddSegment(segment);

  int current_text_length = current_granularity.text.length();

  current_granularity.text += anchor_node->GetTextContentUTF16().substr(
      start_index, end_index - start_index);

  current_granularity.index_map.insert(
      {{current_text_length, current_granularity.text.length()}, segment.id});
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

  ui::AXMovementOptions movement_options(
      ui::AXBoundaryBehavior::kCrossBoundary,
      ui::AXBoundaryDetection::kDontCheckInitialPosition);

  new_position = ax_position_->CreatePositionAtTextBoundary(
      ax::mojom::TextBoundary::kSentenceStart,
      ax::mojom::MoveDirection::kForward, movement_options);

  if (new_position->IsNullPosition() || new_position->AtEndOfAXTree() ||
      !new_position->GetAnchor()) {
    return new_position;
  }

  while (!IsValidAXPosition(new_position, current_granularity, is_pdf, is_docs,
                            current_nodes)) {
    ui::AXNodePosition::AXPositionInstance possible_new_position =
        new_position->CreateNextSentenceStartPosition(movement_options);
    bool use_paragraph = false;

    // If the new position and the previous position are the same, try moving
    // to the next paragraph position instead. This happens rarely, but when
    // it does, we can get stuck in an infinite loop of calling
    // CreateNextSentenceStartPosition, as it will always return the same
    // position.
    if (ArePositionsEqual(possible_new_position, new_position)) {
      use_paragraph = true;
      possible_new_position =
          new_position->CreateNextParagraphStartPosition(movement_options);

      // If after switching to use the paragraph position, the position is
      // in a null position, go ahead and return the null position so
      // speech can terminate properly. Otherwise, speech may get caught
      // in an infinite loop of searching for another item to speak when
      // there's no text left. This happens when the final node to be spoken
      // in the content is followed by an invalid character that causes
      // CreatenextSentenceStartPosition to repeatedly return the same thing.
      if (possible_new_position->IsNullPosition()) {
        return ui::AXNodePosition::AXPosition::CreateNullPosition();
      }
    }

    // If the new position is still the same as the old position after trying
    // a paragraph position, go ahead and return a null position instead, as
    // ending speech early is preferable to getting stuck in an infinite
    // loop.
    if (ArePositionsEqual(possible_new_position, new_position)) {
      return ui::AXNodePosition::AXPosition::CreateNullPosition();
    }

    if (!possible_new_position->GetAnchor()) {
      if (NodeBeenOrWillBeSpoken(current_granularity,
                                 new_position->GetAnchor()->id())) {
        // If the previous position we were looking at was previously spoken,
        // go ahead and return the null position to avoid duplicate nodes
        // being added.
        return possible_new_position;
      }
      return new_position;
    }

    new_position =
        use_paragraph
            ? new_position->CreateNextParagraphStartPosition(movement_options)
            : new_position->CreateNextSentenceStartPosition(movement_options);
  }

  return new_position;
}

int ReadAloudAppModel::GetCurrentTextStartIndex(const ui::AXNodeID& node_id) {
  if (processed_granularities_on_current_page_.size() < 1) {
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

int ReadAloudAppModel::GetHighlightStartIndex(const ui::AXNodeID& node_id,
                                              int boundary_index) {
  if (processed_granularities_on_current_page_.size() < 1) {
    return -1;
  }

  a11y::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (!current_granularity.segments.count(node_id)) {
    return -1;
  }

  std::map<std::pair<int, int>, ui::AXNodeID> index_map =
      current_granularity.index_map;
  for (const auto& [range, id] : index_map) {
    if (id == node_id && range.first <= boundary_index &&
        range.second > boundary_index) {
      // First shift the word boundary index by the starting position within
      // the current speech segment. Then shift this by the starting position
      // within the current node.
      // The first shift is necessary to handle multiple nodes within the
      // same speech segment. e.g.
      //   Node 1: This is a
      //   Node 2: link.
      // While the second shift is necessary to handle multiple speech segments
      // within the same node. e.g.
      //   Node 1: This is a sentence read at once. This is a second sentence
      //           processed after the first sentence completes.
      return (boundary_index - range.first) + GetCurrentTextStartIndex(node_id);
    }
  }

  return -1;
}

int ReadAloudAppModel::GetCurrentTextEndIndex(const ui::AXNodeID& node_id) {
  if (processed_granularities_on_current_page_.size() < 1) {
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
  for (a11y::ReadAloudCurrentGranularity granularity :
       processed_granularities_on_current_page_) {
    if (base::Contains(granularity.segments, id)) {
      return true;
    }
  }

  return false;
}

void ReadAloudAppModel::ResetReadAloudState() {
  ax_position_ = ui::AXNodePosition::AXPosition::CreateNullPosition();
  current_text_index_ = 0;
  processed_granularity_index_ = 0;
  processed_granularities_on_current_page_.clear();
}

bool ReadAloudAppModel::IsValidAXPosition(
    const ui::AXNodePosition::AXPositionInstance& position,
    const a11y::ReadAloudCurrentGranularity& current_granularity,
    bool is_pdf,
    bool is_docs,
    const std::set<ui::AXNodeID>* current_nodes) const {
  ui::AXNode* anchor_node = GetAnchorNode(position);
  bool was_previously_spoken =
      NodeBeenOrWillBeSpoken(current_granularity, anchor_node->id());
  bool is_text_node = a11y::IsTextForReadAnything(anchor_node, is_pdf, is_docs);
  bool contains_node = base::Contains(*current_nodes, anchor_node->id());

  return !was_previously_spoken && is_text_node && contains_node;
}

ui::AXNodeID ReadAloudAppModel::GetNodeIdForCurrentSegmentIndex(
    int index) const {
  // If the granularity index isn't valid, return an invalid id.
  if (processed_granularity_index_ >=
      processed_granularities_on_current_page_.size()) {
    return ui::kInvalidAXNodeID;
  }

  a11y::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  std::map<std::pair<int, int>, ui::AXNodeID> index_map =
      current_granularity.index_map;
  for (const auto& [range, id] : index_map) {
    if (range.first <= index && range.second > index) {
      // If the given index is within a range, return the associated node id.
      return id;
    }
  }

  // If the index isn't part of the current granularity's ranges, return an
  // invalid id.
  return ui::kInvalidAXNodeID;
}

int ReadAloudAppModel::GetNextWordHighlightLength(int start_index) {
  // If the granularity index isn't valid, return 0.
  if (processed_granularity_index_ >=
          processed_granularities_on_current_page_.size() ||
      start_index < 0) {
    // 0 is returned to correspond to a 0-length or empty string.
    return 0;
  }

  a11y::ReadAloudCurrentGranularity current_granularity =
      processed_granularities_on_current_page_[processed_granularity_index_];
  if (start_index > (int)current_granularity.text.length()) {
    return 0;
  }
  // Get the remaining text in the current granularity that occurs after the
  // starting index.
  std::u16string current_text = current_granularity.text.substr(start_index);

  // Get the word length of the next word following the index.
  int word_length = GetNextWord(current_text);
  return word_length;
}

void ReadAloudAppModel::IncrementMetric(const std::string& metric_name) {
  metric_to_count_map_[metric_name]++;
  // Update the count that will be logged on destruction.
  if (metric_to_single_sample_[metric_name]) {
    metric_to_single_sample_[metric_name]->SetSample(
        metric_to_count_map_[metric_name]);
  }
}
