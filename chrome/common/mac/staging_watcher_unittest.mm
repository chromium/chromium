// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/mac/staging_watcher.h"

#include <dispatch/dispatch.h>

#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

enum class KVOOrNot { kUseKVO, kDontUseKVO };

class StagingKeyWatcherTest : public testing::TestWithParam<KVOOrNot> {
 public:
  StagingKeyWatcherTest() = default;
  ~StagingKeyWatcherTest() = default;

 protected:
  void SetUp() override {
    testingBundleID_.reset([[NSString alloc]
        initWithFormat:@"org.chromium.StagingKeyWatcherTest.%d", getpid()]);
    defaults_.reset(
        [[NSUserDefaults alloc] initWithSuiteName:testingBundleID_]);

    [defaults_ removeObjectForKey:[CrStagingKeyWatcher stagingKeyForTesting]];
  }

  void TearDown() override {
    [defaults_ removeObjectForKey:[CrStagingKeyWatcher stagingKeyForTesting]];
  }

  base::scoped_nsobject<CrStagingKeyWatcher> CreateKeyWatcher() {
    base::scoped_nsobject<CrStagingKeyWatcher> keyWatcher(
        [[CrStagingKeyWatcher alloc]
            initWithUserDefaults:defaults_
                     pollingTime:0.5
            disableKVOForTesting:(GetParam() == KVOOrNot::kDontUseKVO)]);

    return keyWatcher;
  }

  void SetDefaultsValue(id value) {
    [defaults_ setObject:value
                  forKey:[CrStagingKeyWatcher stagingKeyForTesting]];
  }

  void ClearDefaultsValueInSeparateProcess() {
    [NSTask launchedTaskWithLaunchPath:@"/usr/bin/defaults"
                             arguments:@[
                               @"delete", testingBundleID_.get(),
                               [CrStagingKeyWatcher stagingKeyForTesting]
                             ]];
  }

  void SetDefaultsValueInSeparateProcess() {
    NSString* appPath = [base::mac::OuterBundle() bundlePath];

    [NSTask launchedTaskWithLaunchPath:@"/usr/bin/defaults"
                             arguments:@[
                               @"write", testingBundleID_.get(),
                               [CrStagingKeyWatcher stagingKeyForTesting],
                               @"-dict", appPath, appPath
                             ]];
  }

 private:
  base::scoped_nsobject<NSString> testingBundleID_;
  base::scoped_nsobject<NSUserDefaults> defaults_;
};

INSTANTIATE_TEST_SUITE_P(KVOandNot,
                         StagingKeyWatcherTest,
                         testing::Values(KVOOrNot::kUseKVO,
                                         KVOOrNot::kDontUseKVO));

}  // namespace

TEST_P(StagingKeyWatcherTest, NoBlockingWhenNoKey) {
  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  [watcher waitForStagingKeyToClear];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenWrongKeyType) {
  SetDefaultsValue(@"this is not a dictionary");

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  [watcher waitForStagingKeyToClear];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenArrayType) {
  SetDefaultsValue(@[ @3, @1, @4, @1, @5 ]);

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  [watcher waitForStagingKeyToClear];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenEmptyArray) {
  SetDefaultsValue(@[]);

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  [watcher waitForStagingKeyToClear];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, NoBlockingWhenEmptyDictionary) {
  SetDefaultsValue(@{});

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  [watcher waitForStagingKeyToClear];
  ASSERT_FALSE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, BlockFunctionality) {
  NSString* appPath = [base::mac::OuterBundle() bundlePath];
  SetDefaultsValue(@{appPath : appPath});

  NSRunLoop* runloop = [NSRunLoop currentRunLoop];
  ASSERT_EQ(nil, [runloop currentMode]);

  dispatch_async(dispatch_get_main_queue(), ^{
    ASSERT_NE(nil, [runloop currentMode]);
    ClearDefaultsValueInSeparateProcess();
  });

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  [watcher waitForStagingKeyToClear];
  ASSERT_TRUE([watcher lastWaitWasBlockedForTesting]);
}

TEST_P(StagingKeyWatcherTest, CallbackOnKeySet) {
  // The staging key begins not set.

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  NSRunLoop* runloop = [NSRunLoop currentRunLoop];

  __block bool observerCalled = false;
  [watcher setStagingKeyChangedObserver:^(BOOL stagingKeySet) {
    observerCalled = true;
    CFRunLoopStop([runloop getCFRunLoop]);
  }];

  SetDefaultsValueInSeparateProcess();
  ASSERT_FALSE([watcher isStagingKeySet]);
  while (!observerCalled && [runloop runMode:NSDefaultRunLoopMode
                                  beforeDate:[NSDate distantFuture]]) {
    /* run! */
  }

  EXPECT_TRUE([watcher isStagingKeySet]);
}

TEST_P(StagingKeyWatcherTest, CallbackOnKeyUnset) {
  NSString* appPath = [base::mac::OuterBundle() bundlePath];
  SetDefaultsValue(@{appPath : appPath});

  base::scoped_nsobject<CrStagingKeyWatcher> watcher = CreateKeyWatcher();
  NSRunLoop* runloop = [NSRunLoop currentRunLoop];

  __block bool observerCalled = false;
  [watcher setStagingKeyChangedObserver:^(BOOL stagingKeySet) {
    observerCalled = true;
    CFRunLoopStop([runloop getCFRunLoop]);
  }];

  ClearDefaultsValueInSeparateProcess();
  ASSERT_TRUE([watcher isStagingKeySet]);
  while (!observerCalled && [runloop runMode:NSDefaultRunLoopMode
                                  beforeDate:[NSDate distantFuture]]) {
    /* run! */
  }

  EXPECT_FALSE([watcher isStagingKeySet]);
}
