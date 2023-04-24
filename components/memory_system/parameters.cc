// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/parameters.h"

namespace memory_system {

GwpAsanParameters::GwpAsanParameters(bool boost_sampling,
                                     base::StringPiece process_type)
    : boost_sampling(boost_sampling), process_type(process_type) {}

ProfilingClientParameters::ProfilingClientParameters(
    version_info::Channel channel,
    metrics::CallStackProfileParams::Process process_type)
    : channel(channel), process_type(process_type) {}

DispatcherParameters::DispatcherParameters(
    PoissonAllocationSamplerInclusion poisson_allocation_sampler_inclusion,
    AllocationTraceRecorderInclusion allocation_trace_recorder_inclusion)
    : poisson_allocation_sampler_inclusion(
          poisson_allocation_sampler_inclusion),
      allocation_trace_recorder_inclusion(allocation_trace_recorder_inclusion) {
}

}  // namespace memory_system