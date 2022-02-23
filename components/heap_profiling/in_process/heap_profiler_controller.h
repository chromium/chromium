// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "components/version_info/channel.h"

// HeapProfilerController controls collection of sampled heap allocation
// snapshots for the current process.
class HeapProfilerController {
 public:
  // `channel` is used to determine the probability that this client will be
  // opted in to profiling.
  explicit HeapProfilerController(version_info::Channel channel);

  HeapProfilerController(const HeapProfilerController&) = delete;
  HeapProfilerController& operator=(const HeapProfilerController&) = delete;

  ~HeapProfilerController();

  // Starts periodic heap snapshot collection.
  void Start();

  // Public for testing.
  using Sample = base::SamplingHeapProfiler::Sample;
  struct SampleComparator {
    bool operator()(const Sample& lhs, const Sample& rhs) const;
  };
  struct SampleValue {
    // Sum of all allocations attributed to this Sample's stack trace.
    size_t total = 0;
    // Count of all allocations attributed to this Sample's stack trace.
    size_t count = 0;
  };

  // The value of the map tracks total size and count of all Samples associated
  // with the key's stack trace.
  // We use a std::map here since most comparisons will early out in one of the
  // first entries in the vector. For reference: the first entry is likely the
  // address of the code calling malloc(). Using a hash-based container would
  // typically entail hashing the entire contents of the stack trace.
  using SampleMap = std::map<Sample, SampleValue, SampleComparator>;

  // Merges samples that have identical stack traces, excluding total and size.
  static SampleMap MergeSamples(const std::vector<Sample>& samples);

  // If this is disabled, the client will not collect heap profiles. If it is
  // enabled, the client may enable the sampling heap profiler (with probability
  // based on the "stable-probability" parameter if the client is on the stable
  // channel, or the "nonstable-probability" parameter otherwise). Sampled heap
  // profiles will then be reported through the metrics service iff metrics
  // reporting is enabled.
  static const base::Feature kHeapProfilerReporting;

  // Uses the exact parameter values for the sampling interval and time between
  // samples, instead of a distribution around those values. This must be called
  // before Start.
  void SuppressRandomnessForTesting();

 private:
  using StoppedFlag = base::RefCountedData<base::AtomicFlag>;

  struct CollectionInterval {
    base::TimeDelta interval;
    bool use_random_interval = false;
  };
  static void ScheduleNextSnapshot(scoped_refptr<StoppedFlag> stopped,
                                   CollectionInterval heap_collection_interval);

  // Takes a heap snapshot unless the `stopped` flag is set.
  // `heap_collection_interval` is used to schedule the next snapshot.
  // `previous_interval` is the time since the previous snapshot, which is used
  // to log metrics about snapshot frequency.
  static void TakeSnapshot(scoped_refptr<StoppedFlag> stopped,
                           CollectionInterval heap_collection_interval,
                           base::TimeDelta previous_interval);

  static void RetrieveAndSendSnapshot();

  // On startup this will be determined randomly based on the current channel
  // and the probability parameters of the HeapProfilerReporting feature.
  const bool profiling_enabled_;

  scoped_refptr<StoppedFlag> stopped_;
  bool suppress_randomness_for_testing_ = false;
};

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
