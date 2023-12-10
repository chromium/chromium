// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/services/mac_notifications/mac_notification_service_utils.h"

#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"

namespace mac_notifications {

NSDictionary* GetMacNotificationUserInfo(
    const mojom::NotificationPtr& notification) {
  const mojom::NotificationMetadataPtr& meta = notification->meta;
  NSString* notification_id = base::SysUTF8ToNSString(meta->id->id);
  NSString* profile_id = base::SysUTF8ToNSString(meta->id->profile->id);
  NSNumber* incognito = [NSNumber numberWithBool:meta->id->profile->incognito];

  NSString* origin_url = base::SysUTF8ToNSString(meta->origin_url.spec());
  NSNumber* type = @(meta->type);
  NSString* user_data_dir = base::SysUTF8ToNSString(meta->user_data_dir);
  NSNumber* settings_button =
      [NSNumber numberWithBool:notification->show_settings_button];

  return @{
    kNotificationId : notification_id,
    kNotificationProfileId : profile_id,
    kNotificationIncognito : incognito,
    kNotificationOrigin : origin_url,
    kNotificationType : type,
    kNotificationUserDataDir : user_data_dir,
    kNotificationHasSettingsButton : settings_button,
  };
}

mojom::NotificationMetadataPtr GetMacNotificationMetadata(
    NSDictionary* user_info) {
  NSString* notification_id_ns = [user_info objectForKey:kNotificationId];
  NSString* profile_id_ns = [user_info objectForKey:kNotificationProfileId];
  BOOL incognito = [[user_info objectForKey:kNotificationIncognito] boolValue];

  auto profile_id = mojom::ProfileIdentifier::New(
      base::SysNSStringToUTF8(profile_id_ns), incognito);
  auto notification_id = mojom::NotificationIdentifier::New(
      base::SysNSStringToUTF8(notification_id_ns), std::move(profile_id));

  int type = [[user_info objectForKey:kNotificationType] intValue];
  NSString* origin_url_ns = [user_info objectForKey:kNotificationOrigin];
  NSString* user_data_dir_ns =
      [user_info objectForKey:kNotificationUserDataDir];

  return mojom::NotificationMetadata::New(
      std::move(notification_id), type,
      GURL(base::SysNSStringToUTF8(origin_url_ns)),
      base::SysNSStringToUTF8(user_data_dir_ns));
}

std::string DeriveMacNotificationId(
    const mojom::NotificationIdentifierPtr& identifier) {
  // i: incognito, r: regular profile
  return base::StrCat({identifier->profile->incognito ? "i" : "r", "|",
                       identifier->profile->id, "|", identifier->id});
}

NSString* const kNotificationButtonOne = @"buttonOne";
NSString* const kNotificationButtonTwo = @"buttonTwo";
NSString* const kNotificationCloseButtonTag = @"closeButton";
NSString* const kNotificationHasSettingsButton =
    @"notificationHasSettingsButton";
NSString* const kNotificationId = @"notificationId";
NSString* const kNotificationIncognito = @"notificationIncognito";
NSString* const kNotificationOrigin = @"notificationOrigin";
NSString* const kNotificationProfileId = @"notificationProfileId";
NSString* const kNotificationSettingsButtonTag = @"settingsButton";
NSString* const kNotificationType = @"notificationType";
NSString* const kNotificationUserDataDir = @"notificationUserDataDir";

}  // namespace mac_notifications
