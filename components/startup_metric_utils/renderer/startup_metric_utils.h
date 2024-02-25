// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_RENDERER_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_RENDERER_STARTUP_METRIC_UTILS_H_

#include "base/component_export.h"
#include "base/time/time.h"

// Utility functions to support metric collection for gpu startup. Timings
// should use TimeTicks whenever possible.

namespace startup_metric_utils {

class COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
    RendererStartupMetricRecorder final {
 public:
  // Call this when the renderer has finished its initialization and is about to
  // call into its main-thread RunLoop. Must be called after
  // RecordStartupProcessCreationTime, because it computes time deltas based on
  // process creation time.
  void RecordRunLoopStart(base::TimeTicks ticks);

 private:
  friend COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
      RendererStartupMetricRecorder& GetRenderer();

  base::TimeTicks run_loop_start_ticks_;

  void RecordRendererStartRunLoopTicks(base::TimeTicks ticks);
};

COMPONENT_EXPORT(STARTUP_METRIC_UTILS)
RendererStartupMetricRecorder& GetRenderer();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_RENDERER_STARTUP_METRIC_UTILS_H_
