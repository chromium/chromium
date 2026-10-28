// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_accessibility_table_detector.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "pdf/accessibility_structs.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

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

}  // namespace pdf
