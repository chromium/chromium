// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_RESPONSE_BUILDER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_RESPONSE_BUILDER_MAC_H_

#import <Foundation/Foundation.h>

@class NSUserNotification;

// Provides a marshallable way for storing the information related to a
// notification response action, clicking on it, clicking on a button etc.
@interface NotificationResponseBuilder : NSObject

+ (NSDictionary*)buildActivatedDictionary:(NSUserNotification*)notification;
+ (NSDictionary*)buildDismissedDictionary:(NSUserNotification*)notification;

@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_RESPONSE_BUILDER_MAC_H_
