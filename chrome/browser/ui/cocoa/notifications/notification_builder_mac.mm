// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"

@implementation NotificationBuilder

- (NSUserNotification*)buildUserNotification {
  base::scoped_nsobject<NSUserNotification> toast(
      [[NSUserNotification alloc] init]);
  [toast setTitle:[_notificationData
                      objectForKey:notification_constants::kNotificationTitle]];
  [toast
      setSubtitle:[_notificationData objectForKey:notification_constants::
                                                      kNotificationSubTitle]];
  [toast setInformativeText:[_notificationData
                                objectForKey:notification_constants::
                                                 kNotificationInformativeText]];

  // Icon
  if ([_notificationData
          objectForKey:notification_constants::kNotificationIcon]) {
    NSImage* image = [_notificationData
        objectForKey:notification_constants::kNotificationIcon];
    [toast setContentImage:image];
  }

  // Type (needed to define the buttons)
  NSNumber* type = [_notificationData
      objectForKey:notification_constants::kNotificationType];

  // Extensions don't have a settings button.
  NSNumber* showSettingsButton = [_notificationData
      objectForKey:notification_constants::kNotificationHasSettingsButton];

  // Buttons
  if ([toast respondsToSelector:@selector(_showsButtons)]) {
    DCHECK([_notificationData
        objectForKey:notification_constants::kNotificationCloseButtonTag]);
    DCHECK([_notificationData
        objectForKey:notification_constants::kNotificationSettingsButtonTag]);
    DCHECK([_notificationData
        objectForKey:notification_constants::kNotificationOptionsButtonTag]);
    DCHECK([_notificationData
        objectForKey:notification_constants::kNotificationHasSettingsButton]);

    BOOL settingsButton = [showSettingsButton boolValue];

    [toast setValue:@YES forKey:@"_showsButtons"];
    // A default close button label is provided by the platform but we
    // explicitly override it in case the user decides to not use the OS
    // language in Chrome. macOS 11 shows a close button in the top-left corner.
    if (!base::mac::IsAtLeastOS11()) {
      [toast
          setOtherButtonTitle:
              [_notificationData objectForKey:notification_constants::
                                                  kNotificationCloseButtonTag]];
    }

    NSMutableArray* buttons = [NSMutableArray arrayWithCapacity:3];
    if ([_notificationData
            objectForKey:notification_constants::kNotificationButtonOne]) {
      [buttons addObject:[_notificationData
                             objectForKey:notification_constants::
                                              kNotificationButtonOne]];
    }
    if ([_notificationData
            objectForKey:notification_constants::kNotificationButtonTwo]) {
      [buttons addObject:[_notificationData
                             objectForKey:notification_constants::
                                              kNotificationButtonTwo]];
    }
    if (settingsButton) {
      // If we can't show an action menu but need a settings button, only show
      // the settings button and don't show developer provided actions.
      if (![toast
              respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]) {
        [buttons removeAllObjects];
      }
      [buttons addObject:[_notificationData
                             objectForKey:notification_constants::
                                              kNotificationSettingsButtonTag]];
    }

    if ([buttons count] == 0) {
      // Don't show action button if no actions needed.
      [toast setHasActionButton:NO];
    } else if ([buttons count] == 1) {
      // Only one action so we don't need a menu. Just set the button title.
      [toast setActionButtonTitle:[buttons firstObject]];
    } else {
      // Show the alternate menu with developer actions and settings if needed.
      DCHECK(
          [toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]);
      DCHECK(
          [toast respondsToSelector:@selector(_alternateActionButtonTitles)]);
      // macOS 11 does not support overriding the text of the overflow button
      // and will always show "Options" via this API. Setting actionButtonTitle
      // just appends another button into the overflow menu. Only the new
      // UNNotification API allows overriding this title on macOS 11.
      if (base::mac::IsAtLeastOS11()) {
        [toast setValue:@NO forKey:@"_hasActionButton"];
      } else {
        [toast setActionButtonTitle:
                   [_notificationData
                       objectForKey:notification_constants::
                                        kNotificationOptionsButtonTag]];
      }
      [toast setValue:@YES forKey:@"_alwaysShowAlternateActionMenu"];
      [toast setValue:buttons forKey:@"_alternateActionButtonTitles"];
    }
  }

  // Identifier
  if ([toast respondsToSelector:@selector(setIdentifier:)]) {
    [toast setValue:[_notificationData objectForKey:notification_constants::
                                                        kNotificationIdentifier]
             forKey:@"identifier"];
  }

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

  toast.get().userInfo = @{
    notification_constants::kNotificationOrigin : origin,
    notification_constants::kNotificationId : notificationId,
    notification_constants::kNotificationProfileId : profileId,
    notification_constants::kNotificationIncognito : incognito,
    notification_constants::kNotificationType : type,
    notification_constants::kNotificationCreatorPid : creatorPid,
    notification_constants::kNotificationHasSettingsButton : showSettingsButton,
  };
  return toast.autorelease();
}

@end
