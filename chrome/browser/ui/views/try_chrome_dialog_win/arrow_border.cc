// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/try_chrome_dialog_win/arrow_border.h"

#include <algorithm>
#include <utility>

#include "cc/paint/paint_canvas.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

ArrowBorder::ArrowBorder(int thickness,
                         ui::ColorId color,
                         ui::ColorId background_color,
                         const gfx::VectorIcon& arrow_icon,
                         const Properties* properties)
    : insets_(gfx::Insets(thickness) + properties->insets),
      color_(color),
      arrow_border_insets_(properties->arrow_border_insets),
      arrow_rotation_(properties->arrow_rotation),
      arrow_(ui::ImageModel::FromVectorIcon(arrow_icon, background_color)) {
}

void ArrowBorder::Paint(const views::View& view, gfx::Canvas* canvas) {
  // Undo DSF to be sure to draw an integral number of pixels for the border.
  // This ensures sharp lines even for fractional scale factors.
  gfx::ScopedCanvas scoped(canvas);
  float dsf = canvas->UndoDeviceScaleFactor();

  // Compute the bounds of the contents within the border (this scaling
  // operation floors the inset values).
  gfx::Rect content_bounds = gfx::Rect(
      view.GetWidget()->GetNativeView()->GetHost()->GetBoundsInPixels().size());
  content_bounds.Inset(
      gfx::ToFlooredInsets(gfx::ConvertInsetsToPixels(insets_, dsf)));

  // Clip out the contents, leaving behind the border.
  canvas->sk_canvas()->clipRect(gfx::RectToSkRect(content_bounds),
                                SkClipOp::kDifference, true);

  // Paint the rectangular border, less the region occupied by the arrow.
  const auto* const color_provider = view.GetColorProvider();
  {
    gfx::ScopedCanvas content_clip(canvas);

    // Clip out the arrow, less its insets.
    gfx::Rect arrow_bounds(arrow_bounds_);
    arrow_bounds.Inset(gfx::ToFlooredInsets(
        gfx::ConvertInsetsToPixels(arrow_border_insets_, dsf)));
    canvas->sk_canvas()->clipRect(gfx::RectToSkRect(arrow_bounds),
                                  SkClipOp::kDifference, true);
    canvas->DrawColor(color_provider->GetColor(color_));
  }

  // Paint the arrow.

  // The arrow is a square icon and must be painted as such.
  gfx::Rect arrow_bounds(arrow_bounds_);
  int size = std::max(arrow_bounds.width(), arrow_bounds.height());
  gfx::Size arrow_size(size, size);

  // The arrow image is square with only the top 14 points (kArrowHeight)
  // containing the relevant drawing. When drawn "toward" the origin (i.e., for
  // a top or left taskbar), the arrow must be offset this extra amount.
  if (arrow_bounds.origin().x() < content_bounds.origin().x())
    arrow_bounds.Offset(arrow_bounds.width() - arrow_size.width(), 0);
  else if (arrow_bounds.origin().y() < content_bounds.origin().y())
    arrow_bounds.Offset(0, arrow_bounds.height() - arrow_size.height());

  gfx::ImageSkia arrow = arrow_.Rasterize(color_provider);
  switch (arrow_rotation_) {
    case ArrowRotation::kNone:
      break;
    case ArrowRotation::k90Degrees:
      arrow = gfx::ImageSkiaOperations::CreateRotatedImage(
          arrow, SkBitmapOperations::ROTATION_90_CW);
      break;
    case ArrowRotation::k180Degrees:
      arrow = gfx::ImageSkiaOperations::CreateRotatedImage(
          arrow, SkBitmapOperations::ROTATION_180_CW);
      break;
    case ArrowRotation::k270Degrees:
      arrow = gfx::ImageSkiaOperations::CreateRotatedImage(
          arrow, SkBitmapOperations::ROTATION_270_CW);
      break;
  }
  canvas->DrawImageIntInPixel(
      arrow.GetRepresentation(dsf), arrow_bounds.x(),
      arrow_bounds.y(), arrow_size.width(), arrow_size.height(), false,
      cc::PaintFlags());
}

gfx::Insets ArrowBorder::GetInsets() const {
  return insets_;
}

gfx::Size ArrowBorder::GetMinimumSize() const {
  return {insets_.width(), insets_.height()};
}
