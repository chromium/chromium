// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_H_

#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"

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
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationBackground
    : public views::Background {
 public:
  MediaNotificationBackground(int top_radius,
                              int bottom_radius,
                              double artwork_max_width_pct);
  ~MediaNotificationBackground() override;

  // views::Background
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

  void UpdateArtwork(const gfx::ImageSkia& image);
  bool UpdateCornerRadius(int top_radius, int bottom_radius);
  bool UpdateArtworkMaxWidthPct(double max_width_pct);

  SkColor GetBackgroundColor(const views::View& owner) const;
  SkColor GetForegroundColor(const views::View& owner) const;

 private:
  friend class MediaNotificationBackgroundTest;
  friend class MediaNotificationViewTest;
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationBackgroundRTLTest,
                           BoundsSanityCheck);

  int GetArtworkWidth(const gfx::Size& view_size) const;
  int GetArtworkVisibleWidth(const gfx::Size& view_size) const;
  gfx::Rect GetArtworkBounds(const views::View& owner) const;
  gfx::Rect GetFilledBackgroundBounds(const views::View& owner) const;
  gfx::Rect GetGradientBounds(const views::View& owner) const;
  SkPoint GetGradientStartPoint(const gfx::Rect& draw_bounds) const;
  SkPoint GetGradientEndPoint(const gfx::Rect& draw_bounds) const;
  SkColor GetDefaultBackgroundColor(const views::View& owner) const;

  int top_radius_;
  int bottom_radius_;

  gfx::ImageSkia artwork_;
  double artwork_max_width_pct_;

  base::Optional<SkColor> background_color_;
  base::Optional<SkColor> foreground_color_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationBackground);
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_BACKGROUND_H_
