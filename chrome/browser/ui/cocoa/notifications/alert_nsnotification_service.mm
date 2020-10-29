// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/alert_nsnotification_service.h"

#import "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#import "chrome/browser/ui/cocoa/notifications/notification_builder_mac.h"
#include "chrome/browser/ui/cocoa/notifications/notification_constants_mac.h"
#import "chrome/browser/ui/cocoa/notifications/notification_response_builder_mac.h"
#import "chrome/browser/ui/cocoa/notifications/xpc_transaction_handler.h"

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
                  withProfileId:(NSString*)profileId {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  for (NSUserNotification* candidate in
       [notificationCenter deliveredNotifications]) {
    NSString* candidateId = [candidate.userInfo
        objectForKey:notification_constants::kNotificationId];

    NSString* candidateProfileId = [candidate.userInfo
        objectForKey:notification_constants::kNotificationProfileId];

    if ([candidateId isEqualToString:notificationId] &&
        [profileId isEqualToString:candidateProfileId]) {
      [notificationCenter removeDeliveredNotification:candidate];
      [_transactionHandler closeTransactionIfNeeded];
      break;
    }
  }
}

- (void)closeAllNotifications {
  [[NSUserNotificationCenter defaultUserNotificationCenter]
      removeAllDeliveredNotifications];
  [_transactionHandler closeTransactionIfNeeded];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                          andIncognito:(BOOL)incognito
                             withReply:(void (^)(NSArray*))reply {
  NSUserNotificationCenter* notificationCenter =
      [NSUserNotificationCenter defaultUserNotificationCenter];
  NSArray* deliveredNotifications = [notificationCenter deliveredNotifications];
  NSMutableArray* notificationIds =
      [NSMutableArray arrayWithCapacity:[deliveredNotifications count]];
  for (NSUserNotification* toast in deliveredNotifications) {
    NSString* candidateProfileId = [toast.userInfo
        objectForKey:notification_constants::kNotificationProfileId];
    BOOL incognitoNotification = [[toast.userInfo
        objectForKey:notification_constants::kNotificationIncognito] boolValue];
    if ([candidateProfileId isEqualToString:profileId] &&
        incognito == incognitoNotification) {
      [notificationIds
          addObject:[toast.userInfo
                        objectForKey:notification_constants::kNotificationId]];
    }
  }
  reply(notificationIds);
}

- (void)userNotificationCenter:(NSUserNotificationCenter*)center
       didActivateNotification:(NSUserNotification*)notification {
  NSDictionary* response =
      [NotificationResponseBuilder buildActivatedDictionary:notification];
  [[_connection remoteObjectProxy] notificationClick:response];
}

// _NSUserNotificationCenterDelegatePrivate:
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
               didDismissAlert:(NSUserNotification*)notification {
  NSDictionary* response =
      [NotificationResponseBuilder buildDismissedDictionary:notification];
  [[_connection remoteObjectProxy] notificationClick:response];
  [_transactionHandler closeTransactionIfNeeded];
}

// _NSUserNotificationCenterDelegatePrivate:
- (void)userNotificationCenter:(NSUserNotificationCenter*)center
    didRemoveDeliveredNotifications:(NSArray*)notifications {
  for (NSUserNotification* notification in notifications) {
    NSDictionary* response =
        [NotificationResponseBuilder buildDismissedDictionary:notification];
    [[_connection remoteObjectProxy] notificationClick:response];
  }
  [_transactionHandler closeTransactionIfNeeded];
}

@end
