// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "CRURegistration.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#import "CRURegistration-Private.h"

#pragma mark - Constants

NSString* const CRURegistrationErrorDomain = @"org.chromium.CRURegistration";
NSString* const CRUReturnCodeErrorDomain = @"org.chromium.CRUReturnCode";
NSString* const CRURegistrationInternalErrorDomain =
    @"org.chromium.CRURegistrationInternal";

typedef NS_ERROR_ENUM(CRURegistrationInternalErrorDomain,
                      CRURegistrationInternalError){
    CRURegistrationInternalErrorTaskAlreadyLaunched = 1,
};

// Keys that may be present in NSError `userInfo` dictionaries.
NSString* const CRUErrnoKey = @"org.chromium.CRUErrno";
NSString* const CRUStdStreamNameKey = @"org.chromium.CRUStdStreamName";

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

  dispatch_queue_t _privateQueue;
  dispatch_queue_t _parentQueue;

  NSMutableArray<CRURegistrationWorkItem*>* _pendingWork;
  CRUAsyncTaskRunner* _currentWork;
}

- (instancetype)initWithAppId:(NSString*)appId
                  targetQueue:(dispatch_queue_t)targetQueue {
  if (self = [super init]) {
    _appId = appId;
    _parentQueue = targetQueue;
    _privateQueue = dispatch_queue_create_with_target(
        "CRURegistration", DISPATCH_QUEUE_SERIAL, targetQueue);
    _pendingWork = [NSMutableArray array];
  }
  return self;
}

- (instancetype)initWithAppId:(NSString*)appId qos:(dispatch_qos_class_t)qos {
  return [self initWithAppId:appId
                 targetQueue:dispatch_get_global_queue(qos, 0)];
}

- (instancetype)initWithAppId:(NSString*)appId {
  return [self initWithAppId:appId qos:QOS_CLASS_UTILITY];
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

@end

#pragma mark - CRURegistration (VisibleForTesting)

@implementation CRURegistration (VisibleForTesting)

- (void)addWorkItems:(NSArray<CRURegistrationWorkItem*>*)items {
  dispatch_async(_privateQueue, ^{
    [self->_pendingWork addObjectsFromArray:items];
    [self syncMaybeStartMoreWork];
  });
}

@end
