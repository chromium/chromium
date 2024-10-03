// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/memory_system_features.h"

#include "build/build_config.h"
#include "components/memory_system/buildflags.h"
#include "partition_alloc/buildflags.h"

namespace memory_system::features {

BASE_FEATURE(kAllocationTraceRecorder,
             "AllocationTraceRecorder",
#if BUILDFLAG(FORCE_ALLOCATION_TRACE_RECORDER)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, force the Allocation Trace Recorder to capture data in all
// processes even if MTE (Memory Tagging Extension) is unavailable.
BASE_FEATURE_PARAM(bool,
                   kAllocationTraceRecorderForceAllProcesses,
                   &kAllocationTraceRecorder,
                   "atr_force_all_processes",
#if BUILDFLAG(FORCE_ALLOCATION_TRACE_RECORDER)
                   true
#else
                   false
#endif
);

}  // namespace memory_system::features
