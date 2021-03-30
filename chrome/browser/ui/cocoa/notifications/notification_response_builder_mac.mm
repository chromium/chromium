// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"

#include "base/check.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"
#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"

@implementation NotificationResponseBuilder

+ (NSDictionary*)buildDictionary:(NSUserNotification*)notification
                       fromAlert:(BOOL)fromAlert
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
    int buttonCount = 1;
    if ([notification
            respondsToSelector:@selector(_alternateActionButtonTitles)]) {
      buttonCount =
          [[notification valueForKey:@"_alternateActionButtonTitles"] count];
    }

    if (buttonCount > 1) {
      // There are multiple buttons in the overflow menu. Get the clicked index.
      buttonIndex =
          [[notification valueForKey:@"_alternateActionIndex"] intValue];
    } else {
      // There was only one button so we know it was clicked.
      buttonIndex = 0;
      buttonCount = 1;
    }

    BOOL settingsButtonRequired = [hasSettingsButton boolValue];
    BOOL clickedLastButton = buttonIndex == buttonCount - 1;

    // The settings button is always the last button if present.
    if (clickedLastButton && settingsButtonRequired) {
      operation = NotificationOperation::NOTIFICATION_SETTINGS;
      buttonIndex = notification_constants::kNotificationInvalidButtonIndex;
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
    notification_constants::
    kNotificationOperation : @(static_cast<int>(operation)),
    notification_constants::kNotificationButtonIndex : @(buttonIndex),
    notification_constants::kNotificationIsAlert : @(fromAlert),
  };
}

+ (NSDictionary*)buildActivatedDictionary:(NSUserNotification*)notification
                                fromAlert:(BOOL)fromAlert {
  return [NotificationResponseBuilder buildDictionary:notification
                                            fromAlert:fromAlert
                                            dismissed:NO];
}

+ (NSDictionary*)buildDismissedDictionary:(NSUserNotification*)notification
                                fromAlert:(BOOL)fromAlert {
  return [NotificationResponseBuilder buildDictionary:notification
                                            fromAlert:fromAlert
                                            dismissed:YES];
}

@end
