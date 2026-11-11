// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "ui/accessibility/ax_node_id_forward.h"

namespace ui {
class AXTree;
}  // namespace ui

// Maps rendered Readability text blocks to the AXNodes their text came from.
class ReadAnythingTextAligner {
 public:
  // Represents a segment of text within an AXNode. A distilled Readability
  // block may be mapped to multiple AXNodes (e.g., a paragraph with a link
  // inside); each part of that mapping is a MappingSegment.
  struct MappingSegment {
    ui::AXNodeID id;
    // The 0-based start and end character offsets within the *Readability
    // block's* text content that correspond to this AXNode.
    int start;
    int end;
    // The 0-based character offset within the *AXNode's* own text content where
    // this segment begins.
    int ax_node_offset;
  };

  // The mapped segments for each rendered block, in the same order as the
  // blocks. A block that couldn't be matched has an empty entry.
  using TextToAXMap = std::vector<std::vector<MappingSegment>>;

  // Why alignment failed to produce a map.
  enum class AlignmentError {
    // The page is still loading and the match rate is below the threshold
    // (including when the tree has no text yet). The caller can retry once
    // more text arrives.
    kLowMatchRate,
    // The loaded tree has no readable text to map against.
    kNoText,
  };

  // Maps each rendered Readability block to the AXNodes its text came from.
  //
  // While the page is still loading, a low match rate returns kLowMatchRate so
  // the caller can try again once more text arrives.
  //
  // Once `is_loaded` is true, any match rate is accepted, and failure only
  // returns kNoText when the tree has no readable text.
  static base::expected<TextToAXMap, AlignmentError> MapRenderedTextToTree(
      const std::vector<std::u16string>& blocks,
      const ui::AXTree& tree,
      bool is_loaded);

 private:
  // One AXNode's slice of `global_ax_tree_text_`. Used to turn a character
  // range in that string back into AXNodes and offsets within them.
  struct AXNodeSegment {
    ui::AXNodeID id;
    size_t text_length;
    // The 0-based starting index of this node's text within the
    // concatenated `global_ax_tree_text_` string.
    size_t start_offset;
  };

  // An index for searching substrings within the flattened
  // `global_ax_tree_text_`. It uses a Suffix Array to find a query of length M
  // in O(M log N) time, where N is the length of the article.
  struct SuffixArray {
    SuffixArray();
    ~SuffixArray();

    // A non-owning reference to the text being indexed.
    std::u16string_view text;

    // A list of starting indices of all suffixes in `text`, sorted
    // lexicographically.
    std::vector<uint32_t> suffix_array;

    // Build the index for the given text. This must be called before FindRange.
    void Build(std::u16string_view text);

    // Finds the range of all occurrences of `query` within the indexed text.
    // Returns a pair of iterators into `suffix_array` defining the range
    // [begin, end). The number of occurrences is std::distance(begin, end).
    std::pair<std::vector<uint32_t>::const_iterator,
              std::vector<uint32_t>::const_iterator>
    FindRange(std::u16string_view query) const;
  };

  // A confirmed alignment point between a distilled Readability block and a
  // character range in the global flattened AXTree text.
  struct AlignmentAnchor {
    size_t block_index;
    size_t block_start;
    size_t block_end;
    size_t ax_start;
    size_t ax_end;
  };

  // A slice of a rendered block that hasn't been mapped yet. The gap alignment
  // steps split these into smaller slices as matches are found.
  struct TextRange {
    size_t block_index;
    // Character offsets within `blocks_[block_index]`.
    size_t start;
    size_t end;

    size_t length() const { return end > start ? end - start : 0; }
    bool empty() const { return length() == 0; }
  };

  ReadAnythingTextAligner(const std::vector<std::u16string>& blocks,
                          const ui::AXTree& tree,
                          bool is_loaded);
  ~ReadAnythingTextAligner();

  // Runs the alignment. Call only once, since the result is moved out.
  base::expected<TextToAXMap, AlignmentError> Align();

  // Identifies blocks that appear exactly once in both the original AXTree
  // and the distilled Readability output.
  std::vector<AlignmentAnchor> FindGloballyUniqueBlocks(
      const SuffixArray& index,
      const base::flat_map<std::u16string_view, int>& block_counts);

  // Keeps the largest set of anchors that appear in the same order in the
  // AXTree text as in the blocks, using a longest increasing subsequence.
  //
  // Gap alignment needs this ordering, so that the ranges between neighboring
  // anchors are valid.
  static std::vector<AlignmentAnchor> FilterMonotonicAnchors(
      std::vector<AlignmentAnchor> candidates);

  // Recursively fills the gaps between established anchors by searching for the
  // longest common unique substrings.
  void GapSubstringAlignment(const SuffixArray& index,
                             const std::vector<AlignmentAnchor>& major_anchors);

  // Converts the gap search space from block indices into unmapped TextRanges
  // to trigger AlignSubstring for the defined gap.
  void AlignGap(const SuffixArray& index,
                size_t block_start,
                size_t block_end,
                size_t ax_start,
                size_t ax_end);

  // Recursively finds and maps the Longest Locally Unique Common Substring
  // (LULCS) within a specific distilled and AXTree gap.
  void AlignSubstring(const SuffixArray& index,
                      std::vector<TextRange> distilled_ranges,
                      size_t ax_start,
                      size_t ax_end);

  // Sequentially maps the longest possible substrings for any remaining text
  // within a gap.
  void AlignRelativeOrder(const SuffixArray& index,
                          std::vector<TextRange> distilled_ranges,
                          size_t ax_start,
                          size_t ax_end);

  // Searches for the Longest Locally Unique Common Substring (LULCS) that
  // appears exactly once in the AXTree gap [ax_start, ax_end).
  //
  // The algorithm iterates through unmapped blocks, using binary search on
  // substring length and the Suffix Array index to find global occurrences
  // of a query of length M in O(M log N) time. It then filters for local
  // uniqueness within the current gap, pruning candidates shorter than the
  // current best match.
  //
  // Complexity: O(G * L * log L * log N) average case, where G is the number
  // of ranges, L is block length, and N is article length.
  AlignmentAnchor FindLongestLocallyUniqueSubstring(
      const SuffixArray& index,
      const std::vector<TextRange>& distilled_ranges,
      size_t ax_start,
      size_t ax_end);

  // Finds the AXNodes covering [`ax_start`, `ax_end`) in
  // `global_ax_tree_text_` and returns a MappingSegment for each one.
  //
  // `block_internal_offset` is where the match starts within the rendered
  // block, usually 0 for whole-block matches. Segment offsets are relative to
  // the block.
  std::vector<MappingSegment> CreateSegmentsForMatch(
      size_t ax_start,
      size_t ax_end,
      size_t block_internal_offset);

  // Fills `global_ax_tree_text_` and `flattened_ax_tree_nodes_` with the
  // tree's readable leaf text, in reading order.
  void FlattenAXTree();

  // Checks if a candidate AXTree range overlaps with text that has already
  // been mapped to a distilled block. This prevents multiple mappings to the
  // same page's text.
  bool IsAXRangeOccupied(size_t ax_start, size_t ax_end) const;

  // The minimum length of a substring match in AlignSubstring().
  static constexpr size_t kMinAnchorLength = 3;

  // The minimum length of a substring match in AlignRelativeOrder().
  static constexpr size_t kMinSequentialMatchLength = 5;

  // Minimum percentage of distilled characters that must map to the AXTree for
  // mapping to be considered complete rather than deferred for a still-loading
  // tree.
  static constexpr size_t kMinReadabilityMappingSuccessRate = 50;

  // The text of each rendered Readability block, in display order. Indexes
  // into this match indexes into `text_to_ax_map_`.
  const raw_ref<const std::vector<std::u16string>> blocks_;

  // The page's AXTree that the blocks are matched against.
  const raw_ref<const ui::AXTree> tree_;

  // Whether the page has finished loading.
  const bool is_loaded_;

  // The result being built, with one entry per block in `blocks_`.
  //
  // A block can map to several AXNodes, e.g. a paragraph containing a link.
  //
  // A block that couldn't be matched has an empty entry.
  TextToAXMap text_to_ax_map_;

  // All readable leaf text in the page's AXTree, joined in reading order. The
  // blocks are searched for in this string.
  std::u16string global_ax_tree_text_;

  // Where each AXNode's text starts in `global_ax_tree_text_`, sorted by
  // position.
  std::vector<AXNodeSegment> flattened_ax_tree_nodes_;

  // Ranges of `global_ax_tree_text_` already matched to a whole block, so gap
  // alignment doesn't match them again. This happens e.g. when a unique block
  // was moved and so isn't in the ordered anchor list.
  //
  // Each pair is [start, end). Sorted and non-overlapping once filled.
  std::vector<std::pair<size_t, size_t>> occupied_ax_ranges_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_
