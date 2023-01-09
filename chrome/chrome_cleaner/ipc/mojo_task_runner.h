// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_MOJO_TASK_RUNNER_H_
#define CHROME_CHROME_CLEANER_IPC_MOJO_TASK_RUNNER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace chrome_cleaner {

// Task runner for Mojo IPC tasks that ensures Mojo lib initialization
// and execution on an IO thread.
class MojoTaskRunner : public base::SingleThreadTaskRunner {
 public:
  // Creates a new instance of MojoTaskRunner with a new IO thread to run IPC
  // tasks.
  static scoped_refptr<MojoTaskRunner> Create();

  // Abstract methods from base::TaskRunner.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

  // Abstract methods from base::SequencedTaskRunner.
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override;

 protected:
  MojoTaskRunner();
  ~MojoTaskRunner() override;

  // Starts a new IO thread to run IPC tasks.
  virtual bool Initialize();

 private:
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<mojo::core::ScopedIPCSupport> ipc_support_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_MOJO_TASK_RUNNER_H_
