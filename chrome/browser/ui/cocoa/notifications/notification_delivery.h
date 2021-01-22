// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_DELIVERY_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_DELIVERY_H_

#import <Foundation/Foundation.h>

#import "chrome/browser/ui/cocoa/notifications/xpc_mach_port.h"

// Collection of protocols used for XPC communication between chrome
// and the alert notification xpc service.

// Protocol for the XPC notification service.
@protocol NotificationDelivery

// Sets the Mach exception handler port to use for the XPCService, and sets
// which notification API to be used. This method must be called first before
// using the other methods in this protocol.
- (void)setUseUNNotification:(BOOL)useUNNotification
           machExceptionPort:(CrXPCMachPort*)port;

// |notificationData| is generated using a NofiticationBuilder object.
- (void)deliverNotification:(NSDictionary*)notificationData;

// Closes an alert with the given |notificationId| and |profileId|.
- (void)closeNotificationWithId:(NSString*)notificationId
                  withProfileId:(NSString*)profileId;

// Closes all the alerts being displayed.
- (void)closeAllNotifications;

// Will invoke |reply| with an array of NSString notification IDs for all alerts
// for |profileId| and |incognito| value currently displayed.
- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                          andIncognito:(BOOL)incognito
                             withReply:(void (^)(NSArray*))reply;
@end

// Response protocol for the XPC notification service to notify Chrome of
// notification interactions.
@protocol NotificationReply

// |notificationResponseData| is generated through a
// NotificationResponseBuilder.
- (void)notificationClick:(NSDictionary*)notificationResponseData;

@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_NOTIFICATION_DELIVERY_H_
