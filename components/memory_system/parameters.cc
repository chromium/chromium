// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/parameters.h"

#include <string_view>

#include "components/sampling_profiler/process_type.h"

namespace memory_system {

GwpAsanParameters::GwpAsanParameters(bool boost_sampling,
                                     std::string_view process_type)
    : boost_sampling(boost_sampling), process_type(process_type) {}

ProfilingClientParameters::ProfilingClientParameters(
    version_info::Channel channel,
    sampling_profiler::ProfilerProcessType process_type)
    : channel(channel), process_type(process_type) {}

DispatcherParameters::DispatcherParameters(
    PoissonAllocationSamplerInclusion poisson_allocation_sampler_inclusion,
    AllocationTraceRecorderInclusion allocation_trace_recorder_inclusion,
    std::string_view process_type)
    : poisson_allocation_sampler_inclusion(
          poisson_allocation_sampler_inclusion),
      allocation_trace_recorder_inclusion(allocation_trace_recorder_inclusion),
      process_type(process_type) {}

}  // namespace memory_system
