// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_

#include <string>

namespace mac_notifications {
enum class ProcessType { kInProcess, kAlertProcess, kAppShimProcess };

// Returns the process type from the current app bundle.
ProcessType ProcessTypeFromAppBundle();

// Returns a suffix to be used in UMA histogram names. Needs to be kept in sync
// with variants of MacOSNotificationStyle in .../notifications/histograms.xml.
std::string MacNotificationStyleSuffix(ProcessType process_type);

// Called when a user performed an action on a notification on macOS.
// |process_type| determines if the notification was an alert or a banner.
// |is_valid| determines if the action data was valid and we passed it along.
void LogMacNotificationActionReceived(ProcessType process_type, bool is_valid);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_
