// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include <cmath>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"

namespace {

// Sets heap sampling interval in bytes.
const char kHeapProfilerSamplingRate[] = "sampling-rate";

constexpr base::TimeDelta kHeapCollectionInterval =
    base::TimeDelta::FromHours(24);

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  return -std::log(base::RandDouble()) * mean;
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
  ScheduleNextSnapshot(stopped_);
}

// static
void HeapProfilerController::ScheduleNextSnapshot(
    scoped_refptr<StoppedFlag> stopped) {
  base::PostDelayedTask(
      FROM_HERE, {base::ThreadPool(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&HeapProfilerController::TakeSnapshot, std::move(stopped)),
      RandomInterval(kHeapCollectionInterval));
}

// static
void HeapProfilerController::TakeSnapshot(
    scoped_refptr<StoppedFlag> stopped) {
  if (stopped->data.IsSet())
    return;
  RetrieveAndSendSnapshot();
  ScheduleNextSnapshot(std::move(stopped));
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot() {
  std::vector<base::SamplingHeapProfiler::Sample> samples =
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
      metrics::CallStackProfileParams::BROWSER_PROCESS,
      metrics::CallStackProfileParams::UNKNOWN_THREAD,
      metrics::CallStackProfileParams::PERIODIC_HEAP_COLLECTION);
  metrics::CallStackProfileBuilder profile_builder(params);

  for (const base::SamplingHeapProfiler::Sample& sample : samples) {
    std::vector<base::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    size_t count = std::max<size_t>(
        static_cast<size_t>(
            std::llround(static_cast<double>(sample.total) / sample.size)),
        1);
    profile_builder.OnSampleCompleted(std::move(frames), sample.total, count);
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
