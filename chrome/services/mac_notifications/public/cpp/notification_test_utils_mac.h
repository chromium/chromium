// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_TEST_UTILS_MAC_H_
#define CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_TEST_UTILS_MAC_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

API_AVAILABLE(macosx(10.14))
@interface FakeUNNotification : NSObject
@property(nonatomic, retain) UNNotificationRequest* request;
@end

API_AVAILABLE(macosx(10.14))
@interface FakeUNNotificationResponse : NSObject
@property(nonatomic, retain) FakeUNNotification* notification;
@property(nonatomic, copy) NSString* actionIdentifier;
@end

#endif  // CHROME_SERVICES_MAC_NOTIFICATIONS_PUBLIC_CPP_NOTIFICATION_TEST_UTILS_MAC_H_
