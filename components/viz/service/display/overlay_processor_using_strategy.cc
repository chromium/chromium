// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_using_strategy.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

#include "components/viz/common/quads/texture_draw_quad.h"

namespace viz {

static void LogStrategyEnumUMA(OverlayStrategy strategy) {
  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy", strategy);
}

OverlayProcessorUsingStrategy::ProposedCandidateKey
OverlayProcessorUsingStrategy::ToProposeKey(
    const OverlayProcessorUsingStrategy::Strategy::OverlayProposedCandidate&
        proposed) {
  return {gfx::ToRoundedRect(proposed.candidate.display_rect),
          proposed.strategy->GetUMAEnum()};
}

// Default implementation of whether a strategy would remove the output surface
// as overlay plane.
bool OverlayProcessorUsingStrategy::Strategy::RemoveOutputSurfaceAsOverlay() {
  return false;
}

OverlayStrategy OverlayProcessorUsingStrategy::Strategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy()
    : OverlayProcessorInterface() {}

OverlayProcessorUsingStrategy::~OverlayProcessorUsingStrategy() = default;

gfx::Rect OverlayProcessorUsingStrategy::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

void OverlayProcessorUsingStrategy::NotifyOverlayPromotion(
    DisplayResourceProvider* display_resource_provider,
    const CandidateList& candidates,
    const QuadList& quad_list) {}

void OverlayProcessorUsingStrategy::SetFrameSequenceNumber(
    uint64_t frame_sequence_number) {
  frame_sequence_number_ = frame_sequence_number;
}

void OverlayProcessorUsingStrategy::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessorUsingStrategy::ProcessForOverlays");
  DCHECK(candidates->empty());
  auto* render_pass = render_passes->back().get();
  bool success = false;

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (render_pass->copy_requests.empty()) {
    if (features::IsOverlayPrioritizationEnabled()) {
      success = AttemptWithStrategiesPrioritized(
          output_color_matrix, render_pass_backdrop_filters, resource_provider,
          render_passes, &surface_damage_rect_list, output_surface_plane,
          candidates, content_bounds, damage_rect);
    } else {
      success = AttemptWithStrategies(
          output_color_matrix, render_pass_backdrop_filters, resource_provider,
          render_passes, &surface_damage_rect_list, output_surface_plane,
          candidates, content_bounds);
    }
  }

  DCHECK(candidates->empty() || success);

  UpdateDamageRect(candidates, &surface_damage_rect_list,
                   &render_pass->quad_list, damage_rect);

  NotifyOverlayPromotion(resource_provider, *candidates,
                         render_pass->quad_list);

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

// This local function simply recomputes the root damage from
// |surface_damage_rect_list| while excluding the damage contribution from a
// specific overlay.
// TODO(petermcneeley): Eventually this code should be commonized in the same
// location as the definition of |SurfaceDamageRectList|
namespace {
gfx::Rect ComputeDamageExcludingIndex(
    uint32_t overlay_damage_index,
    SurfaceDamageRectList* surface_damage_rect_list,
    const gfx::Rect& existing_damage,
    const gfx::Rect& display_rect) {
  gfx::Rect root_damage_rect;

  if (overlay_damage_index == OverlayCandidate::kInvalidDamageIndex) {
    return existing_damage;
  }

  gfx::Rect occluding_rect;
  for (size_t i = 0; i < surface_damage_rect_list->size(); i++) {
    if (overlay_damage_index != i) {
      // Only add damage back in if it is not occluded by the overlay.
      if (!occluding_rect.Contains((*surface_damage_rect_list)[i])) {
        root_damage_rect.Union((*surface_damage_rect_list)[i]);
      }
    } else {
      // |surface_damage_rect_list| is ordered such that from here on the
      // |display_rect| for the overlay will act as an occluder for damage
      // after.
      occluding_rect = display_rect;
    }
  }
  return root_damage_rect;
}
}  // namespace

// Exclude overlay damage from the root damage when possible. In the steady
// state the overlay damage is always removed but transitions can require us to
// apply damage for the entire display size of the overlay. Underlays need to
// provide transition damage on both promotion and demotion as in both cases
// they need to change the primary plane (underlays need a primary plane black
// transparent quad). Overlays only need to produce transition damage on
// demotion as they do not use the primary plane during promoted phase.
void OverlayProcessorUsingStrategy::UpdateDamageRect(
    OverlayCandidateList* candidates,
    SurfaceDamageRectList* surface_damage_rect_list,
    const QuadList* quad_list,
    gfx::Rect* damage_rect) {
  // TODO(petermcneeley): This code only supports one overlay candidate. To
  // support multiple overlays one would need to track the difference set of
  // overlays between frames.
  DCHECK_LE(candidates->size(), 1U);

  gfx::Rect this_frame_overlay_rect;
  bool is_opaque_overlay = false;
  bool is_underlay = false;
  uint32_t exclude_overlay_index = OverlayCandidate::kInvalidDamageIndex;

  for (const OverlayCandidate& overlay : *candidates) {
    this_frame_overlay_rect = GetOverlayDamageRectForOutputSurface(overlay);
    if (overlay.plane_z_order >= 0) {
      // If an overlay candidate comes from output surface, its z-order should
      // be 0.
      overlay_damage_rect_.Union(this_frame_overlay_rect);
      if (overlay.is_opaque) {
        is_opaque_overlay = true;
        exclude_overlay_index = overlay.overlay_damage_index;
      }
    } else {
      // Underlay candidate is assumed to be opaque.
      is_underlay = true;
      exclude_overlay_index = overlay.overlay_damage_index;
    }

    if (overlay.plane_z_order) {
      RecordOverlayDamageRectHistograms((overlay.plane_z_order > 0),
                                        overlay.damage_area_estimate != 0,
                                        damage_rect->IsEmpty());
    }
  }

  // Removes all damage from this overlay and occluded surface damages.
  *damage_rect = ComputeDamageExcludingIndex(
      exclude_overlay_index, surface_damage_rect_list, *damage_rect,
      this_frame_overlay_rect);

  // Track the overlay_rect from frame to frame. If it is the same and nothing
  // is on top of it then that rect doesn't need to be damaged because the
  // drawing is occurring on a different plane. If it is different then that
  // indicates that a different overlay has been chosen and the previous
  // overlay rect should be damaged because it has changed planes from the
  // overlay plane to the main plane. https://crbug.com/875879
  if ((!previous_is_underlay && is_underlay) ||
      this_frame_overlay_rect != previous_frame_overlay_rect_) {
    damage_rect->Union(previous_frame_overlay_rect_);

    // We need to make sure that when we transition to an underlay we damage the
    // region where the underlay will be positioned. This is because a
    // black transparent hole is made for the underlay to show through
    // but its possible that the damage for this quad is less than the
    // complete size of the underlay.  https://crbug.com/1130733
    if (!is_opaque_overlay) {
      damage_rect->Union(this_frame_overlay_rect);
    }
  }

  previous_frame_overlay_rect_ = this_frame_overlay_rect;
  previous_is_underlay = is_underlay;
}

void OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
    base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane || !output_surface_plane->has_value())
    return;

  // If the overlay candidates cover the entire screen, the
  // |output_surface_plane| could be removed.
  if (last_successful_strategy_ &&
      last_successful_strategy_->RemoveOutputSurfaceAsOverlay())
    output_surface_plane->reset();
}

bool OverlayProcessorUsingStrategy::AttemptWithStrategies(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) {
  last_successful_strategy_ = nullptr;
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                          resource_provider, render_pass_list,
                          surface_damage_rect_list, primary_plane, candidates,
                          content_bounds)) {
      // This function is used by underlay strategy to mark the primary plane as
      // enable_blending.
      strategy->AdjustOutputSurfaceOverlay(primary_plane);
      LogStrategyEnumUMA(strategy->GetUMAEnum());
      last_successful_strategy_ = strategy.get();
      return true;
    }
  }

  LogStrategyEnumUMA(OverlayStrategy::kNoStrategyUsed);
  return false;
}

void OverlayProcessorUsingStrategy::SortProposedOverlayCandidatesPrioritized(
    Strategy::OverlayProposedCandidateList* proposed_candidates) {
  // Removes trackers for candidates that are no longer being rendered.
  for (auto it = tracked_candidates.begin(); it != tracked_candidates.end();) {
    if (it->second.IsAbsent()) {
      it = tracked_candidates.erase(it);
    } else {
      ++it;
    }
  }

  // This loop fills in data for the heuristic sort and thresholds candidates.
  for (auto it = proposed_candidates->begin();
       it != proposed_candidates->end();) {
    auto key = ToProposeKey(*it);
    // If no tracking exists we create a new one here.
    auto& track_data = tracked_candidates[key];
    auto display_area = it->candidate.display_rect.size().GetArea();
    track_data.AddRecord(
        frame_sequence_number_,
        static_cast<float>(it->candidate.damage_area_estimate) / display_area,
        it->candidate.resource_id, tracker_config_);

    // Here a series of criteria are considered for wholesale rejection of a
    // candidate. The rational for rejection is usually power improvements but
    // this can indirectly reallocate limited overlay resources to another
    // candidate.
    bool passes_min_threshold =
        ((track_data.IsActivelyChanging(frame_sequence_number_,
                                        tracker_config_) ||
          !prioritization_config_.changing_threshold) &&
         (track_data.GetModeledPowerGain(frame_sequence_number_,
                                         tracker_config_, display_area) > 0 ||
          !prioritization_config_.damage_rate_threshold));

    if (it->candidate.requires_overlay || passes_min_threshold) {
      it->relative_power_gain = track_data.GetModeledPowerGain(
          frame_sequence_number_, tracker_config_, display_area);
      ++it;
    } else {
      // We 'Reset' rather than delete the |track_data| because this candidate
      // will still be present next frame.
      track_data.Reset();
      it = proposed_candidates->erase(it);
    }
  }

  // Heuristic sorting:
  // The stable sort of proposed candidates will not change the prioritized
  // order of candidates that have equal sort. What this means is that in a
  // situation where there are multiple candidates with identical rects we will
  // output a sort that respects the original strategies order. An example of
  // this would be the single_on_top strategy coming before the underlay
  // strategy for a overlay candidate that has zero occlusion. This sort
  // function must provide weak ordering.
  auto prio_config = prioritization_config_;
  std::stable_sort(
      proposed_candidates->begin(), proposed_candidates->end(),
      [prio_config](const auto& a, const auto& b) {
        // DRM/CDM HW overlay required:
        // This comparison is for correctness over performance reasons. Some
        // candidates must be an HW overlay to function. If both require an HW
        // overlay we sort on the remaining criteria below.
        if (a.candidate.requires_overlay ^ b.candidate.requires_overlay) {
          return a.candidate.requires_overlay;
        }

        // Opaque Power Metric:
        // |relative_power_gain| is computed in the tracker for each overlay
        // candidate and being proportional to power saved is directly
        // comparable.
        if (prio_config.power_gain_sort) {
          if (a.relative_power_gain != b.relative_power_gain) {
            return a.relative_power_gain > b.relative_power_gain;
          }
        }

        return false;
      });
}

bool OverlayProcessorUsingStrategy::AttemptWithStrategiesPrioritized(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_pass_list,
    SurfaceDamageRectList* surface_damage_rect_list,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds,
    gfx::Rect* incoming_damage) {
  last_successful_strategy_ = nullptr;

  Strategy::OverlayProposedCandidateList proposed_candidates;
  for (const auto& strategy : strategies_) {
    strategy->ProposePrioritized(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_pass_list, surface_damage_rect_list, primary_plane,
        &proposed_candidates, content_bounds);
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "Viz.DisplayCompositor.OverlayNumProposedCandidates",
      proposed_candidates.size());

  SortProposedOverlayCandidatesPrioritized(&proposed_candidates);

  for (auto&& candidate : proposed_candidates) {
    if (candidate.strategy->AttemptPrioritized(
            output_color_matrix, render_pass_backdrop_filters,
            resource_provider, render_pass_list, surface_damage_rect_list,
            primary_plane, candidates, content_bounds, &candidate)) {
      // This function is used by underlay strategy to mark the primary plane as
      // enable_blending.
      candidate.strategy->AdjustOutputSurfaceOverlay(primary_plane);
      LogStrategyEnumUMA(candidate.strategy->GetUMAEnum());
      last_successful_strategy_ = candidate.strategy;
      OnOverlaySwitchUMA(ToProposeKey(candidate));
      return true;
    }
  }

  if (proposed_candidates.size() == 0) {
    LogStrategyEnumUMA(OverlayStrategy::kNoStrategyUsed);
  } else {
    LogStrategyEnumUMA(OverlayStrategy::kNoStrategyAllFail);
  }
  OnOverlaySwitchUMA(ProposedCandidateKey());
  return false;
}

gfx::Rect OverlayProcessorUsingStrategy::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorUsingStrategy::OnOverlaySwitchUMA(
    OverlayProcessorUsingStrategy::ProposedCandidateKey overlay_tracking_id) {
  auto curr_tick = base::TimeTicks::Now();
  if (!(prev_overlay_tracking_id_ == overlay_tracking_id)) {
    prev_overlay_tracking_id_ = overlay_tracking_id;
    UMA_HISTOGRAM_TIMES("Viz.DisplayCompositor.OverlaySwitchInterval",
                        curr_tick - last_time_interval_switch_overlay_tick_);
    last_time_interval_switch_overlay_tick_ = curr_tick;
  }
}

}  // namespace viz
