// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything/read_anything_text_aligner.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <numeric>
#include <ranges>
#include <stack>
#include <type_traits>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_node_utils.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

ReadAnythingTextAligner::SuffixArray::SuffixArray() = default;
ReadAnythingTextAligner::SuffixArray::~SuffixArray() = default;

void ReadAnythingTextAligner::SuffixArray::Build(std::u16string_view t) {
  this->text = t;
  this->suffix_array.resize(this->text.size());

  // Fill the suffix array with the starting index of every possible suffix in
  // the text: [0, 1, 2, ... N-1].
  std::iota(this->suffix_array.begin(), this->suffix_array.end(), 0);

  // Sort these indices lexicographically based on the text starting at each
  // index to the suffix array.
  std::sort(this->suffix_array.begin(), this->suffix_array.end(),
            [this](uint32_t i, uint32_t j) {
              return this->text.substr(i) < this->text.substr(j);
            });
}

std::pair<std::vector<uint32_t>::const_iterator,
          std::vector<uint32_t>::const_iterator>
ReadAnythingTextAligner::SuffixArray::FindRange(
    std::u16string_view query) const {
  // Find the range of suffixes that start with the query string. Since the
  // suffixes are sorted, all occurrences of the same prefix will be adjacent in
  // the array.
  return std::equal_range(
      this->suffix_array.begin(), this->suffix_array.end(), query,
      [this](const auto& element, const auto& value) {
        if constexpr (std::is_same_v<std::decay_t<decltype(element)>,
                                     uint32_t>) {
          // Comparing a suffix in the array against the query string.
          return this->text.substr(element, value.size()) < value;
        } else {
          // Comparing the query string against a suffix in the array.
          return element < this->text.substr(value, element.size());
        }
      });
}

// static
base::expected<ReadAnythingTextAligner::TextToAXMap,
               ReadAnythingTextAligner::AlignmentError>
ReadAnythingTextAligner::MapRenderedTextToTree(
    const std::vector<std::u16string>& blocks,
    const ui::AXTree& tree,
    bool is_loaded) {
  ReadAnythingTextAligner aligner(is_loaded);
  return aligner.Align(blocks, tree);
}

ReadAnythingTextAligner::ReadAnythingTextAligner(bool is_loaded)
    : is_loaded_(is_loaded) {}

ReadAnythingTextAligner::~ReadAnythingTextAligner() = default;

base::expected<ReadAnythingTextAligner::TextToAXMap,
               ReadAnythingTextAligner::AlignmentError>
ReadAnythingTextAligner::Align(const std::vector<std::u16string>& blocks,
                               const ui::AXTree& tree) {
  base::ElapsedTimer total_timer;
  base::ElapsedTimer step_timer;

  text_to_ax_map_.clear();
  occupied_ax_ranges_.clear();
  // Ensure the mapping storage size matches the input blocks.
  text_to_ax_map_.resize(blocks.size());

  FlattenAXTree(&tree);
  base::TimeDelta flattening_duration = step_timer.Elapsed();

  const bool is_loaded = is_loaded_;

  // If the AXTree is empty, there is no text to map back to. While the page is
  // still loading, treat this as a low match rate so the caller retries once
  // text arrives.
  if (global_ax_tree_text_.empty()) {
    return base::unexpected(is_loaded ? AlignmentError::kNoText
                                      : AlignmentError::kLowMatchRate);
  }

  // Build a frequency map of the distilled blocks.
  step_timer = base::ElapsedTimer();
  size_t total_distilled_chars = 0;
  base::flat_map<std::u16string_view, int> readability_block_counts;
  for (const auto& block : blocks) {
    if (!block.empty()) {
      total_distilled_chars += block.length();
      readability_block_counts[block]++;
    }
  }
  const size_t tree_mapping_coverage =
      total_distilled_chars == 0
          ? 0
          : (global_ax_tree_text_.size() * 100) / total_distilled_chars;
  if (!is_loaded && tree_mapping_coverage < kMinReadabilityMappingSuccessRate) {
    return base::unexpected(AlignmentError::kLowMatchRate);
  }
  base::TimeDelta initial_anchors_duration = step_timer.Elapsed();

  // Build a Suffix Array index of the original page's text. Finding a text
  // block of length M can then be done in O(M log N) time.
  step_timer = base::ElapsedTimer();
  SuffixArray index;
  index.Build(global_ax_tree_text_);
  base::TimeDelta suffix_array_duration = step_timer.Elapsed();

  step_timer = base::ElapsedTimer();
  std::vector<AlignmentAnchor> candidates =
      FindGloballyUniqueBlocks(blocks, index, readability_block_counts);

  std::vector<AlignmentAnchor> major_anchors =
      FilterMonotonicAnchors(std::move(candidates));
  initial_anchors_duration += step_timer.Elapsed();

  step_timer = base::ElapsedTimer();
  GapSubstringAlignment(blocks, index, major_anchors);
  base::TimeDelta gap_alignment_duration = step_timer.Elapsed();

  std::optional<size_t> success_rate;
  if (total_distilled_chars > 0) {
    size_t total_mapped_chars = 0;
    for (const auto& block_segments : text_to_ax_map_) {
      for (const auto& segment : block_segments) {
        total_mapped_chars += (segment.end - segment.start);
      }
    }
    success_rate = (total_mapped_chars * 100) / total_distilled_chars;
    if (!is_loaded &&
        success_rate.value() < kMinReadabilityMappingSuccessRate) {
      text_to_ax_map_.assign(blocks.size(), {});
      occupied_ax_ranges_.clear();
      return base::unexpected(AlignmentError::kLowMatchRate);
    }
  }

  // Sort mapping segments for every block to ensure physical DOM splitting
  // happens in sequential order in the WebUI.
  for (auto& block_segments : text_to_ax_map_) {
    std::sort(block_segments.begin(), block_segments.end(),
              [](const MappingSegment& a, const MappingSegment& b) {
                return a.start < b.start;
              });
  }

  RecordReadabilityMappingMetrics(
      success_rate, total_timer.Elapsed(), flattening_duration,
      suffix_array_duration, initial_anchors_duration, gap_alignment_duration);

  return std::move(text_to_ax_map_);
}

std::vector<ReadAnythingTextAligner::AlignmentAnchor>
ReadAnythingTextAligner::FindGloballyUniqueBlocks(
    const std::vector<std::u16string>& blocks,
    const SuffixArray& index,
    const base::flat_map<std::u16string_view, int>& block_counts) {
  CHECK_EQ(blocks.size(), text_to_ax_map_.size());

  std::vector<AlignmentAnchor> candidates;

  // Skip block 0 (title). Defer its mapping to subsequent steps once the search
  // space has been restricted. This avoids false body matches if the title is
  // synthesized metadata.
  for (size_t i = 1; i < blocks.size(); ++i) {
    const std::u16string& block = blocks[i];

    if (block.empty()) {
      continue;
    }

    // Binary search the Suffix Array to find all occurrences of this block.
    auto range = index.FindRange(block);

    // If the block exists exactly once in the source page and in the distilled
    // output, set the AXNode segments for this block in |text_to_ax_map_| and
    // add it as a candidate for a major anchor.
    auto it = block_counts.find(block);
    if (std::distance(range.first, range.second) == 1 &&
        it != block_counts.end() && it->second == 1) {
      size_t ax_start = *range.first;
      size_t ax_end = ax_start + block.size();
      DUMP_WILL_BE_CHECK(ax_end <= global_ax_tree_text_.size())
          << "Block exceeds total text length";
      if (ax_end > global_ax_tree_text_.size()) {
        continue;
      }

      text_to_ax_map_[i] = CreateSegmentsForMatch(ax_start, ax_end, 0);
      occupied_ax_ranges_.emplace_back(ax_start, ax_end);
      candidates.emplace_back(i, 0, block.size(), ax_start, ax_end);
    }
  }

  // Sort and merge the occupied ranges so IsAXRangeOccupied() can binary
  // search them.
  std::ranges::sort(occupied_ax_ranges_);
  std::vector<std::pair<size_t, size_t>> merged_ranges;
  for (const auto& range : occupied_ax_ranges_) {
    if (!merged_ranges.empty() && range.first <= merged_ranges.back().second) {
      merged_ranges.back().second =
          std::max(merged_ranges.back().second, range.second);
    } else {
      merged_ranges.push_back(range);
    }
  }
  occupied_ax_ranges_ = std::move(merged_ranges);
  return candidates;
}

std::vector<ReadAnythingTextAligner::AlignmentAnchor>
ReadAnythingTextAligner::FilterMonotonicAnchors(
    std::vector<AlignmentAnchor> candidates) {
  if (candidates.empty()) {
    return {};
  }

  size_t n = candidates.size();
  std::vector<size_t> tails_indices(n, 0);
  std::vector<int> prev(n, -1);
  size_t len = 0;

  // Find the longest increasing anchor set using the LIS algorithm.
  for (size_t i = 0; i < n; ++i) {
    // Find the smallest tail that is >= the current anchor's AX position.
    auto it =
        std::lower_bound(tails_indices.begin(), tails_indices.begin() + len, i,
                         [&candidates](size_t tail_idx, size_t current_idx) {
                           return candidates[tail_idx].ax_start <
                                  candidates[current_idx].ax_start;
                         });
    size_t pos = std::distance(tails_indices.begin(), it);

    if (pos > 0) {
      prev[i] = static_cast<int>(tails_indices[pos - 1]);
    }

    tails_indices[pos] = i;
    if (pos == len) {
      len++;
    }
  }

  // Reconstruct the monotonic subsequence via backtracking.
  std::vector<AlignmentAnchor> result;
  for (int i = static_cast<int>(tails_indices[len - 1]); i != -1; i = prev[i]) {
    result.push_back(candidates[i]);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

void ReadAnythingTextAligner::GapSubstringAlignment(
    const std::vector<std::u16string>& blocks,
    const SuffixArray& index,
    const std::vector<AlignmentAnchor>& major_anchors) {
  if (major_anchors.empty()) {
    // If no major anchors were found, treat the whole text as one gap.
    AlignGap(blocks, index, 0, blocks.size(), 0, global_ax_tree_text_.size());
    return;
  }

  // Gap before the first major anchor.
  AlignGap(blocks, index, 0, major_anchors[0].block_index, 0,
           major_anchors[0].ax_start);

  // Gaps between major anchors.
  for (size_t i = 0; i < major_anchors.size() - 1; ++i) {
    AlignGap(blocks, index, major_anchors[i].block_index + 1,
             major_anchors[i + 1].block_index, major_anchors[i].ax_end,
             major_anchors[i + 1].ax_start);
  }

  // Gap after the last major anchor.
  AlignGap(blocks, index, major_anchors.back().block_index + 1, blocks.size(),
           major_anchors.back().ax_end, global_ax_tree_text_.size());
}

void ReadAnythingTextAligner::AlignGap(
    const std::vector<std::u16string>& blocks,
    const SuffixArray& index,
    size_t block_index_start,
    size_t block_index_end,
    size_t ax_start,
    size_t ax_end) {
  if (block_index_start >= block_index_end || ax_start >= ax_end) {
    return;
  }

  // Prepare unmapped ranges for this specific gap. Only attempt to fill
  // gaps for blocks that haven't been anchored yet.
  std::vector<TextRange> ranges;
  for (size_t i = block_index_start; i < block_index_end; ++i) {
    if (!blocks[i].empty() && text_to_ax_map_[i].empty()) {
      ranges.emplace_back(i, 0, blocks[i].size());
    }
  }

  if (!ranges.empty()) {
    AlignSubstring(blocks, index, std::move(ranges), ax_start, ax_end);
  }
}

void ReadAnythingTextAligner::AlignSubstring(
    const std::vector<std::u16string>& blocks,
    const SuffixArray& index,
    std::vector<TextRange> distilled_ranges,
    size_t ax_start,
    size_t ax_end) {
  if (distilled_ranges.empty() || ax_start >= ax_end) {
    return;
  }

  AlignmentAnchor best_match = FindLongestLocallyUniqueSubstring(
      blocks, index, distilled_ranges, ax_start, ax_end);

  // If no sufficiently long match was found, stop recursing in this gap.
  size_t best_match_length = best_match.block_end - best_match.block_start;
  if (best_match_length < kMinAnchorLength) {
    AlignRelativeOrder(blocks, index, std::move(distilled_ranges), ax_start,
                       ax_end);
    return;
  }

  // Map the found substring.
  std::vector<MappingSegment> segments = CreateSegmentsForMatch(
      best_match.ax_start, best_match.ax_end, best_match.block_start);

  auto& block_mappings = text_to_ax_map_[best_match.block_index];
  block_mappings.insert(block_mappings.end(), segments.begin(), segments.end());

  // Split the distilled_ranges into two sets: those before and after the match.
  std::vector<TextRange> pre_ranges;
  std::vector<TextRange> post_ranges;

  for (const auto& tr : distilled_ranges) {
    if (tr.block_index < best_match.block_index) {
      pre_ranges.push_back(tr);
    } else if (tr.block_index > best_match.block_index) {
      post_ranges.push_back(tr);
    } else {
      // tr is the block containing the match. Split it within the block.
      if (best_match.block_start > tr.start) {
        pre_ranges.emplace_back(tr.block_index, tr.start,
                                best_match.block_start);
      }
      if (best_match.block_end < tr.end) {
        post_ranges.emplace_back(tr.block_index, best_match.block_end, tr.end);
      }
    }
  }

  // Recurse on the remaining gaps.
  AlignSubstring(blocks, index, std::move(pre_ranges), ax_start,
                 best_match.ax_start);
  AlignSubstring(blocks, index, std::move(post_ranges), best_match.ax_end,
                 ax_end);
}

ReadAnythingTextAligner::AlignmentAnchor
ReadAnythingTextAligner::FindLongestLocallyUniqueSubstring(
    const std::vector<std::u16string>& blocks,
    const SuffixArray& index,
    const std::vector<TextRange>& distilled_ranges,
    size_t ax_start,
    size_t ax_end) {
  AlignmentAnchor best_match = {0, 0, 0, 0, 0};

  // Check unmapped blocks in the given gap range.
  for (const auto& range : distilled_ranges) {
    const std::u16string& block_text = blocks[range.block_index];

    // Try every character position in the unmapped range as a potential
    // anchor start.
    for (size_t i = range.start; i < range.end; ++i) {
      // Use binary search on the substring length to find the longest locally
      // unique match starting at index 'i'.
      // Only search for lengths longer than our current 'best' to
      // prune the search space. Shorter than kMinAnchorLength is never used.
      size_t current_best_len = best_match.block_end - best_match.block_start;
      size_t low = std::max(current_best_len + 1, kMinAnchorLength);
      size_t high = range.end - i;

      // Remaining text in this range only shrinks as 'i' advances, so stop
      // once it can no longer reach 'low'.
      if (high < low) {
        break;
      }

      while (low <= high) {
        size_t mid = low + (high - low) / 2;
        std::u16string_view query =
            std::u16string_view(block_text).substr(i, mid);

        // Find all global occurrences of the substring.
        auto global_ax_occurrences = index.FindRange(query);

        // Count how many of the global occurrences fall within our current gap
        // that aren't overlapping with existing anchors. Update our best
        // match if unique and longer.
        size_t valid_count = 0;
        size_t first_valid_ax_start = 0;

        for (auto it = global_ax_occurrences.first;
             it != global_ax_occurrences.second; ++it) {
          size_t cand_ax_start = *it;
          size_t cand_ax_end = cand_ax_start + mid;

          if (cand_ax_start >= ax_start && cand_ax_end <= ax_end &&
              !IsAXRangeOccupied(cand_ax_start, cand_ax_end)) {
            valid_count++;
            first_valid_ax_start = cand_ax_start;
            if (valid_count > 1) {
              break;  // Not locally unique.
            }
          }
        }

        if (valid_count == 1) {
          // Found a locally unique match longer than our current best.
          best_match = {range.block_index, i, i + mid, first_valid_ax_start,
                        first_valid_ax_start + mid};
          low = mid + 1;  // Try to find an even longer one.
        } else {
          high = mid - 1;
        }
      }
    }
  }
  return best_match;
}

void ReadAnythingTextAligner::AlignRelativeOrder(
    const std::vector<std::u16string>& blocks,
    const SuffixArray& index,
    std::vector<TextRange> distilled_ranges,
    size_t ax_start,
    size_t ax_end) {
  size_t current_ax_cursor = ax_start;

  for (const auto& range : distilled_ranges) {
    if (current_ax_cursor >= ax_end) {
      break;
    }

    const std::u16string& block_text = blocks[range.block_index];

    // The search length is bounded by the remaining AXTree gap. Adjust it to
    // ensure candidates can physically fit within [current_ax_cursor, ax_end).
    size_t max_possible_len = ax_end - current_ax_cursor;
    size_t start_len = std::min(range.length(), max_possible_len);

    // Attempt to find the largest substring of the current block range that
    // exists in the remaining AXtree gap. A prefix that occurs at a position
    // also occurs there at any shorter length, so binary search the length.
    size_t best_len = 0;
    size_t best_cand_ax_start = 0;
    size_t low = kMinSequentialMatchLength;
    size_t high = start_len;
    while (low <= high) {
      size_t len = low + (high - low) / 2;
      std::u16string_view query =
          std::u16string_view(block_text).substr(range.start, len);
      auto occurrences = index.FindRange(query);
      size_t earliest_ax_start = std::numeric_limits<size_t>::max();

      for (auto it = occurrences.first; it != occurrences.second; ++it) {
        size_t cand_ax_start = *it;
        size_t cand_ax_end = cand_ax_start + len;

        // Verify the occurrence is within the search gap and track the
        // earliest (chronological) candidate to maintain sequential integrity.
        if (cand_ax_start >= current_ax_cursor && cand_ax_end <= ax_end &&
            !IsAXRangeOccupied(cand_ax_start, cand_ax_end)) {
          earliest_ax_start = std::min(earliest_ax_start, cand_ax_start);
        }
      }

      if (earliest_ax_start != std::numeric_limits<size_t>::max()) {
        best_len = len;
        best_cand_ax_start = earliest_ax_start;
        low = len + 1;
      } else {
        high = len - 1;
      }
    }

    // Map the match and advance the cursor for the next sequential block.
    if (best_len > 0) {
      size_t cand_ax_end = best_cand_ax_start + best_len;
      auto segments =
          CreateSegmentsForMatch(best_cand_ax_start, cand_ax_end, range.start);
      auto& block_mappings = text_to_ax_map_[range.block_index];
      block_mappings.insert(block_mappings.end(), segments.begin(),
                            segments.end());

      current_ax_cursor = cand_ax_end;
    }
  }
}

bool ReadAnythingTextAligner::IsAXRangeOccupied(size_t ax_start,
                                                size_t ax_end) const {
  // The ranges are sorted and disjoint, so only the first range ending after
  // `ax_start` can overlap.
  auto it = std::ranges::upper_bound(occupied_ax_ranges_, ax_start, {},
                                     &std::pair<size_t, size_t>::second);
  return it != occupied_ax_ranges_.end() && it->first < ax_end;
}

// TODO: crbug.com/509578412 - Evaluate consolidating logic with existing text
// traversal methods.
void ReadAnythingTextAligner::FlattenAXTree(const ui::AXTree* tree) {
  flattened_ax_tree_nodes_.clear();
  global_ax_tree_text_.clear();
  if (!tree || !tree->root()) {
    return;
  }

  std::stack<ui::AXNode*> stack;
  stack.push(tree->root());

  // Traverse tree in pre-order DFS to build a contiguous text representation.
  while (!stack.empty()) {
    ui::AXNode* node = stack.top();
    stack.pop();

    // Check if current node should be added to |global_ax_tree_text_|.
    // We use IsLeaf() because it identifies nodes that the accessibility
    // engine considers semantic units (like StaticText). This automatically
    // skips "virtual" internal layout nodes like kInlineTextBox (negative
    // IDs) while keeping structural nodes that contain text.
    if (node->IsLeaf()) {
      // Only process nodes that actually contribute readable
      // text. This helper (from read_anything_node_utils.cc) filters out
      // decorative/empty tags that shouldn't be part of the text alignment.
      // Readability never runs on PDFs or Google Docs.
      if (!a11y::IsTextForReadAnything(node, /*is_pdf=*/false,
                                       /*is_docs=*/false)) {
        continue;
      }

      // Use a11y::GetTextContent to get the node's normalized text. Add this
      // text to the global string and record the node's position.
      std::u16string node_text =
          a11y::GetTextContent(node, /*is_pdf=*/false, /*is_docs=*/false);
      if (!node_text.empty()) {
        flattened_ax_tree_nodes_.push_back(
            {node->id(), node_text, global_ax_tree_text_.length()});
        global_ax_tree_text_ += node_text;
      }
      // Since this is a leaf node we don't need to process its children.
      continue;
    }

    // Add unignored descendants to the stack for processing.
    std::vector<ui::AXNode*> children_to_push;
    for (auto it = node->UnignoredChildrenBegin();
         it != node->UnignoredChildrenEnd(); ++it) {
      children_to_push.push_back(&*it);
    }
    // Push children in reverse order for pre-order traversal.
    for (ui::AXNode* child : std::views::reverse(children_to_push)) {
      stack.push(child);
    }
  }
}

std::vector<ReadAnythingTextAligner::MappingSegment>
ReadAnythingTextAligner::CreateSegmentsForMatch(size_t ax_start,
                                                size_t ax_end,
                                                size_t block_internal_offset) {
  std::vector<MappingSegment> segments;

  // Find the first node starting after ax_start and backtrack to land on the
  // segment containing it. This approach is used for binary search ranges,
  // as it identifies the node starting at or before the target offset.
  auto it = std::upper_bound(flattened_ax_tree_nodes_.begin(),
                             flattened_ax_tree_nodes_.end(), ax_start,
                             [](size_t value, const AXNodeSegment& segment) {
                               return value < segment.start_offset;
                             });
  if (it != flattened_ax_tree_nodes_.begin()) {
    --it;
  }

  // Find all AXNodes that overlap the [ax_start, ax_end) range.
  for (; it != flattened_ax_tree_nodes_.end(); ++it) {
    const auto& ax_segment = *it;
    size_t seg_start = ax_segment.start_offset;
    size_t seg_end = seg_start + ax_segment.text.length();

    if (seg_end <= ax_start) {
      continue;
    }
    if (seg_start >= ax_end) {
      break;
    }

    MappingSegment ms;
    ms.id = ax_segment.id;

    // Convert global AXTree offsets to block-relative offsets for the WebUI.
    ms.start = static_cast<int>(std::max(ax_start, seg_start) - ax_start +
                                block_internal_offset);
    ms.end = static_cast<int>(std::min(ax_end, seg_end) - ax_start +
                              block_internal_offset);
    ms.ax_node_offset =
        static_cast<int>(std::max(ax_start, seg_start) - seg_start);
    segments.push_back(ms);
  }
  return segments;
}

void ReadAnythingTextAligner::RecordReadabilityMappingMetrics(
    std::optional<size_t> success_rate,
    base::TimeDelta total_duration,
    base::TimeDelta flattening_duration,
    base::TimeDelta suffix_array_duration,
    base::TimeDelta initial_anchors_duration,
    base::TimeDelta gap_alignment_duration) {
  if (success_rate.has_value()) {
    base::UmaHistogramPercentage(
        "Accessibility.ReadAnything.ReadabilityMapping.SuccessRate",
        success_rate.value());
  }

  base::UmaHistogramTimes(
      "Accessibility.ReadAnything.ReadabilityMapping.1_Flattening."
      "ExecutionTime",
      flattening_duration);
  base::UmaHistogramTimes(
      "Accessibility.ReadAnything.ReadabilityMapping.2_SuffixArray."
      "ExecutionTime",
      suffix_array_duration);
  base::UmaHistogramTimes(
      "Accessibility.ReadAnything.ReadabilityMapping.3_InitialAnchors."
      "ExecutionTime",
      initial_anchors_duration);
  base::UmaHistogramTimes(
      "Accessibility.ReadAnything.ReadabilityMapping.4_GapAlignment."
      "ExecutionTime",
      gap_alignment_duration);
  base::UmaHistogramTimes(
      "Accessibility.ReadAnything.ReadabilityMapping.0_Total.ExecutionTime",
      total_duration);
}
