// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_

#include <string>

namespace mac_notifications {

// Returns if the current app bundle has the alert style set to "alert".
bool IsAppBundleAlertStyle();

// Returns a suffix to be used in UMA histogram names. Needs to be kept in sync
// with variants of MacOSNotificationStyle in .../notifications/histograms.xml.
std::string MacNotificationStyleSuffix(bool is_alert);

// Called when we delivered a new notification to the macOS notification center.
// |is_alert| determines if the notification was an alert or a banner.
// |success| determines if there was an error while delivering the notification.
void LogMacNotificationDelivered(bool is_alert, bool success);

// Called when a user performed an action on a notification on macOS.
// |is_alert| determines if the notification was an alert or a banner.
// |is_valid| determines if the action data was valid and we passed it along.
void LogMacNotificationActionReceived(bool is_alert, bool is_valid);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_
