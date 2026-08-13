// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_strip_overflow_indicator_view.h"

#include "base/check.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"

HorizontalTabStripOverflowIndicatorView::
    HorizontalTabStripOverflowIndicatorView(
        views::OverflowIndicatorAlignment side)
    : side_(side) {
  CHECK(side_ == views::OverflowIndicatorAlignment::kLeft ||
        side_ == views::OverflowIndicatorAlignment::kRight);
  SetCanProcessEventsWithinSubtree(false);
  SetFlipCanvasOnPaintForRTLUI(true);
}

HorizontalTabStripOverflowIndicatorView::
    ~HorizontalTabStripOverflowIndicatorView() = default;

void HorizontalTabStripOverflowIndicatorView::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect contents_bounds = GetContentsBounds();
  if (contents_bounds.IsEmpty()) {
    return;
  }

  const ui::ColorProvider* color_provider = GetColorProvider();
  const SkColor base_color =
      color_provider ? color_provider->GetColor(ui::kColorSysOnSurface)
                     : SK_ColorBLACK;

  const bool is_left = side_ == views::OverflowIndicatorAlignment::kLeft;
  const SkPoint points[2] = {
      SkPoint::Make(is_left ? contents_bounds.x() : contents_bounds.right(),
                    contents_bounds.y()),
      SkPoint::Make(is_left ? contents_bounds.right() : contents_bounds.x(),
                    contents_bounds.y()),
  };

  // Draw a horizontal drop shadow gradient matching standard elevation shadow
  // alpha parameters: key shadow (0x3d / ~24% opacity) and ambient shadow
  // (0x1f / ~12% opacity).
  const SkColor key_color = SkColorSetA(base_color, 0x3d);
  const SkColor ambient_color = SkColorSetA(base_color, 0x1f);
  const SkColor combined_color =
      color_utils::GetResultingPaintColor(key_color, ambient_color);

  const SkColor4f colors[3] = {
      SkColor4f::FromColor(combined_color),
      SkColor4f::FromColor(ambient_color),
      SkColor4f::FromColor(SkColorSetA(base_color, SK_AlphaTRANSPARENT)),
  };

  cc::PaintFlags flags;
  flags.setShader(cc::PaintShader::MakeLinearGradient(
      points, colors, /*pos=*/nullptr, 3, SkTileMode::kClamp));
  canvas->DrawRect(contents_bounds, flags);
}

void HorizontalTabStripOverflowIndicatorView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(HorizontalTabStripOverflowIndicatorView)
END_METADATA
