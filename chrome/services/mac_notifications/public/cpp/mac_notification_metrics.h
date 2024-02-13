// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_

#include <string>

#include "chrome/services/mac_notifications/public/cpp/notification_style.h"

namespace mac_notifications {

// Returns a suffix to be used in UMA histogram names. Needs to be kept in sync
// with variants of MacOSNotificationStyle in .../notifications/histograms.xml.
std::string MacNotificationStyleSuffix(NotificationStyle notification_style);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_MAC_NOTIFICATION_METRICS_H_
