// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CRURegistration-Private contains declarations of CRURegistration
// implementation details that need unit testing.

#ifndef CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_
#define CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_

#import <Foundation/Foundation.h>

extern NSString* const CRUReturnCodeErrorDomain;

/***
 * CRUTaskResultCallback is a block receiving the result of an NSTask
 * invocation.
 *
 * Parameters:
 *  NSData* -- all stdout content, nil if the process never launched.
 *  NSData* -- all stderr content. nil if the process never launched.
 *  NSError* -- return value of the process.
 *      * nil: the process ran and returned zero
 *      * error domain is CRUReturnCodeErrorDomain: process ran and returned
 *      nonzero; error code
 *          is the return value. NSData* arguments will be nonnil.
 *      * any other error domain: the task could not be launched; this is the
 *      error from NSTask or
 *          is in CRURegistrationErrorDomain. NSData* elements will be nil.
 */
typedef void (^CRUTaskResultCallback)(NSData*, NSData*, NSError*);

/**
 * CRUAsyncTaskRunner runs an NSTask and asynchronously accumulates its stdout
 * and stderr streams into NSMutableData buffers.
 */
@interface CRUAsyncTaskRunner : NSObject

- (instancetype)initWithTask:(NSTask*)task
                 targetQueue:(dispatch_queue_t)targetQueue
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

/**
 * launchWithReply launches the task and buffers its output. It calls `reply`
 * with the results of the task when the task completes. If the task cannot
 * be launched, it invokes `reply` with nil NSData* args and the NSError* from
 * NSTask's launch failure.
 */
- (void)launchWithReply:(CRUTaskResultCallback)reply;

@end

#endif  // CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_
