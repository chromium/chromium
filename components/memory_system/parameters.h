// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_SYSTEM_PARAMETERS_H_
#define COMPONENTS_MEMORY_SYSTEM_PARAMETERS_H_

#include <string>
#include <string_view>

#include "components/sampling_profiler/process_type.h"
#include "components/version_info/channel.h"

namespace memory_system {

// Configuration objects for all memory subsystem components. The parameters are
// divided by component. The type of the data corresponds to the type used by
// the component. Therefore, the same data may appear multiple times and with
// varying signatures.

// GWP-ASan specific parameters, please see
// components/gwp_asan/client/gwp_asan.h for details.
struct GwpAsanParameters {
  GwpAsanParameters(bool boost_sampling, std::string_view process_type);

  bool boost_sampling;
  std::string process_type;
};

// ProfilingClient specific parameters, please see
// components/heap_profiling/in_process/heap_profiler_controller.h for details.
struct ProfilingClientParameters {
  ProfilingClientParameters(
      version_info::Channel channel,
      sampling_profiler::ProfilerProcessType process_type);

  version_info::Channel channel;
  sampling_profiler::ProfilerProcessType process_type;
};

// Dispatcher specific parameters, please see
// base/allocator/dispatcher/initializer.h for details.
struct DispatcherParameters {
  // The way the dispatcher should include the PoissonAllocationSampler
  enum class PoissonAllocationSamplerInclusion {
    // Do not include.
    kIgnore,
    // Let the memory-system decide whether to include depending on whether
    // another component (e.g. ProfilingClient) needs it.
    kDynamic,
    // Always include, even if no other component requires it. This is intended
    // for cases where we do not know for sure if any client of
    // PoissonAllocationSampler will become enabled in the course of the
    // runtime.
    //
    // TODO(crbug.com/40062835): Clarify for which components we need to
    // enforce PoissonAllocationSampler.
    kEnforce,
  };

  // The way the dispatcher should include the AllocationTraceRecorder.
  enum class AllocationTraceRecorderInclusion {
    // Do not include.
    kIgnore,
    // Let the memory-system decide whether to include. The trace recorder is
    // currently included if the CPU has MTE support.
    kDynamic,
  };

  explicit DispatcherParameters(
      PoissonAllocationSamplerInclusion poisson_allocation_sampler_inclusion,
      AllocationTraceRecorderInclusion allocation_trace_recorder_inclusion,
      std::string_view process_type);

  PoissonAllocationSamplerInclusion poisson_allocation_sampler_inclusion;
  AllocationTraceRecorderInclusion allocation_trace_recorder_inclusion;
  std::string process_type;
};

}  // namespace memory_system
#endif  // COMPONENTS_MEMORY_SYSTEM_PARAMETERS_H_
