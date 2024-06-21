// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_aloud_traversal_utils.h"

#include "ui/accessibility/ax_text_utils.h"

namespace a11y {

ReadAloudCurrentGranularity::ReadAloudCurrentGranularity() = default;

ReadAloudCurrentGranularity::ReadAloudCurrentGranularity(
    const ReadAloudCurrentGranularity& other) = default;

ReadAloudCurrentGranularity::~ReadAloudCurrentGranularity() = default;
}  // namespace a11y

namespace {

// Returns the index of the next granularity of the given text, such that the
// next granularity is equivalent to text.substr(0, <returned_index>).
int GetNextGranularity(const std::u16string& text,
                       ax::mojom::TextBoundary boundary) {
  // TODO(crbug.com/40927698): Investigate providing correct line breaks
  // or alternatively making adjustments to ax_text_utils to return boundaries
  // that minimize choppiness.
  std::vector<int> offsets;
  return ui::FindAccessibleTextBoundary(text, offsets, boundary, 0,
                                        ax::mojom::MoveDirection::kForward,
                                        ax::mojom::TextAffinity::kDefaultValue);
}

}  // namespace

int GetNextSentence(const std::u16string& text, bool is_pdf) {
  std::u16string filtered_string(text);
  // When we receive text from a pdf node, there are return characters at each
  // visual line break in the page. If these aren't filtered before calling
  // GetNextGranularity on the text, text part of the same sentence will be
  // read as separate segments, which causes speech to sound choppy.
  // e.g. without filtering
  // 'This is a long sentence with \n\r a line break.'
  // will read and highlight "This is a long sentence with" and "a line break"
  // separately.
  if (is_pdf && filtered_string.size() > 0) {
    size_t pos = filtered_string.find_first_of(u"\n\r");
    while (pos != std::string::npos && pos < filtered_string.size() - 2) {
      filtered_string.replace(pos, 1, u" ");
      pos = filtered_string.find_first_of(u"\n\r");
    }
  }
  return GetNextGranularity(filtered_string,
                            ax::mojom::TextBoundary::kSentenceStart);
}

int GetNextWord(const std::u16string& text) {
  return GetNextGranularity(text, ax::mojom::TextBoundary::kWordStart);
}

bool ArePositionsEqual(const ui::AXNodePosition::AXPositionInstance& position,
                       const ui::AXNodePosition::AXPositionInstance& other) {
  return position->GetAnchor() && other->GetAnchor() &&
         (position->CompareTo(*other).value_or(-1) == 0) &&
         (position->text_offset() == other->text_offset());
}

ui::AXNode* GetAnchorNode(
    const ui::AXNodePosition::AXPositionInstance& position) {
  bool is_leaf = position->GetAnchor()->IsChildOfLeaf();
  // If the node is a leaf, use the parent node instead.
  return is_leaf ? position->GetAnchor()->GetLowestPlatformAncestor()
                 : position->GetAnchor();
}

// Returns either the node or the lowest platform ancestor of the node, if it's
// a leaf.
ui::AXNode* GetNextNodeFromPosition(
    const ui::AXNodePosition::AXPositionInstance& ax_position) {
  if (ax_position->GetAnchor()->IsChildOfLeaf()) {
    return ax_position->GetAnchor()->GetLowestPlatformAncestor();
  }

  return ax_position->GetAnchor();
}

// TODO(crbug.com/40927698): See if we can use string util here.
// https://source.chromium.org/chromium/chromium/src/+/main:base/strings/string_util.h;l=448?q=string_util%20punctuation&ss=chromium
bool IsOpeningPunctuation(char& c) {
  return (c == '(' || c == '{' || c == '[' || c == '<');
}

// We should split the current utterance at a paragraph boundary if the
// AXPosition is at the start of a paragraph and we already have nodes in
// our current granularity segment.
bool ShouldSplitAtParagraph(
    const ui::AXNodePosition::AXPositionInstance& position,
    const a11y::ReadAloudCurrentGranularity current_granularity) {
  return position->AtStartOfParagraph() &&
         (current_granularity.node_ids.size() > 0);
}
