// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"

#import <AppKit/AppKit.h>
#import <UserNotifications/UserNotifications.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"

#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

@implementation UNNotificationBuilder

- (UNMutableNotificationContent*)buildUserNotification {
  base::scoped_nsobject<UNMutableNotificationContent> toast(
      [[UNMutableNotificationContent alloc] init]);
  [toast setTitle:[_notificationData
                      objectForKey:notification_constants::kNotificationTitle]];

  [toast
      setSubtitle:[_notificationData objectForKey:notification_constants::
                                                      kNotificationSubTitle]];
  [toast setBody:[_notificationData
                     objectForKey:notification_constants::
                                      kNotificationInformativeText]];

  // Type (needed to define the buttons)
  NSNumber* type = [_notificationData
      objectForKey:notification_constants::kNotificationType];

  NSString* origin =
      [_notificationData
          objectForKey:notification_constants::kNotificationOrigin]
          ? [_notificationData
                objectForKey:notification_constants::kNotificationOrigin]
          : @"";
  DCHECK(
      [_notificationData objectForKey:notification_constants::kNotificationId]);
  NSString* notificationId =
      [_notificationData objectForKey:notification_constants::kNotificationId];

  DCHECK([_notificationData
      objectForKey:notification_constants::kNotificationProfileId]);
  NSString* profileId = [_notificationData
      objectForKey:notification_constants::kNotificationProfileId];

  DCHECK([_notificationData
      objectForKey:notification_constants::kNotificationCreatorPid]);
  NSNumber* creatorPid = [_notificationData
      objectForKey:notification_constants::kNotificationCreatorPid];

  DCHECK([_notificationData
      objectForKey:notification_constants::kNotificationIncognito]);
  NSNumber* incognito = [_notificationData
      objectForKey:notification_constants::kNotificationIncognito];

  [toast setUserInfo:@{
    notification_constants::kNotificationOrigin : origin,
    notification_constants::kNotificationId : notificationId,
    notification_constants::kNotificationProfileId : profileId,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationType : type,
    notification_constants::kNotificationCreatorPid : creatorPid,
  }];

  return toast.autorelease();
}

@end
