// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/public/cpp/notification_style.h"

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"

namespace mac_notifications {

NotificationStyle NotificationStyleFromAppBundle() {
  NSDictionary* infoDictionary = [base::apple::MainBundle() infoDictionary];
  NSString* appModeShortcut = infoDictionary[@"CrAppModeShortcutID"];
  if (appModeShortcut != nil) {
    return NotificationStyle::kAppShim;
  }
  NSString* alertStyle = infoDictionary[@"NSUserNotificationAlertStyle"];
  return [alertStyle isEqualToString:@"alert"] ? NotificationStyle::kAlert
                                               : NotificationStyle::kBanner;
}

}  // namespace mac_notifications
