// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_BUILDER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_BUILDER_MAC_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/notifications/notification_builder_base.h"

@class NSUserNotification;

// Provides a marshallable way for storing the information required to construct
// a NSUserNotification that is to be displayed on the system.
//
// A quick example:
//     base::scoped_nsobject<NotificationBuilder> builder(
//         [[NotificationBuilder alloc] initWithCloseLabel:@"Close"
//                                            optionsLabel:@"Options"
//                                           settingsLabel:@"Settings"]);
//     [builder setTitle:@"Hello"];
//
//     // Build a notification out of the data.
//     NSUserNotification* notification =
//         [builder buildUserNotification];
//
//     // Serialize a notification out of the data.
//     NSDictionary* notificationData = [builder buildDictionary];
//
//     // Deserialize the |notificationData| in to a new builder.
//     base::scoped_nsobject<NotificationBuilder> finalBuilder(
//         [[NotificationBuilder alloc] initWithData:notificationData]);
@interface NotificationBuilder : NotificationBuilderBase

// Returns a notification ready to be displayed out of the provided
// |notificationData|.
- (NSUserNotification*)buildUserNotification;

@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_BUILDER_MAC_H_
