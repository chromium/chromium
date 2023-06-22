// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_

#import <Foundation/Foundation.h>

@class UNUserNotificationCenter;

namespace mac_notifications {

// This enum is used in UMA. Do not delete or re-order entries. New entries
// should only be added at the end.
enum class UNNotificationRequestPermissionResult {
  kRequestFailed = 0,
  kPermissionDenied = 1,
  kPermissionGranted = 2,
  kMaxValue = kPermissionGranted,
};

// This logs the result of asking for notification permissions on macOS. Called
// when we request notification permissions. This happens at startup for both
// banner and alert style notifications and at runtime when the mojo service
// starts up (e.g. when displaying a notification).
void LogUNNotificationRequestPermissionResult(
    UNNotificationRequestPermissionResult result);

// Requests and log the current notifications settings and permissions.
void LogUNNotificationSettings(UNUserNotificationCenter* center);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
