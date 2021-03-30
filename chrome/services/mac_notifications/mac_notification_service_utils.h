// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UTILS_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UTILS_H_

#import <Foundation/Foundation.h>

#include "chrome/services/mac_notifications/public/mojom/mac_notifications.mojom.h"

namespace mac_notifications {

NSDictionary* GetMacNotificationUserInfo(
    const mojom::NotificationPtr& notification);

mojom::NotificationMetadataPtr GetMacNotificationMetadata(
    NSDictionary* user_info);

}  // namespace mac_notifications

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_MAC_NOTIFICATION_SERVICE_UTILS_H_
