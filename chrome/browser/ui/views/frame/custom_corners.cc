// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/custom_corners.h"

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/themed_background.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/gfx/scoped_canvas.h"

CustomCorners::CustomCorners(BrowserView& browser_view)
    : browser_view_(browser_view) {
  // Browser may not yet have a widget, but we need to track the widget in
  // case we need active/inactive frame color.
  if (browser_view_->GetWidget()) {
    // This hooks up the active/inactive state listener.
    OnViewAddedToWidget(&browser_view);
  } else {
    // This will hook up the listener when the browser view is added to a
    // widget.
    browser_view_observation_.Observe(&browser_view);
  }
}

CustomCorners::~CustomCorners() = default;

void CustomCorners::OnViewAddedToWidget(views::View* view) {
  CHECK_EQ(view, &*browser_view_);
  browser_view_observation_.Reset();
  browser_paint_as_active_subscription_ =
      view->GetWidget()->RegisterPaintAsActiveChangedCallback(
          base::BindRepeating(&CustomCorners::OnBrowserPaintAsActiveChanged,
                              base::Unretained(this)));
}

void CustomCorners::PaintPath(gfx::Canvas* canvas,
                              const SkPath& path,
                              ColorChoiceWithAlpha color_choice,
                              bool anti_alias) const {
  if (!color_choice.is_visible()) {
    return;
  }

  if (std::holds_alternative<ToolbarTheme>(color_choice.color) ||
      std::holds_alternative<FrameTheme>(color_choice.color)) {
    gfx::ScopedCanvas scoped(canvas);
    canvas->ClipPath(path, anti_alias);
    // If this theme color should have any transparency, we paint it to a
    // layer so we can adjust the layer's transparency.
    const bool has_transparency = !color_choice.is_opaque();
    if (has_transparency) {
      cc::PaintFlags layer_flags;
      layer_flags.setAlphaf(color_choice.opacity);
      canvas->SaveLayerWithFlags(layer_flags);
    }
    ThemedBackground::PaintBackground(
        canvas, &GetView(), &browser_view(),
        std::holds_alternative<ToolbarTheme>(color_choice.color)
            ? ThemedBackground::ThemeChoice::kToolbarTheme
            : ThemedBackground::ThemeChoice::kFrameTheme);
  } else {
    ui::ColorId color_id = std::get<ui::ColorId>(color_choice.color);

    cc::PaintFlags flags;
    flags.setAntiAlias(anti_alias);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(SkColorSetA(
        GetView().GetColorProvider()->GetColor(color_id),
        std::clamp(static_cast<int>(255 * color_choice.opacity), 0, 255)));
    canvas->DrawPath(path, flags);
  }
}

// static
CustomCorners::VisualCorner CustomCorners::ToVisualCorner(
    CornerOrientation orientation) {
  const bool mirror = base::i18n::IsRTL();
  switch (orientation) {
    case CornerOrientation::kTopLeading:
      return mirror ? VisualCorner::kTopRight : VisualCorner::kTopLeft;
    case CornerOrientation::kTopTrailing:
      return mirror ? VisualCorner::kTopLeft : VisualCorner::kTopRight;
    case CornerOrientation::kBottomLeading:
      return mirror ? VisualCorner::kBottomRight : VisualCorner::kBottomLeft;
    case CornerOrientation::kBottomTrailing:
      return mirror ? VisualCorner::kBottomLeft : VisualCorner::kBottomRight;
  }
}

// static
SkPath CustomCorners::GetCornerPath(VisualCorner orientation,
                                    const gfx::Rect& inbounds,
                                    const gfx::Insets& insets) {
  const gfx::Rect border_rect = inbounds;
  gfx::Rect curve_rect = inbounds;
  curve_rect.Inset(insets);
  const SkVector corner_radius(curve_rect.width(), curve_rect.height());

  // Set up a clip path.
  SkPathBuilder clip_path;
  switch (orientation) {
    case VisualCorner::kTopLeft:
      clip_path.moveTo(border_rect.x(), border_rect.y());
      clip_path.lineTo(curve_rect.right(), border_rect.y());
      if (insets.top()) {
        clip_path.lineTo(curve_rect.right(), curve_rect.y());
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(curve_rect.x(), curve_rect.bottom()));
      if (insets.left()) {
        clip_path.lineTo(border_rect.x(), curve_rect.bottom());
      }
      clip_path.lineTo(border_rect.x(), border_rect.y());
      break;
    case VisualCorner::kTopRight:
      clip_path.moveTo(curve_rect.x(), border_rect.y());
      clip_path.lineTo(border_rect.right(), border_rect.y());
      clip_path.lineTo(border_rect.right(), curve_rect.bottom());
      if (insets.right()) {
        clip_path.lineTo(curve_rect.right(), curve_rect.bottom());
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(curve_rect.x(), curve_rect.y()));
      if (insets.top()) {
        clip_path.lineTo(curve_rect.x(), border_rect.y());
      }
      break;
    case VisualCorner::kBottomLeft:
      clip_path.moveTo(border_rect.x(), curve_rect.y());
      if (insets.left()) {
        clip_path.lineTo(curve_rect.x(), curve_rect.y());
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(curve_rect.right(), curve_rect.bottom()));
      if (insets.bottom()) {
        clip_path.lineTo(curve_rect.right(), border_rect.bottom());
      }
      clip_path.lineTo(border_rect.x(), border_rect.bottom());
      clip_path.lineTo(border_rect.x(), curve_rect.y());
      break;
    case VisualCorner::kBottomRight:
      clip_path.moveTo(border_rect.right(), curve_rect.y());
      clip_path.lineTo(border_rect.right(), border_rect.bottom());
      clip_path.lineTo(curve_rect.x(), border_rect.bottom());
      if (insets.bottom()) {
        clip_path.lineTo(curve_rect.x(), curve_rect.bottom());
      }
      clip_path.arcTo(corner_radius, 0, SkPathBuilder::kSmall_ArcSize,
                      SkPathDirection::kCCW,
                      SkPoint(curve_rect.right(), curve_rect.y()));
      if (insets.right()) {
        clip_path.lineTo(border_rect.right(), curve_rect.y());
      }
      break;
  }

  return clip_path.detach();
}
