// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_using_strategy.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "media/gpu/buildflags.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

#include "components/viz/common/quads/texture_draw_quad.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace viz {
namespace {

// Gets the minimum scaling amount used by either dimension for the src relative
// to the dst.
float GetMinScaleFactor(const OverlayCandidate& candidate) {
  if (candidate.resource_size_in_pixels.IsEmpty() ||
      candidate.uv_rect.IsEmpty()) {
    return 1.0f;
  }
  return std::min(candidate.display_rect.width() /
                      (candidate.uv_rect.width() *
                       candidate.resource_size_in_pixels.width()),
                  candidate.display_rect.height() /
                      (candidate.uv_rect.height() *
                       candidate.resource_size_in_pixels.height()));
}

// Modifies an OverlayCandidate so that the |org_src_rect| (which should
// correspond to the src rect before any modifications were made) is scaled by
// |scale_factor| and then clipped and aligned on integral subsampling
// boundaries. This is used for dealing with required overlays and scaling
// limitations.
void ScaleCandidateSrcRect(const gfx::RectF& org_src_rect,
                           float scale_factor,
                           OverlayCandidate* candidate) {
  gfx::RectF src_rect(org_src_rect);
  src_rect.set_width(org_src_rect.width() / scale_factor);
  src_rect.set_height(org_src_rect.height() / scale_factor);

  // Make it an integral multiple of the subsampling factor.
  constexpr int kSubsamplingFactor = 2;
  src_rect.set_x(kSubsamplingFactor *
                 (std::lround(src_rect.x()) / kSubsamplingFactor));
  src_rect.set_y(kSubsamplingFactor *
                 (std::lround(src_rect.y()) / kSubsamplingFactor));
  src_rect.set_width(kSubsamplingFactor *
                     (std::lround(src_rect.width()) / kSubsamplingFactor));
  src_rect.set_height(kSubsamplingFactor *
                      (std::lround(src_rect.height()) / kSubsamplingFactor));
  // Scale it back into UV space and set it in the candidate.
  candidate->uv_rect = gfx::ScaleRect(
      src_rect, 1.0f / candidate->resource_size_in_pixels.width(),
      1.0f / candidate->resource_size_in_pixels.height());
}
}  // namespace

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

gfx::RectF OverlayProcessorUsingStrategy::Strategy::GetPrimaryPlaneDisplayRect(
    const PrimaryPlane* primary_plane) {
  return primary_plane ? primary_plane->display_rect : gfx::RectF();
}

OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy()
    : OverlayProcessorInterface() {}

OverlayProcessorUsingStrategy::~OverlayProcessorUsingStrategy() = default;

gfx::Rect OverlayProcessorUsingStrategy::GetPreviousFrameOverlaysBoundingRect()
    const {
  gfx::Rect result = overlay_damage_rect_;
  result.Union(previous_frame_overlay_rect_);
  return result;
}

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
    const gfx::Rect& display_rect,
    bool is_opaque_pure_overlay) {
  gfx::Rect root_damage_rect;

  if (overlay_damage_index == OverlayCandidate::kInvalidDamageIndex) {
    // An opaque overlay that is on top will hide any damage underneath.
    // TODO(petermcneeley): This is a special case optimization which could be
    // removed if we had more reliable damage.
    if (is_opaque_pure_overlay) {
      return gfx::SubtractRects(existing_damage, display_rect);
    }
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
      this_frame_overlay_rect, is_opaque_overlay && !is_underlay);

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
        it->candidate.resource_id, tracker_config_,
        it->candidate.overlay_damage_index !=
                OverlayCandidate::kInvalidDamageIndex ||
            it->candidate.assume_damaged);

    // Here a series of criteria are considered for wholesale rejection of a
    // candidate. The rational for rejection is usually power improvements but
    // this can indirectly reallocate limited overlay resources to another
    // candidate.
    bool passes_min_threshold =
        ((track_data.IsActivelyChanging(frame_sequence_number_,
                                        tracker_config_) ||
          !prioritization_config_.changing_threshold) &&
         (track_data.GetModeledPowerGain(frame_sequence_number_,
                                         tracker_config_, display_area) >= 0 ||
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
        // overlay we leave them in order so the topmost one gets the overlay.
        if (a.candidate.requires_overlay || b.candidate.requires_overlay) {
          return a.candidate.requires_overlay && !b.candidate.requires_overlay;
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

  size_t num_proposed_pre_sort = proposed_candidates.size();
  UMA_HISTOGRAM_COUNTS_1000(
      "Viz.DisplayCompositor.OverlayNumProposedCandidates",
      num_proposed_pre_sort);

  SortProposedOverlayCandidatesPrioritized(&proposed_candidates);

  for (auto&& candidate : proposed_candidates) {
    // Underlays change the material so we save it here to record proper UMA.
    DrawQuad::Material quad_material =
        candidate.strategy->GetUMAEnum() != OverlayStrategy::kUnknown
            ? candidate.quad_iter->material
            : DrawQuad::Material::kInvalid;

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_VAAPI)
    // For protected surfaces, which require overlays, we may need to check if
    // that surface can still be displayed. There are cases where HW context
    // loss can occur where it would not be properly displayable.
    // TODO(jkardatzke): This will not handle the case where those buffers are
    // already in flight. That will be fixed by a kernel driver update later.
    if (candidate.candidate.requires_overlay &&
        candidate.candidate.hw_protected_validation_id &&
        media::VaapiWrapper::GetProtectedInstanceID() !=
            candidate.candidate.hw_protected_validation_id) {
      continue;
    }
#endif
    bool used_overlay = candidate.strategy->AttemptPrioritized(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_pass_list, surface_damage_rect_list, primary_plane, candidates,
        content_bounds, &candidate);
    if (!used_overlay && candidate.candidate.requires_overlay) {
      // Check if we likely failed due to scaling capabilities, and if so, try
      // to adjust things to make it work. We do this by tracking what scale
      // factors succeed for downscaling, and then if we hit a failure case we
      // decrease the amount iteratively until it succeeds. We then cache that
      // information as hints to speed up the process next time around.
      // When we scale less, we then clip instead in order to fit into the
      // target area.  This is more visually appealing than blacking out the
      // quad since an overlay is required.
      float scale_factor = GetMinScaleFactor(candidate.candidate);
      if (scale_factor < 1.0f) {
        // When we are trying to determine the min allowed downscale, this is
        // the amount we will adjust the factor by for each iteration we
        // attempt.
        constexpr float kScaleAdjust = 0.05f;
        gfx::RectF org_src_rect = gfx::ScaleRect(
            candidate.candidate.uv_rect,
            candidate.candidate.resource_size_in_pixels.width(),
            candidate.candidate.resource_size_in_pixels.height());
        for (float new_scale_factor = std::min(
                 min_working_scale_,
                 std::max(max_failed_scale_, scale_factor) + kScaleAdjust);
             new_scale_factor < 1.0f; new_scale_factor += kScaleAdjust) {
          float zoom_scale = new_scale_factor / scale_factor;
          ScaleCandidateSrcRect(org_src_rect, zoom_scale, &candidate.candidate);
          if (candidate.strategy->AttemptPrioritized(
                  output_color_matrix, render_pass_backdrop_filters,
                  resource_provider, render_pass_list, surface_damage_rect_list,
                  primary_plane, candidates, content_bounds, &candidate)) {
            used_overlay = true;
            break;
          } else {
            UpdateDownscalingCapabilities(new_scale_factor, /*success=*/false);
          }
        }
      }
    }
    if (used_overlay) {
      // This function is used by underlay strategy to mark the primary plane as
      // enable_blending.
      candidate.strategy->AdjustOutputSurfaceOverlay(primary_plane);
      LogStrategyEnumUMA(candidate.strategy->GetUMAEnum());
      last_successful_strategy_ = candidate.strategy;
      OnOverlaySwitchUMA(ToProposeKey(candidate));
      UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayQuadMaterial",
                                quad_material);
      if (candidate.candidate.requires_overlay) {
        // Track how much we can downscale successfully.
        float scale_factor = GetMinScaleFactor(candidate.candidate);
        if (scale_factor < 1.0f) {
          UpdateDownscalingCapabilities(scale_factor, /*success=*/true);
        }
      }
      return true;
    }
  }

  if (proposed_candidates.size() == 0) {
    LogStrategyEnumUMA(num_proposed_pre_sort != 0
                           ? OverlayStrategy::kNoStrategyFailMin
                           : OverlayStrategy::kNoStrategyUsed);
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

void OverlayProcessorUsingStrategy::UpdateDownscalingCapabilities(
    float scale_factor,
    bool success) {
  if (success) {
    // Adjust the working bound up by this amount so we don't end up with
    // floating point errors based on the true minimum that actually
    // works.
    constexpr float kScaleBoundsTolerance = 0.001f;
    min_working_scale_ =
        std::min(scale_factor + kScaleBoundsTolerance, min_working_scale_);
    // If something worked that failed before, reset the known maximum for
    // failure.
    if (min_working_scale_ < max_failed_scale_)
      max_failed_scale_ = 0.0f;
    return;
  }

  max_failed_scale_ = std::max(max_failed_scale_, scale_factor);
  // If something failed that worked before, reset the known working
  // minimum.
  if (max_failed_scale_ > min_working_scale_)
    min_working_scale_ = 1.0f;
}

}  // namespace viz
