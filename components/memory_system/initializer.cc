// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/initializer.h"

#include <string_view>

#include "components/memory_system/memory_system.h"
#include "components/sampling_profiler/process_type.h"

namespace memory_system {

Initializer::Initializer() = default;
Initializer::~Initializer() = default;

Initializer& Initializer::SetGwpAsanParameters(bool boost_sampling,
                                               std::string_view process_type) {
  gwp_asan_parameters_.emplace(boost_sampling, process_type);
  return *this;
}

Initializer& Initializer::SetProfilingClientParameters(
    version_info::Channel channel,
    sampling_profiler::ProfilerProcessType process_type) {
  profiling_client_parameters_.emplace(channel, process_type);
  return *this;
}

Initializer& Initializer::SetDispatcherParameters(
    DispatcherParameters::PoissonAllocationSamplerInclusion
        poisson_allocation_sampler_inclusion,
    DispatcherParameters::AllocationTraceRecorderInclusion
        allocation_trace_recorder_inclusion,
    std::string_view process_type) {
  dispatcher_parameters_.emplace(poisson_allocation_sampler_inclusion,
                                 allocation_trace_recorder_inclusion,
                                 process_type);
  return *this;
}

void Initializer::Initialize(MemorySystem& memory_system) const {
  memory_system.Initialize(gwp_asan_parameters_, profiling_client_parameters_,
                           dispatcher_parameters_);
}

}  // namespace memory_system
