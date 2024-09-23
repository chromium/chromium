// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_NOTIFICATION_THEME_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_NOTIFICATION_THEME_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"

namespace media_message_center {

struct COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) NotificationTheme {
  SkColor primary_text_color = 0;
  SkColor secondary_text_color = 0;
  SkColor enabled_icon_color = 0;
  SkColor disabled_icon_color = 0;
  SkColor separator_color = 0;
  SkColor background_color = 0;
};

// Defines the color IDs for the media view.
struct COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaColorTheme {
  MediaColorTheme();
  MediaColorTheme(const MediaColorTheme& other);
  MediaColorTheme& operator=(const MediaColorTheme&);
  ~MediaColorTheme();

  // Color ID for primary texts and most media action button icons.
  ui::ColorId primary_foreground_color_id = 0;

  // Color ID for secondary texts.
  ui::ColorId secondary_foreground_color_id = 0;

  // Color ID for the play button icon.
  ui::ColorId play_button_foreground_color_id = 0;

  // Color ID for the play button container.
  ui::ColorId play_button_container_color_id = 0;

  // Color ID for the pause button icon.
  ui::ColorId pause_button_foreground_color_id = 0;

  // Color ID for the pause button container.
  ui::ColorId pause_button_container_color_id = 0;

  // Color ID for media squiggly progress foreground when the media is playing.
  ui::ColorId playing_progress_foreground_color_id = 0;

  // Color ID for media squiggly progress background when the media is playing.
  ui::ColorId playing_progress_background_color_id = 0;

  // Color ID for media squiggly progress foreground when the media is paused.
  ui::ColorId paused_progress_foreground_color_id = 0;

  // Color ID for media squiggly progress background when the media is paused.
  ui::ColorId paused_progress_background_color_id = 0;

  // Color ID for media view background.
  ui::ColorId background_color_id = 0;

  // Color ID for device selector view separator line.
  ui::ColorId separator_color_id = 0;

  // Color ID for device selector view borders.
  ui::ColorId device_selector_border_color_id = 0;

  // Color ID for device selector view foreground.
  ui::ColorId device_selector_foreground_color_id = 0;

  // Color ID for device selector view background.
  ui::ColorId device_selector_background_color_id = 0;

  // Color ID for the stop casting button text.
  ui::ColorId error_foreground_color_id = 0;

  // Color ID for the stop casting button container.
  ui::ColorId error_container_color_id = 0;

  // Color ID for focus rings on UI elements.
  ui::ColorId focus_ring_color_id = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_NOTIFICATION_THEME_H_
