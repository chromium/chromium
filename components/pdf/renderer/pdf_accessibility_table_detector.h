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

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_
