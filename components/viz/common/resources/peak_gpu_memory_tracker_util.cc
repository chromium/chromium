// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/peak_gpu_memory_tracker_util.h"

#include "base/notreached.h"

namespace viz {

namespace {

// Counters to help generate sequence number, which is the unique identifier for
// each PeakGpuMemoryTrackerImpl, depending on the process they've been created
// from.
uint32_t sequence_num_generator_browser = 0;
uint32_t sequence_num_generator_gpu = 0;

}  // namespace

uint32_t GetNextSequenceNumber(SequenceLocation location) {
  // Sequence numbers for PeakGpuMemoryTrackers created in different locations
  // must never return overlapping numbers.
  switch (location) {
    case SequenceLocation::kBrowserProcess:
      // Browser process uses even numbers.
      return sequence_num_generator_browser++ << 1;
    case SequenceLocation::kGpuProcess:
      // GPU process uses odd numbers.
      return ((sequence_num_generator_gpu++ << 1) | 1);
  }
  NOTREACHED();
}

void SetSequenceNumberGeneratorForTesting(uint32_t sequence_num_generator,
                                          SequenceLocation location) {
  if (location == SequenceLocation::kBrowserProcess) {
    sequence_num_generator_browser = sequence_num_generator;
  } else {
    sequence_num_generator_gpu = sequence_num_generator;
  }
}

SequenceLocation GetPeakMemoryUsageRequestLocation(uint32_t sequence_num) {
  if (sequence_num & 1) {
    // Odd |sequence_num| implies that the request for PeakMemoryUsage is from
    // the VizCompositor thread.
    return SequenceLocation::kGpuProcess;
  }
  return SequenceLocation::kBrowserProcess;
}

}  // namespace viz
