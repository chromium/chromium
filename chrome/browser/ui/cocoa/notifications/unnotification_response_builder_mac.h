// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_UNNOTIFICATION_RESPONSE_BUILDER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_UNNOTIFICATION_RESPONSE_BUILDER_MAC_H_

#import <Foundation/Foundation.h>

@class UNNotificationResponse;

// Provides a marshallable way for storing the information related to a
// notification response action, clicking on it, clicking on a button etc.
API_AVAILABLE(macosx(10.14))
@interface UNNotificationResponseBuilder : NSObject

+ (NSDictionary*)buildDictionary:(UNNotificationResponse*)response;

@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_UNNOTIFICATION_RESPONSE_BUILDER_MAC_H_
