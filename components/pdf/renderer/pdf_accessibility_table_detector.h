// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_
#define COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_

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

// Splits each horizontal, left-to-right text run in `text_runs` where a
// vertical line in `ruling_lines`, such as a table column divider, runs through
// the whitespace between two words. `ruling_lines` must be ordered as returned
// by `ExtractRulingLines()`, and `chars` holds the characters of `text_runs`.
// Returns the split text runs, or an empty vector if no run was split.
std::vector<chrome_pdf::AccessibilityTextRunInfo>
SplitTextRunsAtVerticalDividers(
    base::span<const RulingLine> ruling_lines,
    base::span<const chrome_pdf::AccessibilityCharInfo> chars,
    base::span<const chrome_pdf::AccessibilityTextRunInfo> text_runs);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_ACCESSIBILITY_TABLE_DETECTOR_H_
