// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_background_blur_backdrop.h"

#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"

VerticalTabStripBackgroundBlurBackdrop::
    VerticalTabStripBackgroundBlurBackdrop() {
  SetCanProcessEventsWithinSubtree(false);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

VerticalTabStripBackgroundBlurBackdrop::
    ~VerticalTabStripBackgroundBlurBackdrop() = default;

void VerticalTabStripBackgroundBlurBackdrop::UpdateGeometry(
    const VerticalTabStripRegionView* from,
    float alpha) {
  border_path_ =
      from->background()->AsA<CustomCornersBackground>()->GetBackgroundPath();
  if (alpha_ != alpha) {
    alpha_ = alpha;
    SchedulePaint();
  }
}

void VerticalTabStripBackgroundBlurBackdrop::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (alpha_ <= 0.0f || border_path_.isEmpty()) {
    return;
  }

  gfx::ScopedCanvas scoped(canvas);

  canvas->ClipPath(border_path_, false);

  // Use a neutral color approprise for light/dark behind the tabstrip. Using
  // toolbar color will blend with the rest of the UI. Note that this is an
  // arbitrary choice; we probably want to use the frame color and adjust it
  // specifically for whatever Glass is doing to tune the color for legibility.
  auto color = GetColorProvider()->GetColor(kColorToolbar);
  color = SkColorSetA(color, base::ClampRound(alpha_ * 255.0f));
  gfx::Rect rect = GetLocalBounds();
  rect.set_width(VerticalTabStripRegionView::kCollapsedWidth);
  rect = GetMirroredRect(rect);
  canvas->FillRect(rect, color);

  // Paint a border excluding the area already painted, making a subtle visual
  // buffer zone between the edge of the tabstrip and the content it is flying
  // over. This partly mimics the MacOS edge effect on window frames in Glass
  // mode.
  canvas->ClipRect(rect, SkClipOp::kDifference);
  constexpr float kEdgeStrokeWidth = 4.0f;
  cc::PaintFlags flags;
  flags.setStrokeWidth(2.0f * kEdgeStrokeWidth);
  flags.setAntiAlias(true);
  flags.setColor(color);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  canvas->DrawPath(border_path_, flags);
}

BEGIN_METADATA(VerticalTabStripBackgroundBlurBackdrop)
END_METADATA
