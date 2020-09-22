// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"

#import <AppKit/AppKit.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"

#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"

@implementation NotificationBuilder

- (instancetype)initWithCloseLabel:(NSString*)closeLabel
                      optionsLabel:(NSString*)optionsLabel
                     settingsLabel:(NSString*)settingsLabel {
  if ((self = [super init])) {
    [_notificationData
        setObject:closeLabel
           forKey:notification_constants::kNotificationCloseButtonTag];
    [_notificationData
        setObject:optionsLabel
           forKey:notification_constants::kNotificationOptionsButtonTag];
    [_notificationData
        setObject:settingsLabel
           forKey:notification_constants::kNotificationSettingsButtonTag];
  }
  return self;
}

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
          objectForKey:notification_constants::kNotificationImage]) {
    if ([[NSImage class] conformsToProtocol:@protocol(NSSecureCoding)]) {
      NSImage* image = [_notificationData
          objectForKey:notification_constants::kNotificationImage];
      [toast setContentImage:image];
    } else {  // NSImage only conforms to NSSecureCoding from 10.10 onwards.
      base::scoped_nsobject<NSImage> image([[NSImage alloc]
          initWithData:
              [_notificationData
                  objectForKey:notification_constants::kNotificationImage]]);
      [toast setContentImage:image];
    }
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
    // explicitly override it in case the user decides to not
    // use the OS language in Chrome.
    [toast
        setOtherButtonTitle:[_notificationData
                                objectForKey:notification_constants::
                                                 kNotificationCloseButtonTag]];

    // Display the Settings button as the action button if there are either no
    // developer-provided action buttons, or the alternate action menu is not
    // available on this Mac version. This avoids needlessly showing the menu.
    if (![_notificationData
            objectForKey:notification_constants::kNotificationButtonOne] ||
        ![toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]) {
      if (settingsButton) {
        [toast setActionButtonTitle:
                   [_notificationData
                       objectForKey:notification_constants::
                                        kNotificationSettingsButtonTag]];
      } else {
        [toast setHasActionButton:NO];
      }

    } else {
      // Otherwise show the alternate menu, then show the developer actions and
      // finally the settings one if needed.
      DCHECK(
          [toast respondsToSelector:@selector(_alwaysShowAlternateActionMenu)]);
      DCHECK(
          [toast respondsToSelector:@selector(_alternateActionButtonTitles)]);
      [toast setActionButtonTitle:
                 [_notificationData
                     objectForKey:notification_constants::
                                      kNotificationOptionsButtonTag]];
      [toast setValue:@YES forKey:@"_alwaysShowAlternateActionMenu"];

      NSMutableArray* buttons = [NSMutableArray arrayWithCapacity:3];
      [buttons addObject:[_notificationData
                             objectForKey:notification_constants::
                                              kNotificationButtonOne]];
      if ([_notificationData
              objectForKey:notification_constants::kNotificationButtonTwo]) {
        [buttons addObject:[_notificationData
                               objectForKey:notification_constants::
                                                kNotificationButtonTwo]];
      }
      if (settingsButton) {
        [buttons
            addObject:[_notificationData
                          objectForKey:notification_constants::
                                           kNotificationSettingsButtonTag]];
      }

      [toast setValue:buttons forKey:@"_alternateActionButtonTitles"];
    }
  }

  // Tag
  if ([toast respondsToSelector:@selector(setIdentifier:)] &&
      [_notificationData
          objectForKey:notification_constants::kNotificationTag]) {
    [toast setValue:[_notificationData
                        objectForKey:notification_constants::kNotificationTag]
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
