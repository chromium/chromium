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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

API_AVAILABLE(macosx(10.14))
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

API_AVAILABLE(macosx(10.14))
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
    UNNotificationRequestPermissionResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Notifications.Permissions.UNNotification.",
                    MacNotificationStyleSuffix(IsAppBundleAlertStyle()),
                    ".PermissionRequest"}),
      result);
}

void LogUNNotificationSettings(UNUserNotificationCenter* center) {
  [center getNotificationSettingsWithCompletionHandler:^(
              UNNotificationSettings* _Nonnull settings) {
    std::string prefix =
        base::StrCat({"Notifications.Permissions.UNNotification.",
                      MacNotificationStyleSuffix(IsAppBundleAlertStyle())});

    base::UmaHistogramEnumeration(
        base::StrCat({prefix, ".Style"}),
        ConvertNotificationStyle(settings.alertStyle));
    base::UmaHistogramEnumeration(
        base::StrCat({prefix, ".PermissionStatus"}),
        ConvertAuthorizationStatus(settings.authorizationStatus));
  }];
}

}  // namespace mac_notifications
