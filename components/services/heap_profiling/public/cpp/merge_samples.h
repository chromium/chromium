// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_MERGE_SAMPLES_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_MERGE_SAMPLES_H_

#include <map>
#include <vector>

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

namespace heap_profiling {

struct SampleComparator {
  bool operator()(const base::SamplingHeapProfiler::Sample& lhs,
                  const base::SamplingHeapProfiler::Sample& rhs) const;
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
using SampleMap =
    std::map<base::SamplingHeapProfiler::Sample, SampleValue, SampleComparator>;

// Merges samples that have identical stack traces, excluding total and size.
SampleMap MergeSamples(
    const std::vector<base::SamplingHeapProfiler::Sample>& samples);

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_PUBLIC_CPP_MERGE_SAMPLES_H_
