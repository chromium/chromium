// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_table_detector.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <ranges>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
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

// Pages with more horizontal or vertical lines than this (maps, CAD drawings,
// dense charts) are skipped to bound the cost of grouping lines.
constexpr size_t kMaxRulingLinesPerDirection = 1000;

// How far apart, in pixels, a horizontal and a vertical line can be and still
// count as meeting. Strokes at a corner often stop slightly short.
constexpr float kLineConnectionTolerancePx = 3.0f;

// Line positions within this many pixels of a cluster's first position become
// one row or column boundary, so slightly offset cell borders line up.
constexpr float kClusterTolerancePx = 5.0f;

// Minimum number of rows and of columns in a table.
constexpr size_t kMinTableRowsAndColumns = 2;

// Minimum fraction of cells, after trimming empty edges, that must contain
// text. Grids with mostly empty cells are more likely forms or decorative
// boxes than data tables.
constexpr float kMinPopulatedCellFraction = 0.50f;

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

// Disjoint-set (union-find) structure used to group connected line indices.
class DisjointSet {
 public:
  explicit DisjointSet(size_t n) : parent_(n) {
    std::ranges::iota(parent_, 0u);
  }

  size_t Find(size_t i) {
    while (parent_[i] != i) {
      // Shorten the path as it walks.
      parent_[i] = parent_[parent_[i]];
      i = parent_[i];
    }
    return i;
  }

  void Union(size_t i, size_t j) {
    size_t root_i = Find(i);
    size_t root_j = Find(j);
    if (root_i != root_j) {
      parent_[root_i] = root_j;
    }
  }

 private:
  std::vector<size_t> parent_;
};

// Horizontal and vertical lines connected to each other through touching or
// crossing lines, i.e. one candidate table. Has at least one horizontal and one
// vertical line. Holds indices into `h_lines` and `v_lines`.
struct LineGroup {
  std::vector<size_t> horizontal_indices;
  std::vector<size_t> vertical_indices;
};

// Returns whether horizontal `h_line` and vertical `v_line` touch or cross,
// allowing either to fall short of the other by at most
// `kLineConnectionTolerancePx`.
bool LinesConnect(const RulingLine& h_line, const RulingLine& v_line) {
  const float x = v_line.start.x();
  const float y = h_line.start.y();
  return x >= h_line.start.x() - kLineConnectionTolerancePx &&
         x <= h_line.end.x() + kLineConnectionTolerancePx &&
         y >= v_line.start.y() - kLineConnectionTolerancePx &&
         y <= v_line.end.y() + kLineConnectionTolerancePx;
}

// Groups horizontal and vertical lines that touch or cross within
// `kLineConnectionTolerancePx`. `v_lines` must be sorted by x. Lines that
// touch no other line are dropped. For example, these lines form two groups:
//
//   +----+----+       +----+
//   |    |    |       |    |       ------
//   +----+----+       +----+
//   |    |    |
//   |         |
//   +---------+
//
// The middle vertical line stops short of the bottom line, but within the
// tolerance, so it joins the left group. The box is farther away, so it forms
// its own group. The line on the right touches nothing, so it's dropped.
std::vector<LineGroup> GroupConnectedLines(
    base::span<const RulingLine> h_lines,
    base::span<const RulingLine> v_lines) {
  const size_t num_h_lines = h_lines.size();
  const size_t num_v_lines = v_lines.size();
  DisjointSet uf(num_h_lines + num_v_lines);

  // Join each horizontal line with the vertical lines it connects to. Only
  // vertical lines within the horizontal line's x-range, padded by
  // `kLineConnectionTolerancePx`, can connect, so binary search for those.
  for (size_t h_idx = 0; h_idx < num_h_lines; ++h_idx) {
    const RulingLine& h_line = h_lines[h_idx];
    const gfx::Range candidates = GetVerticalLineIndicesInXRange(
        v_lines, gfx::RangeF(h_line.start.x() - kLineConnectionTolerancePx,
                             h_line.end.x() + kLineConnectionTolerancePx));
    for (size_t v_idx = candidates.start(); v_idx < candidates.end(); ++v_idx) {
      if (LinesConnect(h_line, v_lines[v_idx])) {
        uf.Union(h_idx, num_h_lines + v_idx);
      }
    }
  }

  // Collect the lines of each connected group. `uf.Find()` returns the same ID
  // for every line in a group, so that ID is used as the group's index.
  std::vector<LineGroup> groups(num_h_lines + num_v_lines);
  for (size_t h_idx = 0; h_idx < num_h_lines; ++h_idx) {
    groups[uf.Find(h_idx)].horizontal_indices.push_back(h_idx);
  }
  for (size_t v_idx = 0; v_idx < num_v_lines; ++v_idx) {
    groups[uf.Find(num_h_lines + v_idx)].vertical_indices.push_back(v_idx);
  }

  // Keep only groups with both horizontal and vertical lines.
  std::erase_if(groups, [](const LineGroup& group) {
    return group.horizontal_indices.empty() || group.vertical_indices.empty();
  });
  return groups;
}

// Groups nearby values in `coords` and returns each group's mean, in ascending
// order. A group spans up to `kClusterTolerancePx` from its smallest value.
// For example, {0, 30, 34, 60} gives {0, 32, 60}.
std::vector<float> ClusterCoordinates(std::vector<float> coords) {
  std::ranges::sort(coords);
  std::vector<float> clusters;
  for (auto it = coords.begin(); it != coords.end();) {
    auto cluster_end =
        std::upper_bound(it, coords.end(), *it + kClusterTolerancePx);
    clusters.push_back(std::accumulate(it, cluster_end, 0.0f) /
                       static_cast<float>(std::distance(it, cluster_end)));
    it = cluster_end;
  }
  return clusters;
}

// Returns the fixed coordinate of each line in `indices`: y for horizontal
// lines, x for vertical lines.
std::vector<float> LinePositions(base::span<const RulingLine> lines,
                                 base::span<const size_t> indices) {
  return base::ToVector(indices, [&](size_t idx) {
    const RulingLine& line = lines[idx];
    return line.is_horizontal ? line.start.y() : line.start.x();
  });
}

// Returns index `i` such that `boundaries[i] <= value < boundaries[i + 1]`,
// clamped to the first and last intervals.
size_t FindInterval(base::span<const float> boundaries, float value) {
  CHECK_GE(boundaries.size(), 2u);
  // Count the interval starts at or below `value`.
  const base::span<const float> starts =
      boundaries.first(boundaries.size() - 1);
  const size_t count = std::ranges::upper_bound(starts, value) - starts.begin();
  return count > 0 ? count - 1 : 0;
}

// The text run indices in each grid cell, as `cells[row][col]`:
// - Outer: rows.
// - Middle: columns.
// - Inner: indices into `text_runs` of the runs in that cell (empty if none).
using RunIndicesByCell = std::vector<std::vector<std::vector<size_t>>>;

// Puts each text run in the cell that contains its center. Runs centered
// outside the grid are skipped.
RunIndicesByCell AssignRunsToCells(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    const RulingGrid& grid) {
  const size_t row_count = grid.row_boundaries.size() - 1;
  const size_t col_count = grid.column_boundaries.size() - 1;
  RunIndicesByCell cells(row_count,
                         std::vector<std::vector<size_t>>(col_count));

  const gfx::RectF grid_bounds = gfx::BoundingRect(
      gfx::PointF(grid.column_boundaries.front(), grid.row_boundaries.front()),
      gfx::PointF(grid.column_boundaries.back(), grid.row_boundaries.back()));

  for (size_t run_idx = 0; run_idx < text_runs.size(); ++run_idx) {
    const gfx::PointF center = text_runs[run_idx].bounds.CenterPoint();
    if (!grid_bounds.InclusiveContains(center)) {
      continue;
    }
    cells[FindInterval(grid.row_boundaries, center.y())]
         [FindInterval(grid.column_boundaries, center.x())]
             .push_back(run_idx);
  }
  return cells;
}

// Returns the total number of run indices across all cells.
size_t CountAssignedRuns(const RunIndicesByCell& cells) {
  size_t count = 0;
  for (const auto& row : cells) {
    for (const auto& cell_runs : row) {
      count += cell_runs.size();
    }
  }
  return count;
}

// Smallest row and column ranges that cover every non-empty cell, and how many
// cells are non-empty.
struct PopulatedRange {
  gfx::Range rows;
  gfx::Range cols;
  size_t populated_cell_count = 0;
};

// Finds the smallest block of rows and columns that contains every cell with
// text, and counts those cells. At least one cell must have text. For example,
// with text in the X cells below, the block is rows 1-3 and columns 0-1, with 4
// populated cells:
//   . . . .
//   X X . .
//   X . . .
//   . X . .
PopulatedRange FindPopulatedRange(const RunIndicesByCell& cells) {
  size_t min_row = cells.size();
  size_t max_row = 0;
  size_t min_col = cells[0].size();
  size_t max_col = 0;
  size_t populated_cell_count = 0;

  for (size_t r = 0; r < cells.size(); ++r) {
    for (size_t c = 0; c < cells[r].size(); ++c) {
      if (!cells[r][c].empty()) {
        ++populated_cell_count;
        min_row = std::min(min_row, r);
        max_row = std::max(max_row, r);
        min_col = std::min(min_col, c);
        max_col = std::max(max_col, c);
      }
    }
  }

  CHECK_GT(populated_cell_count, 0u);
  return {gfx::Range(min_row, max_row + 1), gfx::Range(min_col, max_col + 1),
          populated_cell_count};
}

// Builds a table from the cells in the given rows and columns, at least one of
// which must have a run. Moves each cell's run indices from `cells` into the
// table. The table's anchor is its lowest run index, i.e. its first text in
// reading order.
HeuristicTable BuildTable(RunIndicesByCell cells,
                          const RulingGrid& grid,
                          gfx::Range rows,
                          gfx::Range cols) {
  const std::vector<float>& row_boundaries = grid.row_boundaries;
  const std::vector<float>& column_boundaries = grid.column_boundaries;
  HeuristicTable table;
  table.bounds = gfx::BoundingRect(
      gfx::PointF(column_boundaries[cols.start()],
                  row_boundaries[rows.start()]),
      gfx::PointF(column_boundaries[cols.end()], row_boundaries[rows.end()]));
  size_t anchor_text_run_index = std::numeric_limits<size_t>::max();
  table.rows.reserve(rows.length());

  // Row `r` spans from row boundary `r` to `r + 1`, across all of `cols`.
  for (size_t r = rows.start(); r < rows.end(); ++r) {
    HeuristicTableRow row;
    row.bounds = gfx::BoundingRect(
        gfx::PointF(column_boundaries[cols.start()], row_boundaries[r]),
        gfx::PointF(column_boundaries[cols.end()], row_boundaries[r + 1]));
    row.cells.reserve(cols.length());

    // Cell `(r, c)` spans from column boundary `c` to `c + 1`. Its run indices
    // move into the table, and the lowest one updates the anchor.
    for (size_t c = cols.start(); c < cols.end(); ++c) {
      HeuristicTableCell cell;
      cell.bounds = gfx::BoundingRect(
          gfx::PointF(column_boundaries[c], row_boundaries[r]),
          gfx::PointF(column_boundaries[c + 1], row_boundaries[r + 1]));
      cell.text_run_indices = std::move(cells[r][c]);
      if (!cell.text_run_indices.empty()) {
        anchor_text_run_index =
            std::min(anchor_text_run_index, cell.text_run_indices.front());
      }
      row.cells.push_back(std::move(cell));
    }
    table.rows.push_back(std::move(row));
  }
  // At least one cell has a run, so the anchor was set.
  CHECK_NE(anchor_text_run_index, std::numeric_limits<size_t>::max());
  table.anchor_text_run_index = anchor_text_run_index;
  return table;
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

std::vector<RulingGrid> DetectRulingGrids(
    base::span<const RulingLine> ruling_lines) {
  const base::span<const RulingLine> v_lines = GetVerticalLines(ruling_lines);
  // `GetVerticalLines()` checks that horizontal lines come first.
  const base::span<const RulingLine> h_lines =
      ruling_lines.first(ruling_lines.size() - v_lines.size());
  if (h_lines.size() > kMaxRulingLinesPerDirection ||
      v_lines.size() > kMaxRulingLinesPerDirection || h_lines.empty() ||
      v_lines.empty()) {
    return {};
  }

  // For each group of connected lines, compute the row and column boundaries.
  // Collect groups with at least 2x2 cells into `grids`.
  std::vector<RulingGrid> grids;
  for (const LineGroup& group : GroupConnectedLines(h_lines, v_lines)) {
    std::vector<float> row_boundaries =
        ClusterCoordinates(LinePositions(h_lines, group.horizontal_indices));
    if (row_boundaries.size() < kMinTableRowsAndColumns + 1) {
      continue;
    }

    std::vector<float> column_boundaries =
        ClusterCoordinates(LinePositions(v_lines, group.vertical_indices));
    if (column_boundaries.size() < kMinTableRowsAndColumns + 1) {
      continue;
    }

    grids.push_back(RulingGrid{
        .row_boundaries = std::move(row_boundaries),
        .column_boundaries = std::move(column_boundaries),
    });
  }
  return grids;
}

std::vector<HeuristicTable> DetectGridTables(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    base::span<const RulingLine> ruling_lines) {
  std::vector<HeuristicTable> tables;
  for (const RulingGrid& grid : DetectRulingGrids(ruling_lines)) {
    RunIndicesByCell cells = AssignRunsToCells(text_runs, grid);
    // Skip grids with no text runs (e.g., a drawing).
    if (CountAssignedRuns(cells) == 0) {
      continue;
    }

    // Find the block of rows and columns that have text, leaving out empty
    // outer ones. Skip grids smaller than 2x2.
    const PopulatedRange populated = FindPopulatedRange(cells);
    if (populated.rows.length() < kMinTableRowsAndColumns ||
        populated.cols.length() < kMinTableRowsAndColumns) {
      continue;
    }

    // Skip grids that are mostly empty.
    const size_t cell_count = populated.rows.length() * populated.cols.length();
    if (populated.populated_cell_count <
        kMinPopulatedCellFraction * cell_count) {
      continue;
    }

    tables.push_back(
        BuildTable(std::move(cells), grid, populated.rows, populated.cols));
  }
  std::ranges::sort(tables, {}, &HeuristicTable::anchor_text_run_index);
  return tables;
}

}  // namespace pdf
