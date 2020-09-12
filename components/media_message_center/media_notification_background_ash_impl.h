// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_ASH_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_ASH_IMPL_H_

#include "components/media_message_center/media_notification_background.h"

#include "base/component_export.h"
#include "ui/gfx/image/image_skia.h"

namespace media_message_center {

// MediaNotificationBackground for CrOS media notifications.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationBackgroundAshImpl
    : public MediaNotificationBackground {
 public:
  MediaNotificationBackgroundAshImpl() = default;
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
  void UpdateDeviceSelectorAvailability(bool availability) override {}
  SkColor GetBackgroundColor(const views::View& owner) const override;
  SkColor GetForegroundColor(const views::View& owner) const override;

 private:
  friend class MediaNotificationBackgroundAshImplTest;

  gfx::Rect GetArtworkBounds(const gfx::Rect& view_bounds) const;

  gfx::ImageSkia artwork_;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_ASH_IMPL_H_
