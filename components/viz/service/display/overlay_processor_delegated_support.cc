// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_delegated_support.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace {

DBG_FLAG_FBOOL("delegated.disable.delegation", disable_delegation)

}  // namespace

namespace viz {

base::expected<std::optional<OverlayCandidate>, DelegationStatus>
TryPromoteDrawQuadForDelegation(
    const OverlayCandidateFactory& candidate_factory,
    const DrawQuad* quad) {
  const gfx::Transform& transform =
      quad->shared_quad_state->quad_to_target_transform;
  const gfx::RectF display_rect = transform.MapRect(gfx::RectF(quad->rect));
  DBG_DRAW_TEXT(
      "delegated.overlay.type",
      gfx::Vector2dF(display_rect.origin().x(), display_rect.origin().y()),
      base::StringPrintf("m=%d rid=%d", static_cast<int>(quad->material),
                         quad->resources.begin()->value()));

  OverlayCandidate candidate;
  OverlayCandidate::CandidateStatus candidate_status =
      candidate_factory.FromDrawQuad(quad, candidate);

  if (candidate_status == OverlayCandidate::CandidateStatus::kSuccess) {
    if (quad->material == DrawQuad::Material::kSolidColor) {
      DBG_DRAW_RECT("delegated.overlay.color", display_rect);
    } else if (quad->material == DrawQuad::Material::kAggregatedRenderPass) {
      DBG_DRAW_RECT("delegated.overlay.aggregated", display_rect);
    } else {
      DBG_DRAW_RECT("delegated.overlay.candidate", display_rect);

      if (!candidate.rounded_corners.IsEmpty()) {
        DBG_DRAW_RECT_OPT("delegated.overlay.candidate_rounded_corners",
                          DBG_OPT_BLUE, candidate.rounded_corners.rect());

        using Corner = gfx::RRectF::Corner;
        const Corner corners[] = {Corner::kUpperLeft, Corner::kUpperRight,
                                  Corner::kLowerRight, Corner::kLowerLeft};
        for (auto corner : corners) {
          auto corner_rect =
              candidate.rounded_corners.CornerBoundingRect(corner);
          DBG_DRAW_RECT_OPT("delegated.overlay.candidate_rounded_corners",
                            DBG_OPT_RED, corner_rect);
        }
      }
    }
    return base::ok(candidate);
  } else if (candidate_status ==
             OverlayCandidate::CandidateStatus::kFailVisible) {
    return base::ok(std::nullopt);
  } else {
    DBG_DRAW_RECT("delegated.overlay.failed", display_rect);
    DBG_LOG("delegated.overlay.failed", "error code %d",
            static_cast<int>(candidate_status));

    switch (candidate_status) {
      case OverlayCandidate::CandidateStatus::kFailNotAxisAligned:
        return base::unexpected(DelegationStatus::kCompositedNotAxisAligned);
      case OverlayCandidate::CandidateStatus::kFailNotAxisAligned3dTransform:
        return base::unexpected(DelegationStatus::kCompositedHas3dTransform);
      case OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dShear:
        return base::unexpected(DelegationStatus::kCompositedHas2dShear);
      case OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dRotation:
        return base::unexpected(DelegationStatus::kCompositedHas2dRotation);
      case OverlayCandidate::CandidateStatus::kFailNotOverlay:
        return base::unexpected(DelegationStatus::kCompositedNotOverlay);
      case OverlayCandidate::CandidateStatus::kFailBlending:
        return base::unexpected(DelegationStatus::kCompositedCandidateBlending);
      case OverlayCandidate::CandidateStatus::kFailQuadNotSupported:
        return base::unexpected(
            DelegationStatus::kCompositedCandidateQuadMaterial);
      case OverlayCandidate::CandidateStatus::kFailBufferFormat:
        return base::unexpected(
            DelegationStatus::kCompositedCandidateBufferFormat);
      case OverlayCandidate::CandidateStatus::kFailNearFilter:
        return base::unexpected(
            DelegationStatus::kCompositedCandidateNearFilter);
      case OverlayCandidate::CandidateStatus::kFailMaskFilterNotSupported:
        return base::unexpected(
            DelegationStatus::kCompositedCandidateMaskFilter);
      case OverlayCandidate::CandidateStatus::kFailHasTransformButCantClip:
        return base::unexpected(
            DelegationStatus::kCompositedCandidateTransformCantClip);
      case OverlayCandidate::CandidateStatus::kFailRpdqWithTransform:
        return base::unexpected(
            DelegationStatus::kCompositedCandidateRpdqWithTransform);
      default:
        return base::unexpected(DelegationStatus::kCompositedCandidateFailed);
    }
  }
}

void DebugLogBeforeDelegation(
    const gfx::Rect& incoming_root_damage,
    const SurfaceDamageRectList& surface_damage_rect_list) {
  DBG_DRAW_RECT("delegated.incoming.damage", incoming_root_damage);
  for (const auto& each : surface_damage_rect_list) {
    DBG_DRAW_RECT("delegated.surface.damage", each);
  }
}

void DebugLogAfterDelegation(DelegationStatus status,
                             const OverlayCandidateList& candidates,
                             const gfx::Rect& outgoing_root_damage) {
  UMA_HISTOGRAM_ENUMERATION("Viz.DelegatedCompositing.Status", status);
  DBG_LOG("delegation_status", "delegation status: %d",
          static_cast<int>(status));
  DBG_DRAW_RECT("delegated.outgoing.damage", outgoing_root_damage);

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates.size());

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                       "DelegatedCompositingStatus", TRACE_EVENT_SCOPE_THREAD,
                       "delegated_status", status);
}

bool ForceDisableDelegation() {
  return disable_delegation();
}

}  // namespace viz
