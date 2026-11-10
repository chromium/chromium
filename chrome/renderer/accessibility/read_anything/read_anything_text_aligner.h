// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
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
  // Represents a segment of the flattened |global_ax_tree_text_| and the AXNode
  // it comes from. This metadata allows the mapping algorithm for text
  // selection to translate a character range found in |global_ax_tree_text_|
  // back into its original AXNode and relative offset.
  struct AXNodeSegment {
    ui::AXNodeID id;
    std::u16string text;
    // The 0-based starting index of this node's text within the
    // concatenated |global_ax_tree_text_| string.
    size_t start_offset;
  };

  // An index for searching substrings within the flattened
  // |global_ax_tree_text_|. It uses a Suffix Array to find any string in O(log
  // N) time, where N is the length of the article.
  struct SuffixArray {
    SuffixArray();
    ~SuffixArray();

    // A non-owning reference to the text being indexed.
    std::u16string_view text;

    // A list of starting indices of all suffixes in |text|, sorted
    // lexicographically.
    std::vector<uint32_t> suffix_array;

    // Build the index for the given text. This must be called before FindRange.
    void Build(std::u16string_view text);

    // Finds the range of all occurrences of |query| within the indexed text.
    // Returns a pair of iterators into |suffix_array| defining the range
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

  // Represents a specific slice of text within a distilled Readability block
  // that is still waiting to be mapped. Used by GapSubstring alignment to
  // narrow down the search as anchors are "pinned".
  struct TextRange {
    size_t block_index;
    // Character offsets within blocks[block_index].
    size_t start;
    size_t end;

    size_t length() const { return end > start ? end - start : 0; }
    bool empty() const { return length() == 0; }
  };

  explicit ReadAnythingTextAligner(bool is_loaded);
  ~ReadAnythingTextAligner();

  // Runs the alignment. Call only once, since the result is moved out.
  base::expected<TextToAXMap, AlignmentError> Align(
      const std::vector<std::u16string>& blocks,
      const ui::AXTree& tree);

  // Identifies blocks that appear exactly once in both the original AXTree
  // and the distilled Readability output.
  std::vector<AlignmentAnchor> FindGloballyUniqueBlocks(
      const std::vector<std::u16string>& blocks,
      const SuffixArray& index,
      const base::flat_map<std::u16string_view, int>& block_counts);

  // Filters alignment anchor candidates using the Longest Increasing
  // Subsequence (LIS) algorithm based on AXTree positions. Establishing a
  // monotonic order is required for the recursive gap alignment step in the
  // text selection mapping algorithm, as it  ensures that the search ranges
  // between anchors are valid and sequential.
  std::vector<AlignmentAnchor> FilterMonotonicAnchors(
      std::vector<AlignmentAnchor> candidates);

  // Recursively fills the gaps between established anchors by searching for the
  // longest common unique substrings.
  void GapSubstringAlignment(const std::vector<std::u16string>& blocks,
                             const SuffixArray& index,
                             const std::vector<AlignmentAnchor>& major_anchors);

  // Converts the gap search space from block indices into unmapped TextRanges
  // to trigger AlignSubstring for the defined gap.
  void AlignGap(const std::vector<std::u16string>& blocks,
                const SuffixArray& index,
                size_t block_start,
                size_t block_end,
                size_t ax_start,
                size_t ax_end);

  // Recursively finds and maps the Longest Locally Unique Common Substring
  // (LULCS) within a specific distilled and AXTree gap.
  void AlignSubstring(const std::vector<std::u16string>& blocks,
                      const SuffixArray& index,
                      std::vector<TextRange> distilled_ranges,
                      size_t ax_start,
                      size_t ax_end);

  // Sequentially maps the longest possible substrings for any remaining text
  // within a gap.
  void AlignRelativeOrder(const std::vector<std::u16string>& blocks,
                          const SuffixArray& index,
                          std::vector<TextRange> distilled_ranges,
                          size_t ax_start,
                          size_t ax_end);

  // Searches for the Longest Locally Unique Common Substring (LULCS) that
  // appears exactly once in the AXTree gap [ax_start, ax_end).
  //
  // The algorithm iterates through unmapped blocks, using binary search on
  // substring length and the Suffix Array index to find global occurrences
  // in O(log N) time. It then filters for local uniqueness within the
  // current gap, pruning candidates shorter than the current best match.
  //
  // Complexity: O(G * L * log L * log N) average case, where G is the number
  // of ranges, L is block length, and N is article length.
  AlignmentAnchor FindLongestLocallyUniqueSubstring(
      const std::vector<std::u16string>& blocks,
      const SuffixArray& index,
      const std::vector<TextRange>& distilled_ranges,
      size_t ax_start,
      size_t ax_end);

  // Identifies all AXNodes that contribute to a given range from
  // |global_ax_tree_text_| and creates MappingSegments with offsets relative to
  // the distilled block.
  // Args:
  //  |ax_start|:  The start index of the match in |global_ax_tree_text|.
  //  |ax_end|:    The end index (exclusive) in |global_ax_tree_text|.
  //  |block_internal_offset|: The starting character position of this match
  // within the rendered Readability block (usually 0 for whole-block matches).
  std::vector<MappingSegment> CreateSegmentsForMatch(
      size_t ax_start,
      size_t ax_end,
      size_t block_internal_offset);

  // Traverses the AXTree to create a flattened text representation for the text
  // selection mapping algorithm.
  void FlattenAXTree(const ui::AXTree* tree);

  // Logs the execution time for each step of the Readability mapping algorithm.
  void RecordReadabilityMappingMetrics(std::optional<size_t> success_rate,
                                       base::TimeDelta total_duration,
                                       base::TimeDelta flattening_duration,
                                       base::TimeDelta suffix_array_duration,
                                       base::TimeDelta initial_anchors_duration,
                                       base::TimeDelta gap_alignment_duration);

  // Checks if a candidate AXTree range overlaps with text that has already
  // been mapped to a distilled block. This prevents multiple mappings to the
  // same page's text.
  bool IsAXRangeOccupied(size_t ax_start, size_t ax_end) const;

  // Whether the page has finished loading.
  const bool is_loaded_;

  // A mapping from a distilled Readability text block (by index) to its
  // corresponding AXTree segments.
  //
  // Structure:
  // The index of this vector matches the index of the block in the blocks
  // passed to Align().
  //   - Each entry is a vector of MappingSegments, because one distilled block
  //     might correspond to multiple underlying AXNodes (e.g., a paragraph
  //     containing a link).
  //   - If a block fails to map, the entry at that index will be an empty
  //     vector.
  TextToAXMap text_to_ax_map_;

  // A contiguous string representation of all leaf text nodes in the original
  // page's AXTree, in reading order. This serves as the global "search space"
  // for the select text mapping algorithm.
  std::u16string global_ax_tree_text_;

  // A mapping that links character offsets in |global_ax_tree_text_| back to
  // their source AXNodes. Each segment stores the AXNodeID and its starting
  // position within the global string. This is used to translate mapping
  // results back into AXTree coordinates.
  std::vector<AXNodeSegment> flattened_ax_tree_nodes_;

  // Keeps track of which parts of the |global_ax_tree_text_| have already been
  // assigned to a full distilled block. Needed to ensure GapSubstringAlignment
  // doesn't map something to already mapped text in a gap (Ex: a shuffled
  // unique block not in the monotonic anchor list).
  // Each pair is [start, end). Sorted and disjoint once populated.
  std::vector<std::pair<size_t, size_t>> occupied_ax_ranges_;

  // The minimum number of characters required for a substring to be considered
  // an anchor during GapSubstringAlignment mapping.
  static constexpr size_t kMinAnchorLength = 3;

  // The minimum number of characters required for a substring to be considered
  // valid during RelativeOrderAlignment mapping.
  static constexpr size_t kMinSequentialMatchLength = 5;

  // Minimum percentage of distilled characters that must map to the AXTree for
  // mapping to be considered complete rather than deferred for a still-loading
  // tree.
  static constexpr size_t kMinReadabilityMappingSuccessRate = 50;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_TEXT_ALIGNER_H_
