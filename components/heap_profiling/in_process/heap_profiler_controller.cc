// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_controller.h"

#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/profiler/module_cache.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/heap_profiling/in_process/heap_profiler_parameters.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/services/heap_profiling/public/cpp/merge_samples.h"
#include "components/version_info/channel.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace heap_profiling {

namespace {

using ProfilingEnabled = HeapProfilerController::ProfilingEnabled;

// Records whether heap profiling is enabled for this process.
// HeapProfilerController will set this on creation, and reset it to
// kNoController on destruction, so that it's always reset to the default
// state after each unit test that creates a HeapProfilerController.
ProfilingEnabled g_profiling_enabled = ProfilingEnabled::kNoController;

using ProcessType = metrics::CallStackProfileParams::Process;

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  double rnd = base::RandDouble();
  if (rnd == 0) {
    // log(0) is an error.
    rnd = std::numeric_limits<double>::min();
  }
  return -std::log(rnd) * mean;
}

// Returns true iff `process_type` is handled by ProcessHistogramName.
bool HasProcessHistogramName(ProcessType process_type) {
  switch (process_type) {
    case ProcessType::kBrowser:
    case ProcessType::kRenderer:
    case ProcessType::kGpu:
    case ProcessType::kUtility:
    case ProcessType::kNetworkService:
      return true;
    case ProcessType::kUnknown:
    default:
      // Profiler should not be enabled for these process types.
      return false;
  }
}

// Returns the full name of a histogram to record by appending the
// ProfiledProcess variant name for `process_type` (defined in
// tools/metrics/histograms/metadata/memory/histograms.xml) to `base_name`.
std::string ProcessHistogramName(base::StringPiece base_name,
                                 ProcessType process_type) {
  switch (process_type) {
    case ProcessType::kBrowser:
      return base::StrCat({base_name, ".Browser"});
    case ProcessType::kRenderer:
      return base::StrCat({base_name, ".Renderer"});
    case ProcessType::kGpu:
      return base::StrCat({base_name, ".GPU"});
    case ProcessType::kUtility:
      return base::StrCat({base_name, ".Utility"});
    case ProcessType::kNetworkService:
      return base::StrCat({base_name, ".NetworkService"});
    case ProcessType::kUnknown:
    default:
      // Profiler should not be enabled for these process types.
      NOTREACHED();
      return std::string();
  }
}

ProfilingEnabled DecideIfCollectionIsEnabled(version_info::Channel channel,
                                             ProcessType process_type) {
  HeapProfilerParameters params =
      GetHeapProfilerParametersForProcess(process_type);
  if (!params.is_supported)
    return ProfilingEnabled::kDisabled;
  const double probability = (channel == version_info::Channel::STABLE)
                                 ? params.stable_probability
                                 : params.nonstable_probability;
  if (base::RandDouble() >= probability)
    return ProfilingEnabled::kDisabled;
  return ProfilingEnabled::kEnabled;
}

// Records a time histogram for the `interval` between snapshots, using the
// appropriate histogram buckets for the platform (desktop or mobile).
// `recording_time` must be one of the {RecordingTime} token variants in the
// definition of HeapProfiling.InProcess.SnapshotInterval.{Platform}.
// {RecordingTime} in tools/metrics/histograms/metadata/memory/histograms.xml.
void RecordUmaSnapshotInterval(base::TimeDelta interval,
                               base::StringPiece recording_time,
                               ProcessType process_type) {
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
  // On mobile, the interval is distributed around a mean of 30 minutes.
  constexpr base::TimeDelta kMinHistogramTime = base::Seconds(30);
  constexpr base::TimeDelta kMaxHistogramTime = base::Hours(3);
  constexpr const char* const kPlatform = "Mobile";
#else
  // On desktop, the interval is distributed around a mean of 1 day.
  constexpr base::TimeDelta kMinHistogramTime = base::Minutes(30);
  constexpr base::TimeDelta kMaxHistogramTime = base::Days(6);
  constexpr const char* const kPlatform = "Desktop";
#endif

  const auto base_name =
      base::StrCat({"HeapProfiling.InProcess.SnapshotInterval.", kPlatform, ".",
                    recording_time});
  base::UmaHistogramCustomTimes(ProcessHistogramName(base_name, process_type),
                                interval, kMinHistogramTime, kMaxHistogramTime,
                                50);
  // Also summarize over all process types.
  base::UmaHistogramCustomTimes(base_name, interval, kMinHistogramTime,
                                kMaxHistogramTime, 50);
}

// Records metrics about the quality of each stack that is sampled. `stack_size`
// is the number of frames in the stack. `num_missing_modules` is the number of
// frames that couldn't be mapped to a module using ModuleCache.
void RecordUmaStackQualityMetrics(ProcessType process_type,
                                  size_t stack_size,
                                  size_t num_missing_modules) {
  // From inspecting reports received on Android, most reports with only 1 to 3
  // frames are clearly truncated, suggesting a problem with the unwinder, or
  // contain a JNI base call that directly allocates. (These are not broken but
  // don't have anything actionable in them.) Reports with 4 frames are more
  // likely to be useful but still have a large proportion of truncated or
  // non-actionable stacks. With 5 or more frames the stacks are more likely
  // than not to be actionable.
  constexpr size_t kMinFramesForGoodQuality = 5;

  const bool has_few_frames = stack_size < kMinFramesForGoodQuality;
  base::UmaHistogramBoolean("HeapProfiling.InProcess.ShortStacks",
                            has_few_frames);
  base::UmaHistogramBoolean(
      ProcessHistogramName("HeapProfiling.InProcess.ShortStacks", process_type),
      has_few_frames);

  if (stack_size > 0) {
    const double module_coverage =
        100.0 * (stack_size - num_missing_modules) / stack_size;
    base::UmaHistogramPercentage("HeapProfiling.InProcess.ModuleCoverage",
                                 module_coverage);
    base::UmaHistogramPercentage(
        ProcessHistogramName("HeapProfiling.InProcess.ModuleCoverage",
                             process_type),
        module_coverage);
  }
}

}  // namespace

HeapProfilerController::SnapshotParams::SnapshotParams(
    base::TimeDelta mean_interval,
    bool use_random_interval,
    scoped_refptr<StoppedFlag> stopped,
    ProcessType process_type,
    base::TimeTicks profiler_creation_time)
    : mean_interval(mean_interval),
      use_random_interval(use_random_interval),
      stopped(std::move(stopped)),
      process_type(process_type),
      profiler_creation_time(profiler_creation_time) {}

HeapProfilerController::SnapshotParams::~SnapshotParams() = default;

HeapProfilerController::SnapshotParams::SnapshotParams(SnapshotParams&& other) =
    default;

HeapProfilerController::SnapshotParams&
HeapProfilerController::SnapshotParams::operator=(SnapshotParams&& other) =
    default;

// static
ProfilingEnabled HeapProfilerController::GetProfilingEnabled() {
  return g_profiling_enabled;
}

HeapProfilerController::HeapProfilerController(version_info::Channel channel,
                                               ProcessType process_type)
    : process_type_(process_type),
      stopped_(base::MakeRefCounted<StoppedFlag>()) {
  // Only one HeapProfilerController should exist at a time in each
  // process. The class is not a singleton so it can be created and
  // destroyed in tests.
  DCHECK_EQ(g_profiling_enabled, ProfilingEnabled::kNoController);
  g_profiling_enabled = DecideIfCollectionIsEnabled(channel, process_type);
}

HeapProfilerController::~HeapProfilerController() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stopped_->data.Set();
  g_profiling_enabled = ProfilingEnabled::kNoController;
}

void HeapProfilerController::StartIfEnabled() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool profiling_enabled =
      g_profiling_enabled == ProfilingEnabled::kEnabled;
  // Only supported processes are assigned a patterned histogram.
  if (HasProcessHistogramName(process_type_)) {
    constexpr char kEnabledHistogramName[] = "HeapProfiling.InProcess.Enabled";
    base::UmaHistogramBoolean(
        ProcessHistogramName(kEnabledHistogramName, process_type_),
        profiling_enabled);
    // Also summarize over all supported process types.
    base::UmaHistogramBoolean(kEnabledHistogramName, profiling_enabled);
  }
  if (!profiling_enabled)
    return;
  HeapProfilerParameters profiler_params =
      GetHeapProfilerParametersForProcess(process_type_);
  // DecideIfCollectionIsEnabled() should return false if not supported.
  DCHECK(profiler_params.is_supported);
  if (profiler_params.sampling_rate_bytes > 0) {
    base::SamplingHeapProfiler::Get()->SetSamplingInterval(
        profiler_params.sampling_rate_bytes);
  }
  base::SamplingHeapProfiler::Get()->Start();
  DCHECK(profiler_params.collection_interval.is_positive());
  SnapshotParams params(
      profiler_params.collection_interval,
      /*use_random_interval=*/!suppress_randomness_for_testing_, stopped_,
      process_type_, creation_time_);
  ScheduleNextSnapshot(std::move(params));
}

void HeapProfilerController::SuppressRandomnessForTesting() {
  suppress_randomness_for_testing_ = true;
}

// static
void HeapProfilerController::ScheduleNextSnapshot(SnapshotParams params) {
  base::TimeDelta interval = params.use_random_interval
                                 ? RandomInterval(params.mean_interval)
                                 : params.mean_interval;
  RecordUmaSnapshotInterval(interval, "Scheduled", params.process_type);
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&HeapProfilerController::TakeSnapshot, std::move(params),
                     /*previous_interval=*/interval),
      interval);
}

// static
void HeapProfilerController::TakeSnapshot(SnapshotParams params,
                                          base::TimeDelta previous_interval) {
  if (params.stopped->data.IsSet())
    return;
  RecordUmaSnapshotInterval(previous_interval, "Taken", params.process_type);
  RetrieveAndSendSnapshot(
      params.process_type,
      base::TimeTicks::Now() - params.profiler_creation_time);
  ScheduleNextSnapshot(std::move(params));
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot(
    ProcessType process_type,
    base::TimeDelta time_since_profiler_creation) {
  using Sample = base::SamplingHeapProfiler::Sample;

  // Always log the total sampled memory before returning. If `samples` is empty
  // this will be logged as 0 MB.
  base::ClampedNumeric<uint64_t> total_sampled_bytes;
  absl::Cleanup log_total_sampled_memory = [&total_sampled_bytes,
                                            &process_type] {
    constexpr int kBytesPerMB = 1024 * 1024;
    base::UmaHistogramMemoryLargeMB(
        ProcessHistogramName("HeapProfiling.InProcess.TotalSampledMemory",
                             process_type),
        base::ClampDiv(total_sampled_bytes, kBytesPerMB));
  };

  std::vector<Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  base::UmaHistogramCounts100000(
      ProcessHistogramName("HeapProfiling.InProcess.SamplesPerSnapshot",
                           process_type),
      samples.size());
  // Also summarize over all process types.
  base::UmaHistogramCounts100000("HeapProfiling.InProcess.SamplesPerSnapshot",
                                 samples.size());
  if (samples.empty())
    return;

  base::ModuleCache module_cache;
  metrics::CallStackProfileParams params(
      process_type, metrics::CallStackProfileParams::Thread::kUnknown,
      metrics::CallStackProfileParams::Trigger::kPeriodicHeapCollection,
      time_since_profiler_creation);
  metrics::CallStackProfileBuilder profile_builder(params);

  SampleMap merged_samples = MergeSamples(samples);

  for (auto& pair : merged_samples) {
    const Sample& sample = pair.first;
    const SampleValue& value = pair.second;

    const size_t stack_size = sample.stack.size();
    std::vector<base::Frame> frames;
    frames.reserve(stack_size);
    size_t num_missing_modules = 0;
    for (const void* frame : sample.stack) {
      const uintptr_t address = reinterpret_cast<const uintptr_t>(frame);
      const base::ModuleCache::Module* module =
          module_cache.GetModuleForAddress(address);
      if (!module) {
        num_missing_modules += 1;
      }
      frames.emplace_back(address, module);
    }

    RecordUmaStackQualityMetrics(process_type, stack_size, num_missing_modules);

    // Heap "samples" represent allocation stacks aggregated over time so
    // do not have a meaningful timestamp.
    profile_builder.OnSampleCompleted(std::move(frames), base::TimeTicks(),
                                      value.total, value.count);

    total_sampled_bytes += value.total;
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}

}  // namespace heap_profiling
