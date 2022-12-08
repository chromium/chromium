// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_view_paint_utils_linux.h"

#include "third_party/skia/include/core/SkRRect.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/view.h"
#include "ui/views/window/frame_background.h"

void PaintRestoredFrameBorderLinux(gfx::Canvas& canvas,
                                   const views::View& view,
                                   views::FrameBackground* frame_background,
                                   const SkRRect& clip,
                                   bool showing_shadow,
                                   const gfx::Insets& border,
                                   const gfx::ShadowValues& shadow_values) {
  const auto* color_provider = view.GetColorProvider();
  if (frame_background) {
    gfx::ScopedCanvas scoped_canvas(&canvas);
    canvas.sk_canvas()->clipRRect(clip, SkClipOp::kIntersect, true);
    auto shadow_inset = showing_shadow ? border : gfx::Insets();
    frame_background->PaintMaximized(
        &canvas, view.GetNativeTheme(), color_provider, shadow_inset.left(),
        shadow_inset.top(), view.width() - shadow_inset.width());
    if (!showing_shadow)
      frame_background->FillFrameBorders(&canvas, &view, border.left(),
                                         border.right(), border.bottom());
  }

  // If rendering shadows, draw a 1px exterior border, otherwise draw a 1px
  // interior border.
  const SkScalar one_pixel = SkFloatToScalar(1 / canvas.image_scale());
  SkRRect outset_rect = clip;
  SkRRect inset_rect = clip;
  if (showing_shadow)
    outset_rect.outset(one_pixel, one_pixel);
  else
    inset_rect.inset(one_pixel, one_pixel);

  cc::PaintFlags flags;
  flags.setColor(color_provider->GetColor(
      showing_shadow ? ui::kColorBubbleBorderWhenShadowPresent
                     : ui::kColorBubbleBorder));
  flags.setAntiAlias(true);
  if (showing_shadow)
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values));

  gfx::ScopedCanvas scoped_canvas(&canvas);
  canvas.sk_canvas()->clipRRect(inset_rect, SkClipOp::kDifference, true);
  canvas.sk_canvas()->drawRRect(outset_rect, flags);
}

gfx::Insets GetRestoredFrameBorderInsetsLinux(
    bool showing_shadow,
    const gfx::Insets& default_border,
    const ui::WindowTiledEdges& tiled_edges,
    const gfx::ShadowValues& shadow_values,
    int resize_border) {
  if (!showing_shadow) {
    auto no_shadow_border = default_border;
    no_shadow_border.set_top(0);
    return no_shadow_border;
  }

  // The border must be at least as large as the shadow.
  gfx::Rect frame_extents;
  for (const auto& shadow_value : shadow_values) {
    const auto shadow_radius = shadow_value.blur() / 4;
    const gfx::InsetsF shadow_insets =
        gfx::InsetsF::TLBR(tiled_edges.top ? 0 : shadow_radius,
                           tiled_edges.left ? 0 : shadow_radius,
                           tiled_edges.bottom ? 0 : shadow_radius,
                           tiled_edges.right ? 0 : shadow_radius);
    gfx::RectF shadow_extents;
    shadow_extents.Inset(-shadow_insets);
    if (!tiled_edges.top) {
      shadow_extents.set_y(shadow_extents.y() + shadow_value.y());
      // If the bottom edge is tiled, fix the height to compensate the addition
      // to the top inset made above.
      if (tiled_edges.bottom)
        shadow_extents.set_height(-shadow_extents.y());
    }
    frame_extents.Union(gfx::ToEnclosingRect(shadow_extents));
  }

  // The border must be at least as large as the input region.
  const auto insets = gfx::Insets::TLBR(tiled_edges.top ? 0 : resize_border,
                                        tiled_edges.left ? 0 : resize_border,
                                        tiled_edges.bottom ? 0 : resize_border,
                                        tiled_edges.right ? 0 : resize_border);
  gfx::Rect input_extents;
  input_extents.Inset(-insets);
  frame_extents.Union(input_extents);

  return gfx::Insets::TLBR(-frame_extents.y(), -frame_extents.x(),
                           frame_extents.bottom(), frame_extents.right());
}
