// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/unnotification_metrics.h"

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#import <string>

#include "base/mac/bundle_locations.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace mac_notifications {

namespace {

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

// Returns the alert style of the current app bundle. Must match the "Style"
// token used in "Notifications.Permissions.UNNotification.*" metrics.
std::string AppBundleNotificationStyle() {
  NSDictionary* infoDictionary = [base::mac::MainBundle() infoDictionary];
  NSString* alertStyle = infoDictionary[@"NSUserNotificationAlertStyle"];
  return [alertStyle isEqualToString:@"alert"] ? "Alerts" : "Banners";
}

}  // namespace

void LogUNNotificationSettings(UNUserNotificationCenter* center) {
  [center getNotificationSettingsWithCompletionHandler:^(
              UNNotificationSettings* _Nonnull settings) {
    std::string prefix =
        base::StrCat({"Notifications.Permissions.UNNotification.",
                      AppBundleNotificationStyle()});

    base::UmaHistogramEnumeration(
        base::StrCat({prefix, ".Style"}),
        ConvertNotificationStyle(settings.alertStyle));
    base::UmaHistogramEnumeration(
        base::StrCat({prefix, ".PermissionStatus"}),
        ConvertAuthorizationStatus(settings.authorizationStatus));
  }];
}

}  // namespace mac_notifications
