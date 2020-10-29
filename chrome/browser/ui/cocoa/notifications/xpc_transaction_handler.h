// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_XPC_TRANSACTION_HANDLER_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_XPC_TRANSACTION_HANDLER_H_

#import <Foundation/Foundation.h>

// Keeps track of whether a XPC transaction is opened.
@interface XPCTransactionHandler : NSObject

// Sets which API to use, NSUserNotificationCenter or UNUserNotificationCenter.
- (void)setUseUNNotification:(BOOL)useUNNotification;

// Only open a new transaction if one is not already opened.
- (void)openTransactionIfNeeded;

// Close the transaction if no alerts are being displayed.
- (void)closeTransactionIfNeeded;
@end

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_XPC_TRANSACTION_HANDLER_H_
