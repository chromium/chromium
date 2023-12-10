// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_PARAMS_H_
#define COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_PARAMS_H_

#include "base/time/time.h"

namespace metrics {

// Parameters to pass back to the metrics provider.
struct CallStackProfileParams {
  // The process in which the collection occurred.
  enum class Process {
    kUnknown,
    kBrowser,
    kRenderer,
    kGpu,
    kUtility,
    kZygote,
    kSandboxHelper,
    kPpapiPlugin,
    kNetworkService,

    kMax = kNetworkService,
  };

  // The thread from which the collection occurred.
  enum class Thread {
    kUnknown,

    // Each process has a 'main thread'. In the Browser process, the 'main
    // thread' is also often called the 'UI thread'.
    kMain,
    kIo,

    // Compositor thread (can be in both renderer and gpu processes).
    kCompositor,

    // Service worker thread.
    kServiceWorker,

    kMax = kServiceWorker,
  };

  // The event that triggered the profile collection.
  enum class Trigger {
    kUnknown,
    kProcessStartup,
    kJankyTask,
    kThreadHung,
    kPeriodicCollection,
    kPeriodicHeapCollection,
    kLast = kPeriodicHeapCollection
  };

  // The default constructor is required for mojo and should not be used
  // otherwise. A valid trigger should always be specified.
  constexpr CallStackProfileParams() = default;

  constexpr CallStackProfileParams(
      Process process,
      Thread thread,
      Trigger trigger,
      base::TimeDelta time_offset = base::TimeDelta())
      : process(process),
        thread(thread),
        trigger(trigger),
        time_offset(time_offset) {}

  // The collection process.
  Process process = Process::kUnknown;

  // The collection thread.
  Thread thread = Thread::kUnknown;

  // The triggering event.
  Trigger trigger = Trigger::kUnknown;

  // The time of the profile, since roughly the start of the process being
  // profiled. 0 indicates that the time is not reported.
  base::TimeDelta time_offset;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACKS_CALL_STACK_PROFILE_PARAMS_H_
