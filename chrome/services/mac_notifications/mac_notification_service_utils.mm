// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/mac_notification_service_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"

namespace mac_notifications {

NSDictionary* GetMacNotificationUserInfo(
    const mojom::NotificationPtr& notification) {
  // TODO(knollr): Fill with actual values from |notification|.
  return @{
    notification_constants::kNotificationOrigin : @"origin",
    notification_constants::
    kNotificationId : base::SysUTF8ToNSString(notification->id->id),
    notification_constants::kNotificationProfileId :
        base::SysUTF8ToNSString(notification->id->profile->id),
    notification_constants::kNotificationIncognito :
        [NSNumber numberWithBool:notification->id->profile->incognito],
    notification_constants::kNotificationType : @0,
    notification_constants::kNotificationCreatorPid : @0,
  };
}

}  // namespace mac_notifications
