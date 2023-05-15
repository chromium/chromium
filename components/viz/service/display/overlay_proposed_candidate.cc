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
namespace {
using RoundedDisplayMaskInfo = TextureDrawQuad::RoundedDisplayMasksInfo;
}  // namespace

// static
std::array<gfx::Rect, RoundedDisplayMaskInfo::kMaxRoundedDisplayMasksCount>
OverlayProposedCandidate::GetRoundedDisplayMasksBounds(
    const OverlayProposedCandidate& proposed_candidate) {
  std::array<gfx::Rect, RoundedDisplayMaskInfo::kMaxRoundedDisplayMasksCount>
      mask_rects;

  if (!proposed_candidate.candidate.has_rounded_display_masks) {
    return mask_rects;
  }

  TextureDrawQuad::RoundedDisplayMasksInfo mask_info =
      TextureDrawQuad::MaterialCast(*proposed_candidate.quad_iter)
          ->rounded_display_masks_info;

  // The rects are rounded as they're snapped by the compositor to pixel
  // unless it is AA'ed, in which case, it won't be overlaid.
  gfx::Rect target_rect = gfx::ToRoundedRect(
      OverlayCandidate::DisplayRectInTargetSpace(proposed_candidate.candidate));

  int16_t origin_mask_radius =
      mask_info.radii[TextureDrawQuad::RoundedDisplayMasksInfo::
                          kOriginRoundedDisplayMaskIndex];
  if (origin_mask_radius != 0) {
    mask_rects[RoundedDisplayMaskInfo::kOriginRoundedDisplayMaskIndex] =
        gfx::Rect(target_rect.x(), target_rect.y(), origin_mask_radius,
                  origin_mask_radius);
  }

  int16_t other_mask_radius =
      mask_info.radii[TextureDrawQuad::RoundedDisplayMasksInfo::
                          kOtherRoundedDisplayMaskIndex];
  if (other_mask_radius != 0) {
    if (mask_info.is_horizontally_positioned) {
      mask_rects[RoundedDisplayMaskInfo::kOtherRoundedDisplayMaskIndex] =
          gfx::Rect(target_rect.x() + target_rect.width() - other_mask_radius,
                    target_rect.y(), other_mask_radius, other_mask_radius);
    } else {
      mask_rects[RoundedDisplayMaskInfo::kOtherRoundedDisplayMaskIndex] =
          gfx::Rect(target_rect.x(),
                    target_rect.y() + target_rect.height() - other_mask_radius,
                    other_mask_radius, other_mask_radius);
    }
  }

  return mask_rects;
}

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
