// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"

#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "mojo/core/embedder/embedder.h"

namespace chrome_cleaner {

// static
scoped_refptr<MojoTaskRunner> MojoTaskRunner::Create() {
  // Ensures thread-safe and unique initialization of the mojo lib.
  static bool mojo_initialization = []() {  // Leaked.
    mojo::core::Init();
    return true;
  }();
  ANALYZER_ALLOW_UNUSED(mojo_initialization);

  scoped_refptr<MojoTaskRunner> mojo_task_runner(new MojoTaskRunner());
  return mojo_task_runner->Initialize() ? mojo_task_runner : nullptr;
}

bool MojoTaskRunner::PostDelayedTask(const base::Location& from_here,
                                     base::OnceClosure task,
                                     base::TimeDelta delay) {
  DCHECK(io_thread_);
  return io_thread_->task_runner()->PostDelayedTask(from_here, std::move(task),
                                                    delay);
}

bool MojoTaskRunner::RunsTasksInCurrentSequence() const {
  DCHECK(io_thread_);
  return io_thread_->task_runner()->RunsTasksInCurrentSequence();
}

bool MojoTaskRunner::PostNonNestableDelayedTask(const base::Location& from_here,
                                                base::OnceClosure task,
                                                base::TimeDelta delay) {
  return io_thread_->task_runner()->PostNonNestableDelayedTask(
      from_here, std::move(task), delay);
}

MojoTaskRunner::MojoTaskRunner() = default;

MojoTaskRunner::~MojoTaskRunner() {
  ipc_support_.reset();
  // Resets the IO thread after resetting the ipc_support_, because its
  // finalization uses the thread's task runner.
  io_thread_.reset();
}

bool MojoTaskRunner::Initialize() {
  io_thread_ = std::make_unique<base::Thread>("MojoThread");
  if (!io_thread_->StartWithOptions(
          base::Thread::Options(base::MessagePumpType::IO, 0))) {
    io_thread_.reset();
    return false;
  }

  ipc_support_ = std::make_unique<mojo::core::ScopedIPCSupport>(
      io_thread_->task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return true;
}

}  // namespace chrome_cleaner
