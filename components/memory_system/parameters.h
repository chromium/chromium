// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORY_SYSTEM_PARAMETERS_H_
#define COMPONENTS_MEMORY_SYSTEM_PARAMETERS_H_

#include <string>

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/version_info/channel.h"

namespace memory_system {

// Configuration objects for all memory subsystem components. The parameters are
// divided by component to initialize. The type of the data corresponds to the
// type used by the component. Therefore, same data may appear multiple times
// and with varying signatures.

// GWP-ASan specific parameters, please see
// components/gwp_asan/client/gwp_asan.h for details.
struct COMPONENT_EXPORT(MEMORY_SYSTEM) GwpAsanParameters {
  GwpAsanParameters(bool boost_sampling, base::StringPiece process_type);

  bool boost_sampling;
  std::string process_type;
};

// ProfilingClient specific parameters, please see
// components/heap_profiling/in_process/heap_profiler_controller.h for details.
struct COMPONENT_EXPORT(MEMORY_SYSTEM) ProfilingClientParameters {
  ProfilingClientParameters(
      version_info::Channel channel,
      metrics::CallStackProfileParams::Process process_type);

  version_info::Channel channel;
  metrics::CallStackProfileParams::Process process_type;
};

}  // namespace memory_system
#endif  // COMPONENTS_MEMORY_SYSTEM_PARAMETERS_H_
