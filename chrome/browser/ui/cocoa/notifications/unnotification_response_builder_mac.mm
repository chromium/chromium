// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/unnotification_response_builder_mac.h"

#import <UserNotifications/UserNotifications.h>

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"

@implementation UNNotificationResponseBuilder

+ (NSDictionary*)buildDictionary:(UNNotificationResponse*)response
                       fromAlert:(BOOL)fromAlert {
  NSDictionary* userInfo =
      [[[[response notification] request] content] userInfo];

  NSString* origin =
      [userInfo objectForKey:notification_constants::kNotificationOrigin]
          ? [userInfo objectForKey:notification_constants::kNotificationOrigin]
          : @"";
  DCHECK([userInfo objectForKey:notification_constants::kNotificationId]);
  NSString* notificationId =
      [userInfo objectForKey:notification_constants::kNotificationId];

  DCHECK(
      [userInfo objectForKey:notification_constants::kNotificationProfileId]);
  NSString* profileId =
      [userInfo objectForKey:notification_constants::kNotificationProfileId];

  NSNumber* creatorPid =
      [userInfo objectForKey:notification_constants::kNotificationCreatorPid];

  DCHECK(
      [userInfo objectForKey:notification_constants::kNotificationIncognito]);
  NSNumber* incognito =
      [userInfo objectForKey:notification_constants::kNotificationIncognito];
  NSNumber* notificationType =
      [userInfo objectForKey:notification_constants::kNotificationType];

  int buttonIndex = notification_constants::kNotificationInvalidButtonIndex;

  NotificationOperation operation = NotificationOperation::NOTIFICATION_CLICK;

  if ([[response actionIdentifier]
          isEqual:UNNotificationDismissActionIdentifier]) {
    operation = NotificationOperation::NOTIFICATION_CLOSE;
  } else if ([[response actionIdentifier]
                 isEqual:UNNotificationDefaultActionIdentifier]) {
    operation = NotificationOperation::NOTIFICATION_CLICK;
  } else if ([[response actionIdentifier]
                 isEqualToString:notification_constants::
                                     kNotificationCloseButtonTag]) {
    operation = NotificationOperation::NOTIFICATION_CLOSE;
  } else if ([[response actionIdentifier]
                 isEqualToString:notification_constants::
                                     kNotificationSettingsButtonTag]) {
    operation = NotificationOperation::NOTIFICATION_SETTINGS;
  } else if ([[response actionIdentifier]
                 isEqualToString:notification_constants::
                                     kNotificationButtonOne]) {
    operation = NotificationOperation::NOTIFICATION_CLICK;
    buttonIndex = 0;
  } else if ([[response actionIdentifier]
                 isEqualToString:notification_constants::
                                     kNotificationButtonTwo]) {
    operation = NotificationOperation::NOTIFICATION_CLICK;
    buttonIndex = 1;
  } else {
    NOTREACHED();
  }

  return @{
    notification_constants::kNotificationOrigin : origin,
    notification_constants::kNotificationId : notificationId,
    notification_constants::kNotificationProfileId : profileId,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationCreatorPid : creatorPid ? creatorPid
                                                                 : @0,
    notification_constants::kNotificationType : notificationType,
    notification_constants::
    kNotificationOperation : @(static_cast<int>(operation)),
    notification_constants::kNotificationButtonIndex : @(buttonIndex),
    notification_constants::kNotificationIsAlert : @(fromAlert),
  };
}

@end
