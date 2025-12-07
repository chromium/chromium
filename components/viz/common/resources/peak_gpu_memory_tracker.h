// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_PEAK_GPU_MEMORY_TRACKER_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_PEAK_GPU_MEMORY_TRACKER_H_

#include "components/viz/common/viz_common_export.h"

namespace viz {

// Tracks the peak memory of the GPU service for its lifetime. Upon its
// destruction a report will be requested from the GPU service. The peak will be
// reported to UMA Histograms.
//
// If the GPU is lost during this objects lifetime, there will be no
// corresponding report of usage. The same for if there is never a successful
// GPU connection.
//
// See `content::PeakGpuMemoryTrackerFactory::Create` for creation of
// PeakGpuMemoryTracker in the browser process.

class VIZ_COMMON_EXPORT PeakGpuMemoryTracker {
 public:
  // The type of user interaction, for which the GPU Peak Memory Usage is being
  // observed.
  enum class Usage {
    CHANGE_TAB,
    PAGE_LOAD,
    SCROLL,
    USAGE_MAX = SCROLL,
  };

  virtual ~PeakGpuMemoryTracker() = default;

  // Invalidates this tracker, no UMA Histogram report is generated.
  virtual void Cancel() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_PEAK_GPU_MEMORY_TRACKER_H_
