// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_

#include "base/feature_list.h"
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

// Chance that this client will report heap samples through a metrics
// provider if it's on the stable channel.
extern const base::FeatureParam<double> kStableProbability;

// Chance that this client will report heap samples through a metrics
// provider if it's on a non-stable channel.
extern const base::FeatureParam<double> kNonStableProbability;

// Mean time between snapshots.
extern const base::FeatureParam<base::TimeDelta> kCollectionInterval;

// Returns the sampling rate in bytes to use for `process_type`. 0 means to use
// PoissonAllocationSampler's default sampling rate.
size_t GetSamplingRateForProcess(
    sampling_profiler::ProfilerProcessType process_type);

// Returns the probability of sampling a `process_type` process in each
// snapshot, from 0 to 100.
int GetSnapshotProbabilityForProcess(
    sampling_profiler::ProfilerProcessType process_type);

// Returns the load factor that PoissonAllocationSampler's hash set should use
// in a `process_type` process. 0.0 means to use the hash set's default.
float GetHashSetLoadFactorForProcess(
    sampling_profiler::ProfilerProcessType process_type);

}  // namespace heap_profiling

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_PARAMETERS_H_
