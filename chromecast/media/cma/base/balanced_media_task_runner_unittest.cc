// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <list>
#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chromecast/media/cma/base/balanced_media_task_runner_factory.h"
#include "chromecast/media/cma/base/media_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

namespace {

struct MediaTaskRunnerTestContext {
  MediaTaskRunnerTestContext();
  ~MediaTaskRunnerTestContext();

  scoped_refptr<MediaTaskRunner> media_task_runner;

  bool is_pending_task;

  std::vector<base::TimeDelta> task_timestamp_list;

  size_t task_index;
  base::TimeDelta max_timestamp;
};

MediaTaskRunnerTestContext::MediaTaskRunnerTestContext() {
}

MediaTaskRunnerTestContext::~MediaTaskRunnerTestContext() {
}

}  // namespace

class BalancedMediaTaskRunnerTest : public testing::Test {
 public:
  BalancedMediaTaskRunnerTest();

  BalancedMediaTaskRunnerTest(const BalancedMediaTaskRunnerTest&) = delete;
  BalancedMediaTaskRunnerTest& operator=(const BalancedMediaTaskRunnerTest&) =
      delete;

  ~BalancedMediaTaskRunnerTest() override;

  void SetupTest(base::TimeDelta max_delta,
                 const std::vector<std::vector<int> >& timestamps_in_ms,
                 const std::vector<size_t>& pattern,
                 const std::vector<int>& expected_task_timestamps_ms);
  void ProcessAllTasks();

 protected:
  // Expected task order based on their timestamps.
  std::list<base::TimeDelta> expected_task_timestamps_;

 private:
  void ScheduleTask();
  void Task(size_t task_runner_id, base::TimeDelta timestamp);

  void OnTestTimeout();

  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;

  // Schedule first a task on media task runner #scheduling_pattern[0]
  // then a task on media task runner #scheduling_pattern[1] and so on.
  // Wrap around when reaching the end of the pattern.
  std::vector<size_t> scheduling_pattern_;
  size_t pattern_index_;

  // For each media task runner, keep a track of which task has already been
  // scheduled.
  std::vector<MediaTaskRunnerTestContext> contexts_;

  base::OnceClosure quit_closure_;
};

BalancedMediaTaskRunnerTest::BalancedMediaTaskRunnerTest() {
}

BalancedMediaTaskRunnerTest::~BalancedMediaTaskRunnerTest() {
}

void BalancedMediaTaskRunnerTest::SetupTest(
    base::TimeDelta max_delta,
    const std::vector<std::vector<int> >& timestamps_in_ms,
    const std::vector<size_t>& pattern,
    const std::vector<int>& expected_task_timestamps_ms) {
  media_task_runner_factory_ = new BalancedMediaTaskRunnerFactory(max_delta);

  scheduling_pattern_ = pattern;
  pattern_index_ = 0;

  // Setup each task runner.
  size_t n = timestamps_in_ms.size();
  contexts_.resize(n);
  for (size_t k = 0; k < n; k++) {
    contexts_[k].media_task_runner =
        media_task_runner_factory_->CreateMediaTaskRunner(
            base::SingleThreadTaskRunner::GetCurrentDefault());
    contexts_[k].is_pending_task = false;
    contexts_[k].task_index = 0;
    contexts_[k].task_timestamp_list.resize(
        timestamps_in_ms[k].size());
    for (size_t i = 0; i < timestamps_in_ms[k].size(); i++) {
      contexts_[k].task_timestamp_list[i] =
          base::Milliseconds(timestamps_in_ms[k][i]);
    }
  }

  // Expected task order (for tasks that are actually run).
  for (size_t k = 0; k < expected_task_timestamps_ms.size(); k++) {
    expected_task_timestamps_.push_back(
        base::Milliseconds(expected_task_timestamps_ms[k]));
  }
}

void BalancedMediaTaskRunnerTest::ProcessAllTasks() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BalancedMediaTaskRunnerTest::OnTestTimeout,
                     base::Unretained(this)),
      base::Seconds(5));
  ScheduleTask();
  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
}

void BalancedMediaTaskRunnerTest::ScheduleTask() {
  bool has_task = false;
  for (size_t k = 0; k < contexts_.size(); k++) {
    if (contexts_[k].task_index < contexts_[k].task_timestamp_list.size())
      has_task = true;
  }
  if (!has_task) {
    std::move(quit_closure_).Run();
    return;
  }

  size_t next_pattern_index =
      (pattern_index_ + 1) % scheduling_pattern_.size();

  size_t task_runner_id = scheduling_pattern_[pattern_index_];
  MediaTaskRunnerTestContext& context = contexts_[task_runner_id];

  // Check whether all tasks have been scheduled for that task runner
  // or if there is already one pending task.
  if (context.task_index >= context.task_timestamp_list.size() ||
      context.is_pending_task) {
    pattern_index_ = next_pattern_index;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&BalancedMediaTaskRunnerTest::ScheduleTask,
                                  base::Unretained(this)));
    return;
  }

  bool expected_may_run = false;
  if (context.task_timestamp_list[context.task_index] >=
      context.max_timestamp) {
    expected_may_run = true;
    context.max_timestamp = context.task_timestamp_list[context.task_index];
  }

  bool may_run = context.media_task_runner->PostMediaTask(
      FROM_HERE,
      base::BindOnce(&BalancedMediaTaskRunnerTest::Task, base::Unretained(this),
                     task_runner_id,
                     context.task_timestamp_list[context.task_index]),
      context.task_timestamp_list[context.task_index]);
  EXPECT_EQ(may_run, expected_may_run);

  if (may_run)
    context.is_pending_task = true;

  context.task_index++;
  pattern_index_ = next_pattern_index;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&BalancedMediaTaskRunnerTest::ScheduleTask,
                                base::Unretained(this)));
}

void BalancedMediaTaskRunnerTest::Task(
    size_t task_runner_id, base::TimeDelta timestamp) {
  ASSERT_FALSE(expected_task_timestamps_.empty());
  EXPECT_EQ(timestamp, expected_task_timestamps_.front());
  expected_task_timestamps_.pop_front();

  contexts_[task_runner_id].is_pending_task = false;

  // Release task runner if the task has ended
  // otherwise, the task runner may may block other streams
  auto& context = contexts_[task_runner_id];
  if (context.task_index >= context.task_timestamp_list.size()) {
    context.media_task_runner = nullptr;
  }
}

void BalancedMediaTaskRunnerTest::OnTestTimeout() {
  ADD_FAILURE() << "Test timed out";
  std::move(quit_closure_).Run();
}

TEST_F(BalancedMediaTaskRunnerTest, OneTaskRunner) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Timestamps of tasks for the single task runner.
  int timestamps0_ms[] = {0, 10, 20, 30, 40, 30, 50, 60, 20, 30, 70};
  std::vector<std::vector<int> > timestamps_ms(1);
  timestamps_ms[0] = std::vector<int>(
      timestamps0_ms, timestamps0_ms + std::size(timestamps0_ms));

  // Scheduling pattern.
  std::vector<size_t> scheduling_pattern(1);
  scheduling_pattern[0] = 0;

  // Expected results.
  int expected_timestamps[] = {0, 10, 20, 30, 40, 50, 60, 70};
  std::vector<int> expected_timestamps_ms(
      std::vector<int>(expected_timestamps,
                       expected_timestamps + std::size(expected_timestamps)));

  SetupTest(base::Milliseconds(30), timestamps_ms, scheduling_pattern,
            expected_timestamps_ms);
  ProcessAllTasks();
  EXPECT_TRUE(expected_task_timestamps_.empty());
}

TEST_F(BalancedMediaTaskRunnerTest, TwoTaskRunnerUnbalanced) {
  base::test::SingleThreadTaskEnvironment task_environment;

  // Timestamps of tasks for the 2 task runners.
  int timestamps0_ms[] = {0, 10, 20, 30, 40, 30, 50, 60, 20, 30, 70};
  int timestamps1_ms[] = {5, 15, 25, 35, 45, 35, 55, 65, 25, 35, 75};
  std::vector<std::vector<int> > timestamps_ms(2);
  timestamps_ms[0] = std::vector<int>(
      timestamps0_ms, timestamps0_ms + std::size(timestamps0_ms));
  timestamps_ms[1] = std::vector<int>(
      timestamps1_ms, timestamps1_ms + std::size(timestamps1_ms));

  // Scheduling pattern.
  size_t pattern[] = {1, 0, 0, 0, 0};
  std::vector<size_t> scheduling_pattern =
      std::vector<size_t>(pattern, pattern + std::size(pattern));

  // Expected results.
  int expected_timestamps[] = {
    5, 0, 10, 20, 30, 15, 40, 25, 50, 35, 60, 45, 70, 55, 65, 75 };
  std::vector<int> expected_timestamps_ms(
      std::vector<int>(expected_timestamps,
                       expected_timestamps + std::size(expected_timestamps)));

  SetupTest(base::Milliseconds(30), timestamps_ms, scheduling_pattern,
            expected_timestamps_ms);
  ProcessAllTasks();
  EXPECT_TRUE(expected_task_timestamps_.empty());
}

TEST_F(BalancedMediaTaskRunnerTest, TwoStreamsOfDifferentLength) {
  base::test::SingleThreadTaskEnvironment task_environment;

  std::vector<std::vector<int>> timestamps = {
      // One longer stream and one shorter stream.
      // The longer stream runs first, then the shorter stream begins.
      // After shorter stream ends, it shouldn't block the longer one.
      {0, 20, 40, 60, 80, 100, 120, 140, 160},
      {51, 61, 71, 81},
  };

  std::vector<int> expected_timestamps = {
      0, 20, 40, 60, 51, 80, 61, 71, 81, 100, 120, 140, 160};

  std::vector<size_t> scheduling_pattern = {
      0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0};

  SetupTest(base::Milliseconds(30), timestamps, scheduling_pattern,
            expected_timestamps);
  ProcessAllTasks();
  EXPECT_TRUE(expected_task_timestamps_.empty());
}

}  // namespace media
}  // namespace chromecast
