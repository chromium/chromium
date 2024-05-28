// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_H_
#define CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_H_

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

NS_ASSUME_NONNULL_BEGIN

/** The domain for user or system errors reported by CRURegistration. */
extern NSString* const CRURegistrationErrorDomain;

/**
 * The domain for internal errors from CRURegistration. Clients should never
 * encounter these; please file a bug if you get errors in this domain.
 */
extern NSString* const CRURegistrationInternalErrorDomain;

/**
 * NSError userInfo dict key mapped to the POSIX errno for NSErrors in
 * CRURegistrationErrorDomain with underlying POSIX causes.
 */
extern NSString* const CRUErrnoKey;

typedef NS_ERROR_ENUM(CRURegistrationErrorDomain, CRURegistrationError){
    /**
     * CRURegistration couldn't read a stream (stdout or stderr) when running
     * a subprocess. The POSIX error code for the error is available in the
     * error's user data under CRUErrnoKey.
     */
    CRURegistrationErrorTaskStreamUnreadable = 1,

    /**
     * CRURegistration couldn't find the updater or the updater installer.
     */
    CRURegistrationErrorHelperNotFound = 2,
};

/**
 * CRURegistration interfaces with Chromium Updater to configure and retrieve
 * information about an app, or to install the updater for the current user. Its
 * methods can be invoked from any thread or queue.
 *
 * Do not block CRURegistration's target queue synchronously waiting for a
 * callback from CRURegistration; this causes deadlock. Invoking CRURegistration
 * methods on this (or any) queue without subsequently synchronously waiting for
 * a provided callback to execute is safe. CRURegistration does not block its
 * target queue.
 */
@interface CRURegistration : NSObject

/**
 * Initializes a CRURegistration instance to manage Chromium Updater's
 * information about the app with the provided ID, using a specified queue
 * for execution and callbacks. This queue can be serial or concurrent, but
 * typically should not be the main queue.
 *
 * @param appId The ID of the app this CRURegistration instance operates on.
 * @param targetQueue Dispatch queue for callbacks and internal operations.
 *     If this queue is blocked, CRURegistration will get stuck.
 */
- (instancetype)initWithAppId:(NSString*)appId
                  targetQueue:(dispatch_queue_t)targetQueue
    NS_DESIGNATED_INITIALIZER;

/**
 * Initializes a CRURegistration instance to manage Chromium Updater's
 * information about the app with the provided ID, using a global concurrent
 * queue for execution (with the specified quality of service).
 *
 * @param appId The ID of the app this CRURegistration instance operates on.
 * @param qos Identifier for the global concurrent queue to use for callbacks
 *     and internal operations. See Apple's documentation for
 *     `dispatch_get_global_queue` for more details:
 *     https://developer.apple.com/documentation/dispatch/1452927-dispatch_get_global_queue
 */
- (instancetype)initWithAppId:(NSString*)appId qos:(dispatch_qos_class_t)qos;

/**
 * Initializes a CRURegistration instance to manage Chromium Updater's
 * information about the app with the provided ID, using the `QOS_CLASS_UTILITY`
 * global concurrent queue for execution.
 *
 * @param appId The ID of the app this CRURegistration instance operates on.
 */
- (instancetype)initWithAppId:(NSString*)appId;

/**
 * CRURegistration cannot be initialized without an app ID.
 */
- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_H_
