// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "CRURegistration.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#import "CRURegistration-Private.h"
#include "chrome/updater/updater_branding.h"

#pragma mark - Constants

NSString* const CRURegistrationErrorDomain = @"org.chromium.CRURegistration";
NSString* const CRUReturnCodeErrorDomain = @"org.chromium.CRUReturnCode";
NSString* const CRURegistrationInternalErrorDomain =
    @"org.chromium.CRURegistrationInternal";

// Keys that may be present in NSError `userInfo` dictionaries.
NSString* const CRUErrnoKey = @"org.chromium.CRUErrno";
NSString* const CRUStdStreamNameKey = @"org.chromium.CRUStdStreamName";
NSString* const CRUStderrKey = @"org.chromium.CRUStderr";
NSString* const CRUStdoutKey = @"org.chromium.CRUStdout";
NSString* const CRUReturnCodeKey = @"org.chromium.CRUReturnCode";

#pragma mark - CRUAsyncTaskRunner

@implementation CRUAsyncTaskRunner {
  // These fields are written once during init and never again.
  dispatch_queue_t _parentQueue;
  dispatch_queue_t _privateQueue;
  NSTask* _task;

  // These fields are guarded by `_privateQueue`.
  BOOL _launched;
  NSMutableData* _taskStdoutData;
  NSMutableData* _taskStderrData;
  NSError* _taskManagementError;
  NSPipe* _taskStdoutPipe;
  NSPipe* _taskStderrPipe;
  dispatch_group_t _done_group;
}

- (instancetype)initWithTask:(NSTask*)task
                 targetQueue:(dispatch_queue_t)targetQueue {
  if (self = [super init]) {
    _task = task;
    _parentQueue = targetQueue;
    _privateQueue = dispatch_queue_create_with_target(
        "CRUAsyncTaskRunner", DISPATCH_QUEUE_SERIAL, targetQueue);
  }
  return self;
}

- (void)launchWithReply:(CRUTaskResultCallback)reply {
  dispatch_async(_privateQueue, ^{
    [self syncLaunchWithReply:reply];
  });
}

- (void)syncLaunchWithReply:(CRUTaskResultCallback)reply {
  if (_launched) {
    NSString* taskUrl = _task.executableURL.description;
    NSString* argList = [_task.arguments componentsJoinedByString:@"\n"];
    dispatch_async(_parentQueue, ^{
      reply(nil, nil,
            [NSError
                errorWithDomain:CRURegistrationInternalErrorDomain
                           code:CRURegistrationInternalErrorTaskAlreadyLaunched
                       userInfo:@{
                         NSFilePathErrorKey : taskUrl,
                         NSDebugDescriptionErrorKey : argList,
                       }]);
    });
    return;
  }

  _taskStdoutPipe = [NSPipe pipe];
  _taskStderrPipe = [NSPipe pipe];
  _task.standardOutput = _taskStdoutPipe;
  _task.standardError = _taskStderrPipe;
  _taskStdoutData = [NSMutableData data];
  _taskStderrData = [NSMutableData data];
  _taskManagementError = nil;

  _done_group = dispatch_group_create();
  // Enter for task configuration, to avoid invoking the handler block if the
  // task exits before we've gotten a chance to start processing its output.
  dispatch_group_enter(_done_group);

  dispatch_group_notify(_done_group, _privateQueue, ^{
    self->_task.terminationHandler = nil;
    if (self->_taskManagementError) {
      // The task never launched; touching task.terminationStatus would crash.
      dispatch_async(self->_parentQueue, ^{
        reply(nil, nil, self->_taskManagementError);
      });
      return;
    }

    NSError* returnCodeError = nil;
    if (self->_task.terminationStatus) {
      returnCodeError = [NSError errorWithDomain:CRUReturnCodeErrorDomain
                                            code:self->_task.terminationStatus
                                        userInfo:nil];
    }
    dispatch_async(self->_parentQueue, ^{
      reply([[NSString alloc] initWithData:self->_taskStdoutData
                                  encoding:NSUTF8StringEncoding],
            [[NSString alloc] initWithData:self->_taskStderrData
                                  encoding:NSUTF8StringEncoding],
            returnCodeError);
    });
  });

  // All fields are prepared and the result callback is armed. Hand off to
  // `syncFinishLaunching` so we can early-out on failure without specifically
  // balancing _done_group on each exit path.
  [self syncFinishLaunching];
  dispatch_group_leave(_done_group);
}

- (void)syncFinishLaunching {
  // Local reference to avoid referring to queue-protected fields of `_self`
  // without necessarily being on `_private_queue` -- there are no guarantees
  // about where an NSTask's termination handler is executed.
  dispatch_group_t done_group = _done_group;
  // Enter `_done_group` for task execution itself.
  dispatch_group_enter(done_group);
  _task.terminationHandler = ^(NSTask* unused) {
    dispatch_group_leave(done_group);
  };

  NSError* launchError = nil;
  if (![_task launchAndReturnError:&launchError]) {
    _taskManagementError = launchError;
    // Cancel the `enter`, since the termination handler will never run.
    dispatch_group_leave(done_group);
    return;
  }

  // Task is launched, kick off async I/O.

  [self syncSubscribeAsyncOnHandle:_taskStdoutPipe.fileHandleForReading
                              into:_taskStdoutData
                             named:@"stdout"];
  [self syncSubscribeAsyncOnHandle:_taskStderrPipe.fileHandleForReading
                              into:_taskStderrData
                             named:@"stderr"];
}

- (void)syncSubscribeAsyncOnHandle:(NSFileHandle*)readHandle
                              into:(NSMutableData*)dataOut
                             named:(NSString*)streamName {
  dispatch_group_enter(_done_group);
  dispatch_io_t stdoutIO = dispatch_io_create(
      DISPATCH_IO_STREAM, readHandle.fileDescriptor, _privateQueue,
      ^(int unused) {
        NSError* cleanupError = nil;
        NSAssert([readHandle closeAndReturnError:&cleanupError],
                 @"couldn't close task %@: %@", streamName, cleanupError);
      });
  dispatch_io_read(
      stdoutIO, 0, SIZE_MAX, _privateQueue,
      ^(bool done, dispatch_data_t chunk, int error) {
        if (chunk) {
          // dispatch_data_t may be cast to NSData in 64-bit software:
          // https://developer.apple.com/documentation/dispatch/dispatch_data_t?language=objc
          [dataOut appendData:(NSData*)chunk];
        }
        if (done || error) {
          dispatch_io_close(stdoutIO, 0);
          if (error && !self->_taskManagementError) {
            self->_taskManagementError = [NSError
                errorWithDomain:CRURegistrationErrorDomain
                           code:CRURegistrationErrorTaskStreamUnreadable
                       userInfo:@{
                         CRUErrnoKey : @(error),
                         CRUStdStreamNameKey : streamName,
                       }];
          }
          dispatch_group_leave(self->_done_group);
        }
      });
}

@end  // CRUAsyncTaskRunner

#pragma mark - CRURegistrationWorkItem

@implementation CRURegistrationWorkItem

@synthesize binPathCallback = _binPathCallback;
@synthesize args = _args;
@synthesize resultCallback = _resultCallback;

@end

#pragma mark - CRURegistration

@implementation CRURegistration {
  // Immutable fields.
  NSString* _appId;
  NSString* _existenceCheckerPath;

  dispatch_queue_t _privateQueue;
  dispatch_queue_t _parentQueue;

  NSMutableArray<CRURegistrationWorkItem*>* _pendingWork;
  CRUAsyncTaskRunner* _currentWork;
}

- (instancetype)initWithAppId:(NSString*)appId
         existenceCheckerPath:(NSString*)xcPath
                  targetQueue:(dispatch_queue_t)targetQueue {
  if (self = [super init]) {
    _appId = appId;
    _existenceCheckerPath = xcPath;
    _parentQueue = targetQueue;
    _privateQueue = dispatch_queue_create_with_target(
        "CRURegistration", DISPATCH_QUEUE_SERIAL, targetQueue);
    _pendingWork = [NSMutableArray array];
  }
  return self;
}

- (instancetype)initWithAppId:(NSString*)appId
         existenceCheckerPath:(NSString*)xcPath
                          qos:(dispatch_qos_class_t)qos {
  return [self initWithAppId:appId
        existenceCheckerPath:xcPath
                 targetQueue:dispatch_get_global_queue(qos, 0)];
}

- (instancetype)initWithAppId:(NSString*)appId
         existenceCheckerPath:(NSString*)xcPath {
  return [self initWithAppId:appId
        existenceCheckerPath:xcPath
                         qos:QOS_CLASS_UTILITY];
}

/**
 * newKSAdminItem constructs a CRURegistrationWorkItem that will invoke ksadmin.
 */
- (CRURegistrationWorkItem*)newKSAdminItem {
  CRURegistrationWorkItem* ret = [[CRURegistrationWorkItem alloc] init];
  ret.binPathCallback = ^{
    return [self syncFindBestKSAdmin];
  };
  return ret;
}

- (void)fetchTagWithReply:(void (^)(NSString* _Nullable,
                                    NSError* _Nullable))reply {
  if (!reply) {
    return;
  }
  CRURegistrationWorkItem* fetchTagItem = [self newKSAdminItem];
  fetchTagItem.args = @[
    @"--print-tag",
    @"--productid",
    _appId,
    @"--xcpath",
    _existenceCheckerPath,
  ];
  fetchTagItem.resultCallback =
      ^(NSString* gotStdout, NSString* gotStderr, NSError* gotFailure) {
        if (gotFailure) {
          NSError* finalError = [self wrapError:gotFailure
                                     withStdout:gotStdout
                                      andStderr:gotStderr];
          dispatch_async(self->_parentQueue, ^{
            reply(nil, finalError);
          });
          return;
        }
        if (gotStdout.length) {
          // Trim off the trailing newline.
          NSString* tag = [gotStdout substringToIndex:gotStdout.length - 1];
          dispatch_async(self->_parentQueue, ^{
            reply(tag, nil);
          });
          return;
        }
        // Empty stdout implies "no tag".
        dispatch_async(self->_parentQueue, ^{
          reply(@"", nil);
        });
      };
  [self addWorkItems:@[ fetchTagItem ]];
}

- (void)registerVersion:(NSString*)version
                  reply:(void (^_Nullable)(NSError*))reply {
  NSAssert(version, @"nil version provided to registerVersion for app %@.",
           _appId);
  if (!version) {
    if (reply) {
      NSString* localAppId = _appId;
      dispatch_async(_parentQueue, ^{
        reply([NSError
            errorWithDomain:CRURegistrationErrorDomain
                       code:CRURegistrationErrorInvalidArgument
                   userInfo:@{
                     NSDebugDescriptionErrorKey :
                         [NSString stringWithFormat:
                                       @"CRURegistration's registerVersion for "
                                       @"app %@ was called with nil version.",
                                       localAppId],
                   }]);
      });
    }
    return;
  }

  CRURegistrationWorkItem* registerItem = [self newKSAdminItem];
  registerItem.args = @[
    @"--register",
    @"--productid",
    _appId,
    @"--version",
    version,
    @"--xcpath",
    _existenceCheckerPath,
  ];
  registerItem.resultCallback =
      ^(NSString* gotStdout, NSString* gotStderr, NSError* gotFailure) {
          if (reply) {
            dispatch_async(self->_parentQueue, ^{
              reply([self wrapError:gotFailure
                         withStdout:gotStdout
                          andStderr:gotStderr]);
            });
          }
      };

  [self addWorkItems:@[ registerItem ]];
}

- (void)installUpdaterWithReply:(void (^)(NSError* _Nullable))reply {
  CRURegistrationWorkItem* installItem = [[CRURegistrationWorkItem alloc] init];
  installItem.binPathCallback = ^{
    return [self syncBundledHelperPath];
  };
  installItem.args = @[ @"--install" ];
  installItem.resultCallback =
      ^(NSString* gotStdout, NSString* gotStderr, NSError* gotFailure) {
        if (reply) {
          dispatch_async(self->_parentQueue, ^{
            reply([self wrapError:gotFailure
                       withStdout:gotStdout
                        andStderr:gotStderr]);
          });
        }
      };
  [self addWorkItems:@[ installItem ]];
}

- (void)markActiveWithReply:(void (^)(NSError* _Nullable))reply {
  // "Mark active" doesn't use an external program, so it may run concurrently
  // with out-of-process tasks. It still uses _privateQueue so the SSD/HDD
  // access runs with the intended priority.
  dispatch_async(_privateQueue, ^{
    NSError* error;
    BOOL success = [self syncWriteActiveFileWithError:&error];
    if (!reply) {
      return;
    }
    dispatch_async(self->_parentQueue, ^{
      reply(success
                ? nil
                : [NSError errorWithDomain:CRURegistrationErrorDomain
                                      code:CRURegistrationErrorFilesystem
                                  userInfo:@{NSUnderlyingErrorKey : error}]);
    });
  });
}

#pragma mark - CRURegistration private methods

- (void)syncMaybeStartMoreWork {
  if (_currentWork || !_pendingWork.count) {
    return;
  }

  CRURegistrationWorkItem* nextItem = _pendingWork.firstObject;
  // NSMutableArray is actually a deque, so the obvious approach is performant.
  [_pendingWork removeObjectAtIndex:0];

  NSURL* taskURL = nextItem.binPathCallback();
  if (!taskURL) {
    dispatch_async(_parentQueue, ^{
      nextItem.resultCallback(
          nil, nil,
          [NSError errorWithDomain:CRURegistrationErrorDomain
                              code:CRURegistrationErrorHelperNotFound
                          userInfo:nil]);
    });
    [self syncMaybeStartMoreWork];
    return;
  }

  NSTask* task = [[NSTask alloc] init];
  task.executableURL = taskURL;
  task.arguments = nextItem.args;

  _currentWork = [[CRUAsyncTaskRunner alloc] initWithTask:task
                                              targetQueue:_privateQueue];
  [_currentWork
      launchWithReply:^(NSString* taskOut, NSString* taskErr, NSError* error) {
        self->_currentWork = nil;
        dispatch_async(self->_parentQueue, ^{
          nextItem.resultCallback(taskOut, taskErr, error);
        });
        [self syncMaybeStartMoreWork];
      }];
}

- (NSURL*)syncBundledHelperPath {
  NSURL* bundleURL = NSBundle.mainBundle.bundleURL;
  NSString* helperPathInBundle = [NSString
      stringWithFormat:@"Contents/Helpers/%1$s.app/Contents/MacOS/%1$s",
                       PRODUCT_FULLNAME_STRING];
  NSURL* helperURL = [bundleURL URLByAppendingPathComponent:helperPathInBundle
                                                isDirectory:NO];
  NSFileManager* fm = [NSFileManager defaultManager];
  if ([fm isExecutableFileAtPath:helperURL.path]) {
    return helperURL;
  }
  // Look for a test updater bundle instead.
  helperPathInBundle =
      [NSString stringWithFormat:
                    @"Contents/Helpers/%1$s_test.app/Contents/MacOS/%1$s_test",
                    PRODUCT_FULLNAME_STRING];
  helperURL = [bundleURL URLByAppendingPathComponent:helperPathInBundle
                                         isDirectory:NO];
  if ([fm isExecutableFileAtPath:helperURL.path]) {
    return helperURL;
  }
  return nil;
}

/**
 * Writes an empty file to the path the updater uses as a "product was active"
 * sentinel for the provided app ID. Stomps on any file already at this path.
 * Does not validate the app ID; app IDs are assumed not to be under user
 * control, so a malicious string here could theoretically be some kind of
 * weird directory traversal attack.
 */
- (BOOL)syncWriteActiveFileWithError:(NSError**)error {
  NSFileManager* fm = [NSFileManager defaultManager];
  NSURL* library = [fm URLForDirectory:NSLibraryDirectory
                              inDomain:NSUserDomainMask
                     appropriateForURL:nil
                                create:NO
                                 error:error];
  if (!library) {
    return NO;
  }
  NSString* activesPathUnderLibrary =
      [NSString stringWithFormat:@"%s/%s/Actives", COMPANY_SHORTNAME_STRING,
                                 KEYSTONE_NAME];
  NSURL* activesPath =
      [library URLByAppendingPathComponent:activesPathUnderLibrary
                               isDirectory:YES];
  if (![fm createDirectoryAtURL:activesPath
          withIntermediateDirectories:YES
                           attributes:nil
                                error:error]) {
    return NO;
  }
  NSURL* target = [activesPath URLByAppendingPathComponent:_appId
                                               isDirectory:NO];
  return [@"" writeToFile:target.path
               atomically:NO
                 encoding:NSUTF8StringEncoding
                    error:error];
}

@end

#pragma mark - CRURegistration (VisibleForTesting)

@implementation CRURegistration (VisibleForTesting)

- (void)addWorkItems:(NSArray<CRURegistrationWorkItem*>*)items {
  dispatch_async(_privateQueue, ^{
    [self->_pendingWork addObjectsFromArray:items];
    [self syncMaybeStartMoreWork];
  });
}

- (NSURL*)syncFindBestKSAdmin {
  NSFileManager* fm = [NSFileManager defaultManager];
  NSArray<NSURL*>* libraries =
      [fm URLsForDirectory:NSLibraryDirectory
                 inDomains:NSUserDomainMask | NSLocalDomainMask];
  NSString* ksadminPathUnderLibrary = [NSString
      stringWithFormat:@"%s/%s/%s.bundle/Contents/Helpers/ksadmin",
                       COMPANY_SHORTNAME_STRING, KEYSTONE_NAME, KEYSTONE_NAME];
  // URLsForDirectory returns paths in ascending order of domain mask values.
  // To match Keystone's behavior, we prefer local domain (machine install) over
  // user domain; local domain has the higher numerical value, so we test
  // these in reverse order.
  for (NSURL* library in libraries.reverseObjectEnumerator) {
    NSURL* candidate =
        [library URLByAppendingPathComponent:ksadminPathUnderLibrary
                                 isDirectory:NO];
    if ([fm isExecutableFileAtPath:candidate.path]) {
      return candidate;
    }
  }
  return nil;
}

- (NSError*)wrapError:(NSError*)error
           withStdout:(NSString*)gotStdout
            andStderr:(NSString*)gotStderr {
  if (!error) {
    return nil;
  }

  // Check for errors already ready for user presentation.
  if ([error.domain isEqual:CRURegistrationErrorDomain] ||
      [error.domain isEqual:CRURegistrationInternalErrorDomain]) {
    return error;
  }

  // We're going to need to wrap this error. Start with common error info.
  NSMutableDictionary* userInfo = [NSMutableDictionary
      dictionaryWithDictionary:@{NSUnderlyingErrorKey : error}];
  if (gotStdout) {
    userInfo[CRUStdoutKey] = gotStdout;
  }
  if (gotStderr) {
    userInfo[CRUStderrKey] = gotStderr;
  }
  id maybeFilePath = error.userInfo[NSFilePathErrorKey];
  if (maybeFilePath) {
    userInfo[NSFilePathErrorKey] = maybeFilePath;
  }
  id maybeURL = error.userInfo[NSURLErrorKey];
  if (maybeURL) {
    userInfo[NSURLErrorKey] = maybeURL;
  }

  // Check for helper task failure.
  if ([error.domain isEqual:CRUReturnCodeErrorDomain]) {
    userInfo[CRUReturnCodeKey] = @(error.code);
    return [NSError errorWithDomain:CRURegistrationErrorDomain
                               code:CRURegistrationErrorTaskFailed
                           userInfo:userInfo];
  }

  // Check for errors reported by NSTask if it cannot find the task, or the file
  // specified is not executable. NSTask returns the same error code for both.
  // This NSTask behavior was determined experimentally -- Apple does not
  // document the errors that NSTask can emit -- so promoting this to a
  // HelperNotFound error should be considered "best-efort".
  if ([error.domain isEqual:NSCocoaErrorDomain] &&
      error.code == NSFileNoSuchFileError) {
    return [NSError errorWithDomain:CRURegistrationErrorDomain
                               code:CRURegistrationErrorHelperNotFound
                           userInfo:userInfo];
  }

  // Unrecognized error.
  return [NSError errorWithDomain:CRURegistrationInternalErrorDomain
                             code:CRURegistrationInternalErrorUnrecognized
                         userInfo:userInfo];
}

@end
