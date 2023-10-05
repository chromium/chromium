// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_ASH_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_ASH_IMPL_H_

#include "components/media_message_center/media_notification_background.h"

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

class SkPath;

namespace gfx {
class Rect;
}

namespace media_message_center {

// MediaNotificationBackground for CrOS media notifications.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationBackgroundAshImpl
    : public MediaNotificationBackground {
 public:
  explicit MediaNotificationBackgroundAshImpl(bool paint_artwork = true);
  MediaNotificationBackgroundAshImpl(
      const MediaNotificationBackgroundAshImpl&) = delete;
  MediaNotificationBackgroundAshImpl& operator=(
      const MediaNotificationBackgroundAshImpl&) = delete;
  ~MediaNotificationBackgroundAshImpl() override = default;

  // MediaNotificationBackground implementations.
  void Paint(gfx::Canvas* canvas, views::View* view) const override;
  void UpdateArtwork(const gfx::ImageSkia& image) override;
  bool UpdateCornerRadius(int top_radius, int bottom_radius) override;
  bool UpdateArtworkMaxWidthPct(double max_width_pct) override;
  void UpdateFavicon(const gfx::ImageSkia& icon) override {}
  void UpdateDeviceSelectorVisibility(bool visible) override {}
  SkColor GetBackgroundColor(const views::View& owner) const override;
  SkColor GetForegroundColor(const views::View& owner) const override;

 private:
  friend class MediaNotificationBackgroundAshImplTest;

  gfx::Rect GetArtworkBounds(const gfx::Rect& view_bounds) const;

  SkPath GetArtworkClipPath(const gfx::Rect& view_bounds) const;

  gfx::ImageSkia artwork_;

  // True if this should paint the artwork as part of the background.
  bool paint_artwork_;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_ASH_IMPL_H_
