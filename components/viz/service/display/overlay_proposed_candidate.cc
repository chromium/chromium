// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_proposed_candidate.h"

#include <array>

#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/overlay_processor_strategy.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayProposedCandidate::OverlayProposedCandidate(
    QuadList::Iterator it,
    OverlayCandidate overlay_candidate,
    OverlayProcessorStrategy* overlay_strategy)
    : quad_iter(it), candidate(overlay_candidate), strategy(overlay_strategy) {}

OverlayProposedCandidate::OverlayProposedCandidate(
    const OverlayProposedCandidate&) = default;
OverlayProposedCandidate& OverlayProposedCandidate::operator=(
    const OverlayProposedCandidate&) = default;

OverlayProposedCandidate::~OverlayProposedCandidate() = default;

ProposedCandidateKey OverlayProposedCandidate::ToProposeKey(
    const OverlayProposedCandidate& proposed) {
  return {proposed.candidate.tracking_id, proposed.strategy->GetUMAEnum()};
}

}  // namespace viz
