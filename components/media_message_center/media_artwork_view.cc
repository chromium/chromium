// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_artwork_view.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace media_message_center {

namespace {

// Get target bounds of the image fitting into |canvas_size|.
// If |image_size| is greater than |expect_size| in any dimension,
// we shrink it down to fit, keep the size otherwise.
gfx::Rect GetTargetBound(const gfx::Size image_size,
                         const gfx::Size expect_size,
                         const gfx::Size canvas_size) {
  gfx::Size target_size = image_size;
  if (image_size.width() > expect_size.width() ||
      image_size.height() > expect_size.height()) {
    const float scale = std::min(
        expect_size.width() / static_cast<float>(image_size.width()),
        expect_size.height() / static_cast<float>(image_size.height()));
    target_size = gfx::ScaleToFlooredSize(image_size, scale);
  }

  int offset_x = (canvas_size.width() - target_size.width()) / 2;
  int offset_y = (canvas_size.height() - target_size.height()) / 2;
  return gfx::Rect(offset_x, offset_y, target_size.width(),
                   target_size.height());
}

}  // anonymous namespace

MediaArtworkView::MediaArtworkView(float corner_radius,
                                   const gfx::Size& artwork_size,
                                   const gfx::Size& favicon_size)
    : corner_radius_(corner_radius),
      artwork_size_(artwork_size),
      favicon_size_(favicon_size) {}

void MediaArtworkView::SetVignetteColor(const SkColor& vignette_color) {
  if (vignette_color_ == vignette_color)
    return;
  vignette_color_ = vignette_color;
  OnPropertyChanged(&vignette_color_, views::kPropertyEffectsPaint);
}

SkColor MediaArtworkView::GetVignetteColor() const {
  return vignette_color_;
}

void MediaArtworkView::SetBackgroundColor(const SkColor& background_color) {
  background_color_ = background_color;
}

void MediaArtworkView::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
}

void MediaArtworkView::SetFavicon(const gfx::ImageSkia& favicon) {
  favicon_ = favicon;
}

void MediaArtworkView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  {
    // Paint background.
    cc::PaintFlags paint_flags;
    paint_flags.setStyle(cc::PaintFlags::kFill_Style);
    paint_flags.setAntiAlias(true);
    paint_flags.setColor(background_color_);
    canvas->DrawRect(gfx::Rect(artwork_size_), paint_flags);
  }

  // Draw image if we have artwork; fallback to favicon if we don't.
  if (!image_.isNull()) {
    gfx::Rect target =
        GetTargetBound(image_.size(), artwork_size_, artwork_size_);
    canvas->DrawImageInt(image_, 0, 0, image_.width(), image_.height(),
                         target.x(), target.y(), target.width(),
                         target.height(), false);
  } else if (!favicon_.isNull()) {
    gfx::Rect target =
        GetTargetBound(favicon_.size(), favicon_size_, artwork_size_);
    canvas->DrawImageInt(favicon_, 0, 0, favicon_.width(), favicon_.height(),
                         target.x(), target.y(), target.width(),
                         target.height(), false);
  }

  {
    auto path = SkPath().addRoundRect(RectToSkRect(GetLocalBounds()),
                                      corner_radius_, corner_radius_);
    path.toggleInverseFillType();
    cc::PaintFlags paint_flags;
    paint_flags.setStyle(cc::PaintFlags::kFill_Style);
    paint_flags.setAntiAlias(true);
    paint_flags.setColor(vignette_color_);
    canvas->DrawPath(path, paint_flags);
  }
}

BEGIN_METADATA(MediaArtworkView, views::View)
ADD_PROPERTY_METADATA(SkColor, VignetteColor)
END_METADATA

}  // namespace media_message_center
