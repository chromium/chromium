// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_using_strategy.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/quad_list.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_combination_cache.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

#include "components/viz/common/quads/texture_draw_quad.h"

namespace viz {
namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Used by UMA histogram that tells us if we're attempting multiple overlays,
// or why we aren't.
enum class AttemptingMultipleOverlays {
  kYes = 0,
  kNoFeatureDisabled = 1,
  kNoRequiredOverlay = 2,
  kNoUnsupportedStrategy = 3,
  kMaxValue = kNoUnsupportedStrategy,
};

constexpr char kShouldAttemptMultipleOverlaysHistogramName[] =
    "Compositing.Display.OverlayProcessorUsingStrategy."
    "ShouldAttemptMultipleOverlays";
constexpr char kNumOverlaysPromotedHistogramName[] =
    "Compositing.Display.OverlayProcessorUsingStrategy.NumOverlaysPromoted";
constexpr char kNumOverlaysAttemptedHistogramName[] =
    "Compositing.Display.OverlayProcessorUsingStrategy.NumOverlaysAttempted";
constexpr char kNumOverlaysFailedHistogramName[] =
    "Compositing.Display.OverlayProcessorUsingStrategy.NumOverlaysFailed";

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

OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy()
    : max_overlays_config_(features::MaxOverlaysConsidered()) {}

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
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/181974042):  Remove when color space is plumbed.
  if (output_surface_plane)
    primary_plane_color_space_ = output_surface_plane->color_space;
#endif
  TRACE_EVENT0("viz", "OverlayProcessorUsingStrategy::ProcessForOverlays");
  DCHECK(candidates->empty());
  auto* render_pass = render_passes->back().get();
  bool success = false;

  UMA_HISTOGRAM_COUNTS_1000(
      "Compositing.Display.OverlayProcessorUsingStrategy.NumQuadsConsidered",
      render_pass->quad_list.size());

  DBG_DRAW_RECT("overlay.incoming.damage", (*damage_rect));
  for (auto&& each : surface_damage_rect_list) {
    DBG_DRAW_RECT("overlay.surface.damage", each);
  }

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (render_pass->copy_requests.empty()) {
    success = AttemptWithStrategies(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_passes, &surface_damage_rect_list, output_surface_plane,
        candidates, content_bounds, damage_rect);
  }
  LogCheckOverlaySupportMetrics();

  DCHECK(candidates->empty() || success);
  UMA_HISTOGRAM_COUNTS_100(kNumOverlaysPromotedHistogramName,
                           candidates->size());

  UpdateOverlayStatusMap(*candidates);
  UpdateDamageRect(surface_damage_rect_list, *damage_rect);

  NotifyOverlayPromotion(resource_provider, *candidates,
                         render_pass->quad_list);

  for (auto& selected_candidate : *candidates) {
    DBG_DRAW_RECT("overlay.selected.rect", selected_candidate.display_rect);
  }

  DBG_DRAW_RECT("overlay.outgoing.damage", (*damage_rect));

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

void OverlayProcessorUsingStrategy::CheckOverlaySupport(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidate_list) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/181974042):  Remove when color space is plumbed.
  if (primary_plane)
    primary_plane_color_space_ = primary_plane->color_space;
#endif

  base::ElapsedTimer timer;
  CheckOverlaySupportImpl(primary_plane, candidate_list);
  check_overlay_support_call_count_++;

  base::TimeDelta time = timer.Elapsed();

  static constexpr base::TimeDelta kMinTime = base::Microseconds(1);
  static constexpr base::TimeDelta kMaxTime = base::Milliseconds(10);
  static constexpr int kTimeBuckets = 50;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Compositing.Display.OverlayProcessorUsingStrategy.CheckOverlaySupportUs",
      time, kMinTime, kMaxTime, kTimeBuckets);
}

void OverlayProcessorUsingStrategy::ClearOverlayCombinationCache() {
  overlay_combination_cache_.ClearCache();
}

// This local function simply recomputes the root damage from
// |surface_damage_rect_list| while excluding the damage contribution from a
// specific overlay.
// TODO(petermcneeley): Eventually this code should be commonized in the same
// location as the definition of |SurfaceDamageRectList|
gfx::Rect OverlayProcessorUsingStrategy::ComputeDamageExcludingOverlays(
    const SurfaceDamageRectList& surface_damage_rect_list,
    const gfx::Rect& existing_damage) {
  if (curr_overlays_.empty()) {
    return existing_damage;
  }

  if (curr_overlays_.size() == 1) {
    auto& status = curr_overlays_.begin()->second;
    if (status.damage_index == OverlayCandidate::kInvalidDamageIndex) {
      // An opaque overlay that is on top will hide any damage underneath.
      // TODO(petermcneeley): This is a special case optimization which could be
      // removed if we had more reliable damage.
      if (status.is_opaque && !status.is_underlay) {
        return gfx::SubtractRects(existing_damage, status.overlay_rect);
      }
      return existing_damage;
    }
  }

  // Create a list of iterators to all `curr_overlays_` entries, sorted by
  // damage index, so we can encounter them in order as we loop through
  // `surface_damage_rect_list`.
  std::vector<OverlayStatusMap::const_iterator> overlay_status_iters;
  overlay_status_iters.reserve(curr_overlays_.size());
  for (auto it = curr_overlays_.begin(); it != curr_overlays_.end(); ++it) {
    overlay_status_iters.push_back(it);
  }
  std::sort(overlay_status_iters.begin(), overlay_status_iters.end(),
            [](const auto& it1, const auto& it2) {
              return it1->second.damage_index < it2->second.damage_index;
            });

  gfx::Rect computed_damage_rect;
  auto overlay_status_it = overlay_status_iters.begin();
  std::vector<gfx::Rect> occluding_rects;
  for (size_t i = 0; i < surface_damage_rect_list.size(); i++) {
    size_t curr_overlay_damage_index =
        overlay_status_it != overlay_status_iters.end()
            ? (*overlay_status_it)->second.damage_index
            : OverlayCandidate::kInvalidDamageIndex;
    if (curr_overlay_damage_index == i) {
      const OverlayStatus& status = (*overlay_status_it)->second;
      // |surface_damage_rect_list| is ordered such that from here on this
      // |overlay_rect| will act as an occluder for damage after.
      if (status.is_opaque) {
        occluding_rects.push_back(status.overlay_rect);
      }
      overlay_status_it++;
    } else {
      gfx::Rect curr_surface_damage = surface_damage_rect_list[i];

      // The |surface_damage_rect_list| can include damage rects coming from
      // outside and partially outside the original |existing_damage| area. This
      // is due to the conditional inclusion of these damage rects based on
      // target damage in surface aggregator. So by restricting this damage to
      // the |existing_damage| we avoid unnecessary final damage output.
      // https://crbug.com/1197609
      curr_surface_damage.Intersect(existing_damage);

      // Only add damage back in if it is not occluded by any overlays.
      bool is_occluded = base::ranges::any_of(
          occluding_rects, [&curr_surface_damage](const gfx::Rect& occluder) {
            return occluder.Contains(curr_surface_damage);
          });
      if (!is_occluded) {
        computed_damage_rect.Union(curr_surface_damage);
      }
    }
  }
  return computed_damage_rect;
}

OverlayProcessorUsingStrategy::OverlayStatus::OverlayStatus(
    const OverlayCandidate& candidate,
    const gfx::Rect& key,
    const OverlayStatusMap& prev_overlays) {
  overlay_rect = ToEnclosedRect(candidate.display_rect);
  damage_rect = candidate.damage_rect;
  damage_index = candidate.overlay_damage_index;
  damage_area_estimate = candidate.damage_area_estimate;
  has_mask_filter = candidate.has_mask_filter;
  plane_z_order = candidate.plane_z_order;
  is_underlay = candidate.plane_z_order < 0;
  is_opaque = candidate.is_opaque;

  auto prev_it = prev_overlays.find(key);
  if (prev_it != prev_overlays.end()) {
    is_new = false;
    prev_was_opaque = prev_it->second.is_opaque;
    prev_was_underlay = prev_it->second.is_underlay;
    prev_has_mask_filter = prev_it->second.has_mask_filter;
  } else {
    is_new = true;
    prev_was_opaque = true;
    prev_was_underlay = false;
    prev_has_mask_filter = false;
  }
}

OverlayProcessorUsingStrategy::OverlayStatus::OverlayStatus(
    const OverlayStatus&) = default;
OverlayProcessorUsingStrategy::OverlayStatus&
OverlayProcessorUsingStrategy::OverlayStatus::operator=(const OverlayStatus&) =
    default;
OverlayProcessorUsingStrategy::OverlayStatus::~OverlayStatus() = default;

void OverlayProcessorUsingStrategy::UpdateOverlayStatusMap(
    const OverlayCandidateList& candidates) {
  // Move `curr_overlays_` into `prev_overlays_`
  prev_overlays_.clear();
  curr_overlays_.swap(prev_overlays_);

  for (auto& candidate : candidates) {
    gfx::Rect key = GetOverlayDamageRectForOutputSurface(candidate);
    curr_overlays_.emplace(key, OverlayStatus(candidate, key, prev_overlays_));
  }
}

// Exclude overlay damage from the root damage when possible. In the steady
// state the overlay damage is always removed but transitions can require us to
// apply damage for the entire display size of the overlay. Underlays need to
// provide transition damage on both promotion and demotion as in both cases
// they need to change the primary plane (underlays need a primary plane black
// transparent quad). Overlays only need to produce transition damage on
// demotion as they do not use the primary plane during promoted phase.
void OverlayProcessorUsingStrategy::UpdateDamageRect(
    const SurfaceDamageRectList& surface_damage_rect_list,
    gfx::Rect& damage_rect) {
  DCHECK_LE(curr_overlays_.size(),
            static_cast<size_t>(max_overlays_considered_));

  // Remove all damage caused by these overlays, and any damage they occlude.
  damage_rect =
      ComputeDamageExcludingOverlays(surface_damage_rect_list, damage_rect);
  previous_frame_overlay_rect_ = gfx::Rect();

  for (auto& [key, status] : curr_overlays_) {
    if (!status.is_underlay) {
      overlay_damage_rect_.Union(status.overlay_rect);
    }
    previous_frame_overlay_rect_.Union(status.overlay_rect);

    // Our current overlays need to damage the primary plane in these cases:
    //  - A previous overlay became an Underlay this frame
    //  - An overlay became transparent this frame
    //  - An newly promoted underlay or transparent overlay
    //  - An overlay that added/removed a mask filter this frame
    //
    //  Rationale:
    //  - Related bugs: https://crbug.com/875879  https://crbug.com/1107460
    //  - We need to make sure that when we transition to an underlay, we damage
    //    the region where the underlay will be positioned. This is because a
    //    black transparent hole is made for the underlay to show through,
    //    but its possible that the damage for this quad is less than the
    //    complete size of the underlay. https://crbug.com/1130733
    //  - The primary plane may be visible underneath transparent overlays, so
    //    we need to damage it to remove any trace this quad left behind.
    //    https://buganizer.corp.google.com/issues/192294199
    if ((!status.prev_was_underlay && status.is_underlay) ||
        (status.prev_was_opaque && !status.is_opaque) ||
        (status.is_new && (status.is_underlay || !status.is_opaque)) ||
        (status.has_mask_filter != status.prev_has_mask_filter)) {
      damage_rect.Union(status.overlay_rect);
    }
  }
  // Damage is required for any overlays from the last frame that got demoted
  // this frame.
  for (auto& [key, status] : prev_overlays_) {
    // Overlays present last frame that are absent this frame have been demoted.
    if (curr_overlays_.find(key) == curr_overlays_.end()) {
      damage_rect.Union(status.overlay_rect);
    }
  }

  // Record each on top and underlay candidate.
  for (auto it : curr_overlays_) {
    const auto& status = it.second;
    if (status.plane_z_order != 0) {
      RecordOverlayDamageRectHistograms(status.plane_z_order > 0,
                                        status.damage_area_estimate != 0.f,
                                        damage_rect.IsEmpty());
    }
  }
}

void OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
    absl::optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane || !output_surface_plane->has_value())
    return;

  // If the overlay candidates cover the entire screen, the
  // |output_surface_plane| could be removed.
  if (last_successful_strategy_ &&
      last_successful_strategy_->RemoveOutputSurfaceAsOverlay())
    output_surface_plane->reset();
}

void OverlayProcessorUsingStrategy::SortProposedOverlayCandidates(
    std::vector<OverlayProposedCandidate>* proposed_candidates) {
  // Removes trackers for candidates that are no longer being rendered.
  for (auto it = tracked_candidates_.begin();
       it != tracked_candidates_.end();) {
    if (it->second.IsAbsent()) {
      it = tracked_candidates_.erase(it);
    } else {
      ++it;
    }
  }

  // This loop fills in data for the heuristic sort and thresholds candidates.
  for (auto it = proposed_candidates->begin();
       it != proposed_candidates->end();) {
    auto key = OverlayProposedCandidate::ToProposeKey(*it);
    // If no tracking exists we create a new one here.
    auto& track_data = tracked_candidates_[key];
    DBG_DRAW_TEXT_OPT(
        "candidate.surface.id", DBG_OPT_GREEN,
        it->candidate.display_rect.origin(),
        base::StringPrintf("%X , %d", key.tracking_id, key.strategy_id)
            .c_str());
    DBG_DRAW_TEXT_OPT(
        "candidate.mean.damage", DBG_OPT_GREEN,
        it->candidate.display_rect.origin(),
        base::StringPrintf(
            " %f, %f %d", track_data.MeanFrameRatioRate(tracker_config_),
            track_data.GetDamageRatioRate(),
            static_cast<int>(it->candidate.resource_id.value())));
    const auto display_area = it->candidate.display_rect.size().GetArea();
    // The |force_update| case is where we have damage and a damage index but
    // there are no changes in the |resource_id|. This is only known to occur
    // for low latency surfaces (inking like in the google keeps application).
    const bool force_update = it->candidate.overlay_damage_index !=
                                  OverlayCandidate::kInvalidDamageIndex &&
                              it->candidate.damage_area_estimate != 0.f;
    track_data.AddRecord(frame_sequence_number_,
                         it->candidate.damage_area_estimate / display_area,
                         it->candidate.resource_id, tracker_config_,
                         force_update);
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

bool OverlayProcessorUsingStrategy::AttemptWithStrategies(
    const SkM44& output_color_matrix,
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
  std::vector<OverlayProposedCandidate> proposed_candidates;
  for (const auto& strategy : strategies_) {
    strategy->Propose(output_color_matrix, render_pass_backdrop_filters,
                      resource_provider, render_pass_list,
                      surface_damage_rect_list, primary_plane,
                      &proposed_candidates, content_bounds);
  }

  size_t num_proposed_pre_sort = proposed_candidates.size();
  UMA_HISTOGRAM_COUNTS_1000(
      "Viz.DisplayCompositor.OverlayNumProposedCandidates",
      num_proposed_pre_sort);

  SortProposedOverlayCandidates(&proposed_candidates);
  if (proposed_candidates.size() == 0) {
    LogStrategyEnumUMA(num_proposed_pre_sort != 0
                           ? OverlayStrategy::kNoStrategyFailMin
                           : OverlayStrategy::kNoStrategyUsed);
  }

  if (ShouldAttemptMultipleOverlays(proposed_candidates)) {
    auto* render_pass = render_pass_list->back().get();
    return AttemptMultipleOverlays(proposed_candidates, primary_plane,
                                   render_pass, *candidates);
  }

  bool has_required_overlay = false;
  for (auto&& candidate : proposed_candidates) {
    // Underlays change the material so we save it here to record proper UMA.
    DrawQuad::Material quad_material =
        candidate.strategy->GetUMAEnum() != OverlayStrategy::kUnknown
            ? candidate.quad_iter->material
            : DrawQuad::Material::kInvalid;
    if (candidate.candidate.requires_overlay)
      has_required_overlay = true;

    bool used_overlay = candidate.strategy->Attempt(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_pass_list, surface_damage_rect_list, primary_plane, candidates,
        content_bounds, candidate);
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
          if (candidate.strategy->Attempt(
                  output_color_matrix, render_pass_backdrop_filters,
                  resource_provider, render_pass_list, surface_damage_rect_list,
                  primary_plane, candidates, content_bounds, candidate)) {
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
      OnOverlaySwitchUMA(OverlayProposedCandidate::ToProposeKey(candidate));
      UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayQuadMaterial",
                                quad_material);
      if (candidate.candidate.requires_overlay) {
        // Track how much we can downscale successfully.
        float scale_factor = GetMinScaleFactor(candidate.candidate);
        if (scale_factor < 1.0f) {
          UpdateDownscalingCapabilities(scale_factor, /*success=*/true);
        }
      }
      RegisterOverlayRequirement(has_required_overlay);
      return true;
    }
  }
  RegisterOverlayRequirement(has_required_overlay);

  if (proposed_candidates.size() != 0) {
    LogStrategyEnumUMA(OverlayStrategy::kNoStrategyAllFail);
  }
  OnOverlaySwitchUMA(ProposedCandidateKey());
  return false;
}

bool OverlayProcessorUsingStrategy::ShouldAttemptMultipleOverlays(
    const std::vector<OverlayProposedCandidate>& sorted_candidates) {
  if (max_overlays_config_ <= 1) {
    UMA_HISTOGRAM_ENUMERATION(kShouldAttemptMultipleOverlaysHistogramName,
                              AttemptingMultipleOverlays::kNoFeatureDisabled);
    return false;
  }

  for (auto& proposed : sorted_candidates) {
    // When candidates that require overlays fail, they get retried with
    // different scale factors. This becomes complicated when using multiple
    // overlays at once so we won't attempt multiple in that case.
    if (proposed.candidate.requires_overlay) {
      UMA_HISTOGRAM_ENUMERATION(kShouldAttemptMultipleOverlaysHistogramName,
                                AttemptingMultipleOverlays::kNoRequiredOverlay);
      return false;
    }
    // Using multiple overlays only makes sense with SingleOnTop and Underlay
    // strategies.
    OverlayStrategy type = proposed.strategy->GetUMAEnum();
    if (type != OverlayStrategy::kSingleOnTop &&
        type != OverlayStrategy::kUnderlay) {
      UMA_HISTOGRAM_ENUMERATION(
          kShouldAttemptMultipleOverlaysHistogramName,
          AttemptingMultipleOverlays::kNoUnsupportedStrategy);
      return false;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(kShouldAttemptMultipleOverlaysHistogramName,
                            AttemptingMultipleOverlays::kYes);
  return true;
}

bool OverlayProcessorUsingStrategy::AttemptMultipleOverlays(
    const std::vector<OverlayProposedCandidate>& sorted_candidates,
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    AggregatedRenderPass* render_pass,
    OverlayCandidateList& candidates) {
  if (sorted_candidates.empty()) {
    UMA_HISTOGRAM_COUNTS_100(kNumOverlaysAttemptedHistogramName, 0);
    UMA_HISTOGRAM_COUNTS_100(kNumOverlaysFailedHistogramName, 0);
    return false;
  }

  OverlayCombinationToTest result =
      overlay_combination_cache_.GetOverlayCombinationToTest(
          sorted_candidates, max_overlays_considered_);
  std::vector<OverlayProposedCandidate> test_candidates =
      result.candidates_to_test;
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.Display.OverlayProcessorUsingStrategy."
      "CandidateCombinationPreviouslySucceeded",
      result.previously_succeeded);

  bool testing_underlay = false;
  // We'll keep track of the underlays that we're testing so we can assign their
  // `plane_z_order`s based on their order in the QuadList.
  std::vector<std::vector<OverlayProposedCandidate>::iterator> underlay_iters;

  for (auto it = test_candidates.begin(); it != test_candidates.end(); ++it) {
    switch (it->strategy->GetUMAEnum()) {
      case OverlayStrategy::kSingleOnTop:
        // Ordering of on top candidates doesn't matter (they can't overlap), so
        // they can all have z = 1.
        it->candidate.plane_z_order = 1;
        break;
      case OverlayStrategy::kUnderlay:
        testing_underlay = true;
        underlay_iters.push_back(it);
        break;
      default:
        // Unsupported strategy type.
        NOTREACHED();
    }
  }

  // We don't sort the actual items in `test_candidates` here in order to
  // maintain the power-gain sorted order.
  AssignUnderlayZOrders(underlay_iters);

  candidates.reserve(test_candidates.size());
  for (auto& proposed_candidate : test_candidates) {
    candidates.push_back(proposed_candidate.candidate);
  }

  if (!testing_underlay || !primary_plane) {
    CheckOverlaySupport(primary_plane, &candidates);
  } else {
    OverlayProcessorStrategy::PrimaryPlane new_plane_candidate(*primary_plane);
    new_plane_candidate.enable_blending = true;
    // Check for support.
    CheckOverlaySupport(&new_plane_candidate, &candidates);
  }
  const int num_overlays_attempted = candidates.size();

  bool underlay_used = false;
  auto cand_it = candidates.begin();
  auto test_it = test_candidates.begin();
  while (cand_it != candidates.end()) {
    // Update the test candidates so we can use EraseIf below, and so we can
    // tell the OverlayCombinationCache which ones succeeded/failed.
    test_it->candidate.overlay_handled = cand_it->overlay_handled;
    if (cand_it->overlay_handled && cand_it->plane_z_order < 0) {
      underlay_used = true;
    }
    cand_it++;
    test_it++;
  }
  overlay_combination_cache_.DeclarePromotedCandidates(test_candidates);

  // Remove failed candidates
  base::EraseIf(candidates, [](auto& cand) { return !cand.overlay_handled; });
  base::EraseIf(test_candidates, [](auto& proposed) -> bool {
    return !proposed.candidate.overlay_handled;
  });
  const int num_overlays_promoted = candidates.size();

  UMA_HISTOGRAM_COUNTS_100(kNumOverlaysAttemptedHistogramName,
                           num_overlays_attempted);
  UMA_HISTOGRAM_COUNTS_100(kNumOverlaysFailedHistogramName,
                           num_overlays_attempted - num_overlays_promoted);

  if (candidates.empty()) {
    LogStrategyEnumUMA(OverlayStrategy::kNoStrategyAllFail);
    return false;
  }

  if (underlay_used && primary_plane) {
    // Using underlays means the primary plane needs blending enabled.
    primary_plane->enable_blending = true;
  }

  // Sort test candidates in reverse order so we can commit them from back to
  // front. This makes sure none of the quad iterators are invalidated when some
  // are removed from the QuadList as they're committed.
  //
  // TODO(khaslett): Remove this hacky workaround. Instead of erasing quads we
  // could probably replace them with solid colour quads or make them invisible
  // instead.
  std::sort(test_candidates.begin(), test_candidates.end(),
            [](const OverlayProposedCandidate& c1,
               const OverlayProposedCandidate& c2) -> bool {
              return c1.quad_iter.index() > c2.quad_iter.index();
            });
  // Commit successful candidates.
  for (auto& test_candidate : test_candidates) {
    test_candidate.strategy->CommitCandidate(test_candidate, render_pass);
    LogStrategyEnumUMA(test_candidate.strategy->GetUMAEnum());
  }

  return true;
}

void OverlayProcessorUsingStrategy::AssignUnderlayZOrders(
    std::vector<std::vector<OverlayProposedCandidate>::iterator>&
        underlay_iters) {
  // Sort the underlay iterators by DrawQuad order, frontmost first.
  std::sort(
      underlay_iters.begin(), underlay_iters.end(),
      [](const std::vector<OverlayProposedCandidate>::iterator& c1,
         const std::vector<OverlayProposedCandidate>::iterator& c2) -> bool {
        return c1->quad_iter.index() < c2->quad_iter.index();
      });
  // Assign underlay candidate plane_z_orders based on DrawQuad order.
  int underlay_z_order = -1;
  for (auto& it : underlay_iters) {
    it->candidate.plane_z_order = underlay_z_order--;
  }
}

gfx::Rect OverlayProcessorUsingStrategy::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorUsingStrategy::OnOverlaySwitchUMA(
    ProposedCandidateKey overlay_tracking_id) {
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
  // This is the worst case scale factor we should ever run into. In reality
  // it's actually more like 0.68, but I'm making it larger to be safe and we
  // also always add 0.05 to this value when we make use of it so we are
  // effectively bounding it at 0.75. We can end up getting incorrect signals
  // about scaling capabilities when displays power off and overlay promotion
  // doesn't work, so for that reason so we can't assume all failures are
  // legitimate.
  constexpr float kMaxFailedScaleMin = 0.70f;
  max_failed_scale_ = std::min(max_failed_scale_, kMaxFailedScaleMin);
}

void OverlayProcessorUsingStrategy::LogCheckOverlaySupportMetrics() {
  UMA_HISTOGRAM_COUNTS_100(
      "Compositing.Display.OverlayProcessorUsingStrategy."
      "CheckOverlaySupportCallCount",
      check_overlay_support_call_count_);
  check_overlay_support_call_count_ = 0;
}

}  // namespace viz
