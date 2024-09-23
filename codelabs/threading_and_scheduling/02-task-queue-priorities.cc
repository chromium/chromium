// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/logging.h"
#include "base/message_loop/message_pump.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"

enum class TaskType : unsigned char {
  kSource1 = 1,
  kSource2 = 2,
};

enum class TaskPriority : base::sequence_manager::TaskQueue::QueuePriority {
  kHighPriority = 0,
  kNormalPriority = 1,

  kNumPriorities = 2,
};

void Task(std::string_view tq, int task_number) {
  if (tq == "A") {
    LOG(INFO) << "TaskQueue(" << tq << "): " << task_number;
  } else {
    LOG(INFO) << "    TaskQueue(" << tq << "): " << task_number;
  }
}

void QuitTask(base::OnceClosure quit_closure) {
  LOG(INFO) << "Quittin' time!";
  std::move(quit_closure).Run();
}

int main() {
  base::PlatformThread::SetName("SchedulingDemoMain");

  std::unique_ptr<base::MessagePump> pump =
      base::MessagePump::Create(base::MessagePumpType::DEFAULT);

  base::sequence_manager::SequenceManager::PrioritySettings priority_settings(
      TaskPriority::kNumPriorities, TaskPriority::kNormalPriority);

  // Create the main thread's SequenceManager; it will choose which of the two
  // below TaskQueues will be served at each spin of the loop, based on their
  // priority.
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager =
      base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          std::move(pump),
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetPrioritySettings(std::move(priority_settings))
              .Build());

  // Create two TaskQueues that feed into the main thread's SequenceManager,
  // each with a different priority to demonstrate that TaskQueue is the
  // principal unit of ordering, and that tasks across queues don't have to run
  // in posting-order, while non-delayed tasks posted to the same queue are
  // always run in FIFO order.
  base::sequence_manager::TaskQueue::Handle tq_a =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::QueueName::TEST_TQ));
  tq_a->SetQueuePriority(TaskPriority::kNormalPriority);
  base::sequence_manager::TaskQueue::Handle tq_b =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::QueueName::TEST2_TQ));
  tq_b->SetQueuePriority(TaskPriority::kHighPriority);

  // Get TaskRunners for both TaskQueues.
  scoped_refptr<base::SingleThreadTaskRunner> a_runner_1 =
      tq_a->CreateTaskRunner(static_cast<int>(TaskType::kSource1));
  sequence_manager->SetDefaultTaskRunner(a_runner_1);
  scoped_refptr<base::SingleThreadTaskRunner> a_runner_2 =
      tq_a->CreateTaskRunner(static_cast<int>(TaskType::kSource2));
  scoped_refptr<base::SingleThreadTaskRunner> b_runner =
      tq_b->CreateTaskRunner(static_cast<int>(TaskType::kSource2));

  base::RunLoop run_loop;

  // This example shows that the TaskRunner you choose to post a task with to a
  // TaskQueue doesn't affect the ordering of the tasks in the queue. It is
  // TaskQueue that is the principal unit of ordering, not TaskRunner; all tasks
  // in a queue run in FIFO order with respect to each other, but tasks *across*
  // queues are run in queue priority order. We post all of the tasks to `tq_a`
  // before posting any to `tq_b`, but observe that `tq_b` tasks run first due
  // to priority.
  for (int i = 1; i <= 20;) {
    a_runner_1->PostTask(FROM_HERE, base::BindOnce(&Task, "A", i++));
    a_runner_2->PostTask(FROM_HERE, base::BindOnce(&Task, "A", i++));
  }
  for (int i = 1; i <= 20; ++i) {
    b_runner->PostTask(FROM_HERE, base::BindOnce(&Task, "B", i));
  }

  a_runner_1->PostDelayedTask(FROM_HERE,
                              base::BindOnce(&QuitTask, run_loop.QuitClosure()),
                              base::Seconds(1));
  run_loop.Run();
  return 0;
}
