// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/mac_notifications/mac_notification_service_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"

namespace mac_notifications {

NSDictionary* GetMacNotificationUserInfo(
    const mojom::NotificationPtr& notification) {
  const mojom::NotificationMetadataPtr& meta = notification->meta;
  NSString* notification_id = base::SysUTF8ToNSString(meta->id->id);
  NSString* profile_id = base::SysUTF8ToNSString(meta->id->profile->id);
  NSNumber* incognito = [NSNumber numberWithBool:meta->id->profile->incognito];

  NSString* origin_url = base::SysUTF8ToNSString(meta->origin_url.spec());
  NSNumber* type = @(meta->type);
  NSNumber* creator_pid = @(meta->creator_pid);
  NSNumber* settings_button =
      [NSNumber numberWithBool:notification->show_settings_button];

  return @{
    notification_constants::kNotificationId : notification_id,
    notification_constants::kNotificationProfileId : profile_id,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationOrigin : origin_url,
    notification_constants::kNotificationType : type,
    notification_constants::kNotificationCreatorPid : creator_pid,
    notification_constants::kNotificationHasSettingsButton : settings_button,
  };
}

mojom::NotificationMetadataPtr GetMacNotificationMetadata(
    NSDictionary* user_info) {
  NSString* notification_id_ns =
      [user_info objectForKey:notification_constants::kNotificationId];
  NSString* profile_id_ns =
      [user_info objectForKey:notification_constants::kNotificationProfileId];
  BOOL incognito = [[user_info
      objectForKey:notification_constants::kNotificationIncognito] boolValue];

  auto profile_id = mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profile_id_ns), incognito);
  auto notification_id = mojom::NotificationIdentifier::New(
      base::SysNSStringToUTF8(notification_id_ns), std::move(profile_id));

  int type = [[user_info objectForKey:notification_constants::kNotificationType]
      intValue];
  NSString* origin_url_ns =
      [user_info objectForKey:notification_constants::kNotificationOrigin];
  int creator_pid = [[user_info
      objectForKey:notification_constants::kNotificationCreatorPid] intValue];

  return mojom::NotificationMetadata::New(
      std::move(notification_id), type,
      GURL(base::SysNSStringToUTF8(origin_url_ns)), creator_pid);
}

}  // namespace mac_notifications
