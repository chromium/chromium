// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_

#import <Foundation/Foundation.h>

#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

@class UNUserNotificationCenter;
@class UNNotificationSettings;

namespace mac_notifications {

// This logs the result of asking for notification permissions on macOS. Called
// when we request notification permissions. This happens at startup for both
// banner and alert style notifications and at runtime when the mojo service
// starts up (e.g. when displaying a notification).
void LogUNNotificationRequestPermissionResult(
    mojom::RequestPermissionResult result);

// Logs the current notifications settings and permissions.
void LogUNNotificationSettings(UNNotificationSettings* settings);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
