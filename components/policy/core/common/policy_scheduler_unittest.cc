// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_scheduler.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicySchedulerTest : public testing::Test {
 public:
  void DoTask(PolicyScheduler::TaskCallback callback) {
    do_counter_++;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void OnTaskDone(bool success) {
    done_counter_++;

    // Terminate PolicyScheduler after 5 iterations.
    if (done_counter_ >= 5) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&PolicySchedulerTest::Terminate,
                                    base::Unretained(this)));
    }
  }

  // To simulate a slow task the callback is captured instead of running it.
  void CaptureCallbackForSlowTask(PolicyScheduler::TaskCallback callback) {
    do_counter_++;
    slow_callback_ = std::move(callback);
  }

  // Runs the captured callback to simulate the end of the slow task.
  void PostSlowTaskCallback() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(slow_callback_), true));
  }

  void Terminate() { scheduler_.reset(); }

 protected:
  int do_counter_ = 0;
  int done_counter_ = 0;
  std::unique_ptr<PolicyScheduler> scheduler_;

  PolicyScheduler::TaskCallback slow_callback_;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(PolicySchedulerTest, Run) {
  scheduler_ = std::make_unique<PolicyScheduler>(
      base::BindRepeating(&PolicySchedulerTest::DoTask, base::Unretained(this)),
      base::BindRepeating(&PolicySchedulerTest::OnTaskDone,
                          base::Unretained(this)),
      base::TimeDelta::Max());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, done_counter_);
}

TEST_F(PolicySchedulerTest, Loop) {
  scheduler_ = std::make_unique<PolicyScheduler>(
      base::BindRepeating(&PolicySchedulerTest::DoTask, base::Unretained(this)),
      base::BindRepeating(&PolicySchedulerTest::OnTaskDone,
                          base::Unretained(this)),
      base::TimeDelta());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, done_counter_);
}

TEST_F(PolicySchedulerTest, Reschedule) {
  scheduler_ = std::make_unique<PolicyScheduler>(
      base::BindRepeating(&PolicySchedulerTest::DoTask, base::Unretained(this)),
      base::BindRepeating(&PolicySchedulerTest::OnTaskDone,
                          base::Unretained(this)),
      base::TimeDelta::Max());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, done_counter_);

  // Delayed action is not run.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, done_counter_);

  // Rescheduling with 0 delay causes it to run.
  scheduler_->ScheduleTaskNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, done_counter_);
}

TEST_F(PolicySchedulerTest, OverlappingTasks) {
  scheduler_ = std::make_unique<PolicyScheduler>(
      base::BindRepeating(&PolicySchedulerTest::CaptureCallbackForSlowTask,
                          base::Unretained(this)),
      base::BindRepeating(&PolicySchedulerTest::OnTaskDone,
                          base::Unretained(this)),
      base::TimeDelta::Max());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, do_counter_);
  EXPECT_EQ(0, done_counter_);

  // Second action doesn't start while first is still pending.
  scheduler_->ScheduleTaskNow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, do_counter_);
  EXPECT_EQ(0, done_counter_);

  // After first action has finished, the second is started.
  PostSlowTaskCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, do_counter_);
  EXPECT_EQ(1, done_counter_);

  // Let the second action finish.
  PostSlowTaskCallback();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, do_counter_);
  EXPECT_EQ(2, done_counter_);
}

}  // namespace policy
