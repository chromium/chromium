// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/heap_profiling/public/cpp/profiling_client.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if !BUILDFLAG(IS_IOS)
#include "components/services/heap_profiling/public/cpp/heap_profiling_trace_source.h"
#endif

#if BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_interception_apple.h"
#endif  // BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

using base::allocator::dispatcher::AllocationSubsystem;

namespace heap_profiling {

ProfilingClient::ProfilingClient() = default;
ProfilingClient::~ProfilingClient() = default;

void ProfilingClient::BindToInterface(
    mojo::PendingReceiver<mojom::ProfilingClient> receiver) {
  receivers_.Add(this, std::move(receiver));
}

#if BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
void ShimNewMallocZonesAndReschedule(base::Time end_time,
                                     base::TimeDelta delay) {
  allocator_shim::ShimNewMallocZones();

  if (base::Time::Now() > end_time) {
    return;
  }

  base::TimeDelta next_delay = delay * 2;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ShimNewMallocZonesAndReschedule, end_time, next_delay),
      delay);
}
#endif  // BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // && BULDFLAG(USE_ALLOCATOR_SHIM)

void ProfilingClient::StartProfiling(mojom::ProfilingParamsPtr params,
                                     StartProfilingCallback callback) {
  if (started_profiling_)
    return;
  started_profiling_ = true;

#if BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  // On macOS, this call is necessary to shim malloc zones that were created
  // after startup. This cannot be done during shim initialization because the
  // task scheduler has not yet been initialized.
  //
  // Wth PartitionAlloc, the shims are already in place, calling this leads to
  // an infinite loop.
  base::Time end_time = base::Time::Now() + base::Minutes(1);
  base::TimeDelta initial_delay = base::Seconds(1);
  ShimNewMallocZonesAndReschedule(end_time, initial_delay);
#endif  // BUILDFLAG(IS_APPLE) && !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
        // && PA_BUILDFLAG(USE_ALLOCATOR_SHIM)

  StartProfilingInternal(std::move(params), std::move(callback));

#if !BUILDFLAG(IS_IOS)
  // Create trace source so that it registers itself to the tracing system.
  HeapProfilingTraceSource::GetInstance();
#endif
}

namespace {

bool g_initialized_ = false;

base::Lock& GetOnInitAllocatorShimLock() {
  static base::NoDestructor<base::Lock> instance;
  return *instance;
}

base::OnceClosure& GetOnInitAllocatorShimCallback() {
  static base::NoDestructor<base::OnceClosure> instance;
  return *instance;
}

scoped_refptr<base::TaskRunner>& GetOnInitAllocatorShimTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::TaskRunner>> instance;
  return *instance;
}

// In NATIVE stack mode, whether to insert stack names into the backtraces.
bool g_include_thread_names = false;

void InitAllocationRecorder(mojom::ProfilingParamsPtr params) {
  using base::trace_event::AllocationContextTracker;
  using CaptureMode = base::trace_event::AllocationContextTracker::CaptureMode;

  // Must be done before hooking any functions that make stack traces.
  base::debug::EnableInProcessStackDumping();

  if (params->stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES) {
    g_include_thread_names = true;
    base::SamplingHeapProfiler::Get()->SetRecordThreadNames(true);
  }

  switch (params->stack_mode) {
    case mojom::StackMode::NATIVE_WITH_THREAD_NAMES:
    case mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES:
      // This would track task contexts only.
      AllocationContextTracker::SetCaptureMode(CaptureMode::kNativeStack);
      break;
  }
}

// Notifies the test clients that allocation hooks have been initialized.
void AllocatorHooksHaveBeenInitialized() {
  base::AutoLock lock(GetOnInitAllocatorShimLock());
  g_initialized_ = true;
  if (!GetOnInitAllocatorShimCallback())
    return;
  GetOnInitAllocatorShimTaskRunner()->PostTask(
      FROM_HERE, std::move(GetOnInitAllocatorShimCallback()));
}

mojom::AllocatorType ConvertType(AllocationSubsystem type) {
  switch (type) {
    case AllocationSubsystem::kAllocatorShim:
      return mojom::AllocatorType::kMalloc;
    case AllocationSubsystem::kPartitionAllocator:
      return mojom::AllocatorType::kPartitionAlloc;
    case AllocationSubsystem::kManualForTesting:
      NOTREACHED_IN_MIGRATION();
      return mojom::AllocatorType::kMalloc;
  }
}

}  // namespace

void InitTLSSlot() {
  base::SamplingHeapProfiler::Init();
}

bool SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  base::AutoLock lock(GetOnInitAllocatorShimLock());
  if (g_initialized_)
    return true;
  GetOnInitAllocatorShimCallback() = std::move(callback);
  GetOnInitAllocatorShimTaskRunner() = task_runner;
  return false;
}

void ProfilingClient::StartProfilingInternal(mojom::ProfilingParamsPtr params,
                                             StartProfilingCallback callback) {
  size_t sampling_rate = params->sampling_rate;
  InitAllocationRecorder(std::move(params));
  auto* profiler = base::SamplingHeapProfiler::Get();
  profiler->SetSamplingInterval(sampling_rate);
  profiler->Start();
  AllocatorHooksHaveBeenInitialized();
  std::move(callback).Run();
}

void ProfilingClient::RetrieveHeapProfile(
    RetrieveHeapProfileCallback callback) {
  auto* profiler = base::SamplingHeapProfiler::Get();
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      profiler->GetSamples(/*profile_id=*/0);
  // It's important to retrieve strings after samples, as otherwise it could
  // miss a string referenced by a sample.
  std::vector<const char*> strings = profiler->GetStrings();
  mojom::HeapProfilePtr profile = mojom::HeapProfile::New();
  profile->samples.reserve(samples.size());
  std::unordered_set<const char*> thread_names;
  for (const auto& sample : samples) {
    auto mojo_sample = mojom::HeapProfileSample::New();
    mojo_sample->allocator = ConvertType(sample.allocator);
    mojo_sample->size = sample.size;
    mojo_sample->total = sample.total;
    mojo_sample->context_id = reinterpret_cast<uintptr_t>(sample.context);
    mojo_sample->stack.reserve(sample.stack.size() +
                               (g_include_thread_names ? 1 : 0));
    mojo_sample->stack.insert(
        mojo_sample->stack.end(),
        reinterpret_cast<const uintptr_t*>(sample.stack.data()),
        reinterpret_cast<const uintptr_t*>(sample.stack.data() +
                                           sample.stack.size()));
    if (g_include_thread_names) {
      static const char* kUnknownThreadName = "<unknown>";
      const char* thread_name =
          sample.thread_name ? sample.thread_name : kUnknownThreadName;
      mojo_sample->stack.push_back(reinterpret_cast<uintptr_t>(thread_name));
      thread_names.insert(thread_name);
    }
    profile->samples.push_back(std::move(mojo_sample));
  }
  profile->strings.reserve(strings.size() + thread_names.size());
  for (const char* string : strings)
    profile->strings.emplace(reinterpret_cast<uintptr_t>(string), string);
  for (const char* string : thread_names)
    profile->strings.emplace(reinterpret_cast<uintptr_t>(string), string);

  std::move(callback).Run(std::move(profile));
}

void ProfilingClient::AddHeapProfileToTrace(
    AddHeapProfileToTraceCallback callback) {
  auto* profiler = base::SamplingHeapProfiler::Get();
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      profiler->GetSamples(/*profile_id=*/0);

#if !BUILDFLAG(IS_IOS)
  bool success =
      HeapProfilingTraceSource::GetInstance()->AddToTraceIfEnabled(samples);
#else
  bool success = false;
  // Tracing is not supported in iOS.
  NOTREACHED_IN_MIGRATION();
#endif

  std::move(callback).Run(success);
}

}  // namespace heap_profiling
