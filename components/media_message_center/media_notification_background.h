// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_H_

#include "ui/views/background.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace media_message_center {

// Interface for media notification view's background.
class MediaNotificationBackground : public views::Background {
 public:
  // views::Background.
  void Paint(gfx::Canvas* canvas, views::View* view) const override = 0;

  virtual void UpdateArtwork(const gfx::ImageSkia& image) = 0;

  // Return true if corner radius is successfully updated.
  virtual bool UpdateCornerRadius(int top_radius, int bottom_radius) = 0;

  // Return true if artwork max with percentage is successfully updated.
  virtual bool UpdateArtworkMaxWidthPct(double max_width_pct) = 0;

  virtual void UpdateFavicon(const gfx::ImageSkia& icon) = 0;
  virtual void UpdateDeviceSelectorVisibility(bool visible) = 0;

  virtual SkColor GetBackgroundColor(const views::View& owner) const = 0;
  virtual SkColor GetForegroundColor(const views::View& owner) const = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_H_
