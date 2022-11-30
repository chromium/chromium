// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_SAMPLE_SAMPLE_EXECUTOR_H_
#define COMPONENTS_CRONET_NATIVE_SAMPLE_SAMPLE_EXECUTOR_H_

// Cronet sample is expected to be used outside of Chromium infrastructure,
// and as such has to rely on STL directly instead of //base alternatives.
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include "cronet_c.h"

// Sample implementation of Cronet_Executor interface using static
// methods to map C API into instance of C++ class.
class SampleExecutor {
 public:
  SampleExecutor();
  ~SampleExecutor();

  // Gets Cronet_ExecutorPtr implemented by |this|.
  Cronet_ExecutorPtr GetExecutor();

  // Shuts down the executor, so all pending tasks are destroyed without
  // getting executed.
  void ShutdownExecutor();

 private:
  // Runs tasks in |task_queue_| until |stop_thread_loop_| is set to true.
  void RunTasksInQueue();
  static void ThreadLoop(SampleExecutor* executor);

  // Adds |runnable| to |task_queue_| to execute on |executor_thread_|.
  void Execute(Cronet_RunnablePtr runnable);
  // Implementation of Cronet_Executor methods.
  static void Execute(Cronet_ExecutorPtr self, Cronet_RunnablePtr runnable);

  // Synchronise access to |task_queue_| and |stop_thread_loop_|;
  std::mutex lock_;
  // Tasks to run.
  std::queue<Cronet_RunnablePtr> task_queue_;
  // Notified if task is added to |task_queue_| or |stop_thread_loop_| is set.
  std::condition_variable task_available_;
  // Set to true to stop running tasks.
  bool stop_thread_loop_ = false;

  // Thread on which tasks are executed.
  std::thread executor_thread_;

  Cronet_ExecutorPtr const executor_;
};

#endif  // COMPONENTS_CRONET_NATIVE_SAMPLE_SAMPLE_EXECUTOR_H_
