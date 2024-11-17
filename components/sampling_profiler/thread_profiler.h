// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAMPLING_PROFILER_THREAD_PROFILER_H_
#define COMPONENTS_SAMPLING_PROFILER_THREAD_PROFILER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/sampling_profiler/call_stack_profile_params.h"
#include "components/sampling_profiler/process_type.h"

namespace sampling_profiler {

class ThreadProfilerClient;

// ThreadProfiler performs startup and periodic profiling of Chrome
// threads.
class ThreadProfiler {
 public:
  ThreadProfiler(const ThreadProfiler&) = delete;
  ThreadProfiler& operator=(const ThreadProfiler&) = delete;

  ~ThreadProfiler();

  // Creates a profiler for a main thread and immediately starts it. This
  // function should only be used when profiling the main thread of a
  // process. The returned profiler must be destroyed prior to thread exit to
  // stop the profiling.
  //
  // SetMainThreadTaskRunner() should be called after the message loop has been
  // started on the thread. It is the caller's responsibility to ensure that
  // the instance returned by this function is still alive when the static API
  // SetMainThreadTaskRunner() is used. The latter is static to support Chrome's
  // set up where the ThreadProfiler is created in chrome/app which cannot be
  // easily accessed from chrome_browser_main.cc which sets the task runner.
  static std::unique_ptr<ThreadProfiler> CreateAndStartOnMainThread();

  // Sets the task runner when profiling on the main thread. This occurs in a
  // separate call from CreateAndStartOnMainThread so that startup profiling can
  // occur prior to message loop start. The task runner is associated with the
  // instance returned by CreateAndStartOnMainThread(), which must be alive when
  // this is called.
  static void SetMainThreadTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Sets a callback to create auxiliary unwinders, for handling additional,
  // non-native-code unwind scenarios. Currently used to support
  // unwinding V8 JavaScript frames.
  void SetAuxUnwinderFactory(
      const base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>&
          factory);

  // Creates a profiler for a child thread and immediately starts it. This
  // should be called from a task posted on the child thread immediately after
  // thread start. The thread will be profiled until exit.
  static void StartOnChildThread(ProfilerThreadType thread);

  // Sets the instance of ThreadProfilerClient to provide embedder-specific
  // implementation logic. This instance must be set early, before any of the
  // above static methods are called.
  static void SetClient(std::unique_ptr<ThreadProfilerClient> client);

  // Retrieve the ThreadProfilerClient instance provided via SetClient().
  static ThreadProfilerClient* GetClient();

 private:
  class WorkIdRecorder;

  // Creates the profiler. The task runner will be supplied for child threads
  // but not for main threads.
  explicit ThreadProfiler(
      ProfilerThreadType thread,
      scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner =
          scoped_refptr<base::SingleThreadTaskRunner>());

  // Creates a sampling profiler, for either the startup or periodic profiling.
  std::unique_ptr<base::StackSamplingProfiler> CreateSamplingProfiler(
      base::StackSamplingProfiler::SamplingParams sampling_params,
      CallStackProfileParams::Trigger trigger,
      base::OnceClosure builder_completed_callback);

  // Posts a task on |owning_thread_task_runner| to start the next periodic
  // sampling collection on the completion of the previous collection.
  static void OnPeriodicCollectionCompleted(
      scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner,
      base::WeakPtr<ThreadProfiler> thread_profiler);

  // Sets the task runner when profiling on the main thread. This occurs in a
  // separate call from CreateAndStartOnMainThread so that startup profiling can
  // occur prior to message loop start.
  void SetMainThreadTaskRunnerImpl(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Posts a delayed task to start the next periodic sampling collection.
  void ScheduleNextPeriodicCollection();

  // Creates a new periodic profiler and initiates a collection with it.
  void StartPeriodicSamplingCollection();

  const ProfilerProcessType process_;
  const ProfilerThreadType thread_;

  scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

  std::unique_ptr<WorkIdRecorder> work_id_recorder_;

  base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>
      aux_unwinder_factory_;

  std::unique_ptr<base::StackSamplingProfiler> startup_profiler_;

  std::unique_ptr<base::StackSamplingProfiler> periodic_profiler_;
  std::unique_ptr<base::PeriodicSamplingScheduler> periodic_sampling_scheduler_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ThreadProfiler> weak_factory_{this};
};

}  // namespace sampling_profiler

#endif  // COMPONENTS_SAMPLING_PROFILER_THREAD_PROFILER_H_
