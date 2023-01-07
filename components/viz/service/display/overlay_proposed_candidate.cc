// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_proposed_candidate.h"

#include "components/viz/service/display/overlay_processor_strategy.h"

namespace viz {

ProposedCandidateKey OverlayProposedCandidate::ToProposeKey(
    const OverlayProposedCandidate& proposed) {
  return {proposed.candidate.tracking_id, proposed.strategy->GetUMAEnum()};
}

}  // namespace viz
