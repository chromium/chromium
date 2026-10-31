// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_table_detector.h"

#include <algorithm>
#include <ranges>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "components/pdf/renderer/pdf_accessibility_text_utils.h"
#include "pdf/accessibility_structs.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/range/range_f.h"

namespace pdf {

namespace {

// How straight a segment must be, in pixels, to count as horizontal or
// vertical.
constexpr float kAxisAlignedTolerancePx = 1.5f;

// Minimum line length in pixels to count as a ruling line and ignore tiny
// marks.
constexpr float kMinRuleLengthPx = 8.0f;

// Maximum thickness, in pixels, of a filled rectangle drawn as a ruling line.
// Producers such as Chrome's print to PDF draw table borders as filled
// rectangles about 1 to 3 pt thick.
constexpr float kThinRuleMaxThicknessPx = 4.0f;

// Maximum distance between segments on the same line to merge them into one.
constexpr float kCollinearMergeTolerancePx = 2.0f;

// A vertical ruling line separates two words if it is in the gap between them,
// or within this many pixels to the left or right of the gap.
constexpr float kRulingLineGapTolerancePx = 2.0f;

// Minimum horizontal gap between two words for a vertical ruling line to
// separate them: the larger of a fixed pixel floor and a fraction of the text
// height. Character boxes are snapped to whole points (1.33 px), so the floor
// sits above that step.
constexpr float kMinDividerGapPx = 3.0f;
constexpr float kMinDividerGapHeightRatio = 0.25f;

// Inset, in pixels, from each side of a character when checking whether a
// vertical ruling line runs through it, so text touching a cell border does not
// disqualify the border.
constexpr float kGlyphCrossingInsetPx = 1.0f;

// Minimum fraction of a text run's height that a vertical ruling line must
// cover to count as running alongside or through its text, so lines that only
// overlap at their ends are ignored.
constexpr float kMinVerticalCoverageRatio = 0.5f;

// An axis-aligned line segment. `position` is the fixed perpendicular
// coordinate (y for horizontal, x for vertical), and `start` to `end` is the
// span along the axis.
struct AxisAlignedSegment {
  float position = 0.0f;
  float start = 0.0f;
  float end = 0.0f;
};

// Appends the centerline of `rect` to `horizontal_segments` or
// `vertical_segments` if one dimension is within `max_thickness` and the other
// meets `kMinRuleLengthPx`.
void AppendCenterline(const gfx::RectF& rect,
                      float max_thickness,
                      std::vector<AxisAlignedSegment>& horizontal_segments,
                      std::vector<AxisAlignedSegment>& vertical_segments) {
  const gfx::PointF center = rect.CenterPoint();
  if (rect.width() >= kMinRuleLengthPx && rect.height() <= max_thickness) {
    horizontal_segments.push_back({center.y(), rect.x(), rect.right()});
  } else if (rect.width() <= max_thickness &&
             rect.height() >= kMinRuleLengthPx) {
    vertical_segments.push_back({center.x(), rect.y(), rect.bottom()});
  }
}

// Merges segments that lie on the same line and touch or overlap.
//
// Segments whose positions are within `kCollinearMergeTolerancePx` of the
// smallest position in a group are collinear. Within each group, segments
// separated by at most `kCollinearMergeTolerancePx` along the axis are merged
// into one segment. Returns segments sorted by position, then start.
std::vector<AxisAlignedSegment> MergeAxisAlignedSegments(
    std::vector<AxisAlignedSegment> segments) {
  // Sort by position so collinear segments are adjacent.
  std::ranges::sort(segments, {}, &AxisAlignedSegment::position);
  std::vector<AxisAlignedSegment> merged_segments;
  base::span<AxisAlignedSegment> remaining(segments);
  while (!remaining.empty()) {
    // A group is every segment whose position is within tolerance of the
    // group's first (smallest) position. Comparing against the first position
    // instead of the previous segment's stops a series of slightly offset
    // segments (e.g. at y = 0, 1.5, 3, 4.5) from chaining into one group.
    const float group_position = remaining[0].position;
    size_t group_size = 1;
    while (group_size < remaining.size() &&
           remaining[group_size].position - group_position <=
               kCollinearMergeTolerancePx) {
      ++group_size;
    }

    // Sort the group by `start` (left to right for horizontal segments, top to
    // bottom for vertical) so the sweep below visits segments in order.
    base::span<AxisAlignedSegment> group = remaining.first(group_size);
    std::ranges::sort(group, {}, &AxisAlignedSegment::start);

    // Walk the sorted segments, merging each one into `current` if it starts
    // no more than `kCollinearMergeTolerancePx` past the end of `current`.
    // All merged segments use `group_position` as their position.
    AxisAlignedSegment current = group[0];
    current.position = group_position;
    for (const AxisAlignedSegment& next : group.subspan<1u>()) {
      if (next.start <= current.end + kCollinearMergeTolerancePx) {
        // Use max() so a segment contained in `current` does not shorten it.
        current.end = std::max(current.end, next.end);
      } else {
        // The gap is too large, so finish `current` and start a new segment
        // from `next`.
        merged_segments.push_back(current);
        current = next;
        current.position = group_position;
      }
    }
    merged_segments.push_back(current);

    // Move on to the first segment after this group.
    remaining = remaining.subspan(group_size);
  }

  // Groups are visited in ascending position and each group is swept in
  // ascending `start`, so `merged_segments` is already sorted.
  return merged_segments;
}

// Returns the left edge of each character in the horizontal, left-to-right
// `run`, followed by the right edge of its last character, so character `i`
// spans from edge `i` to edge `i + 1`.
std::vector<float> GetCharEdges(
    const chrome_pdf::AccessibilityTextRunInfo& run,
    base::span<const chrome_pdf::AccessibilityCharInfo> run_chars) {
  std::vector<float> edges;
  edges.reserve(run_chars.size() + 1);
  float edge = run.bounds.x();
  edges.push_back(edge);
  for (const chrome_pdf::AccessibilityCharInfo& char_info : run_chars) {
    edge += static_cast<float>(char_info.char_width);
    edges.push_back(edge);
  }
  return edges;
}

// Returns the range of indices of the lines in `vertical_lines`, which must be
// sorted by x, whose x is in `x_range`.
gfx::Range GetVerticalLineIndicesInXRange(
    base::span<const RulingLine> vertical_lines,
    const gfx::RangeF& x_range) {
  auto get_x = [](const RulingLine& line) { return line.start.x(); };
  auto begin =
      std::ranges::lower_bound(vertical_lines, x_range.start(), {}, get_x);
  auto end = std::ranges::upper_bound(begin, vertical_lines.end(),
                                      x_range.end(), {}, get_x);
  return gfx::Range(static_cast<size_t>(begin - vertical_lines.begin()),
                    static_cast<size_t>(end - vertical_lines.begin()));
}

// Returns whether the vertical `line` covers at least
// `kMinVerticalCoverageRatio` of the height of `bounds`. Returns false if
// `bounds` has no height.
bool CoversVerticalRange(const RulingLine& line, const gfx::RectF& bounds) {
  CHECK(!line.is_horizontal);
  if (bounds.height() <= 0.0f) {
    return false;
  }
  const float overlap = std::min(line.end.y(), bounds.bottom()) -
                        std::max(line.start.y(), bounds.y());
  return overlap >= kMinVerticalCoverageRatio * bounds.height();
}

// Sets `line_crosses_text[i]` to true for each line `vertical_lines[i]`, sorted
// by x, that runs through `box`, shrunk horizontally by `kGlyphCrossingInsetPx`
// on each side. `line_crosses_text` must have one entry per line. Never resets
// entries to false, so calling this for each box in turn marks every line that
// runs through any of them.
void MarkLinesCrossingBox(base::span<const RulingLine> vertical_lines,
                          const gfx::RectF& box,
                          base::span<bool> line_crosses_text) {
  CHECK_EQ(line_crosses_text.size(), vertical_lines.size());
  const float inner_left = box.x() + kGlyphCrossingInsetPx;
  const float inner_right = box.right() - kGlyphCrossingInsetPx;
  if (inner_left > inner_right) {
    return;
  }
  // `GetVerticalLineIndicesInXRange()` binary searches for the lines within
  // the box's x-range, so only those lines are checked.
  const gfx::Range indices = GetVerticalLineIndicesInXRange(
      vertical_lines, gfx::RangeF(inner_left, inner_right));
  // Visit each of those lines along with its entry in `line_crosses_text`.
  for (auto [line, crosses_text] : std::views::zip(
           vertical_lines.subspan(indices.start(), indices.length()),
           line_crosses_text.subspan(indices.start(), indices.length()))) {
    crosses_text = crosses_text || CoversVerticalRange(line, box);
  }
}

// Returns the vertical lines in `ruling_lines`, which must be ordered as
// returned by `ExtractRulingLines()`. The vertical lines are sorted by x, as
// needed by the binary searches in `GetVerticalLineIndicesInXRange()`.
base::span<const RulingLine> GetVerticalLines(
    base::span<const RulingLine> ruling_lines) {
  CHECK(std::ranges::is_partitioned(ruling_lines, &RulingLine::is_horizontal));
  auto vertical_begin =
      std::ranges::partition_point(ruling_lines, &RulingLine::is_horizontal);
  base::span<const RulingLine> vertical_lines = ruling_lines.subspan(
      static_cast<size_t>(vertical_begin - ruling_lines.begin()));
  CHECK(std::ranges::is_sorted(vertical_lines, {}, [](const RulingLine& line) {
    return line.start.x();
  }));
  return vertical_lines;
}

// Returns the vertical lines in `ruling_lines` that don't run through any
// character in `text_runs`, such as table column dividers (as opposed to chart
// gridlines behind text), sorted by x. `chars` holds the characters of
// `text_runs`.
std::vector<RulingLine> GetVerticalDividers(
    base::span<const RulingLine> ruling_lines,
    base::span<const chrome_pdf::AccessibilityCharInfo> chars,
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs) {
  const base::span<const RulingLine> vertical_lines =
      GetVerticalLines(ruling_lines);
  if (vertical_lines.empty()) {
    return {};
  }

  auto line_crosses_text =
      base::HeapArray<bool>::WithSize(vertical_lines.size());
  for (const chrome_pdf::AccessibilityTextRunInfo& run : text_runs) {
    auto [run_chars, remaining_chars] = chars.split_at(run.len);
    chars = remaining_chars;
    if (!IsLeftToRightRun(run)) {
      // Character positions are only computed for left-to-right runs, so treat
      // the whole run as one box.
      MarkLinesCrossingBox(vertical_lines, run.bounds, line_crosses_text);
      continue;
    }
    const std::vector<float> edges = GetCharEdges(run, run_chars);
    // Check each character's box, skipping whitespace since dividers are
    // expected to run through the spaces between cells.
    for (size_t i = 0; i < run_chars.size(); ++i) {
      if (!IsWhitespace(run_chars[i])) {
        MarkLinesCrossingBox(
            vertical_lines,
            gfx::RectF(edges[i], run.bounds.y(), edges[i + 1] - edges[i],
                       run.bounds.height()),
            line_crosses_text);
      }
    }
  }

  std::vector<RulingLine> vertical_dividers;
  vertical_dividers.reserve(vertical_lines.size());
  for (auto [line, crosses_text] :
       std::views::zip(vertical_lines, line_crosses_text)) {
    if (!crosses_text) {
      vertical_dividers.push_back(line);
    }
  }
  return vertical_dividers;
}

// Returns whether a line in `vertical_dividers`, sorted by x, lies in the
// horizontal `gap` between two words of `run` and covers enough of the run's
// height.
bool IsSeparatedByVerticalDivider(
    base::span<const RulingLine> vertical_dividers,
    const chrome_pdf::AccessibilityTextRunInfo& run,
    const gfx::RangeF& gap) {
  // Table cells have padding around their dividers. Require a gap that scales
  // with text size.
  const float min_gap = std::max(
      kMinDividerGapPx, kMinDividerGapHeightRatio * run.bounds.height());
  if (gap.length() < min_gap) {
    return false;
  }
  const gfx::Range indices = GetVerticalLineIndicesInXRange(
      vertical_dividers, gfx::RangeF(gap.start() - kRulingLineGapTolerancePx,
                                     gap.end() + kRulingLineGapTolerancePx));
  return std::ranges::any_of(
      vertical_dividers.subspan(indices.start(), indices.length()),
      [&](const RulingLine& line) {
        return CoversVerticalRange(line, run.bounds);
      });
}

// Returns the part of `run` covering the character offsets in `chars`, spanning
// `x_range` horizontally.
chrome_pdf::AccessibilityTextRunInfo MakeRunPart(
    const chrome_pdf::AccessibilityTextRunInfo& run,
    const gfx::Range& chars,
    const gfx::RangeF& x_range) {
  chrome_pdf::AccessibilityTextRunInfo part = run;
  part.start_index += base::checked_cast<uint32_t>(chars.start());
  part.len = base::checked_cast<uint32_t>(chars.length());
  part.bounds = gfx::RectF(x_range.start(), run.bounds.y(), x_range.length(),
                           run.bounds.height());
  return part;
}

// Splits `run` at every space between two words that a line in
// `vertical_dividers` runs through, and appends all the parts to `split_runs`
// in order. Each space stays with the part on its left. Appends `run` unchanged
// if there is no split.
void AppendRunParts(
    const chrome_pdf::AccessibilityTextRunInfo& run,
    base::span<const chrome_pdf::AccessibilityCharInfo> run_chars,
    base::span<const RulingLine> vertical_dividers,
    std::vector<chrome_pdf::AccessibilityTextRunInfo>& split_runs) {
  if (!IsLeftToRightRun(run)) {
    split_runs.push_back(run);
    return;
  }

  // First character of the current part, as an offset into `run`. Each split
  // moves it to the word after the split.
  uint32_t part_start = 0;

  // Scan for each span of consecutive whitespace. If a divider separates the
  // words on either side, append the part ending after the whitespace, and
  // start the next part at the word after it (on the right).
  uint32_t i = 0;
  const std::vector<float> edges = GetCharEdges(run, run_chars);
  while (i < run.len) {
    if (!IsWhitespace(run_chars[i])) {
      ++i;
      continue;
    }
    const uint32_t whitespace_start = i;
    while (i < run.len && IsWhitespace(run_chars[i])) {
      ++i;
    }
    const uint32_t whitespace_end = i;
    // Only split between two words.
    if (whitespace_start == 0 || whitespace_end == run.len ||
        !IsSeparatedByVerticalDivider(
            vertical_dividers, run,
            gfx::RangeF(edges[whitespace_start], edges[whitespace_end]))) {
      continue;
    }
    split_runs.push_back(
        MakeRunPart(run, gfx::Range(part_start, whitespace_end),
                    gfx::RangeF(edges[part_start], edges[whitespace_start])));
    part_start = whitespace_end;
  }

  // No whitespace was split by a divider, so keep the run unchanged.
  if (part_start == 0) {
    split_runs.push_back(run);
    return;
  }

  // Add the final (right-most) part if the run was split.
  split_runs.push_back(
      MakeRunPart(run, gfx::Range(part_start, run.len),
                  gfx::RangeF(edges[part_start], run.bounds.right())));
}

// Remaps `range` to the split text runs.
void RemapTextRunRange(base::span<const uint32_t> first_split_run_indices,
                       chrome_pdf::AccessibilityTextRunRangeInfo& range) {
  const uint32_t start = first_split_run_indices[range.index];
  const uint32_t end = first_split_run_indices[range.index + range.count];
  range.index = start;
  range.count = end - start;
}

}  // namespace

std::vector<RulingLine> ExtractRulingLines(
    base::span<const chrome_pdf::AccessibilityLineSegment> line_segments,
    base::span<const gfx::RectF> filled_rects) {
  // Keep segments that are nearly horizontal or vertical and long enough, and
  // filled rectangles that are thin and long enough, snapped to their
  // centerline.
  std::vector<AxisAlignedSegment> horizontal_segments;
  std::vector<AxisAlignedSegment> vertical_segments;
  for (const chrome_pdf::AccessibilityLineSegment& segment : line_segments) {
    AppendCenterline(gfx::BoundingRect(segment.start, segment.end),
                     kAxisAlignedTolerancePx, horizontal_segments,
                     vertical_segments);
  }
  for (const gfx::RectF& rect : filled_rects) {
    AppendCenterline(rect, kThinRuleMaxThicknessPx, horizontal_segments,
                     vertical_segments);
  }

  const std::vector<AxisAlignedSegment> merged_horizontal =
      MergeAxisAlignedSegments(std::move(horizontal_segments));
  const std::vector<AxisAlignedSegment> merged_vertical =
      MergeAxisAlignedSegments(std::move(vertical_segments));

  std::vector<RulingLine> lines;
  lines.reserve(merged_horizontal.size() + merged_vertical.size());
  for (const AxisAlignedSegment& h : merged_horizontal) {
    lines.push_back(
        {{h.start, h.position}, {h.end, h.position}, /*is_horizontal=*/true});
  }
  for (const AxisAlignedSegment& v : merged_vertical) {
    lines.push_back(
        {{v.position, v.start}, {v.position, v.end}, /*is_horizontal=*/false});
  }
  return lines;
}

SplitTextRunsResult SplitTextRunsAtVerticalDividers(
    base::span<const RulingLine> ruling_lines,
    base::span<const chrome_pdf::AccessibilityCharInfo> chars,
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs) {
  const std::vector<RulingLine> vertical_dividers =
      GetVerticalDividers(ruling_lines, chars, text_runs);
  if (vertical_dividers.empty()) {
    return {};
  }

  SplitTextRunsResult result;
  result.first_split_run_indices.reserve(text_runs.size() + 1);
  result.first_split_run_indices.push_back(0);
  for (const chrome_pdf::AccessibilityTextRunInfo& run : text_runs) {
    auto [run_chars, remaining_chars] = chars.split_at(run.len);
    AppendRunParts(run, run_chars, vertical_dividers, result.text_runs);
    result.first_split_run_indices.push_back(
        base::checked_cast<uint32_t>(result.text_runs.size()));
    chars = remaining_chars;
  }
  // `result.text_runs` contains at least all of the `text_runs`. If their sizes
  // are equal then none of the runs were actually split.
  if (result.text_runs.size() == text_runs.size()) {
    return {};
  }
  CHECK_GT(result.text_runs.size(), text_runs.size());
  return result;
}

void RemapTextRunIndices(base::span<const uint32_t> first_split_run_indices,
                         chrome_pdf::AccessibilityPageObjects& page_objects) {
  // Text runs already break at annotation boundaries, so each annotation still
  // covers whole runs after splitting.
  for (chrome_pdf::AccessibilityLinkInfo& link : page_objects.links) {
    RemapTextRunRange(first_split_run_indices, link.text_range);
  }
  for (chrome_pdf::AccessibilityHighlightInfo& highlight :
       page_objects.highlights) {
    RemapTextRunRange(first_split_run_indices, highlight.text_range);
  }
  for (chrome_pdf::AccessibilityFormGraphicInfo& graphic :
       page_objects.graphics) {
    RemapTextRunRange(first_split_run_indices, graphic.text_range);
  }
  for (chrome_pdf::AccessibilityImageInfo& image : page_objects.images) {
    image.text_run_index = first_split_run_indices[image.text_run_index];
  }
}

}  // namespace pdf
