// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROPOSED_CANDIDATE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROPOSED_CANDIDATE_H_

#include <array>

#include "base/containers/flat_set.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class OverlayProcessorStrategy;

struct ProposedCandidateKey {
  OverlayCandidate::TrackingId tracking_id =
      OverlayCandidate::kDefaultTrackingId;
  OverlayStrategy strategy_id = OverlayStrategy::kUnknown;

  bool operator==(const ProposedCandidateKey& other) const {
    return (tracking_id == other.tracking_id &&
            strategy_id == other.strategy_id);
  }

  bool operator<(const ProposedCandidateKey& other) const {
    if (tracking_id != other.tracking_id) {
      return tracking_id < other.tracking_id;
    }

    return static_cast<int>(strategy_id) < static_cast<int>(other.strategy_id);
  }
};

struct ProposedCandidateKeyHasher {
  std::size_t operator()(const ProposedCandidateKey& k) const {
    return base::Hash(&k, sizeof(k));
  }
};

// Represents a candidate that could promote a specific `DrawQuad` to an overlay
// using a specific `OverlayProcessorStrategy`.
class VIZ_SERVICE_EXPORT OverlayProposedCandidate {
 public:
  OverlayProposedCandidate(QuadList::Iterator it,
                           OverlayCandidate overlay_candidate,
                           OverlayProcessorStrategy* overlay_strategy);

  OverlayProposedCandidate(const OverlayProposedCandidate&);
  OverlayProposedCandidate& operator=(const OverlayProposedCandidate&);

  ~OverlayProposedCandidate();

  // Returns the bounds of rounded display masks in target space that are
  // associated with the `proposed_candidate`.
  static std::array<
      gfx::Rect,
      TextureDrawQuad::RoundedDisplayMasksInfo::kMaxRoundedDisplayMasksCount>
  GetRoundedDisplayMasksBounds(
      const OverlayProposedCandidate& proposed_candidate);

  static ProposedCandidateKey ToProposeKey(
      const OverlayProposedCandidate& proposed);

  // An iterator in the QuadList.
  QuadList::Iterator quad_iter;
  OverlayCandidate candidate;
  raw_ptr<OverlayProcessorStrategy> strategy = nullptr;

  // heuristic sort element
  int relative_power_gain = 0;

  // Keys of candidates with rounded display masks that occlude `this`.
  base::flat_set<ProposedCandidateKey> occluding_mask_keys;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROPOSED_CANDIDATE_H_
