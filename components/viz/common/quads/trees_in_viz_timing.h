// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_TREES_IN_VIZ_TIMING_H_
#define COMPONENTS_VIZ_COMMON_QUADS_TREES_IN_VIZ_TIMING_H_

#include "base/time/time.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

// Information passed from viz clients used to calculate stage time
// of TreesInViz project stages.
struct VIZ_COMMON_EXPORT TreesInVizTiming {
  // The time at which an update has been received by the viz process.
  base::TimeTicks start_update_display_tree;
  // The time at which LTHI in Viz has finished processing the layer tree update
  // received from CC and begun preparing the tree for drawing.
  base::TimeTicks start_prepare_to_draw;
  // The time at which Viz has started to draw the active tree.
  base::TimeTicks start_draw_layers;
  // The time at which Viz submitted a compositor frame to itself.
  base::TimeTicks submit_compositor_frame;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_TREES_IN_VIZ_TIMING_H_
