// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/mac_notification_metrics.h"

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace mac_notifications {

std::string MacNotificationStyleSuffix(NotificationStyle notification_style) {
  switch (notification_style) {
    case NotificationStyle::kBanner:
      return "Banner";
    case NotificationStyle::kAlert:
      return "Alert";
    case NotificationStyle::kAppShim:
      return "AppShim";
  }
  NOTREACHED_IN_MIGRATION();
}

}  // namespace mac_notifications
