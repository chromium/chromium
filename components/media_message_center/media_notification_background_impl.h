// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_IMPL_H_

#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "components/media_message_center/media_notification_background.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace media_message_center {

// MediaNotificationBackground draws a custom background for the media
// notification showing the artwork clipped to a rounded rectangle faded into a
// background color.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationBackgroundImpl
    : public MediaNotificationBackground {
 public:
  MediaNotificationBackgroundImpl(int top_radius,
                                  int bottom_radius,
                                  double artwork_max_width_pct);
  ~MediaNotificationBackgroundImpl() override;

  // views::Background
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  void UpdateArtwork(const gfx::ImageSkia& image) override;
  bool UpdateCornerRadius(int top_radius, int bottom_radius) override;
  bool UpdateArtworkMaxWidthPct(double max_width_pct) override;
  void UpdateFavicon(const gfx::ImageSkia& icon) override;
  void UpdateDeviceSelectorAvailability(bool availability) override;

  SkColor GetBackgroundColor(const views::View& owner) const override;
  SkColor GetForegroundColor(const views::View& owner) const override;

 private:
  friend class MediaNotificationBackgroundImplTest;
  friend class MediaNotificationViewImplTest;
  friend class MediaNotificationViewModernImplTest;
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationBackgroundImplRTLTest,
                           BoundsSanityCheck);

  // Shade factor used on favicon dominant color before set as background color.
  static constexpr double kBackgroundFaviconColorShadeFactor = 0.35;

  int GetArtworkWidth(const gfx::Size& view_size) const;
  int GetArtworkVisibleWidth(const gfx::Size& view_size) const;
  gfx::Rect GetArtworkBounds(const views::View& owner) const;
  gfx::Rect GetFilledBackgroundBounds(const views::View& owner) const;
  gfx::Rect GetGradientBounds(const views::View& owner) const;
  gfx::Rect GetBottomGradientBounds(const views::View& owner) const;
  SkPoint GetGradientStartPoint(const gfx::Rect& draw_bounds) const;
  SkPoint GetGradientEndPoint(const gfx::Rect& draw_bounds) const;
  SkColor GetDefaultBackgroundColor(const views::View& owner) const;
  void UpdateColorsInternal();

  int top_radius_;
  int bottom_radius_;

  gfx::ImageSkia favicon_;
  gfx::ImageSkia artwork_;
  double artwork_max_width_pct_;
  bool audio_device_selector_availability_ = false;

  base::Optional<SkColor> background_color_;
  base::Optional<SkColor> foreground_color_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationBackgroundImpl);
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_IMPL_H_
