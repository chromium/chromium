// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_KEYSTONE_KS_TICKETS_H_
#define CHROME_UPDATER_MAC_KEYSTONE_KS_TICKETS_H_

#import <Foundation/Foundation.h>

@class KSPathExistenceChecker;

// KSTicketStore holds a class method for reading an NSDictionary of NSString
// to KSTickets.
@interface KSTicketStore : NSObject

+ (nullable NSDictionary*)readStoreWithPath:(nonnull NSString*)path;

@end

#endif  // CHROME_UPDATER_MAC_KEYSTONE_KS_TICKETS_H_
