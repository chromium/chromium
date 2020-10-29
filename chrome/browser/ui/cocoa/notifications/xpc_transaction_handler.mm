// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "base/notreached.h"
#import "chrome/browser/ui/cocoa/notifications/xpc_transaction_handler.h"

@class NSUserNotificationCenter;

@implementation XPCTransactionHandler {
  BOOL _transactionOpen;
  BOOL _useUNNotification;
}

- (void)setUseUNNotification:(BOOL)useUNNotification {
  _useUNNotification = useUNNotification;
}

- (void)openTransactionIfNeeded {
  @synchronized(self) {
    if (_transactionOpen) {
      return;
    }
    xpc_transaction_begin();
    _transactionOpen = YES;
  }
}

- (void)closeTransactionIfNeeded {
  @synchronized(self) {
    if (_useUNNotification) {
      NOTIMPLEMENTED();
      return;
    }

    NSUserNotificationCenter* notificationCenter =
        [NSUserNotificationCenter defaultUserNotificationCenter];
    NSUInteger showing = [[notificationCenter deliveredNotifications] count];
    if (showing == 0 && _transactionOpen) {
      xpc_transaction_end();
      _transactionOpen = NO;
    }
  }
}
@end
