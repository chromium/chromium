// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/windows_services/elevated_tracing_service/process_watcher.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/windows_services/elevated_tracing_service/with_child_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace elevated_tracing_service {

// A test fixture for ProcessWatcher that has facilities for coordinating with a
// child process.
using ProcessWatcherTest = WithChildTest;

// Tests that ProcessWatcher runs its callback when the watched process
// terminates.
TEST_F(ProcessWatcherTest, ProcessExitsFirst) {
  base::RunLoop run_loop;
  ::testing::StrictMock<base::MockOnceClosure> mock_closure;
  EXPECT_CALL(mock_closure, Run())
      .WillOnce(base::test::RunOnceClosure(run_loop.QuitClosure()));

  // Spin off a child process and start watching it.
  auto child_process = SpawnChildWithEventHandles(kExitWhenSignaled);
  ASSERT_TRUE(child_process.IsValid());
  ProcessWatcher watcher(std::move(child_process), mock_closure.Get());

  // Signal the child to terminate.
  SignalChildTermination();

  // Wait for the watcher to notice that the child has terminated.
  run_loop.Run();
}

// Tests that ProcessWatcher doesn't run its callback if it is destroyed before
// the watched process terminates.
TEST_F(ProcessWatcherTest, WatcherDiesFirst) {
  // Start the child process and wait for it to get going.
  auto child_process = SpawnChildWithEventHandles(kExitWhenSignaled);
  ASSERT_TRUE(child_process.IsValid());
  WaitForChildStart();

  // Start and then stop watching the child; expecting the callback to not run.
  ::testing::StrictMock<base::MockOnceClosure> mock_closure;
  ProcessWatcher(std::move(child_process), mock_closure.Get());

  // Signal the child to terminate.
  SignalChildTermination();
}

}  // namespace elevated_tracing_service
