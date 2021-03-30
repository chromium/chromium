// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/unnotification_builder_mac.h"

#import <AppKit/AppKit.h>
#import <UserNotifications/UserNotifications.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"

@implementation UNNotificationBuilder

- (void)setIconPath:(NSString*)iconPath {
  [_notificationData setObject:iconPath
                        forKey:notification_constants::kNotificationIconPath];
}

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

  // This uses a private API to prevent notifications from dismissing on default
  // action instead of clicking on a button
  if ([toast respondsToSelector:@selector
             (shouldPreventNotificationDismissalAfterDefaultAction)]) {
    [toast setValue:@YES
             forKey:@"shouldPreventNotificationDismissalAfterDefaultAction"];
  }

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

  // Icon
  if ([_notificationData
          objectForKey:notification_constants::kNotificationIconPath]) {
    NSURL* url = [NSURL
        fileURLWithPath:
            [_notificationData
                objectForKey:notification_constants::kNotificationIconPath]];
    // When the files are saved using NotificationImageRetainer, they're saved
    // without the .png extension. So |options| here is used to tell the system
    // that the file is of type PNG, as NotificationImageRetainer converts files
    // to PNG before writing them.
    UNNotificationAttachment* attachment = [UNNotificationAttachment
        attachmentWithIdentifier:notificationId
                             URL:url
                         options:@{
                           UNNotificationAttachmentOptionsTypeHintKey :
                               (__bridge NSString*)kUTTypePNG
                         }
                           error:nil];

    if (attachment != nil)
      [toast setAttachments:@[ attachment ]];
  }

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
