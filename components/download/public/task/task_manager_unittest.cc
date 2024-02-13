// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/task/task_manager_impl.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {
namespace {

class MockTaskScheduler : public TaskScheduler {
 public:
  MockTaskScheduler() = default;
  ~MockTaskScheduler() override = default;

  // TaskScheduler implementation.
  MOCK_METHOD6(ScheduleTask,
               void(DownloadTaskType, bool, bool, int, int64_t, int64_t));
  MOCK_METHOD1(CancelTask, void(DownloadTaskType));
};

class MockTaskWaiter {
 public:
  MockTaskWaiter() = default;
  ~MockTaskWaiter() = default;

  MOCK_METHOD1(TaskFinished, void(bool));
};

class TaskManagerImplTest : public testing::Test {
 public:
  TaskManagerImplTest()
      : task_runner_(new base::TestMockTimeTaskRunner),
        current_default_handle_(task_runner_) {
    auto scheduler = std::make_unique<MockTaskScheduler>();
    task_scheduler_ = scheduler.get();
    task_manager_ = std::make_unique<TaskManagerImpl>(std::move(scheduler));
  }

  TaskManagerImplTest(const TaskManagerImplTest&) = delete;
  TaskManagerImplTest& operator=(const TaskManagerImplTest&) = delete;

  ~TaskManagerImplTest() override = default;

 protected:
  TaskManager::TaskParams CreateTaskParams() {
    TaskManager::TaskParams params;
    params.require_unmetered_network = false;
    params.require_charging = false;
    params.optimal_battery_percentage = 15;
    params.window_start_time_seconds = 0;
    params.window_end_time_seconds = 10;
    return params;
  }

  void ExpectCallToScheduleTask(DownloadTaskType task_type,
                                const TaskManager::TaskParams& params,
                                int call_count) {
    EXPECT_CALL(
        *task_scheduler_,
        ScheduleTask(task_type, params.require_unmetered_network,
                     params.require_charging, params.optimal_battery_percentage,
                     params.window_start_time_seconds,
                     params.window_end_time_seconds))
        .Times(call_count);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle current_default_handle_;

  std::unique_ptr<TaskManagerImpl> task_manager_;
  raw_ptr<MockTaskScheduler> task_scheduler_;
};

}  // namespace

TEST_F(TaskManagerImplTest, ScheduleTask) {
  auto params = CreateTaskParams();

  // Schedule a task.
  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 1);
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  // Schedule another task with same params. This should be a no-op.
  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 0);
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  // Schedule another task with different params. This task should override the
  // already scheduled task.
  params.optimal_battery_percentage = 20;
  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 1);
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, UnscheduleTask) {
  auto params = CreateTaskParams();
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  EXPECT_CALL(*task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .Times(1);
  task_manager_->UnscheduleTask(DownloadTaskType::DOWNLOAD_TASK);

  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);
  EXPECT_CALL(*task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .Times(1);
  task_manager_->UnscheduleTask(DownloadTaskType::DOWNLOAD_TASK);

  // Cancel is called even if we are not aware of scheduling a task.
  EXPECT_CALL(*task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .Times(1);
  task_manager_->UnscheduleTask(DownloadTaskType::DOWNLOAD_TASK);

  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, NotifyTaskFinished) {
  auto params = CreateTaskParams();

  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  MockTaskWaiter waiter;
  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));
  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));

  EXPECT_CALL(waiter, TaskFinished(false)).Times(1);
  task_manager_->NotifyTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false);
  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, DifferentTasksCanBeRunIndependently) {
  auto params = CreateTaskParams();

  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 1);
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  ExpectCallToScheduleTask(DownloadTaskType::CLEANUP_TASK, params, 1);
  task_manager_->ScheduleTask(DownloadTaskType::CLEANUP_TASK, params);

  MockTaskWaiter waiter;
  auto callback1 =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));
  auto callback2 =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));

  EXPECT_CALL(waiter, TaskFinished(false)).Times(1);
  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback1));
  task_manager_->NotifyTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false);

  EXPECT_CALL(waiter, TaskFinished(true)).Times(1);
  task_manager_->OnStartScheduledTask(DownloadTaskType::CLEANUP_TASK,
                                      std::move(callback2));
  task_manager_->NotifyTaskFinished(DownloadTaskType::CLEANUP_TASK, true);

  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, ScheduleTaskWithDifferentParamsWhileRunningTask) {
  auto params = CreateTaskParams();
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  MockTaskWaiter waiter;
  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));

  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));

  params.optimal_battery_percentage = 20;
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 1);
  EXPECT_CALL(waiter, TaskFinished(false)).Times(1);
  task_manager_->NotifyTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false);
  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, ScheduleTaskWithSameParamsWhileRunningTask) {
  auto params = CreateTaskParams();
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  MockTaskWaiter waiter;
  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));

  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 0);
  EXPECT_CALL(waiter, TaskFinished(false)).Times(1);
  task_manager_->NotifyTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false);
  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, StopTaskWillClearTheCallback) {
  auto params = CreateTaskParams();
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  MockTaskWaiter waiter;
  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));

  EXPECT_CALL(waiter, TaskFinished(false)).Times(0);
  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));
  task_manager_->OnStopScheduledTask(DownloadTaskType::DOWNLOAD_TASK);

  task_manager_->NotifyTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false);

  task_runner_->RunUntilIdle();
}

// Verifies that OnStartScheduledTask() can be called without preceding
// ScheduleTask() calls.
TEST_F(TaskManagerImplTest, StartTaskWithoutPendingParams) {
  MockTaskWaiter waiter;
  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));
  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));
  EXPECT_CALL(waiter, TaskFinished(false)).Times(1);
  task_manager_->NotifyTaskFinished(DownloadTaskType::DOWNLOAD_TASK, false);
  task_runner_->RunUntilIdle();
}

// Verifies that OnStopScheduledTask() can be called without preceding
// ScheduleTask() calls.
TEST_F(TaskManagerImplTest, StopTaskWithoutPendingParams) {
  MockTaskWaiter waiter;
  EXPECT_CALL(waiter, TaskFinished(false)).Times(0);

  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));
  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));
  task_manager_->OnStopScheduledTask(DownloadTaskType::DOWNLOAD_TASK);

  task_runner_->RunUntilIdle();
}

TEST_F(TaskManagerImplTest, StopTaskWillSchedulePendingParams) {
  auto params = CreateTaskParams();
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  MockTaskWaiter waiter;
  auto callback =
      base::BindOnce(&MockTaskWaiter::TaskFinished, base::Unretained(&waiter));

  task_manager_->OnStartScheduledTask(DownloadTaskType::DOWNLOAD_TASK,
                                      std::move(callback));

  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 0);
  params.optimal_battery_percentage = 20;
  task_manager_->ScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params);

  ExpectCallToScheduleTask(DownloadTaskType::DOWNLOAD_TASK, params, 1);
  task_manager_->OnStopScheduledTask(DownloadTaskType::DOWNLOAD_TASK);

  task_runner_->RunUntilIdle();
}

}  // namespace download
