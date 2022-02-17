// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROPOSED_CANDIDATE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROPOSED_CANDIDATE_H_

#include "components/viz/common/quads/quad_list.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class OverlayProcessorStrategy;

// Represents a candidate that could promote a specific `DrawQuad` to an overlay
// using a specific `OverlayProcessorStrategy`.
class VIZ_SERVICE_EXPORT OverlayProposedCandidate {
 public:
  OverlayProposedCandidate(QuadList::Iterator it,
                           OverlayCandidate overlay_candidate,
                           OverlayProcessorStrategy* overlay_strategy)
      : quad_iter(it),
        candidate(overlay_candidate),
        strategy(overlay_strategy) {}

  // An iterator in the QuadList.
  QuadList::Iterator quad_iter;
  // This is needed to sort candidates based on DrawQuad order.
  size_t quad_index;
  OverlayCandidate candidate;
  raw_ptr<OverlayProcessorStrategy> strategy = nullptr;

  // heuristic sort element
  int relative_power_gain = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROPOSED_CANDIDATE_H_
