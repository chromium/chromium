// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_

#include "base/feature_list.h"
#include "base/json/json_value_converter.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/metrics/call_stacks/call_stack_profile_params.h"

namespace heap_profiling {

// If this is disabled, the client will not collect heap profiles. If it is
// enabled, the client may enable the sampling heap profiler (with probability
// based on the "stable-probability" parameter if the client is on the stable
// channel, or the "nonstable-probability" parameter otherwise). Sampled heap
// profiles will then be reported through the metrics service iff metrics
// reporting is enabled.
BASE_DECLARE_FEATURE(kHeapProfilerReporting);

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
  bool UpdateFromJSON(base::StringPiece json_string);
};

// Returns a default set of parameters to use if not overridden for a
// specific process.
HeapProfilerParameters GetDefaultHeapProfilerParameters();

// Returns the set of process parameters to use for `process_type`. This will be
// identical to the result of GetDefaultHeapProfilerParameters() unless
// overridden by a field trial.
HeapProfilerParameters GetHeapProfilerParametersForProcess(
    metrics::CallStackProfileParams::Process process_type);

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_
