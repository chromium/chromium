// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
#define COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/synchronization/atomic_flag.h"

// HeapProfilerController controls collection of sampled heap allocation
// snapshots for the current process.
class HeapProfilerController {
 public:
  HeapProfilerController();
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

 private:
  using StoppedFlag = base::RefCountedData<base::AtomicFlag>;

  static void ScheduleNextSnapshot(scoped_refptr<StoppedFlag> stopped,
                                   base::TimeDelta heap_collection_interval);
  static void TakeSnapshot(scoped_refptr<StoppedFlag> stopped,
                           base::TimeDelta heap_collection_interval);
  static void RetrieveAndSendSnapshot();

  scoped_refptr<StoppedFlag> stopped_;

  DISALLOW_COPY_AND_ASSIGN(HeapProfilerController);
};

#endif  // COMPONENTS_HEAP_PROFILING_IN_PROCESS_HEAP_PROFILER_CONTROLLER_H_
