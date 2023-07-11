// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/message_loop/message_pump.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"  // For IOThreadDelegate.
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"

// For this example, these are global so that each task running on each thread
// can easily grab the *other* thread's task runner to post continuation tasks
// to.
static scoped_refptr<base::SingleThreadTaskRunner> g_main_thread_task_runner;
static scoped_refptr<base::SingleThreadTaskRunner> g_io_thread_task_runner;

void RunOnIOThread();

void RunOnUIThread() {
  LOG(INFO) << "RunOnUIThread()";
  g_io_thread_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunOnIOThread), base::Seconds(1));
}

void RunOnIOThread() {
  LOG(INFO) << "RunOnIOThread()";
  g_main_thread_task_runner->PostDelayedTask(
      FROM_HERE, base::BindOnce(&RunOnUIThread), base::Seconds(1));
}

// In this example we're simulating Chromium's UI/IO thread architecture in a
// simplified way. The `IOThreadDelegate` represents the physical "IO" thread
// that each Chromium process has, running alongside the main thread. It is
// similar to `content::BrowserIOThreadDelegate`, for example.
class IOThreadDelegate : public base::Thread::Delegate {
 public:
  IOThreadDelegate() {
    // Create an unbound SequenceManager{Impl}. It will be bound in
    // `BindToCurrentThread()` on the new physical thread, once it starts
    // running.
    owned_sequence_manager_ =
        base::sequence_manager::CreateUnboundSequenceManager();
    task_queue_ = owned_sequence_manager_->CreateTaskQueue(
        base::sequence_manager::TaskQueue::Spec(
            base::sequence_manager::TaskQueue::Spec(
                base::sequence_manager::QueueName::IO_DEFAULT_TQ)));
    default_task_runner_ = task_queue_->task_runner();
    owned_sequence_manager_->SetDefaultTaskRunner(default_task_runner_);
    // Set the global TaskRunner-to-this-thread, so that the main thread can
    // post tasks to the IO thread.
    g_io_thread_task_runner = default_task_runner_;
  }

  // base::Thread::Delegate implementation.
  // This is similar to i.e.,
  // `content::BrowserIOThreadDelegate::BindToCurrentThread()`, and is the first
  // function to run on the new physical thread.
  void BindToCurrentThread() override {
    owned_sequence_manager_->BindToMessagePump(
        base::MessagePump::Create(base::MessagePumpType::IO));
    owned_sequence_manager_->SetDefaultTaskRunner(GetDefaultTaskRunner());
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetDefaultTaskRunner() override {
    return default_task_runner_;
  }

 private:
  std::unique_ptr<base::sequence_manager::SequenceManager>
      owned_sequence_manager_;
  base::sequence_manager::TaskQueue::Handle task_queue_;
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;
};

int main() {
  base::PlatformThread::SetName("SchedulingDemoMain");

  std::unique_ptr<base::MessagePump> pump =
      base::MessagePump::Create(base::MessagePumpType::UI);

  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager =
      base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
          std::move(pump));

  // Create a default TaskQueue that feeds into the SequenceManager.
  base::sequence_manager::TaskQueue::Handle main_task_queue =
      sequence_manager->CreateTaskQueue(base::sequence_manager::TaskQueue::Spec(
          base::sequence_manager::TaskQueue::Spec(
              base::sequence_manager::QueueName::DEFAULT_TQ)));

  // Get a default TaskRunner for the main (UI) thread.
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner =
      main_task_queue->task_runner();
  sequence_manager->SetDefaultTaskRunner(default_task_runner);

  // Set the global TaskRunner-to-this-thread, so that the IO thread can post
  // tasks to the main thread.
  g_main_thread_task_runner = default_task_runner;

  // Create an IO thread to run alongside the main thread.
  std::unique_ptr<IOThreadDelegate> delegate =
      std::make_unique<IOThreadDelegate>();
  base::Thread io_thread("IOThread");
  base::Thread::Options options;
  options.delegate = std::move(delegate);
  if (!io_thread.StartWithOptions(std::move(options))) {
    LOG(FATAL) << "The thread failed to start!";
  }
  // END create a new thread.

  base::RunLoop run_loop;
  g_main_thread_task_runner->PostTask(FROM_HERE,
                                      base::BindOnce(&RunOnUIThread));
  g_main_thread_task_runner->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                                             base::Seconds(10));
  run_loop.Run();
  return 0;
}
