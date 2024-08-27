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

/**
 * NSError userInfo dict key mapped to the string captured from the stderr
 * output of a command-line tool invoked by CRURegistration. This error is
 * usually intended to be human-readable, rather than machine-readable.
 */
extern NSString* const CRUStderrKey;

/**
 * NSError userInfo dict key mapped to the string captured from the stdout
 * output of a command-line tool invoked by CRURegistration. May contain partial
 * or incorrect output from the command that failed.
 */
extern NSString* const CRUStdoutKey;

/**
 * NSError userInfo dict key mapped to the return code of a failing task (as
 * an NSNumber wrapping NSInteger).
 */
extern NSString* const CRUReturnCodeKey;

typedef NS_ERROR_ENUM(CRURegistrationErrorDomain, CRURegistrationError){
    /**
     * `CRURegistration` couldn't read a stream (stdout or stderr) when running
     * a subprocess. The POSIX error code for the error is available in the
     * error's user data under `CRUErrnoKey`.
     */
    CRURegistrationErrorTaskStreamUnreadable = 1,

    /**
     * `CRURegistration` couldn't find the updater or the updater installer.
     */
    CRURegistrationErrorHelperNotFound = 2,

    /**
     * The updater component invoked by `CRURegistration` returned an error. Its
     * error message is available in the error's user data under `CRUStderrKey`.
     * Any partial output, usable for debugging purposes, is available under
     * `CRUStdoutKey`.
     */
    CRURegistrationErrorTaskFailed = 3,

    /**
     * Parameters for a CRURegistration operation were invalid. Details about
     * the invalid parameters are available in the error's `userInfo` dict
     * under `NSDebugDescriptionErrorKey`.
     *
     * Invalid arguments may also show up as `CRURegistrationErrorTaskFailed`,
     * if they are detected by the underlying helper binary instead of
     * `CRURegistration` itself. Whenever `CRURegistration` replies with this
     * error, it also fails an `NSAssert` so the issue can be discovered
     * in debug builds while the faulty call is still on the stack.
     */
    CRURegistrationErrorInvalidArgument = 4,

    /**
     * A macOS API failed in a way that implies that important parts of the
     * filesystem are not usable for the requested operation, which
     * `CRURegistration` was running in-process. The issue is something
     * different from "could not find the updater".
     *
     * File system errors encountered while trying to find a helper to run a
     * task out-of-process are reported as `CRURegistrationErrorHelperNotFound`.
     * Errors encountered by such helpers are always reported as
     * `CRURegistrationErrorTaskFailed` instead.
     */
    CRURegistrationErrorFilesystem = 5,
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
 * @param xcPath The absolute path of the application bundle for the instance
 *     of the app this CRURegistration operates on, or another path the updater
 *     registers to verify the existence of an app on disk.
 * @param targetQueue Dispatch queue for callbacks and internal operations.
 *     If this queue is blocked, CRURegistration will get stuck.
 */
- (instancetype)initWithAppId:(NSString*)appId
         existenceCheckerPath:(NSString*)xcPath
                  targetQueue:(dispatch_queue_t)targetQueue
    NS_DESIGNATED_INITIALIZER;

/**
 * Initializes a CRURegistration instance to manage Chromium Updater's
 * information about the app with the provided ID, using a global concurrent
 * queue for execution (with the specified quality of service).
 *
 * @param appId The ID of the app this CRURegistration instance operates on.
 * @param xcPath The absolute path of the application bundle for the instance
 *     of the app this CRURegistration operates on, or another path the updater
 *     registers to verify the existence of an app on disk.
 * @param qos Identifier for the global concurrent queue to use for callbacks
 *     and internal operations. See Apple's documentation for
 *     `dispatch_get_global_queue` for more details:
 *     https://developer.apple.com/documentation/dispatch/1452927-dispatch_get_global_queue
 */
- (instancetype)initWithAppId:(NSString*)appId
         existenceCheckerPath:(NSString*)xcPath
                          qos:(dispatch_qos_class_t)qos;

/**
 * Initializes a CRURegistration instance to manage Chromium Updater's
 * information about the app with the provided ID, using the `QOS_CLASS_UTILITY`
 * global concurrent queue for execution.
 *
 * @param appId The ID of the app this CRURegistration instance operates on.
 * @param xcPath The absolute path of the application bundle for the instance
 *     of the app this CRURegistration operates on, or another path the updater
 *     registers to verify the existence of an app on disk.
 */
- (instancetype)initWithAppId:(NSString*)appId
         existenceCheckerPath:(NSString*)xcPath;

/**
 * CRURegistration cannot be initialized without an app ID and existence
 * checker path.
 */
- (instancetype)init NS_UNAVAILABLE;

/**
 * Asynchronously registers the app for updates, owned by the current user.
 *
 * If the app is already registered for the current user, this updates the
 * registration.
 *
 * When registration has completed, `reply` will be dispatched to the target
 * queue. If registration succeeds, the `NSError*` argument will be nil.
 * If it does not, the `NSError*` argument to `reply` will contain a
 * descriptive error.
 *
 * `reply` may be nil if the application is not interested in the result of
 * registration.
 */
- (void)registerVersion:(NSString*)version
                  reply:(void (^_Nullable)(NSError* _Nullable))reply;

/**
 * Asynchronously retrieves the tag for the registered app.
 *
 * `reply` will be dispatched to the target queue when tag reading has completed
 * or failed. If the tag can be read successfully, it will be passed as the
 * NSString* argument of the reply block and the NSError* argument will be nil.
 * If the tag cannot be read, the NSString* will be nil while the NSError* will
 * contain a descriptive error.
 *
 * If the tag is empty, the NSString* argument will be the empty string (and
 * there is no error). If no app with the provided ID is registered or the
 * updater is not installed, an error occurs.
 */
- (void)fetchTagWithReply:(void (^)(NSString* _Nullable,
                                    NSError* _Nullable))reply;

/**
 * Asynchronously installs the updater, as the current user.
 *
 * `reply` will be dispatched to the target queue when installation has
 * completed or failed. If the installation is skipped because there is already
 * an updater present at the same or higher version, it is considered to have
 * completed successfully.
 *
 * If installation succeeds (or is skipped), the `NSError*` argument to `reply`
 * will be nil. If it fails, the argument will contain a descriptive error.
 */
- (void)installUpdaterWithReply:(void (^_Nullable)(NSError* _Nullable))reply;

/**
 * Asynchronously marks the current product active for the current user.
 *
 * This may run concurrently with other operations performed by CRURegistration
 * and return out-of-order, although it will still run under, and reply on,
 * the target queue. `reply` will be dispatched to the target queue when the
 * "active" file for the provided app ID has been written (or the attempt to
 * do so has failed).
 */
- (void)markActiveWithReply:(void (^_Nullable)(NSError* _Nullable))reply;
@end

NS_ASSUME_NONNULL_END

#endif  // CHROME_UPDATER_MAC_CLIENT_LIB_CRUREGISTRATION_H_
