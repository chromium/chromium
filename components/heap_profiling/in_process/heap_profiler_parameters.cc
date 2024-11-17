// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/heap_profiling/in_process/heap_profiler_parameters.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_converter.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/sampling_profiler/process_type.h"
#include "components/variations/variations_switches.h"

namespace heap_profiling {

namespace {

// Platform-specific parameter defaults.

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// Average 1M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 1'000'000;

// Default on iOS is equal to mean value of process uptime. Android is
// more similar to iOS than to Desktop.
constexpr base::TimeDelta kDefaultCollectionInterval = base::Minutes(30);
#else
// Average 10M bytes per sample.
constexpr int kDefaultSamplingRateBytes = 10'000'000;

// Default on desktop is once per day.
constexpr base::TimeDelta kDefaultCollectionInterval = base::Days(1);
#endif

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

constexpr HeapProfilerParameters kDefaultHeapProfilerParameters{
    .is_supported = false,
    // If a process overrides `is_supported`, use the following defaults.
    .stable_probability = kDefaultStableProbability,
    .nonstable_probability = kDefaultNonStableProbability,
    .sampling_rate_bytes = kDefaultSamplingRateBytes,
    .collection_interval = kDefaultCollectionInterval,
};

// Feature parameters.

// JSON-encoded parameter map that will set the default parameters for the
// heap profiler unless overridden by the process-specific parameters below.
constexpr base::FeatureParam<std::string> kDefaultParameters{
    &kHeapProfilerReporting, "default-params", ""};

// JSON-encoded parameter map that will override the default parameters for the
// browser process.
constexpr base::FeatureParam<std::string> kBrowserProcessParameters{
    &kHeapProfilerReporting, "browser-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for
// renderer processes.
constexpr base::FeatureParam<std::string> kRendererProcessParameters{
    &kHeapProfilerReporting, "renderer-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for the
// GPU process.
constexpr base::FeatureParam<std::string> kGPUProcessParameters{
    &kHeapProfilerReporting, "gpu-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for
// utility processes.
constexpr base::FeatureParam<std::string> kUtilityProcessParameters{
    &kHeapProfilerReporting, "utility-process-params", ""};

// JSON-encoded parameter map that will override the default parameters for the
// network process.
constexpr base::FeatureParam<std::string> kNetworkProcessParameters{
    &kHeapProfilerReporting, "network-process-params", ""};

// Interprets `value` as a positive number of minutes, and writes the converted
// value to `result`. If `value` contains anything other than a positive
// integer, returns false to indicate a conversion failure.
bool ConvertCollectionInterval(const base::Value* value,
                               base::TimeDelta* result) {
  if (!value) {
    // Missing values are ok, so report success without updating `result`.
    return true;
  }
  if (value->is_int()) {
    const int minutes = value->GetInt();
    if (minutes > 0) {
      *result = base::Minutes(minutes);
      return true;
    }
  }
  return false;
}

}  // namespace

BASE_FEATURE(kHeapProfilerReporting,
             "HeapProfilerReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHeapProfilerCentralControl,
             "HeapProfilerCentralControl",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kGpuSnapshotProbability{
    &kHeapProfilerCentralControl, "gpu-prob-pct", 100};

const base::FeatureParam<int> kNetworkSnapshotProbability{
    &kHeapProfilerCentralControl, "network-prob-pct", 100};

// Sample 10% of renderer processes by default, because last time this was
// evaluated (2024-08) the 50th %ile of renderer process count
// (Memory.RenderProcessHost.Count.All) ranged from 8 on Windows to 18 on Mac.
// 10% is an easy default between 1/18 and 1/8.
const base::FeatureParam<int> kRendererSnapshotProbability{
    &kHeapProfilerCentralControl, "renderer-prob-pct", 10};

// Sample 50% of utility processes by default, because last time this was
// evaluated (2024-08) the profiler collected 1.8x as many snapshots on Mac and
// 2.4x as many snapshots on Windows for each browser process snapshot.
const base::FeatureParam<int> kUtilitySnapshotProbability{
    &kHeapProfilerCentralControl, "utility-prob-pct", 50};

// static
void HeapProfilerParameters::RegisterJSONConverter(
    base::JSONValueConverter<HeapProfilerParameters>* converter) {
  converter->RegisterBoolField("is-supported",
                               &HeapProfilerParameters::is_supported);
  converter->RegisterDoubleField("stable-probability",
                                 &HeapProfilerParameters::stable_probability);
  converter->RegisterDoubleField(
      "nonstable-probability", &HeapProfilerParameters::nonstable_probability);
  converter->RegisterIntField("sampling-rate-bytes",
                              &HeapProfilerParameters::sampling_rate_bytes);
  converter->RegisterCustomValueField(
      "collection-interval-minutes",
      &HeapProfilerParameters::collection_interval, &ConvertCollectionInterval);
}

bool HeapProfilerParameters::UpdateFromJSON(std::string_view json_string) {
  if (json_string.empty())
    return true;

  base::JSONValueConverter<HeapProfilerParameters> converter;
  std::optional<base::Value> value =
      base::JSONReader::Read(json_string, base::JSON_ALLOW_TRAILING_COMMAS |
                                              base::JSON_ALLOW_COMMENTS);
  if (value && converter.Convert(*value, this))
    return true;

  // Error reading JSON params. Disable the heap sampler. This will be reported
  // when HeapProfilerController logs HeapProfiling.InProcess.Enabled.
  is_supported = false;
  return false;
}

HeapProfilerParameters GetDefaultHeapProfilerParameters() {
  HeapProfilerParameters params = kDefaultHeapProfilerParameters;
  params.UpdateFromJSON(kDefaultParameters.Get());
  return params;
}

HeapProfilerParameters GetHeapProfilerParametersForProcess(
    sampling_profiler::ProfilerProcessType process_type) {
  using Process = sampling_profiler::ProfilerProcessType;

  HeapProfilerParameters params = kDefaultHeapProfilerParameters;

  // Apply per-process defaults.
  switch (process_type) {
    case Process::kBrowser:
      params.is_supported = true;
      break;
    case Process::kNetworkService:
      if (base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
        params.is_supported = true;
      }
      break;
    case Process::kGpu:
      if (base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
        params.is_supported = true;
        // Use half the threshold used in the browser process, because last time
        // it was validated the GPU process allocated a bit over half as much
        // memory at the median.
        params.sampling_rate_bytes = params.sampling_rate_bytes / 2;
      }
      break;
    case Process::kRenderer:
      if (base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
        params.is_supported = true;
      }
      break;
    case Process::kUtility:
      if (base::FeatureList::IsEnabled(kHeapProfilerCentralControl)) {
        params.is_supported = true;
        // Use 1/10th the threshold used in the browser process, because last
        // time it was validated with the default sampling rate (2024-08) the
        // sampler collected 6% to 11% as many samples per snapshot in the
        // utility process, depending on platform.
        params.sampling_rate_bytes = params.sampling_rate_bytes / 10;
      }
      break;
    case Process::kUnknown:
    default:
      // Do nothing. Profiler hasn't been tested in these process types.
      break;
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, child processes may not set the channel correctly
  // (https://crbug.com/329286893) so the profiler must only be enabled in the
  // browser process unless kHeapProfilerCentralControl is set.
  CHECK(process_type == Process::kBrowser || !params.is_supported ||
        base::FeatureList::IsEnabled(kHeapProfilerCentralControl));
#endif

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarking) ||
      !base::FeatureList::IsEnabled(kHeapProfilerReporting)) {
    params.is_supported = false;
    return params;
  }

  // Override with field trial parameters if any are set.
  if (!params.UpdateFromJSON(kDefaultParameters.Get())) {
    // After an error is detected don't alter `params` further.
    return params;
  }
  switch (process_type) {
    case Process::kBrowser:
      params.UpdateFromJSON(kBrowserProcessParameters.Get());
      break;
    case Process::kRenderer:
      params.UpdateFromJSON(kRendererProcessParameters.Get());
      break;
    case Process::kGpu:
      params.UpdateFromJSON(kGPUProcessParameters.Get());
      break;
    case Process::kUtility:
      params.UpdateFromJSON(kUtilityProcessParameters.Get());
      break;
    case Process::kNetworkService:
      params.UpdateFromJSON(kNetworkProcessParameters.Get());
      break;
    case Process::kUnknown:
    default:
      // Do nothing. Profiler hasn't been tested in these process types.
      break;
  }
  return params;
}

}  // namespace heap_profiling
