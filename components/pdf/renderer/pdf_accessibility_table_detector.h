// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "pdf/accessibility_structs.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace pdf {

// A horizontal or vertical ruling line on a page, such as a table border or
// column divider, in the same pixel coordinates as
// `chrome_pdf::AccessibilityTextRunInfo::bounds`.
struct RulingLine {
  friend bool operator==(const RulingLine&, const RulingLine&) = default;

  // For horizontal lines, `start.y() == end.y()` and `start.x() <= end.x()`.
  // For vertical lines, `start.x() == end.x()` and `start.y() <= end.y()`.
  gfx::PointF start;
  gfx::PointF end;
  bool is_horizontal = false;
};

// Extracts horizontal and vertical ruling lines from `line_segments` and from
// the centerlines of thin `filled_rects`, and merges touching or overlapping
// collinear lines. Returns horizontal lines first, then vertical lines, each
// sorted by position and then start.
std::vector<RulingLine> ExtractRulingLines(
    base::span<const chrome_pdf::AccessibilityLineSegment> line_segments,
    base::span<const gfx::RectF> filled_rects);

// The result of `SplitTextRunsAtVerticalDividers()`.
struct SplitTextRunsResult {
  // All text runs in order, with each split run replaced by its parts.
  std::vector<chrome_pdf::AccessibilityTextRunInfo> text_runs;
  // `first_split_run_indices[i]` is the index in `text_runs` of the first part
  // of original run `i`, with one extra entry for the end. Pass it to
  // `RemapTextRunIndices()` to update annotations that refer to text runs by
  // index. Returning this map instead of remapped annotations keeps splitting
  // independent of annotations, and lets callers copy annotations only when a
  // run was split.
  std::vector<uint32_t> first_split_run_indices;
};

// Splits each horizontal, left-to-right text run in `text_runs` where a
// vertical line in `ruling_lines`, such as a table column divider, runs through
// the whitespace between two words. Ignores lines that run through any
// character, such as chart gridlines behind text. `ruling_lines` must be
// ordered as returned by `ExtractRulingLines()`, and `chars` holds the
// characters of `text_runs`. Returns an empty `SplitTextRunsResult` if no run
// was split.
SplitTextRunsResult SplitTextRunsAtVerticalDividers(
    base::span<const RulingLine> ruling_lines,
    base::span<const chrome_pdf::AccessibilityCharInfo> chars,
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs);

// Remaps the text run indices in `page_objects` to the split text runs, using
// `first_split_run_indices` from `SplitTextRunsResult`.
void RemapTextRunIndices(base::span<const uint32_t> first_split_run_indices,
                         chrome_pdf::AccessibilityPageObjects& page_objects);

// A grid drawn with ruling lines. Boundaries are ascending, in page-relative
// pixels (the same space as `chrome_pdf::AccessibilityTextRunInfo::bounds`).
// Each vector has at least 3 entries, so the grid has at least 2x2 cells. For
// example:
//
//        0    30   60   90
//     0  +----+----+----+
//        |    |    |    |   row_boundaries:    {0, 30, 60}
//    30  +----+----+----+   column_boundaries: {0, 30, 60, 90}
//        |    |    |    |
//    60  +----+----+----+
//
// A line that spans only part of the grid still adds a boundary across the
// whole grid, so every row has the same columns.
struct RulingGrid {
  friend bool operator==(const RulingGrid&, const RulingGrid&) = default;

  std::vector<float> row_boundaries;
  std::vector<float> column_boundaries;
};

// A cell of a detected grid: the area between two adjacent row boundaries and
// two adjacent column boundaries.
struct HeuristicTableCell {
  // Page-relative pixels.
  gfx::RectF bounds;
  // Indices into the page's text runs whose centers fall in `bounds`, in
  // ascending (stream) order. Empty for cells without text.
  std::vector<size_t> text_run_indices;
};

// One row of a detected grid.
struct HeuristicTableRow {
  // Page-relative pixels.
  gfx::RectF bounds;
  // One cell per column, left to right, including empty cells.
  std::vector<HeuristicTableCell> cells;
};

// A table detected from a grid of ruling lines. Every row has the same number
// of cells; there are no row or column spans.
struct HeuristicTable {
  // Page-relative pixels.
  gfx::RectF bounds;
  // Lowest text run index in the table. Callers emit the table at this
  // position in reading order.
  size_t anchor_text_run_index = 0;
  // Rows from top to bottom. At least 2 rows of at least 2 cells each.
  std::vector<HeuristicTableRow> rows;
};

// Finds grids formed by horizontal and vertical ruling lines that touch or
// cross. Doesn't look at text. `ruling_lines` must be ordered as returned by
// `ExtractRulingLines()`. Returns grids in no particular order.
std::vector<RulingGrid> DetectRulingGrids(
    base::span<const RulingLine> ruling_lines);

// Detects tables drawn as connected grids of horizontal and vertical ruling
// lines. Returns tables sorted by `anchor_text_run_index`, with each cell's
// `text_run_indices` ascending. Tables share no text runs: when two would, such
// as a grid nested in another grid's cell, keeps the one with the earlier
// anchor, or the larger one if the anchors are equal.
std::vector<HeuristicTable> DetectGridTables(
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs,
    base::span<const RulingLine> ruling_lines);

// Returns the text run indices in all of `table`'s cells, row by row and left
// to right within each row.
std::vector<size_t> GetTableTextRunIndices(const HeuristicTable& table);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_
