// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/merge_samples.h"

#include <algorithm>
#include <cmath>

namespace heap_profiling {

namespace {
using Sample = base::SamplingHeapProfiler::Sample;
}

bool SampleComparator::operator()(const Sample& lhs, const Sample& rhs) const {
  // We consider two samples to be equal if and only if their stacks are equal.
  // Note that equal stack implies equal allocator. It's technically possible
  // for two equal stacks to have different thread names, but it's an edge
  // condition and marking them as equal will not significantly change analysis.
  return lhs.stack < rhs.stack;
}

// Merges samples that have identical stack traces, excluding total and size.
SampleMap MergeSamples(const std::vector<Sample>& samples) {
  SampleMap results;
  for (const Sample& sample : samples) {
    size_t count = std::max<size_t>(
        static_cast<size_t>(
            std::llround(static_cast<double>(sample.total) / sample.size)),
        1);
    // Either update the existing entry or construct a new entry [with default
    // initializer 0].
    SampleValue& value = results[sample];
    value.total += sample.total;
    value.count += count;
  }
  return results;
}

}  // namespace heap_profiling
