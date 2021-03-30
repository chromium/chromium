// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/notification_builder_base.h"

#import <AppKit/AppKit.h>

#include "chrome/services/mac_notifications/public/cpp/notification_operation.h"

@implementation NotificationBuilderBase

- (instancetype)initWithCloseLabel:(NSString*)closeLabel
                      optionsLabel:(NSString*)optionsLabel
                     settingsLabel:(NSString*)settingsLabel {
  if ((self = [super init])) {
    _notificationData.reset([[NSMutableDictionary alloc] init]);
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

- (instancetype)initWithDictionary:(NSDictionary*)data {
  if ((self = [super init])) {
    _notificationData.reset([data copy]);
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  if (title.length) {
    [_notificationData setObject:title
                          forKey:notification_constants::kNotificationTitle];
  }
}

- (void)setSubTitle:(NSString*)subTitle {
  if (subTitle.length) {
    [_notificationData setObject:subTitle
                          forKey:notification_constants::kNotificationSubTitle];
  }
}

- (void)setContextMessage:(NSString*)contextMessage {
  if (contextMessage.length) {
    [_notificationData
        setObject:contextMessage
           forKey:notification_constants::kNotificationInformativeText];
  }
}

- (void)setButtons:(NSString*)primaryButton
    secondaryButton:(NSString*)secondaryButton {
  DCHECK(primaryButton.length);
  [_notificationData setObject:primaryButton
                        forKey:notification_constants::kNotificationButtonOne];
  if (secondaryButton.length) {
    [_notificationData
        setObject:secondaryButton
           forKey:notification_constants::kNotificationButtonTwo];
  }
}

- (void)setIdentifier:(NSString*)identifier {
  DCHECK(identifier.length);
  [_notificationData setObject:identifier
                        forKey:notification_constants::kNotificationIdentifier];
}

- (void)setOrigin:(NSString*)origin {
  if (origin.length) {
    [_notificationData setObject:origin
                          forKey:notification_constants::kNotificationOrigin];
  }
}

- (void)setNotificationId:(NSString*)notificationId {
  DCHECK(notificationId.length);
  [_notificationData setObject:notificationId
                        forKey:notification_constants::kNotificationId];
}

- (void)setProfileId:(NSString*)profileId {
  DCHECK(profileId.length);
  [_notificationData setObject:profileId
                        forKey:notification_constants::kNotificationProfileId];
}

- (void)setIncognito:(BOOL)incognito {
  [_notificationData setObject:[NSNumber numberWithBool:incognito]
                        forKey:notification_constants::kNotificationIncognito];
}

- (void)setCreatorPid:(NSNumber*)pid {
  [_notificationData setObject:pid
                        forKey:notification_constants::kNotificationCreatorPid];
}

- (void)setNotificationType:(NSNumber*)notificationType {
  [_notificationData setObject:notificationType
                        forKey:notification_constants::kNotificationType];
}

- (void)setRenotify:(BOOL)renotify {
  [_notificationData setObject:[NSNumber numberWithBool:renotify]
                        forKey:notification_constants::kNotificationRenotify];
}

- (void)setShowSettingsButton:(BOOL)showSettingsButton {
  [_notificationData
      setObject:[NSNumber numberWithBool:showSettingsButton]
         forKey:notification_constants::kNotificationHasSettingsButton];
}

- (void)setIcon:(NSImage*)icon {
  if (!icon)
    return;

  [_notificationData setObject:icon
                        forKey:notification_constants::kNotificationIcon];
}

- (void)setClosedFromAlert:(BOOL)fromAlert {
  [_notificationData
      setObject:@(static_cast<int>(NotificationOperation::NOTIFICATION_CLOSE))
         forKey:notification_constants::kNotificationOperation];
  [_notificationData
      setObject:@(notification_constants::kNotificationInvalidButtonIndex)
         forKey:notification_constants::kNotificationButtonIndex];
  [_notificationData setObject:@(fromAlert)
                        forKey:notification_constants::kNotificationIsAlert];
}

- (NSDictionary*)buildDictionary {
  return [[_notificationData copy] autorelease];
}

@end
