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

#if BUILDFLAG(IS_WIN)
// If enabled, every allocation and free is reported as an ETW event so that
// external profilers can attribute PartitionAlloc and allocator-shim memory to
// call stacks. Events are only written while a tracing session has enabled the
// provider, but installing the observer alone makes every allocation go through
// the dispatcher, so this stays off by default. Being a feature rather than a
// switch, it propagates to child processes for free.
//
// This is a diagnostic to turn on by hand for a profiling run, not something
// to enable for users or in a study: the dispatcher hook costs a few percent
// even with no session listening.
BASE_FEATURE(kAllocationEtwTracing, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<AllocationEtwTracingProcesses>::Option
    kAllocationEtwTracingProcessesOptions[] = {
        {AllocationEtwTracingProcesses::kBrowserOnly, "browser-only"},
        {AllocationEtwTracingProcesses::kRendererOnly, "renderer-only"},
        {AllocationEtwTracingProcesses::kGpuOnly, "gpu-only"},
        {AllocationEtwTracingProcesses::kBrowserAndRenderer,
         "browser-and-renderer"},
        {AllocationEtwTracingProcesses::kBrowserAndGpu, "browser-and-gpu"},
        {AllocationEtwTracingProcesses::kNonRenderer, "non-renderer"},
        {AllocationEtwTracingProcesses::kAllProcesses, "all-processes"},
};

// Processes left out here never install the observer, so unlike processes that
// a tracing session deselects through the process type keywords, they do not
// pay for the allocator hooks either. The name and values follow
// PartitionAlloc's "enabled-processes" parameter.
BASE_FEATURE_ENUM_PARAM(AllocationEtwTracingProcesses,
                        kAllocationEtwTracingProcesses,
                        &kAllocationEtwTracing,
                        "enabled-processes",
                        AllocationEtwTracingProcesses::kAllProcesses,
                        &kAllocationEtwTracingProcessesOptions);
#endif

}  // namespace memory_system::features
