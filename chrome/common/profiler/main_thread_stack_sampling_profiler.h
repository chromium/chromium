// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_MAIN_THREAD_STACK_SAMPLING_PROFILER_H_
#define CHROME_COMMON_PROFILER_MAIN_THREAD_STACK_SAMPLING_PROFILER_H_

#include <memory>

namespace sampling_profiler {
class ThreadProfiler;
}

// A wrapper class that installs unwinder prerequisites and begins profiling
// stack samples upon construction, and ensures correct shutdown behavior on
// destruction. Should only be used on the main thread of a process. Samples are
// collected for the thread of the current process where this object is
// constructed, and only if profiling is enabled for the thread. This data is
// used to understand startup performance behavior, and the object should
// therefore be created as early during initialization as possible.
class MainThreadStackSamplingProfiler {
 public:
  MainThreadStackSamplingProfiler();

  MainThreadStackSamplingProfiler(const MainThreadStackSamplingProfiler&) =
      delete;
  MainThreadStackSamplingProfiler& operator=(
      const MainThreadStackSamplingProfiler&) = delete;

  ~MainThreadStackSamplingProfiler();

 private:
  // A profiler that periodically samples stack traces. Used to understand
  // thread and process startup behavior.
  std::unique_ptr<sampling_profiler::ThreadProfiler> sampling_profiler_;
};

#endif  // CHROME_COMMON_PROFILER_MAIN_THREAD_STACK_SAMPLING_PROFILER_H_
