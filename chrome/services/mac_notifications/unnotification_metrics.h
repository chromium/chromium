// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_

#import <Foundation/Foundation.h>

@class UNUserNotificationCenter;

namespace mac_notifications {

// This file is used to record metrics specific for UNNotifications.

// This enum is used in UMA. Do not delete or re-order entries. New entries
// should only be added at the end.
enum class UNNotificationStyle {
  kNone = 0,
  kBanners = 1,
  kAlerts = 2,
  kMaxValue = kAlerts,
};

// This enum is used in UMA. Do not delete or re-order entries. New entries
// should only be added at the end.
enum class UNNotificationPermissionStatus {
  kNotRequestedYet = 0,
  kPermissionDenied = 1,
  kPermissionGranted = 2,
  kMaxValue = kPermissionGranted,
};

// Requests and log the current notifications settings and permissions.
API_AVAILABLE(macosx(10.14))
void LogUNNotificationSettings(UNUserNotificationCenter* center);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_UNNOTIFICATION_METRICS_H_
