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

void ReadAloudCurrentGranularity::AddText(ui::AXNodeID id,
                                          int text_start,
                                          int text_end,
                                          const std::u16string& text_to_add) {
  DCHECK((text_end - text_start) == (int)text_to_add.size());

  ReadAloudTextSegment segment{id, text_start, text_end};
  segments[segment.id] = segment;
  node_ids.push_back(segment.id);

  int current_text_length = text.length();
  text += text_to_add;
  index_map.insert({{current_text_length, text.length()}, id});
}

std::vector<ReadAloudTextSegment>
ReadAloudCurrentGranularity::GetSegmentsForRange(int start_index,
                                                 int end_index) {
  if (start_index >= end_index) {
    return {};
  }

  auto start = index_map.upper_bound({start_index, INT_MAX});
  if (start == index_map.begin()) {
    // start_index is too low
    return {};
  }
  // Rewind by 1 to find the actual start from the upper_bound
  --start;

  auto end = index_map.upper_bound({end_index, INT_MAX});
  if (end == index_map.begin()) {
    // end_index is too low
    return {};
  }

  std::vector<ReadAloudTextSegment> ret;
  while (start != end) {
    auto range = start->first;
    if ((start_index >= range.second) || (end_index <= range.first)) {
      break;
    }
    ui::AXNodeID node = start->second;
    auto segment = segments[node];

    // For the first and last segments, we need to adjust the appropriate
    // boundary to take start_index and end_index into account. For middle
    // segments, we simply add the entire segment.
    int text_start = (start_index >= range.first)
                         ? (segment.text_start + start_index - range.first)
                         : segment.text_start;
    int text_end = (end_index < range.second)
                       ? (segment.text_start + end_index - range.first)
                       : segment.text_end;

    ret.push_back(ReadAloudTextSegment(node, text_start, text_end));
    ++start;
  }

  return ret;
}

void ReadAloudCurrentGranularity::CalculatePhrases() {
  if (text.size() == 0) {
    phrase_boundaries.clear();
    return;
  }

  // Add a phrase boundary every 3 words. TODO(crbug.com/330749762): replace
  // with the correct phrase calculation.
  std::size_t start = 0;
  int count = 0;
  do {
    if (count % 3 == 0) {
      phrase_boundaries.push_back(start);
    }
    int next_word = GetNextWord(text.substr(start));
    if (next_word == 0) {
      break;
    }
    start += next_word;
    ++count;
    if (start >= text.size()) {
      break;
    }
  } while (start);
  phrase_boundaries.push_back(text.size());
}

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
