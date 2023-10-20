// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CODELABS_MOJO_EXAMPLES_PROCESS_BOOTSTRAPPER_H_
#define CODELABS_MOJO_EXAMPLES_PROCESS_BOOTSTRAPPER_H_

#include "base/message_loop/message_pump.h"
#include "base/run_loop.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/threading/thread.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

class ProcessBootstrapper {
 public:
  ProcessBootstrapper();
  ~ProcessBootstrapper();

  // This sets up the main thread with a message pump of `type`, and optionally
  // a dedicated IO thread if `type` is *not* `base::MessagePumpType::IO`.
  void InitMainThread(base::MessagePumpType type) {
    // Creates a sequence manager bound to the main thread with a message pump
    // of some specified type. The message pump determines exactly what the
    // event loop on its thread is capable of (i.e., what *kind* of messages it
    // can "pump"). For example, a `DEFAULT` message pump is capable of
    // processing simple events, like async timers and posted tasks. The `IO`
    // message pump type — which is used in every example in this codelab — is
    // capable of asynchronously processing IO over IPC primitives like file
    // descriptors, used by Mojo. A thread with *that* kind of message pump is
    // required for any process using Mojo for IPC.
    std::unique_ptr<base::MessagePump> pump = base::MessagePump::Create(type);
    sequence_manager =
        base::sequence_manager::CreateSequenceManagerOnCurrentThreadWithPump(
            std::move(pump),
            base::sequence_manager::SequenceManager::Settings::Builder()
                .SetMessagePumpType(type)
                .Build());
    default_tq = std::make_unique<base::sequence_manager::TaskQueue::Handle>(
        sequence_manager->CreateTaskQueue(
            base::sequence_manager::TaskQueue::Spec(
                base::sequence_manager::QueueName::DEFAULT_TQ)));
    sequence_manager->SetDefaultTaskRunner((*default_tq)->task_runner());

    if (type == base::MessagePumpType::DEFAULT) {
      InitDedicatedIOThread();
    }
  }

  // Must be called after `InitMainThread()`.
  void InitMojo(bool as_browser_process) {
    CHECK(default_tq) << "Must call `InitMainThread()` before `InitMojo()`";
    // Basic Mojo initialization for a new process.
    mojo::core::Configuration config;
    // For mojo, one process must be the broker process which is responsible for
    // trusted cross-process introductions etc. Traditionally this is the
    // "browser" process.
    config.is_broker_process = as_browser_process;
    mojo::core::Init(config);

    // The effects of `ScopedIPCSupport` are mostly irrelevant for our simple
    // examples, but this class is used to determine how the IPC system shuts
    // down. The two shutdown options are "CLEAN" and "FAST", and each of these
    // may determine how other processes behave if *this* process has a message
    // pipe that is in the middle of proxying messages to another process where
    // the other end of the message pipe lives.
    //
    // In real Chrome, both the browser and renderer processes can safely use
    // `FAST` mode, because the side effects of quickly terminating the IPC
    // system in the middle of cross-process IPC message proxying is not
    // important. See this class's documentation for more information on
    // shutdown.
    //
    // We initialize `ipc_support` with a task runner for whatever thread should
    // be the IO thread. This means preferring `io_task_runner` when it is
    // non-null, and the default task runner otherwise.
    mojo::core::ScopedIPCSupport ipc_support(
        io_task_runner ? io_task_runner : (*default_tq)->task_runner(),
        mojo::core::ScopedIPCSupport::ShutdownPolicy::FAST);
  }

  std::unique_ptr<base::sequence_manager::TaskQueue::Handle> default_tq;
  std::unique_ptr<base::sequence_manager::SequenceManager> sequence_manager;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner;

 private:
  // Note that you cannot call this if you've ever called
  // `InitMainThread(base::MessagePumpType::IO)` since that means the main
  // thread *itself* the IO thread.
  void InitDedicatedIOThread() {
    io_thread_ = std::make_unique<base::Thread>("ipc!");
    io_thread_->StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    io_task_runner = io_thread_->task_runner();
  }

  std::unique_ptr<base::Thread> io_thread_;
};

ProcessBootstrapper::ProcessBootstrapper() = default;
ProcessBootstrapper::~ProcessBootstrapper() = default;

#endif  // CODELABS_MOJO_EXAMPLES_PROCESS_BOOTSTRAPPER_H_
