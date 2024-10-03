// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_system/memory_system.h"

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/allocator/dispatcher/initializer.h"
#include "base/debug/crash_logging.h"
#include "base/debug/debugging_buildflags.h"
#include "build/build_config.h"
#include "components/gwp_asan/buildflags/buildflags.h"
#include "components/memory_system/buildflags.h"
#include "components/memory_system/memory_system_features.h"
#include "components/memory_system/parameters.h"
#include "partition_alloc/buildflags.h"

#if BUILDFLAG(ENABLE_GWP_ASAN)
#include "components/gwp_asan/client/gwp_asan.h"  // nogncheck
#if BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crashpad.h"  // nogncheck
#endif
#endif

#if BUILDFLAG(IS_IOS) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/ios/ios_util.h"
#include "base/metrics/histogram_functions.h"
#include "partition_alloc/shim/allocator_interception_apple.h"
#include "partition_alloc/shim/allocator_shim.h"
#endif

// HeapProfilerController's dependencies are not compiled on iOS unless
// AllocatorShim is enabled.
#if !BUILDFLAG(IS_IOS) || PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#define HEAP_PROFILING_SUPPORTED 1
#else
#define HEAP_PROFILING_SUPPORTED 0
#endif

#if HEAP_PROFILING_SUPPORTED
#include "components/heap_profiling/in_process/heap_profiler_controller.h"  // nogncheck
#include "components/services/heap_profiling/public/cpp/profiling_client.h"  // nogncheck
#endif

#if HEAP_PROFILING_SUPPORTED
// If profiling is not supported, the PoissonAllocationSampler is removed from
// base, which causes linker errors. Since we need it only for the dispatcher,
// we include it only if both, dispatcher and heap-profiling, are enabled.
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
#include "base/cpu.h"
#include "base/debug/allocation_trace.h"
#include "components/allocation_recorder/crash_client/client.h"
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
#include "components/memory_system/allocation_trace_recorder_statistics_reporter.h"
#endif  // BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
#endif  // BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)

namespace memory_system {
namespace {

#if BUILDFLAG(IS_IOS) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
// Do not install allocator shim on iOS 13.4 due to high crash volume on this
// particular version of OS. TODO(crbug.com/40707342): Remove this workaround
// when/if the bug gets fixed.
bool ShouldInstallAllocatorShim() {
  return !base::ios::IsRunningOnOrLater(13, 4, 0) ||
         base::ios::IsRunningOnOrLater(13, 5, 0);
}
#endif

}  // namespace

struct MemorySystem::Impl {
  Impl();
  ~Impl();

  void Initialize(
      const std::optional<GwpAsanParameters>& gwp_asan_parameters,
      const std::optional<ProfilingClientParameters>&
          profiling_client_parameters,
      const std::optional<DispatcherParameters>& dispatcher_parameters);

 private:
  // Initialization functions for the various subsystems.

  // Structure of information passed from one step to another.
  struct InitializationData {
#if HEAP_PROFILING_SUPPORTED
    bool has_profiling_client_started = false;
#endif
  };

  // Initialize GWP-ASan with the given set of parameters.
  //
  // Initialization will be performed if support for GWP-ASan is compiled in. On
  // ChromeOs Crashpad must be enabled in addition.
  void InitializeGwpASan(const GwpAsanParameters& gwp_asan_parameters,
                         InitializationData& initialization_data);

  // Initialize HeapProfiler with the given set of parameters.
  void InitializeHeapProfiler(
      const ProfilingClientParameters& profiling_client_parameters,
      InitializationData& initialization_data);

  void InitializeDispatcher(const DispatcherParameters& dispatcher_parameters,
                            InitializationData& initialization_data);

  // Has the allocator shim been initialized successfully?
  bool IsAllocatorShimInitialized();

#if HEAP_PROFILING_SUPPORTED
  // Check if the the dispatcher should include the PoissonAllocationSampler as
  // observer.
  bool DispatcherIncludesPoissonAllocationSampler(
      const DispatcherParameters& dispatcher_parameters,
      const InitializationData& initialization_data);

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  // Check if the the dispatcher should include the AllocationTraceRecorder as
  // an observer.
  bool DispatcherIncludesAllocationTraceRecorder(
      const DispatcherParameters& dispatcher_parameters);
#endif

  std::unique_ptr<heap_profiling::HeapProfilerController>
      heap_profiler_controller_;
#endif

#if BUILDFLAG(IS_IOS) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  const bool should_install_allocator_shim_ = ShouldInstallAllocatorShim();
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  struct {
    // We must not delete the recorder upon shutdown. Firstly, we do not have a
    // possibility to remove an allocation hook reliably. So, once installed,
    // the recorder may constantly be used by the allocation hooks. Secondly,
    // the reporting may continue using the recorder event after destruction
    // (see AllocationTraceRecorderStatisticsReporter for details). Therefore,
    // we extend its lifetime as much as possible by making it an unmanaged
    // pointer and not deleting in the course of the destruction of the memory
    // system.
    raw_ptr<base::debug::tracer::AllocationTraceRecorder> recorder;
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
    internal::AllocationTraceRecorderStatisticsReporter reporting;
#endif
  } allocation_recording_;
#endif
};

MemorySystem::Impl::Impl() {
#if BUILDFLAG(IS_IOS) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (should_install_allocator_shim_) {
    allocator_shim::InitializeAllocatorShim();
  }
#endif

#if HEAP_PROFILING_SUPPORTED
  // The TLS slot used by the memlog allocator shim needs to be initialized
  // early to ensure that it gets assigned a low slot number. If it gets
  // initialized too late, the glibc TLS system will require a malloc call in
  // order to allocate storage for a higher slot number. Since malloc is hooked,
  // this causes re-entrancy into the allocator shim, while the TLS object is
  // partially-initialized, which the TLS object is supposed to protect again.
  heap_profiling::InitTLSSlot();
#endif
}

MemorySystem::Impl::~Impl() {
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
  allocation_recording_.reporting = {};
#endif

  if (allocation_recording_.recorder) {
    allocation_recorder::crash_client::UnregisterRecorderWithCrashpad();
  }

  // Do not delete the recorder that |allocation_recording_.recorder| points to
  // to prevent the allocations hooks and the reporting from operating on
  // potentially invalid data. See the declaration of
  // |allocation_recording_.recorder| for details.
#endif  // BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
}

void MemorySystem::Impl::Initialize(
    const std::optional<GwpAsanParameters>& gwp_asan_parameters,
    const std::optional<ProfilingClientParameters>& profiling_client_parameters,
    const std::optional<DispatcherParameters>& dispatcher_parameters) {
  if (!IsAllocatorShimInitialized()) {
    return;
  }

  InitializationData initialization_data;

  if (gwp_asan_parameters) {
    InitializeGwpASan(*gwp_asan_parameters, initialization_data);
  }

  if (profiling_client_parameters) {
    InitializeHeapProfiler(*profiling_client_parameters, initialization_data);
  }

  if (dispatcher_parameters) {
    InitializeDispatcher(*dispatcher_parameters, initialization_data);
  }
}

bool MemorySystem::Impl::IsAllocatorShimInitialized() {
#if BUILDFLAG(IS_IOS) && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (!should_install_allocator_shim_) {
    return false;
  }

  const bool malloc_intercepted = allocator_shim::AreMallocZonesIntercepted();
  base::UmaHistogramBoolean("IOS.Allocator.ShimInstalled", malloc_intercepted);

  return malloc_intercepted;
#else
  return true;
#endif
}

void MemorySystem::Impl::InitializeGwpASan(
    const GwpAsanParameters& gwp_asan_parameters,
    InitializationData& initialization_data) {
#if BUILDFLAG(ENABLE_GWP_ASAN)
  // LUD has the highest priority and the Extreme LUD has the lowest priority.
  // An allocator shim later installed has priority over the already-installed
  // shims.
  gwp_asan::MaybeEnableExtremeLightweightDetector(
      gwp_asan_parameters.boost_sampling,
      gwp_asan_parameters.process_type.c_str());

#if BUILDFLAG(ENABLE_GWP_ASAN_MALLOC)
  gwp_asan::EnableForMalloc(gwp_asan_parameters.boost_sampling,
                            gwp_asan_parameters.process_type);
#endif

#if BUILDFLAG(ENABLE_GWP_ASAN_PARTITIONALLOC)
  gwp_asan::EnableForPartitionAlloc(gwp_asan_parameters.boost_sampling,
                                    gwp_asan_parameters.process_type);
#endif

  gwp_asan::MaybeEnableLightweightDetector(
      gwp_asan_parameters.boost_sampling,
      gwp_asan_parameters.process_type.c_str());
#endif  // BUILDFLAG(ENABLE_GWP_ASAN)
}

void MemorySystem::Impl::InitializeHeapProfiler(
    const ProfilingClientParameters& profiling_client_parameters,
    InitializationData& initialization_data) {
#if HEAP_PROFILING_SUPPORTED
  heap_profiler_controller_ =
      std::make_unique<heap_profiling::HeapProfilerController>(
          profiling_client_parameters.channel,
          profiling_client_parameters.process_type);

  initialization_data.has_profiling_client_started =
      heap_profiler_controller_->StartIfEnabled();
#endif
}

#if HEAP_PROFILING_SUPPORTED
bool MemorySystem::Impl::DispatcherIncludesPoissonAllocationSampler(
    const DispatcherParameters& dispatcher_parameters,
    const InitializationData& initialization_data) {
  switch (dispatcher_parameters.poisson_allocation_sampler_inclusion) {
    case DispatcherParameters::PoissonAllocationSamplerInclusion::kEnforce:
      return true;
    case DispatcherParameters::PoissonAllocationSamplerInclusion::kDynamic:
      return initialization_data.has_profiling_client_started;
    case DispatcherParameters::PoissonAllocationSamplerInclusion::kIgnore:
      return false;
  }
}
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
bool MemorySystem::Impl::DispatcherIncludesAllocationTraceRecorder(
    const DispatcherParameters& dispatcher_parameters) {
  if (!base::FeatureList::IsEnabled(features::kAllocationTraceRecorder)) {
    return false;
  }

  if (features::kAllocationTraceRecorderForceAllProcesses.Get()) {
    // Note: Force enable Allocation Trace Recorder in all processes, even if
    // MTE (Memory Tagging Extension) is not present or is unavailable.
    return true;
  }

  switch (dispatcher_parameters.allocation_trace_recorder_inclusion) {
    case DispatcherParameters::AllocationTraceRecorderInclusion::kDynamic:
      return base::CPU::GetInstanceNoAllocation().has_mte();
    case DispatcherParameters::AllocationTraceRecorderInclusion::kIgnore:
      return false;
  }
}
#endif

void MemorySystem::Impl::InitializeDispatcher(
    const DispatcherParameters& dispatcher_parameters,
    InitializationData& initialization_data) {
#if HEAP_PROFILING_SUPPORTED
  // Include the PoissonAllocationSampler as an optional observer always, even
  // if the inclusion parameter is |kEnforce|. If we distinguish between
  // mandatory and optional, the nesting becomes a real mess once we add yet
  // another observer. Adding the value this way should be fine.
  const bool include_poisson_allocation_sampler =
      DispatcherIncludesPoissonAllocationSampler(dispatcher_parameters,
                                                 initialization_data);

  auto* const poisson_allocation_sampler =
      include_poisson_allocation_sampler ? base::PoissonAllocationSampler::Get()
                                         : nullptr;
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  const bool include_allocation_recorder =
      DispatcherIncludesAllocationTraceRecorder(dispatcher_parameters);

  static auto* const crash_key = base::debug::AllocateCrashKeyString(
      "allocation_trace_recorder", base::debug::CrashKeySize::Size32);
  base::debug::SetCrashKeyString(
      crash_key, include_allocation_recorder ? "enabled" : "disabled");

  if (include_allocation_recorder) {
    allocation_recording_.recorder =
        new base::debug::tracer::AllocationTraceRecorder();

    allocation_recorder::crash_client::RegisterRecorderWithCrashpad(
        *allocation_recording_.recorder);

#if BUILDFLAG(ENABLE_ALLOCATION_TRACE_RECORDER_FULL_REPORTING)
    allocation_recording_.reporting = {
        *allocation_recording_.recorder, dispatcher_parameters.process_type,
        base::Seconds(15), logging::LOGGING_ERROR};
#endif
  }
#endif  // BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)

  base::allocator::dispatcher::CreateInitializer()
#if HEAP_PROFILING_SUPPORTED
      .AddOptionalObservers(poisson_allocation_sampler)
#endif
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
      .AddOptionalObservers(allocation_recording_.recorder.get())
#endif
      .DoInitialize(base::allocator::dispatcher::Dispatcher::GetInstance());
}

MemorySystem::MemorySystem() : impl_(std::make_unique<Impl>()) {}

MemorySystem::~MemorySystem() = default;

void MemorySystem::Initialize(
    const std::optional<GwpAsanParameters>& gwp_asan_parameters,
    const std::optional<ProfilingClientParameters>& profiling_client_parameters,
    const std::optional<DispatcherParameters>& dispatcher_parameters) {
  impl_->Initialize(gwp_asan_parameters, profiling_client_parameters,
                    dispatcher_parameters);
}

}  // namespace memory_system
