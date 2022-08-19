// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "base/profiler/module_cache.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/services/heap_profiling/public/cpp/merge_samples.h"
#include "components/version_info/channel.h"

namespace {

using ProfilingEnabled = HeapProfilerController::ProfilingEnabled;

// Records whether heap profiling is enabled for this process.
// HeapProfilerController will set this on creation, and reset it to
// kNoController on destruction, so that it's always reset to the default
// state after each unit test that creates a HeapProfilerController.
ProfilingEnabled g_profiling_enabled = ProfilingEnabled::kNoController;

using ProcessType = metrics::CallStackProfileParams::Process;

// Platform-specific parameter defaults.

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Average 1M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 1'000'000;

// Default on iOS is equal to mean value of up process time. Android is
// more similar to iOS than to Desktop.
constexpr int kDefaultCollectionIntervalInMinutes = 30;
#else
// Average 10M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 10'000'000;

// Default on desktop is once per day.
constexpr int kDefaultCollectionIntervalInMinutes = 24 * 60;
#endif

// Semicolon-separated list of process names to support. (More convenient than
// commas, which must be url-escaped in the --enable-features command line.)
[[maybe_unused]] constexpr base::FeatureParam<std::string> kSupportedProcesses{
    &HeapProfilerController::kHeapProfilerReporting, "supported-processes",
    "browser"};

// Sets the chance that this client will report heap samples through a metrics
// provider if it's on the stable channel.
[[maybe_unused]] constexpr base::FeatureParam<double> kStableProbability {
  &HeapProfilerController::kHeapProfilerReporting, "stable-probability",
#if BUILDFLAG(IS_ANDROID)
      // With stable-probability 0.01 we get about 4x as many records as before
      // https://crrev.com/c/3309878 landed in 98.0.4742.0, even with ARM64
      // disabled. This is too high a volume to process.
      0.0025
#else
      0.01
#endif
};

// Sets the chance that this client will report heap samples through a metrics
// provider if it's on a non-stable channel.
[[maybe_unused]] constexpr base::FeatureParam<double> kNonStableProbability{
    &HeapProfilerController::kHeapProfilerReporting, "nonstable-probability",
    0.5};

// Sets heap sampling interval in bytes.
constexpr base::FeatureParam<int> kSamplingRateBytes{
    &HeapProfilerController::kHeapProfilerReporting, "sampling-rate",
    kDefaultSamplingRateBytes};

// Sets the interval between snapshots.
constexpr base::FeatureParam<int> kCollectionIntervalMinutes{
    &HeapProfilerController::kHeapProfilerReporting,
    "heap-profiler-collection-interval-minutes",
    kDefaultCollectionIntervalInMinutes};

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

// Returns the string to use in the kSupportedProcesses feature for
// `process_type`, or nullptr if the process is not supported..
const char* ProcessParamString(ProcessType process_type) {
  switch (process_type) {
    case ProcessType::kBrowser:
      return "browser";
    case ProcessType::kRenderer:
      return "renderer";
    case ProcessType::kGpu:
      return "gpu";
    case ProcessType::kUtility:
      return "utility";
    case ProcessType::kNetworkService:
      return "networkService";
    case ProcessType::kUnknown:
    default:
      // Profiler hasn't been tested in these process types.
      return nullptr;
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
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && defined(ARCH_CPU_ARM64)
  // TODO(crbug.com/1297724): Remove this early return after validating that
  // there are no crashes in ModuleCache::CreateModuleForAddress on ARM64. Also
  // re-enable the tests in heap_profiler_controller_unittests.cc.
  return ProfilingEnabled::kDisabled;
#else
  if (!base::FeatureList::IsEnabled(
          HeapProfilerController::kHeapProfilerReporting)) {
    return ProfilingEnabled::kDisabled;
  }
  const char* process_string = ProcessParamString(process_type);
  if (!process_string) {
    // This process type is never supported.
    return ProfilingEnabled::kDisabled;
  }
  const std::vector<std::string> supported_processes =
      base::SplitString(kSupportedProcesses.Get(), ";", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (!base::Contains(supported_processes, process_string))
    return ProfilingEnabled::kDisabled;
  const double probability = (channel == version_info::Channel::STABLE)
                                 ? kStableProbability.Get()
                                 : kNonStableProbability.Get();
  if (base::RandDouble() >= probability)
    return ProfilingEnabled::kDisabled;
  return ProfilingEnabled::kEnabled;
#endif
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

}  // namespace

constexpr base::Feature HeapProfilerController::kHeapProfilerReporting{
    "HeapProfilerReporting", base::FEATURE_ENABLED_BY_DEFAULT};

HeapProfilerController::SnapshotParams::SnapshotParams(
    base::TimeDelta mean_interval,
    bool use_random_interval,
    scoped_refptr<StoppedFlag> stopped,
    ProcessType process_type)
    : mean_interval(mean_interval),
      use_random_interval(use_random_interval),
      stopped(std::move(stopped)),
      process_type(process_type) {}

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
  // Only supported processes are assigned a param string.
  if (ProcessParamString(process_type_)) {
    constexpr char kEnabledHistogramName[] = "HeapProfiling.InProcess.Enabled";
    base::UmaHistogramBoolean(
        ProcessHistogramName(kEnabledHistogramName, process_type_),
        profiling_enabled);
    // Also summarize over all supported process types.
    base::UmaHistogramBoolean(kEnabledHistogramName, profiling_enabled);
  }
  if (!profiling_enabled)
    return;
  int sampling_rate_bytes = kSamplingRateBytes.Get();
  if (sampling_rate_bytes > 0)
    base::SamplingHeapProfiler::Get()->SetSamplingInterval(sampling_rate_bytes);
  base::SamplingHeapProfiler::Get()->Start();
  const int interval = kCollectionIntervalMinutes.Get();
  DCHECK_GT(interval, 0);
  SnapshotParams params(
      /*mean_interval=*/base::Minutes(interval),
      /*use_random_interval=*/!suppress_randomness_for_testing_, stopped_,
      process_type_);
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
  RetrieveAndSendSnapshot(params.process_type);
  ScheduleNextSnapshot(std::move(params));
}

// static
void HeapProfilerController::RetrieveAndSendSnapshot(ProcessType process_type) {
  std::vector<Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  constexpr char kSamplesPerSnapshotHistogramName[] =
      "HeapProfiling.InProcess.SamplesPerSnapshot";
  base::UmaHistogramCounts100000(
      ProcessHistogramName("HeapProfiling.InProcess.SamplesPerSnapshot",
                           process_type),
      samples.size());
  // Also summarize over all process types.
  base::UmaHistogramCounts100000(kSamplesPerSnapshotHistogramName,
                                 samples.size());
  if (samples.empty())
    return;

  base::ModuleCache module_cache;
  metrics::CallStackProfileParams params(
      process_type, metrics::CallStackProfileParams::Thread::kUnknown,
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
    // Heap "samples" represent allocation stacks aggregated over time so
    // do not have a meaningful timestamp.
    profile_builder.OnSampleCompleted(std::move(frames), base::TimeTicks(),
                                      value.total, value.count);
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
