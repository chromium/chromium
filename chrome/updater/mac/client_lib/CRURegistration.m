// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "CRURegistration.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

@implementation CRURegistration {
  // Immutable fields.
  NSString* _appId;

  dispatch_queue_t _privateQueue;
  dispatch_queue_t _parentQueue;
}

- (instancetype)initWithAppId:(NSString*)appId
                  targetQueue:(dispatch_queue_t)targetQueue {
  if (self = [super init]) {
    _appId = appId;
    _parentQueue = targetQueue;
    _privateQueue = dispatch_queue_create_with_target(
        "CRURegistration", DISPATCH_QUEUE_SERIAL, targetQueue);
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

@end
