// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_ALERT_NSNOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_ALERT_NSNOTIFICATION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/notifications/notification_delivery.h"

@class XPCTransactionHandler;

// Implementation of the NotificationDelivery protocol that can display
// notifications of type alert. This uses the legacy NSUserNotificationCenter
// API and is meant for use on macOS versions 10.10 - 10.14. Versions 10.14 and
// above are meant to be supported using UNUserNotificationCenter.
@interface AlertNSNotificationService
    : NSObject <NotificationDelivery, NSUserNotificationCenterDelegate>
- (instancetype)initWithTransactionHandler:(XPCTransactionHandler*)handler
                             xpcConnection:(NSXPCConnection*)connection;
@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_ALERT_NSNOTIFICATION_SERVICE_H_
