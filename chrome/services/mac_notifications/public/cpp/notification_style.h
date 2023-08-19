// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_STYLE_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_STYLE_H_

namespace mac_notifications {

enum class NotificationStyle { kBanner, kAlert, kAppShim };

// Returns the notification style from the current app bundle.
NotificationStyle NotificationStyleFromAppBundle();

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_STYLE_H_
