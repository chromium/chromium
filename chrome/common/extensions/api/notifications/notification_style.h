// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATION_STYLE_H_
#define CHROME_COMMON_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATION_STYLE_H_

#include "ui/gfx/geometry/size.h"

// This structure describes the size in DIPs of each type of image rendered
// by the notification center within a notification.
struct NotificationBitmapSizes {
  NotificationBitmapSizes();
  NotificationBitmapSizes(const NotificationBitmapSizes& other);
  ~NotificationBitmapSizes();

  gfx::Size image_size;
  gfx::Size icon_size;
  gfx::Size button_icon_size;
  gfx::Size app_icon_mask_size;
};

NotificationBitmapSizes GetNotificationBitmapSizes();

#endif  // CHROME_COMMON_EXTENSIONS_API_NOTIFICATIONS_NOTIFICATION_STYLE_H_
