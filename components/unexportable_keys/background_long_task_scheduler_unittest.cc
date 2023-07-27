// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_long_task_scheduler.h"

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/unexportable_keys/background_task_impl.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

// Data shared between all tasks on the background thread.
struct BackgroundThreadData {
  size_t task_count = 0;
};

// FakeTask returns how many tasks has been executed on the background thread
// including the current one, at the moment of the task running.
class FakeTask : public internal::BackgroundTaskImpl<size_t> {
 public:
  explicit FakeTask(BackgroundThreadData& background_data,
                    BackgroundTaskPriority priority,
                    base::OnceCallback<void(size_t)> callback)
      : internal::BackgroundTaskImpl<size_t>(
            base::BindLambdaForTesting(
                [&background_data]() { return ++background_data.task_count; }),
            std::move(callback),
            priority) {}
};

class BackgroundLongTaskSchedulerTest : public testing::Test {
 public:
  BackgroundLongTaskSchedulerTest()
      : background_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({})) {}
  ~BackgroundLongTaskSchedulerTest() override = default;

  void RunAllBackgroundTasks() { task_environment_.RunUntilIdle(); }

  BackgroundLongTaskScheduler& scheduler() { return scheduler_; }

  BackgroundThreadData& background_data() { return background_data_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED};  // QUEUED - tasks don't run until `RunUntilIdle()` is
                    // called.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  BackgroundLongTaskScheduler scheduler_{background_task_runner_};
  BackgroundThreadData background_data_;
};

TEST_F(BackgroundLongTaskSchedulerTest, PostTask) {
  base::test::TestFuture<size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  EXPECT_FALSE(future.IsReady());

  RunAllBackgroundTasks();

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), 1U);
}

TEST_F(BackgroundLongTaskSchedulerTest, PostTwoTasks) {
  base::test::TestFuture<size_t> future;
  base::test::TestFuture<size_t> future2;
  // The first task gets scheduled on the background thread immediately.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserBlocking,
      future2.GetCallback()));

  RunAllBackgroundTasks();

  EXPECT_EQ(future.Get(), 1U);
  EXPECT_EQ(future2.Get(), 2U);
}

TEST_F(BackgroundLongTaskSchedulerTest, PostTwoTasks_Sequentially) {
  base::test::TestFuture<size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  RunAllBackgroundTasks();
  EXPECT_EQ(future.Get(), 1U);

  base::test::TestFuture<size_t> future2;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future2.GetCallback()));
  RunAllBackgroundTasks();
  EXPECT_EQ(future2.Get(), 2U);
}

TEST_F(BackgroundLongTaskSchedulerTest, TaskPriority) {
  base::test::TestFuture<size_t> future;
  base::test::TestFuture<size_t> future2;
  base::test::TestFuture<size_t> future3;
  // The first task gets scheduled on the background thread immediately.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserVisible,
      future2.GetCallback()));
  // `future3` has higher priority than `future2` and should run before, even
  // though it was scheduled after.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserBlocking,
      future3.GetCallback()));

  RunAllBackgroundTasks();

  EXPECT_EQ(future.Get(), 1U);
  EXPECT_EQ(future3.Get(), 2U);
  EXPECT_EQ(future2.Get(), 3U);
}

TEST_F(BackgroundLongTaskSchedulerTest, CancelPendingTask) {
  base::test::TestFuture<size_t> future;
  base::test::TestFuture<size_t> future2;
  base::CancelableOnceCallback<void(size_t)> cancelable_wrapper2(
      future2.GetCallback());
  base::test::TestFuture<size_t> future3;
  // The first task gets scheduled on the background thread immediately.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      cancelable_wrapper2.callback()));
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future3.GetCallback()));

  cancelable_wrapper2.Cancel();
  RunAllBackgroundTasks();

  EXPECT_EQ(future.Get(), 1U);
  // `future2` wasn't run since the task was canceled before it was scheduled.
  EXPECT_EQ(future3.Get(), 2U);
}

TEST_F(BackgroundLongTaskSchedulerTest, CancelRunningTask) {
  base::test::TestFuture<size_t> future;
  base::CancelableOnceCallback<void(size_t)> cancelable_wrapper(
      future.GetCallback());
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      cancelable_wrapper.callback()));

  cancelable_wrapper.Cancel();
  RunAllBackgroundTasks();

  // The main thread callback wasn't run but the background task completed
  // anyways.
  EXPECT_FALSE(future.IsReady());

  // Check that the background count has been incremented by posting another
  // task.
  base::test::TestFuture<size_t> future2;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future2.GetCallback()));
  RunAllBackgroundTasks();
  EXPECT_EQ(future2.Get(), 2U);
}

TEST_F(BackgroundLongTaskSchedulerTest, DurationHistogram) {
  const std::string kBaseHistogramName =
      "Crypto.UnexportableKeys.BackgroundTaskDuration";
  base::HistogramTester histogram_tester;
  base::HistogramTester::CountsMap expected_counts;

  // Execute a `BackgroundTaskPriority::kBestEffort` task.
  base::test::TestFuture<size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  RunAllBackgroundTasks();
  EXPECT_TRUE(future.Wait());

  expected_counts[kBaseHistogramName] = 1;
  expected_counts[kBaseHistogramName + ".BestEffort"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kBaseHistogramName),
              testing::ContainerEq(expected_counts));

  // Execute a `BackgroundTaskPriority::kUserVisible` task.
  base::test::TestFuture<size_t> future2;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserVisible,
      future2.GetCallback()));
  RunAllBackgroundTasks();
  EXPECT_TRUE(future2.Wait());

  expected_counts[kBaseHistogramName] = 2;
  expected_counts[kBaseHistogramName + ".UserVisible"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kBaseHistogramName),
              testing::ContainerEq(expected_counts));

  // Execute a `BackgroundTaskPriority::kUserBlocking` task.
  base::test::TestFuture<size_t> future3;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserBlocking,
      future3.GetCallback()));
  RunAllBackgroundTasks();
  EXPECT_TRUE(future3.Wait());

  expected_counts[kBaseHistogramName] = 3;
  expected_counts[kBaseHistogramName + ".UserBlocking"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kBaseHistogramName),
              testing::ContainerEq(expected_counts));
}

TEST_F(BackgroundLongTaskSchedulerTest, DurationHistogramWithCanceledTasks) {
  base::HistogramTester histogram_tester;

  // The first task gets scheduled on the background thread immediately.
  base::test::TestFuture<size_t> future;
  base::CancelableOnceCallback<void(size_t)> cancelable_wrapper(
      future.GetCallback());
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      cancelable_wrapper.callback()));

  // The second task gets put into a task queue.
  base::test::TestFuture<size_t> future2;
  base::CancelableOnceCallback<void(size_t)> cancelable_wrapper2(
      future.GetCallback());
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserVisible,
      cancelable_wrapper.callback()));

  cancelable_wrapper.Cancel();
  cancelable_wrapper2.Cancel();
  RunAllBackgroundTasks();

  // The first task still ran, so it will be recorded.
  // The second task didn't run and it will not be recorded.
  base::HistogramTester::CountsMap expected_counts = {
      {"Crypto.UnexportableKeys.BackgroundTaskDuration", 1},
      {"Crypto.UnexportableKeys.BackgroundTaskDuration.BestEffort", 1}};
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Crypto.UnexportableKeys.BackgroundTaskDuration"),
              testing::ContainerEq(expected_counts));
}

}  // namespace unexportable_keys
