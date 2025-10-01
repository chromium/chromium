// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/input_on_viz_state_processing_result.h"

#include "base/metrics/histogram_functions.h"

namespace viz {

void EmitStateProcessingResultHistogram(
    InputOnVizStateProcessingResult result) {
  base::UmaHistogramEnumeration("Android.InputOnViz.Viz.StateProcessingResult3",
                                result);
}

}  // namespace viz
