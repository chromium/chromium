// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/context_highlight/context_highlight_overlay_view.h"

#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

ContextHighlightOverlayView::ContextHighlightOverlayView() {
  // This view should not process events, allowing them to pass through to the
  // web content below.
  SetCanProcessEventsWithinSubtree(false);
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // This view needs its own layer to be drawn above the web content.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

ContextHighlightOverlayView::~ContextHighlightOverlayView() = default;

void ContextHighlightOverlayView::UpdateHighlightBounds(
    const cc::TrackedElementBounds& bounds,
    float device_scale_factor) {
  highlight_rects_.clear();
  float dip_scale = 1.0 / device_scale_factor;

  for (const auto& [id, data] : bounds) {
    gfx::Rect transformed_rect =
        gfx::ScaleToRoundedRect(data.visible_bounds, dip_scale);

    highlight_rects_.push_back(transformed_rect);
  }

  // If the bounds have changed, schedule a repaint.
  SchedulePaint();
}

void ContextHighlightOverlayView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  cc::PaintFlags flags;
  // Use a semi-transparent blue for the highlight.
  flags.setColor(SkColorSetA(SK_ColorBLUE, 128));
  flags.setStyle(cc::PaintFlags::kFill_Style);

  for (const auto& rect : highlight_rects_) {
    canvas->DrawRect(rect, flags);
  }
}

BEGIN_METADATA(ContextHighlightOverlayView)
END_METADATA
