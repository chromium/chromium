// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_UNNOTIFICATION_BUILDER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_UNNOTIFICATION_BUILDER_MAC_H_

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/notifications/notification_builder_base.h"

@class UNMutableNotificationContent;

// Provides a marshallable way for storing the information required to construct
// a UNMutableNotificationContent that is to be displayed on the system.
//
// A quick example:
//     base::scoped_nsobject<UNNotificationBuilder> builder(
//         [[UNNotificationBuilder alloc] init]);
//     [builder setTitle:@"Hello"];
//
//     // Build a notification out of the data.
//     UNMutableNotificationContent* notification =
//         [builder buildUserNotification];
//
//     // Serialize a notification out of the data.
//     NSDictionary* notificationData = [builder buildDictionary];
//
//     // Deserialize the |notificationData| in to a new builder.
//     base::scoped_nsobject<UNNotificationBuilder> finalBuilder(
//         [[UNNotificationBuilder alloc] initWithData:notificationData]);
API_AVAILABLE(macosx(10.14))
@interface UNNotificationBuilder : NotificationBuilderBase

// Returns a notification ready to be displayed out of the provided
// |notificationData|.
- (UNMutableNotificationContent*)buildUserNotification;

@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_UNNOTIFICATION_BUILDER_MAC_H_
