// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_background_ash_impl.h"

#include "base/i18n/rtl.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
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
  if ((image_size.width() > kArtworkSize.width() &&
       image_size.height() > kArtworkSize.height()) ||
      (image_size.width() < kArtworkSize.width() ||
       image_size.height() < kArtworkSize.height())) {
    const float scale = std::max(
        kArtworkSize.width() / static_cast<float>(image_size.width()),
        kArtworkSize.height() / static_cast<float>(image_size.height()));
    return gfx::ScaleToFlooredSize(image_size, scale);
  }

  return image_size;
}

}  // namespace

MediaNotificationBackgroundAshImpl::MediaNotificationBackgroundAshImpl(
    bool paint_artwork)
    : paint_artwork_(paint_artwork) {}

gfx::Rect MediaNotificationBackgroundAshImpl::GetArtworkBounds(
    const gfx::Rect& view_bounds) const {
  gfx::Size target_size = ScaleToFitSize(artwork_.size());

  int vertical_offset = (target_size.height() - kArtworkSize.height()) / 2;
  int horizontal_offset = (target_size.width() - kArtworkSize.width()) / 2;

  int bounds_x = base::i18n::IsRTL()
                     ? view_bounds.x() + kArtworkRightMargin - horizontal_offset
                     : view_bounds.right() - kArtworkRightMargin -
                           kArtworkSize.width() - horizontal_offset;

  return gfx::Rect(bounds_x,
                   view_bounds.bottom() - kArtworkBottomMargin -
                       kArtworkSize.height() - vertical_offset,
                   target_size.width(), target_size.height());
}

SkPath MediaNotificationBackgroundAshImpl::GetArtworkClipPath(
    const gfx::Rect& view_bounds) const {
  int x = base::i18n::IsRTL() ? view_bounds.x() + kArtworkRightMargin
                              : view_bounds.right() - kArtworkRightMargin -
                                    kArtworkSize.width();
  int y = view_bounds.bottom() - kArtworkBottomMargin - kArtworkSize.height();

  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(x, y, kArtworkSize.width(),
                                                kArtworkSize.height())),
                    kArtworkCornerRadius, kArtworkCornerRadius);
  return path;
}

void MediaNotificationBackgroundAshImpl::Paint(gfx::Canvas* canvas,
                                               views::View* view) const {
  if (!paint_artwork_)
    return;

  gfx::Rect source_bounds(0, 0, artwork_.width(), artwork_.height());
  gfx::Rect target_bounds = GetArtworkBounds(view->GetContentsBounds());

  canvas->ClipPath(GetArtworkClipPath(view->GetContentsBounds()), true);

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
