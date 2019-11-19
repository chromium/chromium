// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILER_MAIN_THREAD_STACK_SAMPLING_PROFILER_H_
#define CHROME_COMMON_PROFILER_MAIN_THREAD_STACK_SAMPLING_PROFILER_H_

#include <memory>

#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"

class ThreadProfiler;

// A wrapper class that begins profiling stack samples upon construction, and
// ensures correct shutdown behavior on destruction. Should only be used on the
// main thread of a process. Samples are collected for the thread of the current
// process where this object is constructed, and only if profiling is enabled
// for the thread. This data is used to understand startup performance behavior,
// and the object should therefore be created as early during initialization as
// possible.
class MainThreadStackSamplingProfiler {
 public:
  MainThreadStackSamplingProfiler();
  ~MainThreadStackSamplingProfiler();

 private:
  // A profiler that periodically samples stack traces. Used to understand
  // thread and process startup behavior.
  std::unique_ptr<ThreadProfiler> sampling_profiler_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadStackSamplingProfiler);
};

#endif  //  CHROME_COMMON_PROFILER_MAIN_THREAD_STACK_SAMPLING_PROFILER_H_
