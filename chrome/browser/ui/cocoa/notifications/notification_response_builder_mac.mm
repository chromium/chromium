// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"

#include "base/check.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_operation.h"

@implementation NotificationResponseBuilder

+ (NSDictionary*)buildDictionary:(NSUserNotification*)notification
                       dismissed:(BOOL)dismissed {
  NSString* origin =
      [[notification userInfo]
          objectForKey:notification_constants::kNotificationOrigin]
          ? [[notification userInfo]
                objectForKey:notification_constants::kNotificationOrigin]
          : @"";
  DCHECK([[notification userInfo]
      objectForKey:notification_constants::kNotificationId]);
  NSString* notificationId = [[notification userInfo]
      objectForKey:notification_constants::kNotificationId];

  DCHECK([[notification userInfo]
      objectForKey:notification_constants::kNotificationProfileId]);
  NSString* profileId = [[notification userInfo]
      objectForKey:notification_constants::kNotificationProfileId];

  NSNumber* creatorPid = [[notification userInfo]
      objectForKey:notification_constants::kNotificationCreatorPid];

  DCHECK([[notification userInfo]
      objectForKey:notification_constants::kNotificationIncognito]);
  NSNumber* incognito = [[notification userInfo]
      objectForKey:notification_constants::kNotificationIncognito];
  NSNumber* notificationType = [[notification userInfo]
      objectForKey:notification_constants::kNotificationType];
  NSNumber* hasSettingsButton = [[notification userInfo]
      objectForKey:notification_constants::kNotificationHasSettingsButton];

  // Closed notifications are not activated.
  NSUserNotificationActivationType activationType =
      dismissed ? NSUserNotificationActivationTypeNone
                : notification.activationType;
  NotificationOperation operation =
      activationType == NSUserNotificationActivationTypeNone
          ? NotificationOperation::NOTIFICATION_CLOSE
          : NotificationOperation::NOTIFICATION_CLICK;
  int buttonIndex = notification_constants::kNotificationInvalidButtonIndex;

  // Determine whether the user clicked on a button, and if they did, whether it
  // was a developer-provided button or the  Settings button.
  if (activationType == NSUserNotificationActivationTypeActionButtonClicked) {
    NSArray* alternateButtons = @[];
    if ([notification
            respondsToSelector:@selector(_alternateActionButtonTitles)]) {
      alternateButtons =
          [notification valueForKey:@"_alternateActionButtonTitles"];
    }

    BOOL settingsButtonRequired = [hasSettingsButton boolValue];
    BOOL multipleButtons = (alternateButtons.count > 0);

    // No developer actions, just the settings button.
    if (!multipleButtons) {
      DCHECK(settingsButtonRequired);
      operation = NotificationOperation::NOTIFICATION_SETTINGS;
      buttonIndex = notification_constants::kNotificationInvalidButtonIndex;
    } else {
      // 0 based array containing.
      // Button 1
      // Button 2 (optional)
      // Settings (if required)
      NSNumber* actionIndex =
          [notification valueForKey:@"_alternateActionIndex"];
      operation = settingsButtonRequired && (actionIndex.unsignedLongValue ==
                                             alternateButtons.count - 1)
                      ? NotificationOperation::NOTIFICATION_SETTINGS
                      : NotificationOperation::NOTIFICATION_CLICK;
      buttonIndex =
          settingsButtonRequired &&
                  (actionIndex.unsignedLongValue == alternateButtons.count - 1)
              ? notification_constants::kNotificationInvalidButtonIndex
              : actionIndex.intValue;
    }
  }

  return @{
    notification_constants::kNotificationOrigin : origin,
    notification_constants::kNotificationId : notificationId,
    notification_constants::kNotificationProfileId : profileId,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationCreatorPid : creatorPid ? creatorPid
                                                                 : @0,
    notification_constants::kNotificationType : notificationType,
    notification_constants::kNotificationOperation :
        [NSNumber numberWithInt:static_cast<int>(operation)],
    notification_constants::
    kNotificationButtonIndex : [NSNumber numberWithInt:buttonIndex],
  };
}

+ (NSDictionary*)buildActivatedDictionary:(NSUserNotification*)notification {
  return [NotificationResponseBuilder buildDictionary:notification
                                            dismissed:NO];
}

+ (NSDictionary*)buildDismissedDictionary:(NSUserNotification*)notification {
  return [NotificationResponseBuilder buildDictionary:notification
                                            dismissed:YES];
}

@end
