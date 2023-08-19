// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/message_loop/message_pump.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"

// See the documentation below.
enum class TaskType : unsigned char {
  kMain = 1,
};

void RunTaskAndQuit(base::OnceClosure quit_closure) {
  LOG(INFO) << "Whoah, we're doing a task! Now let's quit";
  std::move(quit_closure).Run();
}

int main() {
  base::PlatformThread::SetName("SchedulingDemoMain");

  // Create MessagePump that drives the SequenceManager that gets bound to this
  // thread further down. This pump's type is "default", which is a really
  // simple kind of platform-agtnostic message pump that only has the ability to
  // listen for tasks (possibly with delays) and run them; it can't, for
  // example:
  //   1.) Watch file descriptors or native sockets
  //   2.) Listen to platform-specific UI events, such as WM_PAINT on Windows or
  //       `NSEvent*`s on macOS.
  std::unique_ptr<base::MessagePump> pump =
      base::MessagePump::Create(base::MessagePumpType::DEFAULT);

  // Create the thread's SequenceManager.
  //
  // What this really does is:
  //   1.) Creates an unbound SequenceManager (the default)
  //   2.) Immediately "binds" it to the current thread
  // What (2) really means is that we invoke
  // `SequenceManagerImpl::CompleteInitializationOnBoundThread()`, which sets
  // the thread-local storage SequenceManager to `sequence_manager` below.
  // That's important so that `RunLoop` objects that have no direct access to
  // `sequence_manager` know which manager to tell to "run" later on.
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager =
      base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          std::move(pump),
          base::sequence_manager::SequenceManager::Settings::Builder().Build());

  // Create a default TaskQueue that feeds into the SequenceManager. Inside a
  // SequenceManager, TaskQueue is the basic unit of scheduling...
  base::sequence_manager::TaskQueue::Handle default_task_queue =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::QueueName::DEFAULT_TQ));

  // ... and from a TaskQueue you can get an arbitrary number of TaskRunners,
  // which actually let you post tasks to the underlying/internal queue.
  //
  // Why do we even have this distinction? Why even have multiple TaskRunners
  // associated with the same underlying queue? Answer: this is for task
  // attribution purposes, so you can query metrics to see which TaskRunners
  // (used by different places in your code) are posting what kind of tasks
  // and get other interesting information about them.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      default_task_queue->CreateTaskRunner(static_cast<int>(TaskType::kMain));
  sequence_manager->SetDefaultTaskRunner(task_runner);

  // Now that this thread has a bound sequence manager set up, we can:
  //   1.) Start posting tasks to its queues which are not yet being processed
  //   2.) Start processing tasks from the queues by "running" the loop, which
  //       runs the sequence manager's underlying message pump.
  base::RunLoop run_loop;
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskAndQuit, run_loop.QuitClosure()));
  // This synchronously blocks (by running this thread's loop) until the quit
  // closure is called.
  run_loop.Run();
  return 0;
}
