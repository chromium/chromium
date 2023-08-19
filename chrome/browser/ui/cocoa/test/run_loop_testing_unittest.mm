// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/test/run_loop_testing.h"

#import <Foundation/Foundation.h>

#include "testing/gtest/include/gtest/gtest.h"

@interface TestDelayed : NSObject
@property(readonly, nonatomic) BOOL didWork;
@property(strong, nonatomic) TestDelayed* next;
@end

@implementation TestDelayed
@synthesize didWork = _didWork;
@synthesize next = _next;

- (instancetype)init {
  if ((self = [super init])) {
    [self performSelector:@selector(doWork) withObject:nil afterDelay:0];
  }
  return self;
}

- (void)doWork {
  _didWork = YES;
  [_next performSelector:@selector(doWork) withObject:nil afterDelay:0];
}
@end

TEST(RunLoopTesting, RunAllPending) {
  TestDelayed* tester = [[TestDelayed alloc] init];
  EXPECT_FALSE([tester didWork]);

  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_TRUE([tester didWork]);
}

TEST(RunLoopTesting, NestedWork) {
  TestDelayed* tester = [[TestDelayed alloc] init];
  TestDelayed* nested = [[TestDelayed alloc] init];
  [tester setNext:nested];

  EXPECT_FALSE([tester didWork]);
  EXPECT_FALSE([nested didWork]);

  chrome::testing::NSRunLoopRunAllPending();

  EXPECT_TRUE([tester didWork]);
  EXPECT_TRUE([nested didWork]);
}
