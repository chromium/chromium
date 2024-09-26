// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAMPLING_PROFILER_CALL_STACK_PROFILE_PARAMS_H_
#define COMPONENTS_SAMPLING_PROFILER_CALL_STACK_PROFILE_PARAMS_H_

#include "base/time/time.h"
#include "components/sampling_profiler/process_type.h"

namespace sampling_profiler {

// Parameters to pass back to the metrics provider.
// TODO(crbug.com/354124876): Revisit whether this type needs to live in //base,
// once the core logic has been moved.
struct CallStackProfileParams {
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
      ProfilerProcessType process,
      ProfilerThreadType thread,
      Trigger trigger,
      base::TimeDelta time_offset = base::TimeDelta())
      : process(process),
        thread(thread),
        trigger(trigger),
        time_offset(time_offset) {}

  // The collection process.
  ProfilerProcessType process = ProfilerProcessType::kUnknown;

  // The collection thread.
  ProfilerThreadType thread = ProfilerThreadType::kUnknown;

  // The triggering event.
  Trigger trigger = Trigger::kUnknown;

  // The time of the profile, since roughly the start of the process being
  // profiled. 0 indicates that the time is not reported.
  base::TimeDelta time_offset;
};

}  // namespace sampling_profiler

#endif  // COMPONENTS_SAMPLING_PROFILER_CALL_STACK_PROFILE_PARAMS_H_
