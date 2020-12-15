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

- (void)setIconPath:(NSString*)iconPath {
  [_notificationData setObject:iconPath
                        forKey:notification_constants::kNotificationIconPath];
}

- (UNNotificationCategory*)buildCategory {
  DCHECK(
      [_notificationData objectForKey:notification_constants::kNotificationId]);
  DCHECK([_notificationData
      objectForKey:notification_constants::kNotificationHasSettingsButton]);

  NSMutableArray* buttonsArray = [NSMutableArray arrayWithCapacity:4];

  // Extensions don't have a settings button.
  NSNumber* showSettingsButton = [_notificationData
      objectForKey:notification_constants::kNotificationHasSettingsButton];
  BOOL settingsButton = [showSettingsButton boolValue];

  UNNotificationAction* closeButton = [UNNotificationAction
      actionWithIdentifier:notification_constants::kNotificationCloseButtonTag
                     title:[_notificationData
                               objectForKey:notification_constants::
                                                kNotificationCloseButtonTag]
                   options:UNNotificationActionOptionNone];
  // macOS 11 shows a close button in the top-left corner.
  if (!base::mac::IsAtLeastOS11())
    [buttonsArray addObject:closeButton];

  if ([_notificationData
          objectForKey:notification_constants::kNotificationButtonOne]) {
    UNNotificationAction* buttonOne = [UNNotificationAction
        actionWithIdentifier:notification_constants::kNotificationButtonOne
                       title:[_notificationData
                                 objectForKey:notification_constants::
                                                  kNotificationButtonOne]
                     options:UNNotificationActionOptionNone];
    [buttonsArray addObject:buttonOne];
  }
  if ([_notificationData
          objectForKey:notification_constants::kNotificationButtonTwo]) {
    UNNotificationAction* buttonTwo = [UNNotificationAction
        actionWithIdentifier:notification_constants::kNotificationButtonTwo
                       title:[_notificationData
                                 objectForKey:notification_constants::
                                                  kNotificationButtonTwo]
                     options:UNNotificationActionOptionNone];
    [buttonsArray addObject:buttonTwo];
  }

  if (settingsButton) {
    UNNotificationAction* settingsButton = [UNNotificationAction
        actionWithIdentifier:notification_constants::
                                 kNotificationSettingsButtonTag
                       title:
                           [_notificationData
                               objectForKey:notification_constants::
                                                kNotificationSettingsButtonTag]
                     options:UNNotificationActionOptionNone];
    [buttonsArray addObject:settingsButton];
  }

  // If there are only 2 buttons [Close, button] then the actions array need to
  // be set as [button, Close] so that close is on top. This is to safeguard the
  // order of the buttons in case respondsToSelector:@selector(alternateAction)
  // were to return false.
  if (!base::mac::IsAtLeastOS11() && [buttonsArray count] == 2) {
    // Remove the close button and move it to the end of the array
    [buttonsArray removeObject:closeButton];
    [buttonsArray addObject:closeButton];
  }

  UNNotificationCategory* category = [UNNotificationCategory
      categoryWithIdentifier:
          [_notificationData
              objectForKey:notification_constants::kNotificationId]
                     actions:buttonsArray
           intentIdentifiers:@[]
                     options:UNNotificationCategoryOptionCustomDismissAction];
  [_notificationData
      setObject:[_notificationData
                    objectForKey:notification_constants::kNotificationId]
         forKey:notification_constants::kNotificationCategoryIdentifier];

  // This uses a private API to make sure the close button is always visible in
  // both alerts and banners, and modifies its content so that it is consistent
  // with the rest of the notification buttons. Otherwise, the text inside the
  // close button will come from the Apple API.
  if (!base::mac::IsAtLeastOS11() &&
      [category respondsToSelector:@selector(alternateAction)]) {
    [buttonsArray removeObject:closeButton];
    [category setValue:buttonsArray forKey:@"actions"];
    [category setValue:closeButton forKey:@"_alternateAction"];
  }

  // This uses a private API to change the text of the actions menu title so
  // that it is consistent with the rest of the notification buttons
  if ([category respondsToSelector:@selector(actionsMenuTitle)]) {
    [category setValue:[_notificationData
                           objectForKey:notification_constants::
                                            kNotificationOptionsButtonTag]
                forKey:@"_actionsMenuTitle"];
  }

  return category;
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
                               (NSString*)kUTTypePNG
                         }
                           error:nil];

    if (attachment != nil)
      [toast setAttachments:@[ attachment ]];
  }
  // Category
  if ([_notificationData objectForKey:notification_constants::
                                          kNotificationCategoryIdentifier]) {
    [toast setCategoryIdentifier:
               [_notificationData
                   objectForKey:notification_constants::
                                    kNotificationCategoryIdentifier]];
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
