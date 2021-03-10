// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/alert_nsnotification_service.h"

#import "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/xpc_transaction_handler.h"
#include "chrome/services/mac_notifications/public/cpp/notification_constants_mac.h"

@class NSUserNotificationCenter;

@implementation AlertNSNotificationService {
  base::scoped_nsobject<XPCTransactionHandler> _transactionHandler;
  base::scoped_nsobject<NSXPCConnection> _connection;
}

- (instancetype)initWithTransactionHandler:(XPCTransactionHandler*)handler
                             xpcConnection:(NSXPCConnection*)connection {
  if ((self = [super init])) {
    [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:self];
    _transactionHandler.reset([handler retain]);
    _connection.reset([connection retain]);
  }
  return self;
}

- (void)dealloc {
  [[NSUserNotificationCenter defaultUserNotificationCenter] setDelegate:nil];
  [super dealloc];
}

- (void)setUseUNNotification:(BOOL)useUNNotification
           machExceptionPort:(CrXPCMachPort*)port {
  NOTREACHED();
}

- (void)deliverNotification:(NSDictionary*)notificationData {
  base::scoped_nsobject<NotificationBuilder> builder(
      [[NotificationBuilder alloc] initWithDictionary:notificationData]);

  NSUserNotification* toast = [builder buildUserNotification];
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      deliverNotification:toast];
  [_transactionHandler openTransactionIfNeeded];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  NSArray* deliveredNotifications = [notificationCenter deliveredNotifications];
  for (NSUserNotification* toast in deliveredNotifications) {
    NSString* toastId =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([notificationId isEqualToString:toastId] &&
        [profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      [notificationCenter removeDeliveredNotification:toast];
      [_transactionHandler closeTransactionIfNeeded];
      break;
    }
  }
}

- (void)closeNotificationsWithProfileId:(NSString*)profileId
                              incognito:(BOOL)incognito {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  NSArray* deliveredNotifications = [notificationCenter deliveredNotifications];
  BOOL removedNotifications = NO;

  for (NSUserNotification* toast in deliveredNotifications) {
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      [notificationCenter removeDeliveredNotification:toast];
      removedNotifications = YES;
    }
  }

  if (removedNotifications)
    [_transactionHandler closeTransactionIfNeeded];
}

- (void)closeAllNotifications {
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      removeAllDeliveredNotifications];
  [_transactionHandler closeTransactionIfNeeded];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                             incognito:(BOOL)incognito
                                 reply:(void (^)(NSArray*))reply {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  NSArray* deliveredNotifications = [notificationCenter deliveredNotifications];
  NSMutableArray* notificationIds =
      [NSMutableArray arrayWithCapacity:[deliveredNotifications count]];
  for (NSUserNotification* toast in deliveredNotifications) {
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL toastIncognito = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];

    if ([profileId isEqualToString:toastProfileId] &&
        incognito == toastIncognito) {
      [notificationIds
          addObject:[toast.userInfo
                        objectForKey:notification_constants::kNotificationId]];
    }
  }
  reply(notificationIds);
}

- (void)getAllDisplayedAlertsWithReply:(void (^)(NSArray*))reply {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  NSArray* deliveredNotifications = [notificationCenter deliveredNotifications];
  NSMutableArray* notificationIds =
      [NSMutableArray arrayWithCapacity:[deliveredNotifications count]];
  for (NSUserNotification* toast in deliveredNotifications) {
    NSString* toastId =
        [toast.userInfo objectForKey:notification_constants::kNotificationId];
    NSString* toastProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    NSNumber* toastIncognito = [toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito];

    [notificationIds addObject:@{
      notification_constants::kNotificationId : toastId,
      notification_constants::kNotificationProfileId : toastProfileId,
      notification_constants::kNotificationIncognito : toastIncognito
    }];
  }
  reply(notificationIds);
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification
                                                  fromAlert:YES];
  [[_connection remoteObjectProxy] notificationClick:response];
}

// _NSUserNotificationCenterDelegatePrivate:
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notification {
  NSDictionary* response =
      [NotificationResponseBuilder buildDismissedDictionary:notification
                                                  fromAlert:YES];
  [[_connection remoteObjectProxy] notificationClick:response];
  [_transactionHandler closeTransactionIfNeeded];
}

// _NSUserNotificationCenterDelegatePrivate:
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (NSUserNotification* notification in notifications) {
    NSDictionary* response =
        [NotificationResponseBuilder buildDismissedDictionary:notification
                                                    fromAlert:YES];
    [[_connection remoteObjectProxy] notificationClick:response];
  }
  [_transactionHandler closeTransactionIfNeeded];
}

@end
