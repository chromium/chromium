// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/client_lib/CRURegistration.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include "base/apple/foundation_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/scoped_generic.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#import "chrome/updater/mac/client_lib/CRURegistration-Private.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

// Work queue item callbacks get called on a thread that can't make test
// assertions or expectations, so we need a type to store partial results from
// item callbacks during asynchronous work queue tests, for subsequent
// evaluation on the test thread. Making this an Objective-C type (which must be
// outside of any namespace) makes the test much easier to write, both
// syntactically and due to ARC's advantages in avoiding use-after-free bugs
// via use of __block storage duration.
@interface CRUWorkQueueTestObservation : NSObject
@property(nonatomic) int itemId;
@property(nonatomic, copy) NSString* taskStdOut;
@property(nonatomic, copy) NSString* taskStdErr;
@property(nonatomic, copy) NSError* taskNSErr;
@end

@implementation CRUWorkQueueTestObservation
@synthesize itemId = _itemId;
@synthesize taskStdOut = _taskStdOut;
@synthesize taskStdErr = _taskStdErr;
@synthesize taskNSErr = _taskNSErr;
@end

namespace {

constexpr char kEmitTextTestBinaryName[] = "emit_text";

TEST(CRURegistrationTest, SmokeTest) {
  CRURegistration* registration = [[CRURegistration alloc]
             initWithAppId:
                 @"org.chromium.ChromiumUpdater.CRURegistrationTest.SmokeTest"
      existenceCheckerPath:@"IGNORED"];
  ASSERT_TRUE(registration);
}

enum class EmitTextOutputTarget {
  kOut,
  kErr,
  kBoth,
};

class CRUAsyncTaskRunnerTest : public ::testing::Test {
 protected:
  void SetUp() override;

  bool RunEmitText(NSString* text, int repeats, EmitTextOutputTarget mode);

  NSURL* emit_text_nsurl_ = nil;
  dispatch_queue_t queue_ = nil;

  NSString* got_stdout_ = nil;
  NSString* got_stderr_ = nil;
  NSError* got_error_ = nil;
};

void GetEmitTextNSURL(NSURL** result) {
  ASSERT_TRUE(result);
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_data_path));
  base::FilePath emit_text_path =
      test_data_path.AppendASCII(kEmitTextTestBinaryName);
  ASSERT_TRUE(base::PathExists(emit_text_path))
      << "cannot find: " << emit_text_path;
  *result = base::apple::FilePathToNSURL(emit_text_path);
  ASSERT_TRUE(*result);
}

void CRUAsyncTaskRunnerTest::SetUp() {
  queue_ = dispatch_queue_create_with_target(
      "CRUAsyncTaskRunnerTestBlankOutput", DISPATCH_QUEUE_SERIAL,
      dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
  NSURL* emit_text_nsurl = nil;
  ASSERT_NO_FATAL_FAILURE(GetEmitTextNSURL(&emit_text_nsurl));
  emit_text_nsurl_ = emit_text_nsurl;
}

bool CRUAsyncTaskRunnerTest::RunEmitText(NSString* text,
                                         int repeats,
                                         EmitTextOutputTarget mode) {
  NSTask* task = [[NSTask alloc] init];
  task.executableURL = emit_text_nsurl_;
  NSMutableArray<NSString*>* args = [NSMutableArray arrayWithArray:@[
    [NSString stringWithFormat:@"--text=%@", text],
    [NSString stringWithFormat:@"--count=%d", repeats],
  ]];
  switch (mode) {
    case EmitTextOutputTarget::kBoth:
      [args addObject:@"--stdout"];
      [[fallthrough]];
    case EmitTextOutputTarget::kErr:
      [args addObject:@"--stderr"];
      break;
    case EmitTextOutputTarget::kOut:
      // Stdout-only output is the default.
      break;
  }

  task.arguments = [args copy];

  CRUAsyncTaskRunner* runner = [[CRUAsyncTaskRunner alloc] initWithTask:task
                                                            targetQueue:queue_];

  // Capture task results into ephemeral fields guarded by a lock, copying them
  // into the corresponding fields of `self` if we can acquire the lock.
  // Tests can subsequently use the instance fields without further locking.
  NSConditionLock* results_lock = [[NSConditionLock alloc] initWithCondition:0];
  __block NSString* got_stdout;
  __block NSString* got_stderr;
  __block NSError* got_error;

  [runner launchWithReply:^(NSString* task_out, NSString* task_err,
                            NSError* error) {
    [results_lock lock];
    got_stdout = task_out;
    got_stderr = task_err;
    got_error = error;
    [results_lock unlockWithCondition:1];
  }];

  if (![results_lock
          lockWhenCondition:1
                 beforeDate:[NSDate dateWithTimeIntervalSinceNow:15.0]]) {
    // Timed out; can't read any of the fields, we have no sync relationship
    // with them.
    return false;
  }
  absl::Cleanup result_unlocker = ^{
    [results_lock unlock];
  };

  got_stdout_ = got_stdout;
  got_stderr_ = got_stderr;
  got_error_ = got_error;
  return true;
}

TEST_F(CRUAsyncTaskRunnerTest, BlankOutput) {
  ASSERT_TRUE(RunEmitText(@"invisible", 0, EmitTextOutputTarget::kBoth));
  EXPECT_FALSE(got_error_);
  EXPECT_TRUE(got_stdout_);
  EXPECT_TRUE(got_stderr_);
  EXPECT_EQ(got_stdout_.length, 0U);
  EXPECT_EQ(got_stderr_.length, 0U);
}

TEST_F(CRUAsyncTaskRunnerTest, ShortStdout) {
  ASSERT_TRUE(RunEmitText(@"output", 2, EmitTextOutputTarget::kOut));
  EXPECT_FALSE(got_error_);
  EXPECT_TRUE(got_stdout_);
  EXPECT_TRUE(got_stderr_);
  EXPECT_EQ(got_stderr_.length, 0U);
  EXPECT_NSEQ(@"outputoutput", got_stdout_);
}

TEST_F(CRUAsyncTaskRunnerTest, ShortStderr) {
  ASSERT_TRUE(RunEmitText(@"output", 2, EmitTextOutputTarget::kErr));
  EXPECT_FALSE(got_error_);
  EXPECT_TRUE(got_stdout_);
  EXPECT_TRUE(got_stderr_);
  EXPECT_EQ(got_stdout_.length, 0U);
  EXPECT_NSEQ(@"outputoutput", got_stderr_);
}

TEST_F(CRUAsyncTaskRunnerTest, ShortBoth) {
  ASSERT_TRUE(RunEmitText(@"output", 2, EmitTextOutputTarget::kBoth));
  EXPECT_FALSE(got_error_);
  EXPECT_TRUE(got_stdout_);
  EXPECT_TRUE(got_stderr_);
  EXPECT_NSEQ(@"outputoutput", got_stdout_);
  EXPECT_NSEQ(@"outputoutput", got_stderr_);
}

TEST_F(CRUAsyncTaskRunnerTest, LongBoth) {
  // Construct output longer than any likely default buffer size, but small
  // enough to be practical to use in this test. emit_text flushes its output
  // streams every iteration specifically to make buffer size irrelevant, but
  // using a large total is still useful in case unexpected helpful buffering on
  // the intake side hides incorrect asynchronous stream consumption.
  //
  // Currently, this creates 16 MiB of output.
  const NSUInteger reps = 1048576;
  NSString* const text = @"0123456789ABCDEF";
  ASSERT_TRUE(RunEmitText(text, (int)reps, EmitTextOutputTarget::kBoth));
  EXPECT_FALSE(got_error_);
  EXPECT_TRUE(got_stdout_);
  EXPECT_TRUE(got_stderr_);

  // Construct the expectation string.
  NSMutableArray<NSString*>* chunks = [NSMutableArray arrayWithCapacity:reps];
  for (NSUInteger k = 0; k < reps; k++) {
    [chunks addObject:text];
  }
  NSString* want = [chunks componentsJoinedByString:@""];

  EXPECT_NSEQ(want, got_stdout_);
  EXPECT_NSEQ(want, got_stderr_);
}

TEST_F(CRUAsyncTaskRunnerTest, NonzeroReturn) {
  // `emit_text` returns ERANGE, value 34, if given a negative number of
  // repetitions. This allows testing nonzero return code handling.
  ASSERT_TRUE(RunEmitText(@"error", -1, EmitTextOutputTarget::kBoth));
  EXPECT_TRUE(got_error_);
  EXPECT_TRUE(got_stdout_);
  EXPECT_TRUE(got_stderr_);
  EXPECT_EQ(got_stdout_.length, 0U);
  EXPECT_EQ(got_stderr_.length, 0U);
  EXPECT_NSEQ(CRUReturnCodeErrorDomain, got_error_.domain);
  EXPECT_EQ((NSInteger)34, got_error_.code);
}

TEST_F(CRUAsyncTaskRunnerTest, TaskFailureErrorWrapping) {
  CRURegistration* registration = [[CRURegistration alloc]
             initWithAppId:@"org.chromium.ChromiumUpdater.CRURegistrationTest."
                           @"TaskFailureErrorWrapping"
      existenceCheckerPath:@"IGNORED"];

  ASSERT_TRUE(RunEmitText(@"error", -1, EmitTextOutputTarget::kBoth));
  NSError* wrapped = [registration wrapError:got_error_
                                  withStdout:got_stdout_
                                   andStderr:got_stderr_];
  EXPECT_NSEQ(CRURegistrationErrorDomain, wrapped.domain);
  EXPECT_EQ(CRURegistrationErrorTaskFailed, wrapped.code);
  EXPECT_NSEQ(@"", wrapped.userInfo[CRUStdoutKey]);
  EXPECT_NSEQ(@"", wrapped.userInfo[CRUStderrKey]);
  NSError* underlying = wrapped.userInfo[NSUnderlyingErrorKey];
  ASSERT_TRUE([underlying isKindOfClass:[NSError class]]);
  EXPECT_NSEQ(CRUReturnCodeErrorDomain, underlying.domain);
}

void TestWorkQueueImpl(int item_count) {
  dispatch_queue_t queue = dispatch_queue_create_with_target(
      "TestWorkQueueImpl", DISPATCH_QUEUE_SERIAL,
      dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));

  CRURegistration* registration = [[CRURegistration alloc]
             initWithAppId:@"org.chromium.ChromiumUpdater.CRURegistrationTest."
                           @"WorkQueueTest"
      existenceCheckerPath:@"IGNORED"
               targetQueue:queue];

  NSURL* emit_text_nsurl = nil;
  ASSERT_NO_FATAL_FAILURE(GetEmitTextNSURL(&emit_text_nsurl));

  NSConditionLock* result_order_lock =
      [[NSConditionLock alloc] initWithCondition:0];
  NSMutableArray<CRURegistrationWorkItem*>* items = [NSMutableArray array];
  NSMutableArray<CRUWorkQueueTestObservation*>* observations =
      [NSMutableArray array];

  for (int i = 0; i < item_count; ++i) {
    CRURegistrationWorkItem* item = [[CRURegistrationWorkItem alloc] init];
    item.binPathCallback = ^NSURL* {
      return emit_text_nsurl;
    };
    item.args = @[ [NSString stringWithFormat:@"--text=%d", i] ];
    const int captured_i = i;
    item.resultCallback =
        ^(NSString* task_stdout, NSString* task_stderr, NSError* task_nserr) {
          [result_order_lock lock];
          NSInteger prev_cond = result_order_lock.condition;
          CRUWorkQueueTestObservation* observation =
              [[CRUWorkQueueTestObservation alloc] init];
          observation.itemId = captured_i;
          observation.taskStdOut = task_stdout;
          observation.taskStdErr = task_stderr;
          observation.taskNSErr = task_nserr;
          [observations addObject:observation];
          [result_order_lock unlockWithCondition:prev_cond + 1];
        };
    [items addObject:item];
  }

  [registration addWorkItems:items];

  ASSERT_TRUE([result_order_lock
      lockWhenCondition:(NSInteger)item_count
             beforeDate:[NSDate dateWithTimeIntervalSinceNow:10.0]]);
  absl::Cleanup result_unlocker = ^{
    [result_order_lock unlock];
  };

  ASSERT_EQ((size_t)item_count, observations.count);
  for (int i = 0; i < item_count; ++i) {
    CRUWorkQueueTestObservation* observation = observations[i];
    EXPECT_EQ(i, observation.itemId);
    NSString* expected = [NSString stringWithFormat:@"%d", observation.itemId];
    EXPECT_NSEQ(observation.taskStdOut, expected)
        << "wrong stdout in position " << i << ", item " << observation.itemId;
    EXPECT_NSEQ(observation.taskStdErr, @"")
        << "nonempty stderr in position " << i << ", item "
        << observation.itemId;
    EXPECT_FALSE(observation.taskNSErr)
        << "in position " << i << ", item " << observation.itemId
        << " had error: "
        << base::SysNSStringToUTF8([observation.taskNSErr description]);
  }
}

TEST(CRURegistrationTest, WorkQueueOneItem) {
  ASSERT_NO_FATAL_FAILURE(TestWorkQueueImpl(1));
}

TEST(CRURegistrationTest, WorkQueueThreeItems) {
  ASSERT_NO_FATAL_FAILURE(TestWorkQueueImpl(3));
}

TEST(CRURegistrationTest, WrapNoError) {
  CRURegistration* registration = [[CRURegistration alloc]
             initWithAppId:
                 @"org.chromium.ChromiumUpdater.CRURegistrationTest.WrapError"
      existenceCheckerPath:@"IGNORED"];

  EXPECT_FALSE([registration wrapError:nil withStdout:nil andStderr:nil]);
  EXPECT_FALSE([registration wrapError:nil
                            withStdout:@"irrelevant"
                             andStderr:@"irrelevant"]);
  EXPECT_FALSE([registration wrapError:nil
                            withStdout:nil
                             andStderr:@"irrelevant"]);
  EXPECT_FALSE([registration wrapError:nil
                            withStdout:@"irrelevant"
                             andStderr:@"nil"]);
}

// Verify that `wrapError:withStdout:andStderr:` correctly identifies an NSTask
// error from attempting to run a nonexistent file as "helper not found".
TEST(CRURegistrationTest, WrapMissingTaskTargetError) {
  CRURegistration* registration = [[CRURegistration alloc]
             initWithAppId:
                 @"org.chromium.ChromiumUpdater.CRURegistrationTest.WrapError"
      existenceCheckerPath:@"IGNORED"];

  // Create an NSTask configured to execute a file that does not exist and
  // capture the error that ensues from running it.
  base::ScopedTempDir empty_dir;
  ASSERT_TRUE(empty_dir.CreateUniqueTempDir());
  NSURL* nonexistent_path = base::apple::FilePathToNSURL(
      empty_dir.GetPath().AppendASCII("nonexistent"));
  NSTask* will_fail = [[NSTask alloc] init];
  will_fail.executableURL = nonexistent_path;
  NSError* bad_task_error;
  ASSERT_FALSE([will_fail launchAndReturnError:&bad_task_error]);
  ASSERT_TRUE(bad_task_error);

  NSError* wrapped_error = [registration wrapError:bad_task_error
                                        withStdout:nil
                                         andStderr:nil];
  EXPECT_TRUE(wrapped_error);
  EXPECT_NSEQ(wrapped_error.domain, CRURegistrationErrorDomain);
  EXPECT_EQ(wrapped_error.code, CRURegistrationErrorHelperNotFound);
}

TEST(CRURegistrationTest, WrapErrorUnchanged) {
  CRURegistration* registration = [[CRURegistration alloc]
             initWithAppId:
                 @"org.chromium.ChromiumUpdater.CRURegistrationTest.WrapError"
      existenceCheckerPath:@"IGNORED"];

  NSError* registration_error =
      [NSError errorWithDomain:CRURegistrationErrorDomain
                          code:CRURegistrationErrorTaskFailed
                      userInfo:@{@"test-key" : @"test-value"}];
  NSError* wrapped_reg_error = [registration wrapError:registration_error
                                            withStdout:@"discarded"
                                             andStderr:@"discarded"];
  EXPECT_NSEQ(wrapped_reg_error.domain, CRURegistrationErrorDomain);
  EXPECT_EQ(wrapped_reg_error.code, CRURegistrationErrorTaskFailed);
  EXPECT_NSEQ(wrapped_reg_error.userInfo[@"test-key"], @"test-value");
  EXPECT_FALSE(wrapped_reg_error.userInfo[CRUStdoutKey]);
  EXPECT_FALSE(wrapped_reg_error.userInfo[CRUStderrKey]);
  EXPECT_FALSE(wrapped_reg_error.userInfo[NSUnderlyingErrorKey]);

  NSError* internal_error =
      [NSError errorWithDomain:CRURegistrationInternalErrorDomain
                          code:CRURegistrationInternalErrorUnrecognized
                      userInfo:@{@"test-key" : @"test-value"}];
  NSError* wrapped_internal_error = [registration wrapError:internal_error
                                                 withStdout:@"discarded"
                                                  andStderr:@"discarded"];
  EXPECT_NSEQ(wrapped_internal_error.domain,
              CRURegistrationInternalErrorDomain);
  EXPECT_EQ(wrapped_internal_error.code,
            CRURegistrationInternalErrorUnrecognized);
  EXPECT_NSEQ(wrapped_internal_error.userInfo[@"test-key"], @"test-value");
  EXPECT_FALSE(wrapped_internal_error.userInfo[CRUStdoutKey]);
  EXPECT_FALSE(wrapped_internal_error.userInfo[CRUStderrKey]);
  EXPECT_FALSE(wrapped_internal_error.userInfo[NSUnderlyingErrorKey]);
}

}  // namespace
