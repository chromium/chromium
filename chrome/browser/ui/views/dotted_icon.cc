// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dotted_icon.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace {

constexpr float kRingStrokeWidthDp = 1.5;

// Ring Segments
constexpr int kNumSmallSegments = 4;
constexpr int kNumSpacingSegments = kNumSmallSegments + 1;
constexpr int kLargeSegmentSweepAngle = 160;

// Split the remaining space in half so that half is allocated for the small
// segment of the ring and the other half is for the spacing between segments
constexpr int kAllocatedSpace = (360 - kLargeSegmentSweepAngle) / 2;
constexpr int kSpacingSweepAngle = kAllocatedSpace / kNumSpacingSegments;
constexpr int kSmallSegmentSweepAngle = kAllocatedSpace / kNumSmallSegments;

// Paints arc starting at `start_angle` with a `sweep` in degrees.
// A starting angle of 0 means that the arc starts on the right side of `bounds`
// and continues drawing the arc in a clockwise direction for `sweep` degrees
void PaintArc(gfx::Canvas* canvas,
              const gfx::Rect& bounds,
              const SkScalar start_angle,
              const SkScalar sweep,
              const cc::PaintFlags& flags) {
  gfx::RectF oval(bounds);
  // Inset by half the stroke width to make sure the whole arc is inside
  // the visible rect.
  const double inset = kRingStrokeWidthDp / 2.0;
  oval.Inset(inset);

  SkPath path;
  path.arcTo(RectFToSkRect(oval), start_angle, sweep, true);
  canvas->DrawPath(path, flags);
}

}  // namespace

void PaintRingDottedPath(gfx::Canvas* canvas,
                         const gfx::Rect& ring_bounds,
                         SkColor ring_color,
                         double opacity_ratio) {
  opacity_ratio = std::clamp(opacity_ratio, 0.0, 1.0);

  // Common flags for both parts of the ring.
  cc::PaintFlags flags;
  flags.setColor(ring_color);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setStrokeWidth(kRingStrokeWidthDp);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  const float ring_color_opacity =
      static_cast<float>(SkColorGetA(ring_color)) / SK_AlphaOPAQUE;
  flags.setAlphaf(static_cast<float>(
      gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, opacity_ratio) *
      ring_color_opacity));

  // Draw the large segment centered on the left side.
  const int large_segment_start_angle = 180 - kLargeSegmentSweepAngle / 2;
  PaintArc(canvas, ring_bounds, large_segment_start_angle,
           kLargeSegmentSweepAngle, flags);

  // Draw the small segments evenly spaced around the rest of the ring.
  const int small_segments_start_angle =
      180 + (kLargeSegmentSweepAngle / 2) + kSpacingSweepAngle;
  for (int i = 0; i < kNumSmallSegments; i++) {
    const int start_angle =
        small_segments_start_angle +
        (i * (kSmallSegmentSweepAngle + kSpacingSweepAngle));
    PaintArc(canvas, ring_bounds, start_angle % 360, kSmallSegmentSweepAngle,
             flags);
  }
}
