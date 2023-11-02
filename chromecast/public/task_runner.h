// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_TASK_RUNNER_H_
#define CHROMECAST_PUBLIC_TASK_RUNNER_H_

#include <stdint.h>

namespace chromecast {

// Provides a way for vendor libraries to run code on a specific thread.
// For example, cast_shell supplies an implementation of this interface through
// media APIs (see MediaPipelineDeviceParams) to allow media backends to
// schedule tasks to be run on the media thread.
class TaskRunner {
 public:
  // Subclass and implement 'Run' to supply code to be run by PostTask or
  // PostDelayedTask.  They both take ownership of the Task object passed in
  // and will delete after running the Task.
  class Task {
   public:
    virtual ~Task() {}
    virtual void Run() = 0;
  };

  // This class is intended for use with base callback type. A template has been
  // used to avoid introducing a hard dependency on Chromium base. It is used to
  // convert a chromium-style callback to a Task as defined above.
  template <typename T>
  class CallbackTask : public Task {
   public:
    CallbackTask(T callback) : callback_(std::move(callback)) {}

    ~CallbackTask() override = default;

   private:
    // TaskRunner::Task overrides:
    void Run() override { std::move(callback_).Run(); }

    T callback_;
  };

  // Posts a task to the thread's task queue.  Delay of 0 could mean task
  // runs immediately (within the call to PostTask, if it's called on the
  // target thread) but there also could be some delay (the task could be added
  // to target thread's task queue).
  virtual bool PostTask(Task* task, uint64_t delay_milliseconds) = 0;

 protected:
  virtual ~TaskRunner() {}
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_TASK_RUNNER_H_
