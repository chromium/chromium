// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/notifications/notification_style.h"

#include "ui/message_center/public/cpp/message_center_constants.h"

NotificationBitmapSizes::NotificationBitmapSizes() {
}
NotificationBitmapSizes::NotificationBitmapSizes(
    const NotificationBitmapSizes& other) = default;
NotificationBitmapSizes::~NotificationBitmapSizes() {
}

NotificationBitmapSizes GetNotificationBitmapSizes() {
  NotificationBitmapSizes sizes;
  sizes.image_size =
      gfx::Size(message_center::kNotificationPreferredImageWidth,
                message_center::kNotificationPreferredImageHeight);
  sizes.icon_size = gfx::Size(message_center::kNotificationIconSize,
                              message_center::kNotificationIconSize);
  sizes.button_icon_size =
      gfx::Size(message_center::kNotificationButtonIconSize,
                message_center::kNotificationButtonIconSize);

  sizes.app_icon_mask_size = gfx::Size(message_center::kSmallImageSize,
                                       message_center::kSmallImageSize);
  return sizes;
}
