// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_THREAD_PROFILER_H_
#define CHROME_COMMON_PROFILER_THREAD_PROFILER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/metrics/call_stack_profile_params.h"

// PeriodicSamplingScheduler repeatedly schedules periodic sampling of the
// thread through calls to GetTimeToNextCollection(). This class is exposed
// to allow testing.
class PeriodicSamplingScheduler {
 public:
  PeriodicSamplingScheduler(base::TimeDelta sampling_duration,
                            double fraction_of_execution_time_to_sample,
                            base::TimeTicks start_time);

  PeriodicSamplingScheduler(const PeriodicSamplingScheduler&) = delete;
  PeriodicSamplingScheduler& operator=(const PeriodicSamplingScheduler&) =
      delete;

  virtual ~PeriodicSamplingScheduler();

  // Returns the amount of time between now and the next collection.
  base::TimeDelta GetTimeToNextCollection();

 protected:
  // Virtual to provide seams for test use.
  virtual double RandDouble() const;
  virtual base::TimeTicks Now() const;

 private:
  const base::TimeDelta period_duration_;
  const base::TimeDelta sampling_duration_;
  base::TimeTicks period_start_time_;
};

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
  static void StartOnChildThread(
      metrics::CallStackProfileParams::Thread thread);

  // Returns true if called within a child process that will collect profiles
  // through a CallStackProfileCollector. If so,
  // metrics::CallStackProfileBuilder::SetParentProfileCollectorForChildProcess
  // must be called to to bind the interface through which a profile is sent
  // back to the browser process.
  //
  // Note that the metrics::CallStackProfileCollector interface also must be
  // exposed to the child process.
  static bool ShouldCollectProfilesForChildProcess();

 private:
  class WorkIdRecorder;

  // Creates the profiler. The task runner will be supplied for child threads
  // but not for main threads.
  ThreadProfiler(
      metrics::CallStackProfileParams::Thread thread,
      scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner =
          scoped_refptr<base::SingleThreadTaskRunner>());

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

  const metrics::CallStackProfileParams::Process process_;
  const metrics::CallStackProfileParams::Thread thread_;

  scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

  std::unique_ptr<WorkIdRecorder> work_id_recorder_;

  base::RepeatingCallback<std::unique_ptr<base::Unwinder>()>
      aux_unwinder_factory_;

  std::unique_ptr<base::StackSamplingProfiler> startup_profiler_;

  std::unique_ptr<base::StackSamplingProfiler> periodic_profiler_;
  std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<ThreadProfiler> weak_factory_{this};
};

#endif  // CHROME_COMMON_PROFILER_THREAD_PROFILER_H_
