// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_android.h"

#include <memory>

#include "components/viz/service/display/overlay_candidate_list.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayCandidateValidatorAndroid::OverlayCandidateValidatorAndroid() {}

OverlayCandidateValidatorAndroid::~OverlayCandidateValidatorAndroid() {}

void OverlayCandidateValidatorAndroid::InitializeStrategies() {
  DCHECK(strategies_.empty());
  // For Android, we do not have the ability to skip an overlay, since the
  // texture is already in a SurfaceView.  Ideally, we would honor a 'force
  // overlay' flag that FromDrawQuad would also check.
  // For now, though, just skip the opacity check.  We really have no idea if
  // the underlying overlay is opaque anyway; the candidate is referring to
  // a dummy resource that has no relation to what the overlay contains.
  // https://crbug.com/842931 .
  strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
      this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
}

void OverlayCandidateValidatorAndroid::CheckOverlaySupport(
    const PrimaryPlane* primary_plane,
    OverlayCandidateList* candidates) {
  // There should only be at most a single overlay candidate: the video quad.
  // There's no check that the presented candidate is really a video frame for
  // a fullscreen video. Instead it's assumed that if a quad is marked as
  // overlayable, it's a fullscreen video quad.
  DCHECK_LE(candidates->size(), 1u);

  if (!candidates->empty()) {
    OverlayCandidate& candidate = candidates->front();

    // This quad either will be promoted, or would be if it were backed by a
    // SurfaceView.  Record that it should get a promotion hint.
    candidates->AddPromotionHint(candidate);

    if (candidate.is_backed_by_surface_texture) {
      // This quad would be promoted if it were backed by a SurfaceView.  Since
      // it isn't, we can't promote it.
      return;
    }

    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;
    candidate.plane_z_order = -1;
  }
}

bool OverlayCandidateValidatorAndroid::AllowCALayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorAndroid::AllowDCLayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorAndroid::NeedsSurfaceOccludingDamageRect() const {
  return false;
}

}  // namespace viz
