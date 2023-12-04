// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_SYSTEM_INITIALIZER_H_
#define COMPONENTS_MEMORY_SYSTEM_INITIALIZER_H_

#include "base/strings/string_piece.h"
#include "components/memory_system/parameters.h"
#include "components/metrics/call_stacks/call_stack_profile_params.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace memory_system {

class MemorySystem;

// A convenience class which allows different parts of the configuration being
// set in an expressive way and handing them over to the memory system upon
// initialization.
class Initializer {
 public:
  Initializer();
  ~Initializer();

  Initializer& SetGwpAsanParameters(bool boost_sampling,
                                    base::StringPiece process_type);
  Initializer& SetProfilingClientParameters(
      version_info::Channel channel,
      metrics::CallStackProfileParams::Process process_type);
  Initializer& SetDispatcherParameters(
      DispatcherParameters::PoissonAllocationSamplerInclusion
          poisson_allocation_sampler_inclusion,
      DispatcherParameters::AllocationTraceRecorderInclusion
          allocation_trace_recorder_inclusion,
      base::StringPiece process_type);

  void Initialize(MemorySystem& memory_system) const;

 private:
  absl::optional<GwpAsanParameters> gwp_asan_parameters_;
  absl::optional<ProfilingClientParameters> profiling_client_parameters_;
  absl::optional<DispatcherParameters> dispatcher_parameters_;
};

}  // namespace memory_system
#endif  // COMPONENTS_MEMORY_SYSTEM_INITIALIZER_H_
