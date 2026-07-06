// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_background_blur_backdrop.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"

VerticalTabStripBackgroundBlurBackdrop::
    VerticalTabStripBackgroundBlurBackdrop() {
  SetCanProcessEventsWithinSubtree(false);
}

VerticalTabStripBackgroundBlurBackdrop::
    ~VerticalTabStripBackgroundBlurBackdrop() = default;

void VerticalTabStripBackgroundBlurBackdrop::UpdateGeometry(
    const VerticalTabStripRegionView* from,
    float alpha) {
  SetClipPath(
      from->background()->AsA<CustomCornersBackground>()->GetBackgroundPath());
  alpha_ = alpha;
}

void VerticalTabStripBackgroundBlurBackdrop::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  if (alpha_ <= 0.0f) {
    return;
  }

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
}

BEGIN_METADATA(VerticalTabStripBackgroundBlurBackdrop)
END_METADATA
