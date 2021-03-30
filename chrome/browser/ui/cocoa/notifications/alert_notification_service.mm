// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/alert_notification_service.h"

#include "base/mac/scoped_nsobject.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#import "chrome/browser/ui/cocoa/notifications/alert_nsnotification_service.h"
#import "chrome/browser/ui/cocoa/notifications/xpc_transaction_handler.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"
#include "third_party/crashpad/crashpad/client/crashpad_info.h"
#include "third_party/crashpad/crashpad/client/simple_string_dictionary.h"

namespace {

crashpad::SimpleStringDictionary* GetCrashpadAnnotations() {
  static crashpad::SimpleStringDictionary* annotations = []() {
    auto* annotations = new crashpad::SimpleStringDictionary();
    annotations->SetKeyValue("ptype", "AlertNotificationService.xpc");
    annotations->SetKeyValue("pid", base::NumberToString(getpid()).c_str());
    return annotations;
  }();
  return annotations;
}

}  // namespace

@implementation AlertNotificationService {
  base::scoped_nsobject<XPCTransactionHandler> _transactionHandler;
  base::scoped_nsobject<NSXPCConnection> _connection;
  base::scoped_nsobject<NSObject<NotificationDelivery>> _notificationDelivery;

  // Ensures that the XPC service has been configured for crash reporting.
  // Other messages should not be sent to a new instance of the service
  // before -setMachExceptionPort: is called.
  // Because XPC callouts occur on a concurrent dispatch queue, this must be
  // accessed in a @synchronized(self) block.
  BOOL _didSetExceptionPort;
}

- (instancetype)initWithTransactionHandler:(XPCTransactionHandler*)handler
                             xpcConnection:(NSXPCConnection*)connection {
  if ((self = [super init])) {
    _transactionHandler.reset([handler retain]);
    _connection.reset([connection retain]);
  }
  return self;
}

- (void)setUseUNNotification:(BOOL)useUNNotification
           machExceptionPort:(CrXPCMachPort*)port {
  base::mac::ScopedMachSendRight sendRight([port takeRight]);
  if (!sendRight.is_valid()) {
    NOTREACHED();
    return;
  }

  @synchronized(self) {
    if (_didSetExceptionPort) {
      return;
    }

    [_transactionHandler setUseUNNotification:useUNNotification];
    _notificationDelivery.reset([[AlertNSNotificationService alloc]
        initWithTransactionHandler:_transactionHandler
                     xpcConnection:_connection]);

    crashpad::CrashpadClient client;
    _didSetExceptionPort = client.SetHandlerMachPort(std::move(sendRight));
    DCHECK(_didSetExceptionPort);

    crashpad::CrashpadInfo::GetCrashpadInfo()->set_simple_annotations(
        GetCrashpadAnnotations());
  }
}

- (void)deliverNotification:(NSDictionary*)notificationData {
  DCHECK(_didSetExceptionPort);
  DCHECK(_notificationDelivery);

  [_notificationDelivery deliverNotification:notificationData];
}

- (void)closeNotificationWithId:(NSString*)notificationId
                      profileId:(NSString*)profileId
                      incognito:(BOOL)incognito {
  DCHECK(_didSetExceptionPort);
  DCHECK(_notificationDelivery);

  [_notificationDelivery closeNotificationWithId:notificationId
                                       profileId:profileId
                                       incognito:incognito];
}

- (void)closeNotificationsWithProfileId:(NSString*)profileId
                              incognito:(BOOL)incognito {
  DCHECK(_didSetExceptionPort);
  DCHECK(_notificationDelivery);

  [_notificationDelivery closeNotificationsWithProfileId:profileId
                                               incognito:incognito];
}

- (void)closeAllNotifications {
  DCHECK(_didSetExceptionPort);
  DCHECK(_notificationDelivery);

  [_notificationDelivery closeAllNotifications];
}

- (void)getDisplayedAlertsForProfileId:(NSString*)profileId
                             incognito:(BOOL)incognito
                                 reply:(void (^)(NSArray*))reply {
  DCHECK(_didSetExceptionPort);
  DCHECK(_notificationDelivery);

  [_notificationDelivery getDisplayedAlertsForProfileId:profileId
                                              incognito:incognito
                                                  reply:reply];
}

- (void)getAllDisplayedAlertsWithReply:(void (^)(NSArray*))reply {
  DCHECK(_didSetExceptionPort);
  DCHECK(_notificationDelivery);

  [_notificationDelivery getAllDisplayedAlertsWithReply:reply];
}

@end
