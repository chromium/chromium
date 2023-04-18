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
  // Color ID for texts and icons.
  ui::ColorId primary_foreground_color_id = 0;

  // Color ID for secondary texts.
  ui::ColorId secondary_foreground_color_id = 0;

  // Color ID for time scrubber foreground.
  ui::ColorId primary_container_color_id = 0;

  // Color ID for time scrubber background and play/pause button background.
  ui::ColorId secondary_container_color_id = 0;

  // Color ID for media view background.
  ui::ColorId background_color_id = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_NOTIFICATION_THEME_H_
