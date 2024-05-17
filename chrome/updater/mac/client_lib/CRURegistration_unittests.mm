// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/mac/client_lib/CRURegistration.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/scoped_generic.h"
#include "build/build_config.h"
#import "chrome/updater/mac/client_lib/CRURegistration-Private.h"
#include "net/base/apple/url_conversions.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

namespace {

constexpr char kEmitTextTestBinaryName[] = "emit_text";

TEST(CRURegistrationTest, SmokeTest) {
  CRURegistration* registration = [[CRURegistration alloc]
      initWithAppId:
          @"org.chromium.ChromiumUpdater.CRURegistrationTest.SmokeTest"];
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

void CRUAsyncTaskRunnerTest::SetUp() {
  queue_ = dispatch_queue_create_with_target(
      "CRUAsyncTaskRunnerTestBlankOutput", DISPATCH_QUEUE_SERIAL,
      dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0));
  base::FilePath test_data_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &test_data_path));
  base::FilePath emit_text_path =
      test_data_path.AppendASCII(kEmitTextTestBinaryName);
  ASSERT_TRUE(base::PathExists(emit_text_path))
      << "cannot find: " << emit_text_path;
  GURL emit_text_gurl = net::FilePathToFileURL(emit_text_path);
  emit_text_nsurl_ = net::NSURLWithGURL(emit_text_gurl);
  ASSERT_TRUE(emit_text_nsurl_);
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
  __block NSData* got_stdout;
  __block NSData* got_stderr;
  __block NSError* got_error;

  [runner
      launchWithReply:^(NSData* task_out, NSData* task_err, NSError* error) {
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

  got_stdout_ = got_stdout
                    ? [[NSString alloc] initWithData:got_stdout
                                            encoding:NSUTF8StringEncoding]
                    : nil;
  got_stderr_ = got_stderr
                    ? [[NSString alloc] initWithData:got_stderr
                                            encoding:NSUTF8StringEncoding]
                    : nil;
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

}  // namespace
