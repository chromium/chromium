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

  // Create the main thread's SequenceManager; it will choose at random which of
  // the two below TaskQueues will be served at each spin of the loop, despite
  // them being the same priority.
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager =
      base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          std::move(pump),
          base::sequence_manager::SequenceManager::Settings::Builder()
              .SetRandomTaskSelectionSeed(base::RandUint64())
              .Build());

  // Create two same-priority TaskQueues that feed into the main thread's
  // SequenceManager.
  base::sequence_manager::TaskQueue::Handle tq_a =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::QueueName::TEST_TQ));
  base::sequence_manager::TaskQueue::Handle tq_b =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::QueueName::TEST2_TQ));

  // Get TaskRunners for both TaskQueues.
  scoped_refptr<base::SingleThreadTaskRunner> a_runner_1 =
      tq_a->CreateTaskRunner(static_cast<int>(TaskType::kSource1));
  sequence_manager->SetDefaultTaskRunner(a_runner_1);
  scoped_refptr<base::SingleThreadTaskRunner> b_runner =
      tq_b->CreateTaskRunner(static_cast<int>(TaskType::kSource1));

  base::RunLoop run_loop;

  // Like the last example, this shows that TaskQueue is the principal unit of
  // ordering. All non-delayed tasks posted to the same queue run in FIFO order,
  // but tasks *across* queues in this case run in random order, as directed by
  // the SequenceManager that multiplexes them.
  for (int i = 1; i <= 20; ++i) {
    a_runner_1->PostTask(FROM_HERE, base::BindOnce(&Task, "A", i));
  }
  for (int i = 0; i < 20; ++i) {
    b_runner->PostTask(FROM_HERE, base::BindOnce(&Task, "B", i));
  }

  a_runner_1->PostDelayedTask(FROM_HERE,
                              base::BindOnce(&QuitTask, run_loop.QuitClosure()),
                              base::Seconds(1));
  run_loop.Run();
  return 0;
}
