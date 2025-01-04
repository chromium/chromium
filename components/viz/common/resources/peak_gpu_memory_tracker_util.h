// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_PEAK_GPU_MEMORY_TRACKER_UTIL_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_PEAK_GPU_MEMORY_TRACKER_UTIL_H_

#include <stdint.h>

#include <utility>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// Enum class used to represent the location of PeakGpuMemoryTracker creation.
enum class SequenceLocation {
  kBrowserProcess,
  kGpuProcess,
};

// Returns next sequence number to be used by PeakGpuMemoryTracker to track GPU
// memory usage, according to |location| using sequence_num_generator counters.
VIZ_COMMON_EXPORT uint32_t GetNextSequenceNumber(SequenceLocation location);

// For testing.
VIZ_COMMON_EXPORT void SetSequenceNumberGeneratorForTesting(
    uint32_t sequence_num_generator,
    SequenceLocation location);

VIZ_COMMON_EXPORT SequenceLocation
GetPeakMemoryUsageRequestLocation(uint32_t sequence_num);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_PEAK_GPU_MEMORY_TRACKER_UTIL_H_
