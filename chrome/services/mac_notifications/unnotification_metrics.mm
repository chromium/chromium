// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/unnotification_metrics.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <string>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/services/mac_notifications/public/cpp/mac_notification_metrics.h"

namespace mac_notifications {

namespace {

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

UNNotificationStyle ConvertNotificationStyle(UNAlertStyle alert_style) {
  switch (alert_style) {
    case UNAlertStyleBanner:
      return UNNotificationStyle::kBanners;
    case UNAlertStyleAlert:
      return UNNotificationStyle::kAlerts;
    default:
      return UNNotificationStyle::kNone;
  }
}

UNNotificationPermissionStatus ConvertAuthorizationStatus(
    UNAuthorizationStatus authorization_status) {
  switch (authorization_status) {
    case UNAuthorizationStatusDenied:
      return UNNotificationPermissionStatus::kPermissionDenied;
    case UNAuthorizationStatusAuthorized:
      return UNNotificationPermissionStatus::kPermissionGranted;
    default:
      return UNNotificationPermissionStatus::kNotRequestedYet;
  }
}

}  // namespace

void LogUNNotificationRequestPermissionResult(
    mojom::RequestPermissionResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Notifications.Permissions.UNNotification.",
           MacNotificationStyleSuffix(NotificationStyleFromAppBundle()),
           ".PermissionRequest"}),
      result);
}

void LogUNNotificationSettings(UNNotificationSettings* settings) {
  std::string prefix = base::StrCat(
      {"Notifications.Permissions.UNNotification.",
       MacNotificationStyleSuffix(NotificationStyleFromAppBundle())});

  base::UmaHistogramEnumeration(base::StrCat({prefix, ".Style"}),
                                ConvertNotificationStyle(settings.alertStyle));
  base::UmaHistogramEnumeration(
      base::StrCat({prefix, ".PermissionStatus"}),
      ConvertAuthorizationStatus(settings.authorizationStatus));
}

}  // namespace mac_notifications
