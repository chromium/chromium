// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <cmath>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "base/profiler/module_cache.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/services/heap_profiling/public/cpp/merge_samples.h"

namespace {

// Sets heap sampling interval in bytes.
const char kHeapProfilerSamplingRate[] = "sampling-rate";

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  return -std::log(base::RandDouble()) * mean;
}

// Returns collection interval by trying these steps:
//  - get from command line if available to allow override for a single client
//  - get from finch if available to allow experiment with different intervals
//  - return default interval that is best suited for current OS
int GetCollectionIntervalInMinutes() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  // Default on iOS is equal to mean value of up process time. Android is more
  // similar to iOS than to Desktop.
  const int kDefaultValueInMinutes = 30;
#else
  const int kDefaultValueInMinutes = 24 * 60;
#endif

  return base::GetFieldTrialParamByFeatureAsInt(
      metrics::CallStackProfileMetricsProvider::kHeapProfilerReporting,
      "heap-profiler-collection-interval-minutes", kDefaultValueInMinutes);
}

}  // namespace

HeapProfilerController::HeapProfilerController()
    : stopped_(base::MakeRefCounted<StoppedFlag>()) {}

HeapProfilerController::~HeapProfilerController() {
  stopped_->data.Set();
}

void HeapProfilerController::Start() {
  if (!base::FeatureList::IsEnabled(
          metrics::CallStackProfileMetricsProvider::kHeapProfilerReporting)) {
    return;
  }
  int sampling_rate = base::GetFieldTrialParamByFeatureAsInt(
      metrics::CallStackProfileMetricsProvider::kHeapProfilerReporting,
      kHeapProfilerSamplingRate, 0);
  if (sampling_rate > 0)
    base::SamplingHeapProfiler::Get()->SetSamplingInterval(sampling_rate);
  base::SamplingHeapProfiler::Get()->Start();
  const int interval = GetCollectionIntervalInMinutes();
  DCHECK_GT(interval, 0);
  ScheduleNextSnapshot(stopped_, base::Minutes(interval));
}

// static
void HeapProfilerController::ScheduleNextSnapshot(
    scoped_refptr<StoppedFlag> stopped,
    base::TimeDelta heap_collection_interval) {
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&HeapProfilerController::TakeSnapshot, std::move(stopped),
                     heap_collection_interval),
      RandomInterval(heap_collection_interval));
}

// static
void HeapProfilerController::TakeSnapshot(
    scoped_refptr<StoppedFlag> stopped,
    base::TimeDelta heap_collection_interval) {
  if (stopped->data.IsSet())
    return;
  RetrieveAndSendSnapshot();
  ScheduleNextSnapshot(std::move(stopped), heap_collection_interval);
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot() {
  std::vector<Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  if (samples.empty())
    return;

  size_t malloc_usage =
      base::ProcessMetrics::CreateCurrentProcessMetrics()->GetMallocUsage();
  int malloc_usage_mb = static_cast<int>(malloc_usage >> 20);
  base::UmaHistogramMemoryLargeMB("Memory.HeapProfiler.Browser.Malloc",
                                  malloc_usage_mb);

  base::ModuleCache module_cache;
  metrics::CallStackProfileParams params(
      metrics::CallStackProfileParams::Process::kBrowser,
      metrics::CallStackProfileParams::Thread::kUnknown,
      metrics::CallStackProfileParams::Trigger::kPeriodicHeapCollection);
  metrics::CallStackProfileBuilder profile_builder(params);

  heap_profiling::SampleMap merged_samples =
      heap_profiling::MergeSamples(samples);

  for (auto& pair : merged_samples) {
    const Sample& sample = pair.first;
    const heap_profiling::SampleValue& value = pair.second;

    std::vector<base::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    // Heap "samples" represent allocation stacks aggregated over time so do not
    // have a meaningful timestamp.
    profile_builder.OnSampleCompleted(std::move(frames), base::TimeTicks(),
                                      value.total, value.count);
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
