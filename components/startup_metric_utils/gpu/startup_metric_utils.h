// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_GPU_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_GPU_STARTUP_METRIC_UTILS_H_

#include "base/component_export.h"
#include "base/time/time.h"

// Utility functions to support metric collection for gpu startup. Timings
// should use TimeTicks whenever possible.

namespace startup_metric_utils {

class COMPONENT_EXPORT(STARTUP_METRIC_UTILS) GpuStartupMetricRecorder final {
 public:
  // Call this when the GPU has finished its initialization. Must be called
  // after RecordStartupProcessCreationTime, because it computes time deltas
  // based on process creation time.
  void RecordGpuInitialized(base::TimeTicks ticks);

 private:
  friend COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
      GpuStartupMetricRecorder& GetGpu();

  base::TimeTicks gpu_initialized_ticks_;

  void RecordGpuInitializationTicks(base::TimeTicks ticks);
};

COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
GpuStartupMetricRecorder& GetGpu();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_GPU_STARTUP_METRIC_UTILS_H_
