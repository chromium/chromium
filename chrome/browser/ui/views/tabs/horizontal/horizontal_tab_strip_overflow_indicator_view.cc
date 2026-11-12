// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_strip_overflow_indicator_view.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_shader.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/widget/widget.h"

// static
int HorizontalTabStripOverflowIndicatorView::GetMaxOpaqueThickness() {
  return std::max(TabStyle::Get()->GetBottomExtensionWidth(),
                  TabStyle::Get()->GetTabOverlap());
}

// static
int HorizontalTabStripOverflowIndicatorView::GetMaxTotalThickness() {
  return GetMaxOpaqueThickness() + kShadowThickness;
}

HorizontalTabStripOverflowIndicatorView::
    HorizontalTabStripOverflowIndicatorView(
        views::OverflowIndicatorAlignment side,
        BrowserView* browser_view)
    : side_(side), browser_view_(browser_view) {
  CHECK(side_ == views::OverflowIndicatorAlignment::kLeft ||
        side_ == views::OverflowIndicatorAlignment::kRight);
  CHECK(browser_view_);
  SetCanProcessEventsWithinSubtree(false);
  // The tabstrip overlaps the toolbar below it. Inset the bottom of the
  // indicator by the toolbar overlap so the shadow gradient does not extend
  // past the bottom of the tabstrip into the toolbar.
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, 0, GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap), 0)));
}

HorizontalTabStripOverflowIndicatorView::
    ~HorizontalTabStripOverflowIndicatorView() = default;

void HorizontalTabStripOverflowIndicatorView::SetOpaqueThickness(
    int opaque_thickness) {
  CHECK_GE(opaque_thickness, 0);
  CHECK_LE(opaque_thickness, GetMaxOpaqueThickness());
  if (opaque_thickness_ == opaque_thickness) {
    return;
  }
  opaque_thickness_ = opaque_thickness;
  SchedulePaint();
}

void HorizontalTabStripOverflowIndicatorView::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &HorizontalTabStripOverflowIndicatorView::SchedulePaint,
          base::Unretained(this)));
}

void HorizontalTabStripOverflowIndicatorView::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

void HorizontalTabStripOverflowIndicatorView::OnPaint(gfx::Canvas* canvas) {
  const gfx::Rect contents_bounds = GetContentsBounds();
  if (contents_bounds.IsEmpty()) {
    return;
  }

  const ui::ColorProvider* color_provider = GetColorProvider();
  const bool is_left = (side_ == views::OverflowIndicatorAlignment::kLeft) !=
                       base::i18n::IsRTL();
  const int opaque_width = std::min(opaque_thickness_, contents_bounds.width());
  if (opaque_width > 0) {
    const gfx::Rect opaque_bounds(
        is_left ? contents_bounds.x() : contents_bounds.right() - opaque_width,
        contents_bounds.y(), opaque_width, contents_bounds.height());
    gfx::ScopedCanvas scoped_canvas(canvas);
    canvas->ClipRect(opaque_bounds);
    ThemedBackground::PaintBackground(
        canvas, this, browser_view_,
        ThemedBackground::ThemeChoice::kFrameTheme);
  }

  const int shadow_width =
      std::min(kShadowThickness, contents_bounds.width() - opaque_width);
  if (shadow_width <= 0) {
    return;
  }
  const gfx::Rect shadow_bounds(
      is_left ? contents_bounds.x() + opaque_width
              : contents_bounds.right() - opaque_width - shadow_width,
      contents_bounds.y(), shadow_width, contents_bounds.height());

  const SkColor base_color =
      color_provider ? color_provider->GetColor(ui::kColorShadowBase)
                     : SK_ColorBLACK;

  const SkPoint points[2] = {
      SkPoint::Make(is_left ? shadow_bounds.x() : shadow_bounds.right(),
                    shadow_bounds.y()),
      SkPoint::Make(is_left ? shadow_bounds.right() : shadow_bounds.x(),
                    shadow_bounds.y()),
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
  canvas->DrawRect(shadow_bounds, flags);
}

void HorizontalTabStripOverflowIndicatorView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(HorizontalTabStripOverflowIndicatorView)
END_METADATA
