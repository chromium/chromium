// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_TRAVERSAL_UTILS_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_TRAVERSAL_UTILS_H_

#include <string>

#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_position.h"

// Utilities for traversing the accessibility tree for Read Aloud.
// TODO(crbug.com/346612365): Rename to util instead of utils.

// A current segment of text that will be consumed by Read Aloud.
struct ReadAloudTextSegment {
  // The AXNodeID associated with this particular text segment.
  ui::AXNodeID id;

  // The starting index for the text with the node of the given id.
  int text_start;

  // The ending index for the text with the node of the given id.
  int text_end;
};

namespace a11y {

// During text traversal when adding new text to the current speech segment,
// this is used to indicate the next traversal steps.
enum class TraversalState {
  // The end of the current granularity segment.
  EndOfSegment = 0,
  // Traversal should continue to the next valid AXNode.
  ContinueToNextNode = 1,
  // Traversal should continue with text within the current node.
  ContinueInCurrentNode = 2,
};

// A representation of multiple ReadAloudTextSegments that are processed
// by Read Aloud at a single moment. For example, when using sentence
// granularity, the list of ReadAloudTextSegments in a
// ReadAloudCurrentGranularity will include all ReadAloudTextSegments
// necessary to represent a single sentence.
struct ReadAloudCurrentGranularity {
  ReadAloudCurrentGranularity();
  ReadAloudCurrentGranularity(const ReadAloudCurrentGranularity& other);
  ~ReadAloudCurrentGranularity();

  // Adds a node containing text to the current granularity.
  void AddText(ui::AXNodeID id,
               int text_start,
               int text_end,
               const std::u16string& text);

  // For a given start..end range within `text`, returns a list of nodes and
  // offsets corresponding to that range.
  std::vector<ReadAloudTextSegment> GetSegmentsForRange(int start_index,
                                                        int end_index);

  // Calculate phrase boundaries from the text.
  void CalculatePhrases();

  // Calculate the phrase_boundaries index corresponding to a text index.
  int GetPhraseIndex(int index) {
    return std::upper_bound(phrase_boundaries.begin(), phrase_boundaries.end(),
                            index) -
           phrase_boundaries.begin() - 1;
  }

  // All of the ReadAloudTextSegments in the current granularity.
  std::map<ui::AXNodeID, ReadAloudTextSegment> segments;

  // Because GetCurrentText returns a vector of node ids to be used by
  // TypeScript also store the node ids as a vector for easier retrieval.
  std::vector<ui::AXNodeID> node_ids;

  // Map of text start and end indices of text to a specific AXNodeID.
  // The text for a given segment may span multiple AXNodes, such as
  // Node 1: This is a
  // Node 2: link
  // Node 3: in a separate node.
  // which is presented as a single segment when using sentence granularity.
  // However, when we get word callbacks, we get them in terms of the text
  // index across the entire segment of text, not by node. Therefore, this
  // mapping helps us better parse callbacks for different types of
  // granularity highlighting.
  // TODO(b/40927698): Investigate using this to replace
  // highlightedNodeToOffsetInParent in app.ts
  std::map<std::pair<int, int>, ui::AXNodeID> index_map;

  // The human readable text represented by this segment of node ids. This
  // is stored separately for easier retrieval for non-sentence granularity
  // highlighting.
  std::u16string text;

  // Boundary indices for phrases. Starts at 0.
  std::vector<int> phrase_boundaries;
};
}  // namespace a11y

// Returns the index of the next sentence of the given text, such that the
// next sentence is equivalent to text.substr(0, <returned_index>).
int GetNextSentence(const std::u16string& text, bool is_pdf);

// Returns the index of the next word of the given text, such that the
// next word is equivalent to text.substr(0, <returned_index>).
int GetNextWord(const std::u16string& text);

// Returns true if both positions are non-null and equal.
bool ArePositionsEqual(const ui::AXNodePosition::AXPositionInstance& position,
                       const ui::AXNodePosition::AXPositionInstance& other);

// Returns the correct anchor node from an AXPositionInstance that should be
// used by Read Aloud. AXPosition can sometimes return leaf nodes that don't
// actually correspond to the AXNodes we're using in Reading Mode, so we need
// to get a parent node from the AXPosition's returned anchor.
ui::AXNode* GetAnchorNode(
    const ui::AXNodePosition::AXPositionInstance& position);

// Uses the given AXNodePosition to return the next node that should be spoken
// by Read Aloud.
ui::AXNode* GetNextNodeFromPosition(
    const ui::AXNodePosition::AXPositionInstance& ax_position);

// Returns if the given character can be considered opening puncutation.
// This is used to ensure we're not reading out opening punctuation
// as a separate segment.
bool IsOpeningPunctuation(char& c);

// Returns whether we should split the current utterance at a paragraph
// boundary.
bool ShouldSplitAtParagraph(
    const ui::AXNodePosition::AXPositionInstance& position,
    const a11y::ReadAloudCurrentGranularity current_granularity);

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ALOUD_TRAVERSAL_UTILS_H_
