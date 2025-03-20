// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/memory_system_features.h"

#include "base/clang_profiling_buildflags.h"
#include "build/build_config.h"
#include "build/config/compiler/compiler_buildflags.h"
#include "partition_alloc/buildflags.h"

namespace memory_system::features {

BASE_FEATURE(kAllocationTraceRecorder,
             "AllocationTraceRecorder",
#if BUILDFLAG(CLANG_PGO_PROFILING) || BUILDFLAG(USE_CLANG_COVERAGE)
             // If creating a profiling build include the allocation recorder
             // unconditionally. This way we ensure that the recorder is covered
             // by the profile even if the profiling device doesn't support MTE.
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// If enabled, force the Allocation Trace Recorder to capture data in all
// processes even if MTE (Memory Tagging Extension) is unavailable.
BASE_FEATURE_PARAM(
    bool,
    kAllocationTraceRecorderForceAllProcesses,
    &kAllocationTraceRecorder,
    "atr_force_all_processes",
#if BUILDFLAG(CLANG_PGO_PROFILING) || BUILDFLAG(USE_CLANG_COVERAGE)
    // If creating a profiling build include the allocation recorder
    // unconditionally. This way we ensure that the recorder is covered by the
    // profile even if the profiling device doesn't support MTE.
    true
#else
    false
#endif
);

}  // namespace memory_system::features
