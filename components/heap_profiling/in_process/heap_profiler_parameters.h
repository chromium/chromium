// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_

#include <string_view>

#include "base/feature_list.h"
#include "base/json/json_value_converter.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/sampling_profiler/process_type.h"

namespace heap_profiling {

// If this is disabled, the client will not collect heap profiles. If it is
// enabled, the client may enable the sampling heap profiler (with probability
// based on the "stable-probability" parameter if the client is on the stable
// channel, or the "nonstable-probability" parameter otherwise). Sampled heap
// profiles will then be reported through the metrics service iff metrics
// reporting is enabled.
BASE_DECLARE_FEATURE(kHeapProfilerReporting);

// If this is enabled, heap profiling in subprocesses is controlled centrally
// from the browser process.
BASE_DECLARE_FEATURE(kHeapProfilerCentralControl);

// The probability of including a child process in each snapshot that's taken
// when kHeapProfilerCentralControl is enabled, as a percentage from 0 to 100.
// Defaults to 100, but can be set lower to sub-sample process types that are
// very common (mainly renderers) to keep data volume low. Samples from child
// processes are weighted in inverse proportion to the snapshot probability to
// normalize the aggregated results.
extern const base::FeatureParam<int> kGpuSnapshotProbability;
extern const base::FeatureParam<int> kNetworkSnapshotProbability;
extern const base::FeatureParam<int> kRendererSnapshotProbability;
extern const base::FeatureParam<int> kUtilitySnapshotProbability;

// Parameters to control the heap profiler.
struct HeapProfilerParameters {
  // True if heap profiling is supported, false otherwise.
  bool is_supported = false;

  // Chance that this client will report heap samples through a metrics
  // provider if it's on the stable channel.
  double stable_probability = 0.0;

  // Chance that this client will report heap samples through a metrics
  // provider if it's on a non-stable channel.
  double nonstable_probability = 0.0;

  // Mean heap sampling interval in bytes.
  int sampling_rate_bytes = 0;

  // Mean time between snapshots.
  base::TimeDelta collection_interval;

  // Invoked by JSONValueConverter to parse parameters from JSON.
  static void RegisterJSONConverter(
      base::JSONValueConverter<HeapProfilerParameters>* converter);

  // Overwrites this object's fields with parameters parsed from `json_string`.
  // Missing parameters will not be touched. If parsing fails, returns false and
  // sets `is_supported` to false to ensure heap profiling doesn't run with
  // invalid parameters.
  bool UpdateFromJSON(std::string_view json_string);
};

// Returns a default set of parameters to use if not overridden for a
// specific process.
HeapProfilerParameters GetDefaultHeapProfilerParameters();

// Returns the set of process parameters to use for `process_type`. This will be
// identical to the result of GetDefaultHeapProfilerParameters() unless
// overridden by a field trial.
HeapProfilerParameters GetHeapProfilerParametersForProcess(
    sampling_profiler::ProfilerProcessType process_type);

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_
