// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_CONSTANTS_MAC_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_CONSTANTS_MAC_H_

#import <Foundation/Foundation.h>

namespace notification_constants {

extern NSString* const kNotificationTitle;
extern NSString* const kNotificationSubTitle;
extern NSString* const kNotificationInformativeText;
extern NSString* const kNotificationImage;
extern NSString* const kNotificationButtonOne;
extern NSString* const kNotificationButtonTwo;
extern NSString* const kNotificationTag;
extern NSString* const kNotificationCloseButtonTag;
extern NSString* const kNotificationOptionsButtonTag;
extern NSString* const kNotificationSettingsButtonTag;

extern NSString* const kNotificationOrigin;
extern NSString* const kNotificationId;
extern NSString* const kNotificationProfileId;
extern NSString* const kNotificationIncognito;
extern NSString* const kNotificationType;
extern NSString* const kNotificationOperation;
extern NSString* const kNotificationButtonIndex;
extern NSString* const kNotificationHasSettingsButton;
extern NSString* const kNotificationCreatorPid;

extern NSString* const kAlertXPCServiceName;

// Value used to represent the absence of a button index following a user
// interaction with a notification.
constexpr int kNotificationInvalidButtonIndex = -1;

}  // notification_constants

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_CONSTANTS_MAC_H_
