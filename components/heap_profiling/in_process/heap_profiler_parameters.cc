// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_parameters.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/sampling_profiler/process_type.h"

namespace heap_profiling {

namespace {

// Platform-specific parameter defaults.

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Default on iOS is equal to mean value of process uptime. Android is
// more similar to iOS than to Desktop.
constexpr base::TimeDelta kDefaultCollectionInterval = base::Minutes(30);
#else
// Default on desktop is once per day.
constexpr base::TimeDelta kDefaultCollectionInterval = base::Days(1);
#endif

// Average 10M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 10'000'000;

// The chance that this client will report heap samples through a metrics
// provider if it's on the stable channel.
#if BUILDFLAG(IS_ANDROID)
// With stable-probability 0.01 we get about 4x as many records as before
// https://crrev.com/c/3309878 landed in 98.0.4742.0, even with ARM64
// disabled. This is too high a volume to process.
constexpr double kDefaultStableProbability = 0.0025;
#else
constexpr double kDefaultStableProbability = 0.01;
#endif

// The chance that this client will report heap samples through a metrics
// provider if it's on a non-stable channel.
constexpr double kDefaultNonStableProbability = 0.5;

// The probability of including a child process in each snapshot that's taken,
// as a percentage from 0 to 100. Defaults to 100, but can be set lower to
// sub-sample process types that are very common (mainly renderers) to keep data
// volume low. Samples from child processes are weighted in inverse proportion
// to the snapshot probability to normalize the aggregated results. Set to 0 to
// disable sampling a process completely.

constexpr base::FeatureParam<int> kGpuSnapshotProbability{
    &kHeapProfilerReporting, "gpu-prob-pct", 100};

constexpr base::FeatureParam<int> kNetworkSnapshotProbability{
    &kHeapProfilerReporting, "network-prob-pct", 100};

// Sample 10% of renderer processes by default, because last time this was
// evaluated (2024-08) the 50th %ile of renderer process count
// (Memory.RenderProcessHost.Count2.All) ranged from 8 on Windows to 18 on Mac.
// 10% is an easy default between 1/18 and 1/8.
constexpr base::FeatureParam<int> kRendererSnapshotProbability{
    &kHeapProfilerReporting, "renderer-prob-pct",
#if BUILDFLAG(IS_CHROMEOS)
    // base::debug::TraceStackFramePointers is crashing in ChromeOS rendererer
    // processes. Disable heap profiling there for now.
    // TODO(crbug.com/402542102): Find the root cause and re-enable.
    0
#else
    10
#endif
};

// Sample 50% of utility processes by default, because last time this was
// evaluated (2024-08) the profiler collected 1.8x as many snapshots on Mac and
// 2.4x as many snapshots on Windows for each browser process snapshot.
constexpr base::FeatureParam<int> kUtilitySnapshotProbability{
    &kHeapProfilerReporting, "utility-prob-pct", 50};

// The sampling rates of each process type, in bytes.

constexpr base::FeatureParam<int> kBrowserSamplingRateBytes{
    &kHeapProfilerReporting, "browser-sampling-rate-bytes",
    kDefaultSamplingRateBytes};

// Use half the threshold used in the browser process, because last time it was
// validated the GPU process allocated a bit over half as much memory at the
// median.
constexpr base::FeatureParam<int> kGpuSamplingRateBytes{
    &kHeapProfilerReporting, "gpu-sampling-rate-bytes",
    kDefaultSamplingRateBytes / 2};

constexpr base::FeatureParam<int> kNetworkSamplingRateBytes{
    &kHeapProfilerReporting, "network-sampling-rate-bytes",
    kDefaultSamplingRateBytes};

constexpr base::FeatureParam<int> kRendererSamplingRateBytes{
    &kHeapProfilerReporting, "renderer-sampling-rate-bytes",
    kDefaultSamplingRateBytes};

// Use 1/10th the threshold used in the browser process, because last time it
// was validated with the default sampling rate (2024-08) the sampler collected
// 6% to 11% as many samples per snapshot in the utility process, depending on
// platform.
constexpr base::FeatureParam<int> kUtilitySamplingRateBytes{
    &kHeapProfilerReporting, "utility-sampling-rate-bytes",
    kDefaultSamplingRateBytes / 10};

// The load factor that should be used by PoissonAllocationSampler's hash set in
// each process type.
constexpr base::FeatureParam<double> kBrowserHashSetLoadFactor{
    &kHeapProfilerReporting, "browser-hash-set-load-factor", 1.0};
constexpr base::FeatureParam<double> kGpuHashSetLoadFactor{
    &kHeapProfilerReporting, "gpu-hash-set-load-factor", 1.0};
constexpr base::FeatureParam<double> kNetworkHashSetLoadFactor{
    &kHeapProfilerReporting, "network-hash-set-load-factor", 1.0};
constexpr base::FeatureParam<double> kRendererHashSetLoadFactor{
    &kHeapProfilerReporting, "renderer-hash-set-load-factor", 1.0};
constexpr base::FeatureParam<double> kUtilityHashSetLoadFactor{
    &kHeapProfilerReporting, "utility-hash-set-load-factor", 1.0};

}  // namespace

BASE_FEATURE(kHeapProfilerReporting, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double> kStableProbability{
    &kHeapProfilerReporting, "stable-probability", kDefaultStableProbability};

const base::FeatureParam<double> kNonStableProbability{
    &kHeapProfilerReporting, "nonstable-probability",
    kDefaultNonStableProbability};

const base::FeatureParam<base::TimeDelta> kCollectionInterval{
    &kHeapProfilerReporting, "collection-interval", kDefaultCollectionInterval};

size_t GetSamplingRateForProcess(
    sampling_profiler::ProfilerProcessType process_type) {
  int sampling_rate_bytes;
  switch (process_type) {
    case sampling_profiler::ProfilerProcessType::kBrowser:
      sampling_rate_bytes = kBrowserSamplingRateBytes.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kRenderer:
      sampling_rate_bytes = kRendererSamplingRateBytes.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kGpu:
      sampling_rate_bytes = kGpuSamplingRateBytes.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kUtility:
      sampling_rate_bytes = kUtilitySamplingRateBytes.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kNetworkService:
      sampling_rate_bytes = kNetworkSamplingRateBytes.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kUnknown:
    default:
      // Profiler should not be enabled for these process types.
      NOTREACHED();
  }
  return base::saturated_cast<size_t>(sampling_rate_bytes);
}

int GetSnapshotProbabilityForProcess(
    sampling_profiler::ProfilerProcessType process_type) {
  int snapshot_probability_pct;
  switch (process_type) {
    case sampling_profiler::ProfilerProcessType::kBrowser:
      // Should only be called for child processes.
      NOTREACHED();
    case sampling_profiler::ProfilerProcessType::kGpu:
      snapshot_probability_pct = kGpuSnapshotProbability.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kNetworkService:
      snapshot_probability_pct = kNetworkSnapshotProbability.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kRenderer:
      snapshot_probability_pct = kRendererSnapshotProbability.Get();
      break;
    case sampling_profiler::ProfilerProcessType::kUtility:
      snapshot_probability_pct = kUtilitySnapshotProbability.Get();
      break;
    default:
      // Unsupported process type.
      snapshot_probability_pct = 0;
      break;
  }
  CHECK_GE(snapshot_probability_pct, 0);
  CHECK_LE(snapshot_probability_pct, 100);
  return snapshot_probability_pct;
}

float GetHashSetLoadFactorForProcess(
    sampling_profiler::ProfilerProcessType process_type) {
  switch (process_type) {
    case sampling_profiler::ProfilerProcessType::kBrowser:
      return kBrowserHashSetLoadFactor.Get();
    case sampling_profiler::ProfilerProcessType::kGpu:
      return kGpuHashSetLoadFactor.Get();
    case sampling_profiler::ProfilerProcessType::kNetworkService:
      return kNetworkHashSetLoadFactor.Get();
    case sampling_profiler::ProfilerProcessType::kRenderer:
      return kRendererHashSetLoadFactor.Get();
    case sampling_profiler::ProfilerProcessType::kUtility:
      return kUtilityHashSetLoadFactor.Get();
    default:
      NOTREACHED();
  }
}

}  // namespace heap_profiling
