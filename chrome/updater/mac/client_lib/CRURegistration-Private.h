// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CRURegistration-Private contains declarations of CRURegistration
// implementation details that need unit testing.

#ifndef CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_
#define CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_

#import <Foundation/Foundation.h>

#import "CRURegistration.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString* const CRUReturnCodeErrorDomain;

/***
 * CRUTaskResultCallback is a block receiving the result of an NSTask
 * invocation.
 *
 * Parameters:
 *  NSString* -- all stdout content, nil if the process never launched.
 *  NSString* -- all stderr content. nil if the process never launched.
 *  NSError* -- return value of the process.
 *      * nil: the process ran and returned zero
 *      * error domain is CRUReturnCodeErrorDomain: process ran and returned
 *      nonzero; error code
 *          is the return value. NSData* arguments will be nonnil.
 *      * any other error domain: the task could not be launched; this is the
 *      error from NSTask or
 *          is in CRURegistrationErrorDomain. NSData* elements will be nil.
 */
typedef void (^CRUTaskResultCallback)(NSString* _Nullable,
                                      NSString* _Nullable,
                                      NSError* _Nullable);

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
 * be launched, it invokes `reply` with nil NSString* args and the NSError* from
 * NSTask's launch failure.
 */
- (void)launchWithReply:(CRUTaskResultCallback)reply;

@end

/**
 * CRURegistrationWorkItem represents a task to be constructed and invoked.
 *
 * It is plain data represented as an Objective-C class (instead of a struct)
 * so it can be contained in an NSMutableArray.
 */
@interface CRURegistrationWorkItem : NSObject

/**
 * Callback returning the path of the binary to run. This is invoked immediately
 * before the path is needed to construct an NSTask.
 *
 * This is a callback because some work items -- notably, installing the updater
 * itself -- may affect where future work items should look for the binaries
 * they intend to run, so searching for them needs to be deferred until
 * prior tasks have completed.
 */
@property(nonatomic, copy) NSURL* (^binPathCallback)();

/**
 * Arguments to invoke the NSTask with.
 */
@property(nonatomic, copy) NSArray<NSString*>* args;

/**
 * Handler to asynchronously invoke with task results. This handler is
 * _not_ responsible for cycling the task queue.
 */
@property(nonatomic, copy) CRUTaskResultCallback resultCallback;

@end

@interface CRURegistration (VisibleForTesting)

/**
 * Asynchronously add work items and, if the work queue is not currently being
 * processed, starts processing them. (If work is already in progress, these
 * items will be picked up by its continued execution.)
 */
- (void)addWorkItems:(NSArray<CRURegistrationWorkItem*>*)item;

@end

NS_ASSUME_NONNULL_END

#endif  // CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_
