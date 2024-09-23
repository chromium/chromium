// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/windows_icon_painter.h"

#include "base/numerics/safe_conversions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"

namespace {

// The rounded rect corner radius for MaximizeIcon and RestoreIcon in
// Windows 11.
constexpr float kWin11RoundedCornerRadius = 1.5f;

void DrawRect(gfx::Canvas* canvas,
              const gfx::Rect& rect,
              const cc::PaintFlags& flags) {
  gfx::RectF rect_f(rect);

  // The rect is used as a bounding box, and the stroke is kept within.
  float stroke_half_width = flags.getStrokeWidth() / 2;
  rect_f.Inset(stroke_half_width);

  canvas->DrawRect(rect_f, flags);
}

}  // namespace

Windows10IconPainter::Windows10IconPainter() = default;
Windows10IconPainter::~Windows10IconPainter() = default;

void Windows10IconPainter::PaintMinimizeIcon(gfx::Canvas* canvas,
                                             gfx::Rect& symbol_rect,
                                             cc::PaintFlags& flags) {
  const int y = symbol_rect.CenterPoint().y();
  const gfx::Point p1 = gfx::Point(symbol_rect.x(), y);
  const gfx::Point p2 = gfx::Point(symbol_rect.right(), y);
  canvas->DrawLine(p1, p2, flags);
}

void Windows10IconPainter::PaintMaximizeIcon(gfx::Canvas* canvas,
                                             gfx::Rect& symbol_rect,
                                             cc::PaintFlags& flags) {
  DrawRect(canvas, symbol_rect, flags);
}

void Windows10IconPainter::PaintRestoreIcon(gfx::Canvas* canvas,
                                            gfx::Rect& symbol_rect,
                                            cc::PaintFlags& flags) {
  const int separation = base::ClampFloor(2 * canvas->image_scale());
  symbol_rect.Inset(gfx::Insets::TLBR(separation, 0, 0, separation));

  // Bottom left ("in front") square.
  DrawRect(canvas, symbol_rect, flags);

  // Top right ("behind") square.
  canvas->ClipRect(symbol_rect, SkClipOp::kDifference);
  symbol_rect.Offset(separation, -separation);
  DrawRect(canvas, symbol_rect, flags);
}

void Windows10IconPainter::PaintCloseIcon(gfx::Canvas* canvas,
                                          gfx::Rect& symbol_rect,
                                          cc::PaintFlags& flags) {
  // TODO(bsep): This sometimes draws misaligned at fractional device scales
  // because the button's origin isn't necessarily aligned to pixels.
  flags.setAntiAlias(true);
  canvas->ClipRect(symbol_rect);
  SkPath path;
  path.moveTo(symbol_rect.x(), symbol_rect.y());
  path.lineTo(symbol_rect.right(), symbol_rect.bottom());
  path.moveTo(symbol_rect.right(), symbol_rect.y());
  path.lineTo(symbol_rect.x(), symbol_rect.bottom());
  canvas->DrawPath(path, flags);
}

void Windows10IconPainter::PaintTabSearchIcon(gfx::Canvas* canvas,
                                              gfx::Rect& symbol_rect,
                                              cc::PaintFlags& flags) {
  flags.setAntiAlias(true);
  canvas->ClipRect(symbol_rect);
  // The chevron should occupy the space between the upper and lower quarter
  // of the `symbol_rect` bounds.
  symbol_rect.Inset(gfx::Insets::VH(symbol_rect.height() / 4, 0));
  SkPath path;
  path.moveTo(gfx::PointToSkPoint(symbol_rect.origin()));
  path.lineTo(gfx::PointToSkPoint(symbol_rect.bottom_center()));
  path.lineTo(gfx::PointToSkPoint(symbol_rect.top_right()));
  canvas->DrawPath(path, flags);
}

Windows11IconPainter::Windows11IconPainter() = default;
Windows11IconPainter::~Windows11IconPainter() = default;

void Windows11IconPainter::PaintMaximizeIcon(gfx::Canvas* canvas,
                                             gfx::Rect& symbol_rect,
                                             cc::PaintFlags& flags) {
  gfx::RectF rect_f(symbol_rect);
  flags.setAntiAlias(true);
  const float corner_radius =
      base::ClampFloor(kWin11RoundedCornerRadius * canvas->image_scale());

  // The symbol rect is used as a bounding box, and the stroke is kept within.
  float stroke_half_width = flags.getStrokeWidth() / 2;
  rect_f.Inset(stroke_half_width);

  canvas->DrawRoundRect(rect_f, corner_radius, flags);
}

void Windows11IconPainter::PaintRestoreIcon(gfx::Canvas* canvas,
                                            gfx::Rect& symbol_rect,
                                            cc::PaintFlags& flags) {
  gfx::RectF rect_f(symbol_rect);
  const float separation = 2.0f * canvas->image_scale();
  const int round_rect_radius =
      base::ClampFloor(kWin11RoundedCornerRadius * canvas->image_scale());
  const int top_rect_upper_right_radius = 2 * round_rect_radius;
  flags.setAntiAlias(true);

  // The symbol rect is used as a bounding box, and the stroke is kept within.
  const float stroke_half_width = flags.getStrokeWidth() / 2;
  rect_f.Inset(stroke_half_width);

  // Shrink the rect to make space for the top rect.
  rect_f.Inset(gfx::InsetsF::TLBR(separation, 0, 0, separation));

  gfx::RRectF rrect(rect_f, round_rect_radius);

  // Bottom ("in front") rounded square.
  canvas->sk_canvas()->drawRRect(SkRRect(rrect), flags);

  // The top rounded square is clipped to only draw the top and right edges,
  // and give corners a flat base. The clip rect sits right below the bottom
  // half of the stroke.
  gfx::RRectF clip_rrect(rrect);
  const float clip_rect_growth = separation - stroke_half_width;
  // The upper-right corner radius doesn't need to be updated because |Outset|
  // does that for us.
  clip_rrect.Outset(clip_rect_growth);
  canvas->sk_canvas()->clipRRect(SkRRect(clip_rrect), SkClipOp::kDifference,
                                 true);

  // Top ("behind") rounded square.
  rrect.Offset(separation, -separation);
  rrect.SetCornerRadii(gfx::RRectF::Corner::kUpperRight,
                       top_rect_upper_right_radius,
                       top_rect_upper_right_radius);
  canvas->sk_canvas()->drawRRect(SkRRect(rrect), flags);
}
