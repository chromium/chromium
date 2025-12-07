// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/background_long_task_scheduler.h"

#include "base/cancelable_callback.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/unexportable_keys/background_task_impl.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/background_task_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {

// Data shared between all tasks on the background thread.
struct BackgroundThreadData {
  size_t task_count = 0;
};

// Instructs `FakeTask` to trigger task retries.
struct RetrySettings {
  size_t required_retries = 0;
  size_t max_retries = 0;
};

// FakeTask returns how many tasks has been executed on the background thread
// including the current one, at the moment of the task running.
class FakeTask : public internal::BackgroundTaskImpl<size_t> {
 public:
  explicit FakeTask(
      BackgroundThreadData& background_data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(size_t, size_t)> callback = base::DoNothing(),
      RetrySettings retry_settings = RetrySettings(),
      BackgroundTaskType type = BackgroundTaskType::kSign)
      : internal::BackgroundTaskImpl<size_t>(
            base::BindLambdaForTesting(
                [&background_data]() { return ++background_data.task_count; }),
            std::move(callback),
            priority,
            type,
            retry_settings.max_retries),
        required_retries_(retry_settings.required_retries) {}

  bool ShouldRetryBasedOnResult(const size_t& task_count) const override {
    return GetRetryCount() < required_retries_;
  }

 private:
  const size_t required_retries_;
};

// Helper method to create expectations for a `FakeTask` result.
std::tuple<size_t, size_t> FakeTaskResult(size_t task_count,
                                          size_t retries = 0) {
  return {task_count, retries};
}

// Shortcut functions for converting a task priority and a task type to a
// histogram suffix.
std::string ToString(BackgroundTaskPriority priority) {
  return std::string(GetBackgroundTaskPrioritySuffixForHistograms(priority));
}
std::string ToString(BackgroundTaskType type) {
  return std::string(GetBackgroundTaskTypeSuffixForHistograms(type));
}

}  // namespace

class BackgroundLongTaskSchedulerTest : public testing::Test {
 public:
  BackgroundLongTaskSchedulerTest()
      : background_task_runner_(
            base::ThreadPool::CreateSequencedTaskRunner({})) {}
  ~BackgroundLongTaskSchedulerTest() override = default;

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  BackgroundLongTaskScheduler& scheduler() { return scheduler_; }

  BackgroundThreadData& background_data() { return background_data_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::
          QUEUED,  // QUEUED - tasks don't run until `RunUntilIdle()` is
                   // called.
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  BackgroundLongTaskScheduler scheduler_{background_task_runner_};
  BackgroundThreadData background_data_;
};

TEST_F(BackgroundLongTaskSchedulerTest, PostTask) {
  base::test::TestFuture<size_t, size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  EXPECT_FALSE(future.IsReady());

  task_environment().RunUntilIdle();

  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), FakeTaskResult(1));
}

TEST_F(BackgroundLongTaskSchedulerTest, PostTwoTasks) {
  base::test::TestFuture<size_t, size_t> future;
  base::test::TestFuture<size_t, size_t> future2;
  // The first task gets scheduled on the background thread immediately.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserBlocking,
      future2.GetCallback()));

  task_environment().RunUntilIdle();

  EXPECT_EQ(future.Get(), FakeTaskResult(1));
  EXPECT_EQ(future2.Get(), FakeTaskResult(2));
}

TEST_F(BackgroundLongTaskSchedulerTest, PostTwoTasks_Sequentially) {
  base::test::TestFuture<size_t, size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback()));
  task_environment().RunUntilIdle();
  EXPECT_EQ(future.Get(), FakeTaskResult(1));

  base::test::TestFuture<size_t, size_t> future2;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future2.GetCallback()));
  task_environment().RunUntilIdle();
  EXPECT_EQ(future2.Get(), FakeTaskResult(2));
}

TEST_F(BackgroundLongTaskSchedulerTest, PostTaskFromCallback) {
  base::test::TestFuture<size_t, size_t> future;
  base::test::TestFuture<size_t, size_t> future_from_callback;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      base::BindLambdaForTesting([&](size_t count, size_t retries) {
        future.SetValue(count, retries);
        scheduler().PostTask(std::make_unique<FakeTask>(
            background_data(), BackgroundTaskPriority::kBestEffort,
            future_from_callback.GetCallback()));
      })));

  // Post an extra task to make sure that the scheduling order is respected.
  base::test::TestFuture<size_t, size_t> future2;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future2.GetCallback()));

  task_environment().RunUntilIdle();
  EXPECT_EQ(future.Get(), FakeTaskResult(1));
  EXPECT_EQ(future2.Get(), FakeTaskResult(2));
  EXPECT_EQ(future_from_callback.Get(), FakeTaskResult(3));
}

TEST_F(BackgroundLongTaskSchedulerTest, TaskPriority) {
  base::test::TestFuture<size_t, size_t> future;
  base::test::TestFuture<size_t, size_t> future2;
  base::test::TestFuture<size_t, size_t> future3;
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

  task_environment().RunUntilIdle();

  EXPECT_EQ(future.Get(), FakeTaskResult(1));
  EXPECT_EQ(future3.Get(), FakeTaskResult(2));
  EXPECT_EQ(future2.Get(), FakeTaskResult(3));
}

TEST_F(BackgroundLongTaskSchedulerTest, CancelPendingTask) {
  base::test::TestFuture<size_t, size_t> future;
  base::test::TestFuture<size_t, size_t> future2;
  base::CancelableOnceCallback<void(size_t, size_t)> cancelable_wrapper2(
      future2.GetCallback());
  base::test::TestFuture<size_t, size_t> future3;
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
  task_environment().RunUntilIdle();

  EXPECT_EQ(future.Get(), FakeTaskResult(1));
  // `future2` wasn't run since the task was canceled before it was scheduled.
  EXPECT_EQ(future3.Get(), FakeTaskResult(2));
}

TEST_F(BackgroundLongTaskSchedulerTest, CancelRunningTask) {
  base::test::TestFuture<size_t, size_t> future;
  base::CancelableOnceCallback<void(size_t, size_t)> cancelable_wrapper(
      future.GetCallback());
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      cancelable_wrapper.callback()));

  cancelable_wrapper.Cancel();
  task_environment().RunUntilIdle();

  // The main thread callback wasn't run but the background task completed
  // anyways.
  EXPECT_FALSE(future.IsReady());

  // Check that the background count has been incremented by posting another
  // task.
  base::test::TestFuture<size_t, size_t> future2;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future2.GetCallback()));
  task_environment().RunUntilIdle();
  EXPECT_EQ(future2.Get(), FakeTaskResult(2));
}

TEST_F(BackgroundLongTaskSchedulerTest, DurationHistogram) {
  const std::string kBaseHistogramName =
      "Crypto.UnexportableKeys.BackgroundTaskDuration";
  base::HistogramTester histogram_tester;
  base::HistogramTester::CountsMap expected_counts;

  // Execute a `BackgroundTaskPriority::kBestEffort` task.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort));
  task_environment().RunUntilIdle();

  expected_counts[kBaseHistogramName] = 1;
  expected_counts[kBaseHistogramName + ".BestEffort"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kBaseHistogramName),
              testing::ContainerEq(expected_counts));

  // Execute a `BackgroundTaskPriority::kUserVisible` task.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserVisible));
  task_environment().RunUntilIdle();

  expected_counts[kBaseHistogramName] = 2;
  expected_counts[kBaseHistogramName + ".UserVisible"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kBaseHistogramName),
              testing::ContainerEq(expected_counts));

  // Execute a `BackgroundTaskPriority::kUserBlocking` task.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserBlocking));
  task_environment().RunUntilIdle();

  expected_counts[kBaseHistogramName] = 3;
  expected_counts[kBaseHistogramName + ".UserBlocking"] = 1;
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(kBaseHistogramName),
              testing::ContainerEq(expected_counts));
}

TEST_F(BackgroundLongTaskSchedulerTest, DurationHistogramWithCanceledTasks) {
  base::HistogramTester histogram_tester;

  // The first task gets scheduled on the background thread immediately.
  base::test::TestFuture<size_t, size_t> future;
  base::CancelableOnceCallback<void(size_t, size_t)> cancelable_wrapper(
      future.GetCallback());
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      cancelable_wrapper.callback()));

  // The second task gets put into a task queue.
  base::test::TestFuture<size_t, size_t> future2;
  base::CancelableOnceCallback<void(size_t, size_t)> cancelable_wrapper2(
      future.GetCallback());
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserVisible,
      cancelable_wrapper.callback()));

  cancelable_wrapper.Cancel();
  cancelable_wrapper2.Cancel();
  task_environment().RunUntilIdle();

  // The first task still ran, so it will be recorded.
  // The second task didn't run and it will not be recorded.
  base::HistogramTester::CountsMap expected_counts = {
      {"Crypto.UnexportableKeys.BackgroundTaskDuration", 1},
      {"Crypto.UnexportableKeys.BackgroundTaskDuration.BestEffort", 1}};
  EXPECT_THAT(histogram_tester.GetTotalCountsForPrefix(
                  "Crypto.UnexportableKeys.BackgroundTaskDuration"),
              testing::ContainerEq(expected_counts));
}

TEST_F(BackgroundLongTaskSchedulerTest, QueueWaitAndRunDurationHistograms) {
  const std::string kQueueWaitHistogram =
      "Crypto.UnexportableKeys.BackgroundTaskQueueWaitDuration";
  const std::string kRunHistogram =
      "Crypto.UnexportableKeys.BackgroundTaskRunDuration";
  const std::string kTotalHistogram =
      "Crypto.UnexportableKeys.BackgroundTaskDuration";
  base::HistogramTester histogram_tester;

  // Picking non-overlaping parameters for two tasks so that all metrics fall
  // into different histograms and histogram buckets.
  const struct TaskParams {
    BackgroundTaskPriority priority;
    BackgroundTaskType type;
    base::TimeDelta run_time;
  } kFirstTask{BackgroundTaskPriority::kBestEffort,
               BackgroundTaskType::kGenerateKey, base::Seconds(2)},
      kSecondTask{BackgroundTaskPriority::kUserVisible,
                  BackgroundTaskType::kFromWrappedKey, base::Seconds(5)};

  // The first task gets scheduled on the background thread immediately.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), kFirstTask.priority,
      base::IgnoreArgs<size_t, size_t>(task_environment().QuitClosure()),
      RetrySettings(), kFirstTask.type));
  // Zero wait time as the task queue is empty when the first task is posted.
  histogram_tester.ExpectUniqueTimeSample(
      kQueueWaitHistogram + ToString(kFirstTask.priority), base::TimeDelta(),
      1);

  // Schedule the next task immediately after the first one. Its wait time
  // should be equal to the run time of the first task.
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), kSecondTask.priority, base::DoNothing(),
      RetrySettings(), kSecondTask.type));

  // `FastForwardBy()` would execute already posted tasks so use
  // `AdvanceClock()` instead to emulate that some time passed before background
  // task was executed.
  task_environment().AdvanceClock(kFirstTask.run_time);
  // This should quit right after the first task completes.
  task_environment().RunUntilQuit();

  histogram_tester.ExpectUniqueTimeSample(
      kRunHistogram + ToString(kFirstTask.type), kFirstTask.run_time, 1);
  histogram_tester.ExpectUniqueTimeSample(
      kTotalHistogram + ToString(kFirstTask.priority), kFirstTask.run_time, 1);
  histogram_tester.ExpectUniqueTimeSample(
      kQueueWaitHistogram + ToString(kSecondTask.priority), kFirstTask.run_time,
      1);

  task_environment().AdvanceClock(kSecondTask.run_time);
  task_environment().RunUntilIdle();

  histogram_tester.ExpectUniqueTimeSample(
      kRunHistogram + ToString(kSecondTask.type), kSecondTask.run_time, 1);
  // Total duration is the sum of wait time and run time.
  histogram_tester.ExpectUniqueTimeSample(
      kTotalHistogram + ToString(kSecondTask.priority),
      kFirstTask.run_time + kSecondTask.run_time, 1);

  // Check that base histograms without suffixes were also recorded for both
  // tasks.
  histogram_tester.ExpectTotalCount(kQueueWaitHistogram, 2);
  histogram_tester.ExpectTotalCount(kRunHistogram, 2);
}

TEST_F(BackgroundLongTaskSchedulerTest, RetryOnce) {
  base::test::TestFuture<size_t, size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback(),
      RetrySettings{.required_retries = 1, .max_retries = 2}));
  task_environment().RunUntilIdle();

  // It took two attempts to complete the task.
  EXPECT_EQ(future.Get(), FakeTaskResult(2, /*retries=*/1));
}

TEST_F(BackgroundLongTaskSchedulerTest, RetryTwice) {
  base::test::TestFuture<size_t, size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback(),
      RetrySettings{.required_retries = 2, .max_retries = 2}));
  task_environment().RunUntilIdle();

  // It took three attempts to complete the task.
  EXPECT_EQ(future.Get(), FakeTaskResult(3, /*retries=*/2));
}

TEST_F(BackgroundLongTaskSchedulerTest, RetryReachedMax) {
  base::test::TestFuture<size_t, size_t> future;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      future.GetCallback(),
      RetrySettings{.required_retries = 2, .max_retries = 1}));
  task_environment().RunUntilIdle();

  // Only one retry was allowed.
  EXPECT_EQ(future.Get(), FakeTaskResult(2, /*retries=*/1));
}

TEST_F(BackgroundLongTaskSchedulerTest, RetryWithNonEmptyQueue) {
  // taskA:
  base::test::TestFuture<size_t, size_t> futureA;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      futureA.GetCallback(),
      RetrySettings{.required_retries = 1, .max_retries = 2}));

  // taskB:
  base::test::TestFuture<size_t, size_t> futureB;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      futureB.GetCallback()));

  task_environment().RunUntilIdle();

  // Expected sequence:
  //   1) taskA (failure)
  //   2) taskB (success)
  //   3) taskA (success)
  EXPECT_EQ(futureA.Get(), FakeTaskResult(3, /*retries=*/1));
  EXPECT_EQ(futureB.Get(), FakeTaskResult(2));
}

TEST_F(BackgroundLongTaskSchedulerTest, RetryWithHigherPriority) {
  // taskA:
  base::test::TestFuture<size_t, size_t> futureA;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kUserBlocking,
      futureA.GetCallback(),
      RetrySettings{.required_retries = 1, .max_retries = 2}));

  // taskB:
  base::test::TestFuture<size_t, size_t> futureB;
  scheduler().PostTask(std::make_unique<FakeTask>(
      background_data(), BackgroundTaskPriority::kBestEffort,
      futureB.GetCallback()));

  task_environment().RunUntilIdle();

  // Expected sequence:
  //   1) taskA (failure)
  //   2) taskA (success)
  //   3) taskB (success)
  EXPECT_EQ(futureA.Get(), FakeTaskResult(2, /*retries=*/1));
  EXPECT_EQ(futureB.Get(), FakeTaskResult(3));
}

}  // namespace unexportable_keys
