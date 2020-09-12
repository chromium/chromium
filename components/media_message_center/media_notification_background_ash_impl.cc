// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_background_ash_impl.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/view.h"

namespace media_message_center {

namespace {

constexpr SkColor kBackgroundColor = SK_ColorTRANSPARENT;
constexpr SkColor kForegroundColor = SK_ColorWHITE;

constexpr gfx::Size kArtworkSize(80, 80);
constexpr int kArtworkBottomMargin = 16;
constexpr int kArtworkRightMargin = 16;
constexpr int kArtworkCornerRadius = 4;

gfx::Size ScaleToFitSize(const gfx::Size& image_size) {
  if ((image_size.width() > kArtworkSize.width() ||
       image_size.height() > kArtworkSize.height()) ||
      (image_size.width() < kArtworkSize.width() &&
       image_size.height() < kArtworkSize.height())) {
    const float scale = std::min(
        kArtworkSize.width() / static_cast<float>(image_size.width()),
        kArtworkSize.height() / static_cast<float>(image_size.height()));
    return gfx::ScaleToFlooredSize(image_size, scale);
  }

  return image_size;
}

}  // namespace

gfx::Rect MediaNotificationBackgroundAshImpl::GetArtworkBounds(
    const gfx::Rect& view_bounds) const {
  gfx::Size target_size = ScaleToFitSize(artwork_.size());

  int vertical_offset = (kArtworkSize.height() - target_size.height()) / 2;
  int horizontal_offset = (kArtworkSize.width() - target_size.width()) / 2;

  return gfx::Rect(view_bounds.right() - kArtworkRightMargin -
                       kArtworkSize.width() + horizontal_offset,
                   view_bounds.bottom() - kArtworkBottomMargin -
                       kArtworkSize.height() + vertical_offset,
                   target_size.width(), target_size.height());
}

void MediaNotificationBackgroundAshImpl::Paint(gfx::Canvas* canvas,
                                               views::View* view) const {
  gfx::Rect source_bounds(0, 0, artwork_.width(), artwork_.height());
  gfx::Rect target_bounds = GetArtworkBounds(view->GetContentsBounds());

  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(target_bounds), kArtworkCornerRadius,
                    kArtworkCornerRadius);

  canvas->ClipPath(path, true);

  canvas->DrawImageInt(
      artwork_, source_bounds.x(), source_bounds.y(), source_bounds.width(),
      source_bounds.height(), target_bounds.x(), target_bounds.y(),
      target_bounds.width(), target_bounds.height(), false /* filter */);
}

void MediaNotificationBackgroundAshImpl::UpdateArtwork(
    const gfx::ImageSkia& image) {
  if (artwork_.BackedBySameObjectAs(image))
    return;

  artwork_ = image;
}

bool MediaNotificationBackgroundAshImpl::UpdateCornerRadius(int top_radius,
                                                            int bottom_radius) {
  return false;
}

bool MediaNotificationBackgroundAshImpl::UpdateArtworkMaxWidthPct(
    double max_width_pct) {
  return false;
}

SkColor MediaNotificationBackgroundAshImpl::GetBackgroundColor(
    const views::View& owner) const {
  return kBackgroundColor;
}

SkColor MediaNotificationBackgroundAshImpl::GetForegroundColor(
    const views::View& owner) const {
  return kForegroundColor;
}

}  // namespace media_message_center
