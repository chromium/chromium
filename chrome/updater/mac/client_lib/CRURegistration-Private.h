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

typedef NS_ERROR_ENUM(CRURegistrationInternalErrorDomain,
                      CRURegistrationInternalError){
    CRURegistrationInternalErrorTaskAlreadyLaunched = 1,

    // An underlying API that can fail returned an error that we did not
    // specifically anticipate.
    CRURegistrationInternalErrorUnrecognized = 9999,
};

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

/**
 * Synchronously finds the path to an installed KSAdmin binary. If a systemwide
 * ksadmin is available, it prefers it; otherwise, if a user ksadmin is
 * available, it returns that; if neither can be found, it returns nil.
 *
 * This does not depend on, or mutate, any protected state inside
 * CRURegistration itself, but does check filesystem state. If the updater is
 * concurrently being installed, it might not find it. This is intended for
 * use while CRURegistration is not concurrently running a task.
 */
- (nullable NSURL*)syncFindBestKSAdmin;

/**
 * Wrap an NSError from a failed attempt to run a task with a semantically
 * appropriate domain and code. If the error is an intended part of the library
 * API, it will be in CRURegistrationErrorDomain; otherwise, it represents a
 * library bug that the user should, ideally, not rely on and will be wrapped
 * under CRURegistrationInternalErrorDomain.
 *
 * Errors already in one of these two error domains are returned unchanged.
 * Nil is also returned unchanged. Otherwise:
 * - errors representing nonzero return codes from tasks are converted to
 *   CRURegistrationErrorTaskFailed
 * - "file not found" errors from Apple APIs are assumed to be NSTask failing
 *   to find a binary and are converted to CRURegistrationErrorHelperNotFound
 * - other errors are unexpected
 *
 * If the returned error is not the same as the input error, the result error's
 * `userInfo` dictionary contains a value under `NSUnderlyingErrorKey` with the
 * original error unchanged. A wrapped error's `userInfo` also contains the
 * values of `gotStdout` and `gotStderr` under `CRUStdoutKey` and `CRUStderrKey`
 * respectively. Values in `userInfo` are intended to assist with debugging, but
 * production code should not rely on these values for identifying and handling
 * the disposition of an error; file bugs against this library if more detail
 * is required than is available, or if any internal error is encountered.
 */
- (nullable NSError*)wrapError:(nullable NSError*)error
                    withStdout:(nullable NSString*)gotStdout
                     andStderr:(nullable NSString*)gotStderr;

@end

NS_ASSUME_NONNULL_END

#endif  // CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_PRIVATE_H_
