// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_DELEGATED_SUPPORT_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_DELEGATED_SUPPORT_H_

#include "base/types/expected.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

class OverlayCandidateFactory;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. For some cases in
// |OverlayCandidate::CandidateStatus| feed into this enum but neither is a
// perfect subset of the other.
enum class DelegationStatus {
  kFullDelegation = 0,
  kCompositedOther = 1,
  kCompositedNotAxisAligned = 2,
  kCompositedCheckOverlayFail = 3,
  kCompositedNotOverlay = 4,
  kCompositedTooManyQuads = 5,
  kCompositedBackdropFilter = 6,
  kCompositedCopyRequest = 7,
  kCompositedHas3dTransform = 8,
  kCompositedHas2dShear = 9,
  kCompositedHas2dRotation = 10,
  kCompositedFeatureDisabled = 11,
  kCompositedCandidateFailed = 12,
  kCompositedCandidateBlending = 13,
  kCompositedCandidateQuadMaterial = 14,
  kCompositedCandidateBufferFormat = 15,
  kCompositedCandidateNearFilter = 16,
  // NOTE: DO NOT USE. kCompositedCandidateNotSharedImage has been deprecated.
  kCompositedCandidateNotSharedImage = 17,
  kCompositedCandidateMaskFilter = 18,
  kCompositedCandidateTransformCantClip = 19,
  kCompositedCandidateRpdqWithTransform = 20,
  kMaxValue = kCompositedCandidateRpdqWithTransform
};

// Promote a draw quad to a delegated overlay candidate. Also emit debugging
// information and map |CandidateStatus| to |DelegationStatus| in a uniform way
// across platforms.
// Returns the overlay candidate if it is visible or the reason why delegation
// failed.
base::expected<std::optional<OverlayCandidate>, DelegationStatus>
TryPromoteDrawQuadForDelegation(
    const OverlayCandidateFactory& candidate_factory,
    const DrawQuad* quad);

// Emit logs useful for debugging before delegation, e.g. Visual Debugger logs,
// histograms, trace events, etc.
void DebugLogBeforeDelegation(
    const gfx::Rect& incoming_root_damage,
    const SurfaceDamageRectList& surface_damage_rect_list);

// Same as |DebugLogBeforeDelegation|, but called after delegation has finished.
void DebugLogAfterDelegation(DelegationStatus status,
                             const OverlayCandidateList& candidates,
                             const gfx::Rect& outgoing_root_damage);

// Returns true if delegated compositing should be forcibly disabled, e.g. from
// the Visual Debugger.
bool ForceDisableDelegation();

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_DELEGATED_SUPPORT_H_
