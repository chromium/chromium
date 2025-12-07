// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_win.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "components/viz/service/display/overlay_processor_delegated_support.h"
#include "media/base/win/mf_feature_checks.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

constexpr int kDCLayerDebugBorderWidth = 4;
constexpr gfx::Insets kDCLayerDebugBorderInsets = gfx::Insets(-2);

// Switching between enabling DC layers and not is expensive, so only
// switch away after a large number of frames not needing DC layers have
// been produced.
constexpr int kNumberOfFramesBeforeDisablingDCLayers = 60;

// The maximum number of quads to attempt for delegated compositing. This is an
// arbitrary conservative value picked from experimentation. We don't expect to
// hit these limits in practice, but this guards against degenerate cases.
constexpr size_t kTooManyQuads = 2048;

// Rounded corners have a higher performance cost in DWM so this value is lower
// than |kTooManyQuads|.
constexpr int kTooManyQuadsWithRoundedCorners = 256;

gfx::Rect UpdateRenderPassFromOverlayData(
    const DCLayerOverlayProcessor::RenderPassOverlayData& overlay_data,
    AggregatedRenderPass* render_pass,
    base::flat_map<AggregatedRenderPassId, int>&
        frames_since_using_dc_layers_map,
    const bool force_dcomp_surface) {
  bool was_using_dc_layers =
      frames_since_using_dc_layers_map.contains(render_pass->id);

  // Force a swap chain when there is a copy request, since read back is
  // impossible with a DComp surface.
  //
  // Normally, |DCLayerOverlayProcessor::Process| prevents overlays (and thus
  // forces a swap chain) when there is a copy request, but
  // |frames_since_using_dc_layers_map| implements a one-sided hysteresis that
  // keeps us on DComp surfaces a little after we stop having overlays. If a
  // client issues a copy request while we're in this timeout, we end up asking
  // read back from a DComp surface, which fails later in
  // |SkiaOutputSurfaceImplOnGpu::CopyOutput|.
  const bool force_swap_chain_due_to_copy_request = render_pass->HasCapture();

  bool using_dc_layers;
  // Force DCompSurfaces during delegated ink in order to synchronize the
  // delegated ink visual updates with DComp commits. Doing so eliminates the
  // need to identify the correct swap chain in complicated delegated
  // compositing scenarios.
  if (!overlay_data.promoted_overlays.empty() || force_dcomp_surface) {
    frames_since_using_dc_layers_map[render_pass->id] = 0;
    using_dc_layers = true;
  } else if ((was_using_dc_layers &&
              ++frames_since_using_dc_layers_map[render_pass->id] >=
                  kNumberOfFramesBeforeDisablingDCLayers) ||
             force_swap_chain_due_to_copy_request) {
    frames_since_using_dc_layers_map.erase(render_pass->id);
    using_dc_layers = false;
  } else {
    using_dc_layers = was_using_dc_layers;
  }

  if (using_dc_layers) {
    // We have overlays, so our root surface requires a backing that
    // synchronizes with DComp commit. A swap chain's Present does not
    // synchronize with the DComp tree updates and would result in minor desync
    // during e.g. scrolling videos.
    render_pass->needs_synchronous_dcomp_commit = true;

    // We only need to have a transparent backing if there's underlays, but we
    // unconditionally ask for transparency to avoid thrashing allocations if a
    // video alternated between overlay and underlay.
    render_pass->has_transparent_background = true;
  } else {
    CHECK(!render_pass->needs_synchronous_dcomp_commit);
  }

  if (was_using_dc_layers != using_dc_layers) {
    // The entire surface has to be redrawn if switching from or to direct
    // composition layers, because the previous contents are discarded and some
    // contents would otherwise be undefined.
    return render_pass->output_rect;
  } else {
    // |DCLayerOverlayProcessor::Process| can modify the damage rect of the
    // render pass. We don't modify the damage on the render pass directly since
    // the root pass special-cases this.
    return overlay_data.damage_rect;
  }
}

OverlayCandidateFactory::OverlayContext WindowsDelegatedOverlayContext() {
  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.disable_wire_size_optimization = true;
  context.supports_clip_rect = true;
  context.supports_out_of_window_clip_rect = true;
  context.supports_arbitrary_transform = true;
  context.supports_rounded_display_masks = true;
  context.supports_mask_filter = true;
  context.transform_and_clip_rpdq = true;
  return context;
}

// Center and scale `rect` inside `target_size` to the largest size such that it
// maintains its aspect ratio and is fully contained by `target_size`.
gfx::RectF ContainedAndCenteredRect(const gfx::RectF& rect,
                                    const gfx::SizeF& target_size) {
  // Scale factor that makes `rect` fit inside `target_size`, touching at least
  // two opposite sides.
  const double rect_to_target_size_scale = std::min(
      target_size.width() / rect.width(), target_size.height() / rect.height());

  gfx::RectF scaled_and_centered_rect = gfx::RectF(rect.size());
  scaled_and_centered_rect.Scale(rect_to_target_size_scale);
  scaled_and_centered_rect.Offset(
      (target_size.width() - scaled_and_centered_rect.width()) / 2.0,
      (target_size.height() - scaled_and_centered_rect.height()) / 2.0);

  return scaled_and_centered_rect;
}

}  // anonymous namespace

OverlayProcessorWin::OverlayProcessorWin(
    OutputSurface::DCSupportLevel dc_support_level,
    bool disable_direct_composition_letterbox_video_optimization,
    const DebugRendererSettings* debug_settings,
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor)
    : delegated_compositing_supported_(
          IsDelegatedCompositingSupportedAndEnabled(dc_support_level)
              ? std::make_optional(
                    features::kDelegatedCompositingModeParam.Get())
              : std::nullopt),
      debug_settings_(debug_settings),
      dc_layer_overlay_processor_(std::move(dc_layer_overlay_processor)),
      disable_direct_composition_letterbox_video_optimization_(
          disable_direct_composition_letterbox_video_optimization) {
  DCHECK_GT(dc_support_level, OutputSurface::DCSupportLevel::kNone);
}

OverlayProcessorWin::~OverlayProcessorWin() = default;

bool OverlayProcessorWin::DisableSplittingQuads() const {
  return delegated_compositing_supported_ ==
         features::DelegatedCompositingMode::kFull;
}

bool OverlayProcessorWin::IsOverlaySupported() const {
  return true;
}

gfx::Rect OverlayProcessorWin::GetAndResetOverlayDamage() {
  return std::exchange(overlay_damage_rect_, gfx::Rect());
}

void OverlayProcessorWin::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list_in_root_space,
    std::optional<OverlayCandidate>& primary_plane,
    CandidateList* candidates,
    gfx::Rect* root_damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessorWin::ProcessForOverlays");

  DebugLogBeforeDelegation(*root_damage_rect,
                           surface_damage_rect_list_in_root_space);

  DelegationStatus status = ProcessOverlaysForDelegation(
      resource_provider, render_passes, output_color_matrix,
      render_pass_filters, render_pass_backdrop_filters,
      surface_damage_rect_list_in_root_space, candidates, root_damage_rect);

  if (status != DelegationStatus::kFullDelegation) {
    // Fall back to promoting overlays from the output surface plane.
    ProcessOverlaysFromOutputSurfacePlane(
        resource_provider, render_passes, output_color_matrix,
        render_pass_filters, render_pass_backdrop_filters,
        surface_damage_rect_list_in_root_space, candidates, root_damage_rect);

    CHECK(primary_plane);
    primary_plane->is_opaque =
        !render_passes->back()->has_transparent_background;
    primary_plane->layer_id = gfx::OverlayLayerId::MakeVizInternalRenderPass(
        render_passes->back()->id);
  }

  // Sort back-to-front to make the subsequent operations easier.
  std::ranges::sort(*candidates, {}, &OverlayCandidate::plane_z_order);

  if (is_page_fullscreen_mode_ &&
      base::FeatureList::IsEnabled(
          features::kEarlyFullScreenVideoOptimization)) {
    TryPromoteFullScreenVideo(*render_passes->back(), *candidates,
                              *root_damage_rect);
  }

  if (pending_remove_primary_plane_) {
    primary_plane.reset();
    pending_remove_primary_plane_ = false;
  }
  if (primary_plane) {
    // Insert the primary plane above all underlays.
    const auto insert_positon = std::ranges::find_if(
        *candidates, [](const auto& z_order) { return z_order > 0; },
        &OverlayCandidate::plane_z_order);
    candidates->insert(insert_positon, std::move(primary_plane).value());
    primary_plane.reset();
  }

  DebugLogAfterDelegation(status, *candidates, *root_damage_rect);

  frame_has_forced_dcomp_surface_ = false;
}

DelegationStatus OverlayProcessorWin::ProcessOverlaysForDelegation(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    const SurfaceDamageRectList& surface_damage_rect_list_in_root_space,
    CandidateList* candidates,
    gfx::Rect* root_damage_rect) {
  // Do not attempt delegated compositing if we do not support DComp textures
  // (and therefore cannot possibly scanout quad resources) or if the feature is
  // disabled.
  if (ForceDisableDelegation() || !delegated_compositing_supported_) {
    return DelegationStatus::kCompositedFeatureDisabled;
  }

  const bool is_full_delegated_compositing =
      delegated_compositing_supported_ ==
      features::DelegatedCompositingMode::kFull;

  OverlayCandidateFactory factory(
      render_passes->back().get(), resource_provider,
      &surface_damage_rect_list_in_root_space, &output_color_matrix,
      gfx::RectF(render_passes->back()->output_rect), &render_pass_filters,
      WindowsDelegatedOverlayContext());

  base::expected<DelegatedCompositingResult, DelegationStatus>
      delegation_result = TryDelegatedCompositing(
          is_full_delegated_compositing, *render_passes, factory,
          render_pass_backdrop_filters, resource_provider);

  if (delegation_result.has_value()) {
    OverlayCandidateList delegated_candidates =
        std::move(delegation_result.value().candidates);
    if (!is_full_delegated_compositing) {
      PromotedRenderPassesInfo promoted_render_passes_info =
          std::move(delegation_result.value().promoted_render_passes_info);

      DCLayerOverlayProcessor::RenderPassOverlayDataMap
          surface_content_render_passes =
              UpdatePromotedRenderPassPropertiesAndGetSurfaceContentPasses(
                  *render_passes, promoted_render_passes_info);

      dc_layer_overlay_processor_->Process(
          resource_provider, render_pass_filters, render_pass_backdrop_filters,
          surface_damage_rect_list_in_root_space, is_page_fullscreen_mode_,
          surface_content_render_passes);

      // Remove entries that were not seen this frame. These counters are used
      // to avoid thrashing between swap chain and DComp surface allocations,
      // but are not useful when the render pass backing itself doesn't exist.
      base::EraseIf(
          frames_since_using_dc_layers_map_,
          [&surface_content_render_passes](const auto& frames_since_kv) {
            const auto& [pass_id, _num_frames] = frames_since_kv;
            return std::ranges::none_of(
                surface_content_render_passes,
                [&pass_id](const auto& overlay_data_kv) {
                  const auto& [pass, _data] = overlay_data_kv;
                  return pass_id == pass->id;
                });
          });

      for (auto& [render_pass, overlay_data] : surface_content_render_passes) {
        render_pass->damage_rect = UpdateRenderPassFromOverlayData(
            overlay_data, render_pass, frames_since_using_dc_layers_map_,
            frame_has_forced_dcomp_surface_);

        DBG_LOG_OPT("delegated.overlay.log", DBG_OPT_BLUE,
                    "Partially delegated pass{id: %llu, damage: %s}, "
                    "overlay_data{overlays: %zu, damage: %s}",
                    render_pass->id.value(),
                    render_pass->damage_rect.ToString().c_str(),
                    overlay_data.promoted_overlays.size(),
                    overlay_data.damage_rect.ToString().c_str());

        if (debug_settings_->show_dc_layer_debug_borders) {
          InsertDebugBorderDrawQuadsForOverlayCandidates(
              overlay_data.promoted_overlays, render_pass,
              render_pass->damage_rect);
        }
      }

      previous_frame_overlay_rect_ =
          InsertSurfaceContentOverlaysAndSetPlaneZOrder(
              std::move(surface_content_render_passes), delegated_candidates);
    } else {
      // We're doing full delegated compositing so we don't have to keep track
      // of any surfaces for render passes. If we fall out of delegated
      // compositing this will be repopulated.
      frames_since_using_dc_layers_map_.clear();

      // The candidates are already in back-to-front order, but we will
      // explicitly set their z-index because DCLayerTree expects it.
      previous_frame_overlay_rect_ =
          InsertSurfaceContentOverlaysAndSetPlaneZOrder(
              /*surface_content_render_passes=*/{}, delegated_candidates);
    }

    RemovePrimaryPlane(*render_passes->back(), *root_damage_rect);

    *candidates = std::move(delegated_candidates);

    return DelegationStatus::kFullDelegation;
  } else {
    return delegation_result.error();
  }
}

void OverlayProcessorWin::ProcessOverlaysFromOutputSurfacePlane(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    const SurfaceDamageRectList& surface_damage_rect_list_in_root_space,
    CandidateList* candidates,
    gfx::Rect* root_damage_rect) {
  auto* root_render_pass = render_passes->back().get();

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      render_pass_overlay_data_map;
  auto emplace_pair = render_pass_overlay_data_map.emplace(
      root_render_pass, DCLayerOverlayProcessor::RenderPassOverlayData());
  DCHECK(emplace_pair.second);  // Verify insertion occurred.
  DCHECK_EQ(emplace_pair.first->first, root_render_pass);
  DCLayerOverlayProcessor::RenderPassOverlayData&
      root_render_pass_overlay_data = emplace_pair.first->second;
  root_render_pass_overlay_data.damage_rect = *root_damage_rect;
  dc_layer_overlay_processor_->Process(
      resource_provider, render_pass_filters, render_pass_backdrop_filters,
      surface_damage_rect_list_in_root_space, is_page_fullscreen_mode_,
      render_pass_overlay_data_map);
  if (frames_since_using_dc_layers_map_.size() > 1 ||
      !frames_since_using_dc_layers_map_.contains(root_render_pass->id)) {
    // We're switching off of delegated compositing or the root render pass ID
    // has changed and we only expect |UpdateRenderPassFromOverlayData| to
    // insert a single entry for the root pass, so we can remove all other
    // entries.
    frames_since_using_dc_layers_map_.clear();
  }
  *root_damage_rect = UpdateRenderPassFromOverlayData(
      root_render_pass_overlay_data, root_render_pass,
      frames_since_using_dc_layers_map_, frame_has_forced_dcomp_surface_);
  *candidates = std::move(root_render_pass_overlay_data.promoted_overlays);
  if (!root_render_pass->copy_requests.empty()) {
    // A DComp surface is not readable by viz.
    // |DCLayerOverlayProcessor::Process| should avoid overlay candidates if
    // there are e.g. copy output requests present.
    CHECK(!root_render_pass->needs_synchronous_dcomp_commit);
  }

  if (debug_settings_->show_dc_layer_debug_borders) {
    InsertDebugBorderDrawQuadsForOverlayCandidates(
        *candidates, root_render_pass, *root_damage_rect);
  }
}

void OverlayProcessorWin::SetFrameHasDelegatedInk() {
  const bool is_partially_delegated_compositing =
      delegated_compositing_supported_ ==
      features::DelegatedCompositingMode::kLimitToUi;

  // kDCompSurfacesForDelegatedInk is for delegated ink to work with partial
  // delegated compositing. This should be true if the feature is enabled or
  // partial delegated compositing is enabled - a condition which requires the
  // use of DCOMP surfaces for delegated ink.
  const bool should_use_d_comp_surfaces_for_delegated_ink =
      is_partially_delegated_compositing ||
      base::FeatureList::IsEnabled(features::kDCompSurfacesForDelegatedInk);
  frame_has_forced_dcomp_surface_ |=
      should_use_d_comp_surfaces_for_delegated_ink;
}

void OverlayProcessorWin::SetUsingDCLayersForTesting(
    AggregatedRenderPassId render_pass_id,
    bool value) {
  CHECK_IS_TEST();
  if (value) {
    frames_since_using_dc_layers_map_[render_pass_id] = 0;
  } else {
    frames_since_using_dc_layers_map_.erase(render_pass_id);
  }
}

void OverlayProcessorWin::InsertDebugBorderDrawQuadsForOverlayCandidates(
    const OverlayCandidateList& dc_layer_overlays,
    AggregatedRenderPass* render_pass,
    gfx::Rect& damage_rect) {
  auto* shared_quad_state = render_pass->CreateAndAppendSharedQuadState();
  auto& quad_list = render_pass->quad_list;

  // Add debug borders for the root damage rect after overlay promotion.
  {
    SkColor4f border_color = SkColors::kGreen;
    auto it =
        quad_list.InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
            quad_list.begin(), 1u);
    auto* debug_quad = static_cast<DebugBorderDrawQuad*>(*it);

    gfx::Rect rect = damage_rect;
    rect.Inset(kDCLayerDebugBorderInsets);
    debug_quad->SetNew(shared_quad_state, rect, rect, border_color,
                       kDCLayerDebugBorderWidth);
  }

  // We assume the render pass transform is invertible, otherwise we could not
  // have promoted overlays from it.
  const gfx::Transform root_target_to_pass =
      render_pass->transform_to_root_target.GetCheckedInverse();

  // Add debug borders for overlays/underlays
  for (const auto& dc_layer : dc_layer_overlays) {
    gfx::Rect overlay_rect = gfx::ToEnclosingRect(
        OverlayCandidate::DisplayRectInTargetSpace(dc_layer));
    if (dc_layer.clip_rect) {
      overlay_rect.Intersect(*dc_layer.clip_rect);
    }
    overlay_rect = root_target_to_pass.MapRect(overlay_rect);

    // Overlay:red, Underlay:blue.
    SkColor4f border_color =
        dc_layer.plane_z_order > 0 ? SkColors::kRed : SkColors::kBlue;
    auto it =
        quad_list.InsertBeforeAndInvalidateAllPointers<DebugBorderDrawQuad>(
            quad_list.begin(), 1u);
    auto* debug_quad = static_cast<DebugBorderDrawQuad*>(*it);

    overlay_rect.Inset(kDCLayerDebugBorderInsets);
    debug_quad->SetNew(shared_quad_state, overlay_rect, overlay_rect,
                       border_color, kDCLayerDebugBorderWidth);
  }

  // Mark the entire output as damaged because the border quads might not be
  // inside the current damage rect.  It's far simpler to mark the entire
  // output as damaged instead of accounting for individual border quads which
  // can change positions across frames.
  damage_rect = render_pass->output_rect;
}

bool OverlayProcessorWin::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorWin::SetIsPageFullscreen(bool enabled) {
  is_page_fullscreen_mode_ = enabled;
}

OverlayProcessorWin::PromotedRenderPassesInfo::PromotedRenderPassesInfo() =
    default;
OverlayProcessorWin::PromotedRenderPassesInfo::~PromotedRenderPassesInfo() =
    default;

OverlayProcessorWin::PromotedRenderPassesInfo::PromotedRenderPassesInfo(
    OverlayProcessorWin::PromotedRenderPassesInfo&&) = default;
OverlayProcessorWin::PromotedRenderPassesInfo&
OverlayProcessorWin::PromotedRenderPassesInfo::operator=(
    OverlayProcessorWin::PromotedRenderPassesInfo&&) = default;

OverlayProcessorWin::DelegatedCompositingResult::DelegatedCompositingResult() =
    default;
OverlayProcessorWin::DelegatedCompositingResult::~DelegatedCompositingResult() =
    default;

OverlayProcessorWin::DelegatedCompositingResult::DelegatedCompositingResult(
    OverlayProcessorWin::DelegatedCompositingResult&&) = default;
OverlayProcessorWin::DelegatedCompositingResult&
OverlayProcessorWin::DelegatedCompositingResult::operator=(
    OverlayProcessorWin::DelegatedCompositingResult&&) = default;

base::expected<OverlayProcessorWin::DelegatedCompositingResult,
               DelegationStatus>
OverlayProcessorWin::TryDelegatedCompositing(
    bool is_full_delegated_compositing,
    const AggregatedRenderPassList& render_passes,
    const OverlayCandidateFactory& factory,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    const DisplayResourceProvider* resource_provider) const {
  const AggregatedRenderPass* root_render_pass = render_passes.back().get();

  if (root_render_pass->HasCapture()) {
    DBG_LOG_OPT(
        "delegated.overlay.log", DBG_OPT_RED,
        "Root pass has capture: copy_requests = %zu, video_capture_enabled "
        "= %d",
        root_render_pass->copy_requests.size(),
        root_render_pass->video_capture_enabled);
    return base::unexpected(DelegationStatus::kCompositedCopyRequest);
  }

  if (root_render_pass->quad_list.size() > kTooManyQuads) {
    return base::unexpected(DelegationStatus::kCompositedTooManyQuads);
  }

  DelegatedCompositingResult result;
  result.candidates.reserve(root_render_pass->quad_list.size());

  const bool allow_promotion_hinting =
      media::SupportMediaFoundationClearPlayback();

  int draw_quad_rounded_corner_count = 0;

  // The quad that renders underneath the current quad in the following loop.
  const DrawQuad* quad_below = nullptr;

  // Try to promote all the quads in the root pass to overlay.
  for (const auto* quad : root_render_pass->quad_list.BackToFront()) {
    std::optional<OverlayCandidate> dc_layer;
    {
      auto candidate_result = TryPromoteDrawQuadForDelegation(factory, quad);

      if (allow_promotion_hinting && !quad->resource_id.is_null() &&
          resource_provider->DoesResourceWantPromotionHint(quad->resource_id)) {
        // The OverlayStateService should always be initialized by
        // GpuServiceImpl at creation - CHECK here just to assert there aren't
        // any corner cases where this isn't true.
        auto* overlay_state_service = OverlayStateService::GetInstance();
        CHECK(overlay_state_service->IsInitialized());
        overlay_state_service->SetPromotionHint(
            resource_provider->GetMailbox(quad->resource_id),
            /*promoted=*/candidate_result.has_value());
      }

      if (candidate_result.has_value()) {
        if (auto& candidate = candidate_result.value()) {
          dc_layer = std::move(candidate);

          if (is_page_fullscreen_mode_ &&
              quad->material == DrawQuad::Material::kTextureContent) {
            // Note we're using the root render pass output rect in full screen
            // mode as an approximation of the monitor size.
            const gfx::Rect display_rect = root_render_pass->output_rect;
            dc_layer->possible_video_fullscreen_letterboxing =
                DCLayerOverlayProcessor::IsPossibleFullScreenLetterboxing(
                    quad_below, display_rect);
          }
        } else {
          // This quad can be intentionally skipped.
          continue;
        }
      } else {
        return base::unexpected(candidate_result.error());
      }
    }

    dc_layer->layer_id =
        gfx::OverlayLayerId(quad->shared_quad_state->layer_namespace_id,
                            quad->shared_quad_state->layer_id);

    // Store metadata on RPDQ overlays for post-processing in
    // |UpdatePromotedRenderPassProperties| to support partially delegated
    // compositing.
    if (dc_layer->rpdq) {
      if (render_pass_backdrop_filters.contains(
              dc_layer->rpdq->render_pass_id)) {
        // We don't delegate composting of backdrop filters to the OS.
        return base::unexpected(DelegationStatus::kCompositedBackdropFilter);
      }

      auto render_pass_it =
          std::ranges::find(render_passes, dc_layer->rpdq->render_pass_id,
                            &AggregatedRenderPass::id);
      CHECK(render_pass_it != render_passes.end());

      result.promoted_render_passes_info.promoted_render_passes.insert(
          raw_ref<AggregatedRenderPass>::from_ptr(render_pass_it->get()));
      result.promoted_render_passes_info.promoted_rpdqs.push_back(
          raw_ref<const AggregatedRenderPassDrawQuad>::from_ptr(
              dc_layer->rpdq.get()));
    }

    result.candidates.push_back(std::move(dc_layer).value());

    const auto& candidate = result.candidates.back();
    if (!candidate.rounded_corners.IsEmpty()) {
      draw_quad_rounded_corner_count++;
      if (draw_quad_rounded_corner_count > kTooManyQuadsWithRoundedCorners) {
        return base::unexpected(DelegationStatus::kCompositedTooManyQuads);
      }
    }

    // Iterating back-to-front means this quad will appear below the next one.
    quad_below = quad;
  }

  return base::ok(std::move(result));
}

void OverlayProcessorWin::RemovePrimaryPlane(
    const AggregatedRenderPass& root_render_pass,
    gfx::Rect& root_damage_rect) {
  // Set this to the full output rect unconditionally on success. This is
  // unioned with the next frame's damage (via |GetAndResetOverlayDamage|) to
  // fully damage the root surface if the next frame fails delegation. Since
  // delegated compositing succeeded here, the previous frame's
  // |overlay_damage_rect_| influence on |root_damage_rect| is cleared below.
  //
  // In the case of resize, we will be correctly damaged from another source.
  overlay_damage_rect_ = root_render_pass.output_rect;

  // `SkiaRenderer` expects no root damage when the primary plane is not
  // present.
  root_damage_rect = gfx::Rect();

  pending_remove_primary_plane_ = true;
}

void OverlayProcessorWin::TryPromoteFullScreenVideo(
    const AggregatedRenderPass& root_render_pass,
    OverlayCandidateList& candidates,
    gfx::Rect& root_damage_rect) {
  if (candidates.empty()) {
    // No candidates means no possible full screen video candidate.
    return;
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  // This function assumes the candidates list is sorted.
  CHECK(
      std::ranges::is_sorted(candidates, {}, &OverlayCandidate::plane_z_order));
#endif

  // Find the front-most full screen video candidate by searching from the back.
  auto frontmost_candidate_it = std::ranges::find_if(
      candidates.rbegin(), candidates.rend(), [](const auto& candidate) {
        return candidate.possible_video_fullscreen_letterboxing &&
               (candidate.priority_hint ==
                    gfx::OverlayPriorityHint::kHardwareProtection ||
                candidate.priority_hint == gfx::OverlayPriorityHint::kVideo);
      });
  if (frontmost_candidate_it == candidates.rend()) {
    return;
  }

  const gfx::RectF target_rect =
      OverlayCandidate::DisplayRectInTargetSpace(*frontmost_candidate_it);
  const gfx::Rect& root_pass_output_rect = root_render_pass.output_rect;

  // The ideal rect is `target_rect` scaled to fit and centered inside the full
  // screen render pass output rect.
  const gfx::RectF ideal_full_screen_rect = ContainedAndCenteredRect(
      target_rect, gfx::SizeF(root_pass_output_rect.size()));

  // Allow up to a pixel of wiggle room for checking the clip rect and
  // centeredness of the video. This is in case the video renderer doesn't
  // supply a perfectly placed video quad and assumes that up to a pixel
  // of adjustment is forgivable.
  constexpr float kTightTolerance = 1.0f;

  const bool candidate_is_not_clipped =
      !frontmost_candidate_it->clip_rect ||
      gfx::RectF(frontmost_candidate_it->clip_rect.value())
          .ApproximatelyEqual(ideal_full_screen_rect, kTightTolerance,
                              kTightTolerance);
  if (!candidate_is_not_clipped) {
    // If the video is clipped (and the clip rect is not equal to the target
    // rect), then we cannot apply the full screen optimization since we do
    // not currently support using the clipped region as the source rect for
    // video frames.
    //
    // We conservatively don't allow clip rects that are larger than the video,
    // but this may not be a requirement.
    return;
  }

  // `DirectCompositionLetterboxVideoOptimization` can adjust videos that
  // are almost letterboxed by a small amount to allow more videos to
  // utilize the full screen optimizations. The larger this tolerance, the
  // more the video may jump around when we enable/disable the optimization.
  const bool fix_almost_full_screen =
      !disable_direct_composition_letterbox_video_optimization_ &&
      base::FeatureList::IsEnabled(
          features::kDirectCompositionLetterboxVideoOptimization);
  const float tolerance = fix_almost_full_screen ? 10.f : kTightTolerance;

  if (!target_rect.ApproximatelyEqual(ideal_full_screen_rect, tolerance,
                                      tolerance)) {
    // We can possibly allow the full screen optimization for videos on frames
    // that contain a video with only a black background behind it, but we would
    // need better detection of the background mat to do so (i.e.
    // `possible_video_fullscreen_letterboxing`).
    return;
  }

  frontmost_candidate_it->display_rect = ideal_full_screen_rect;
  frontmost_candidate_it->transform = gfx::Transform();
  frontmost_candidate_it->clip_rect = root_pass_output_rect;
  frontmost_candidate_it->overlay_type = gfx::OverlayType::kFullScreen;

  // Try to remove the primary plane if the video full occludes it. If the
  // video is underneath e.g. controls or captions, we cannot remove the
  // primary plane.
  const bool video_is_above_primary_plane =
      frontmost_candidate_it->plane_z_order > 0;
  if (video_is_above_primary_plane) {
    RemovePrimaryPlane(root_render_pass, root_damage_rect);
  }

  // Erase any overlay candidates occluded by the video. This just reduces the
  // number of overlays we send to `DCompPresenter`.
  candidates.erase(candidates.begin(),
                   std::prev(frontmost_candidate_it.base()));
}

// static
DCLayerOverlayProcessor::RenderPassOverlayDataMap OverlayProcessorWin::
    UpdatePromotedRenderPassPropertiesAndGetSurfaceContentPasses(
        const AggregatedRenderPassList& render_passes,
        const PromotedRenderPassesInfo& promoted_render_passes_info) {
  struct Embedder {
    // RAW_PTR_EXCLUSION: Stack-scoped.
    RAW_PTR_EXCLUSION const AggregatedRenderPassDrawQuad* rpdq = nullptr;
    bool is_overlay = false;
  };

  // Returns true if the |render_pass| or a RPDQ that embeds it will require viz
  // to read the render pass' backing to compose the frame.
  const auto BackingWillBeReadInViz =
      [](const AggregatedRenderPass& render_pass,
         const std::vector<Embedder>& embedders) {
        if (render_pass.HasCapture()) {
          return true;
        }

        // Filters require an intermediate surface to be applied.
        if (!render_pass.filters.IsEmpty() ||
            !render_pass.backdrop_filters.IsEmpty()) {
          return true;
        }

        // Resolving mipmaps requires reading the backing.
        if (render_pass.generate_mipmap) {
          return true;
        }

        // Check if any embedders need to read the backing.
        if (std::ranges::any_of(embedders, [](const auto& embedder) {
              if (!embedder.is_overlay) {
                // Non-overlay embedders need to be read in viz
                return true;
              }

              if (!embedder.rpdq->mask_resource_id().is_null() ||
                  embedder.rpdq->shared_quad_state->mask_filter_info
                      .HasGradientMask()) {
                return true;
              }

              return false;
            })) {
          return true;
        }

        return false;
      };

  // The root render pass will never have embedders, but may e.g. have a copy
  // request that requires it to be read.
  render_passes.back()->will_backing_be_read_by_viz =
      BackingWillBeReadInViz(*render_passes.back().get(), {});

  if (promoted_render_passes_info.promoted_render_passes.empty()) {
    return {};
  }

  // A map that give us backwards pointers from a render pass overlay to its
  // embedders.
  base::flat_map<AggregatedRenderPassId, std::vector<Embedder>> embedders;
  for (const auto& pass : render_passes) {
    if (pass == render_passes.front()) {
      // The first pass cannot embed other render passes.
      continue;
    }

    for (const auto* quad : pass->quad_list) {
      if (const auto* rpdq =
              quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
        auto it = std::ranges::find(
            promoted_render_passes_info.promoted_render_passes,
            rpdq->render_pass_id, &AggregatedRenderPass ::id);
        if (it == promoted_render_passes_info.promoted_render_passes.end()) {
          // We don't need to track embedders of render passes that are not
          // going to overlay since we can assume those will be read by viz.
          continue;
        }

        embedders[(*it)->id].push_back(Embedder{
            .rpdq = rpdq,
            .is_overlay = std::ranges::find(
                              promoted_render_passes_info.promoted_rpdqs, rpdq,
                              [](const auto& rpdq) { return &rpdq.get(); }) !=
                          promoted_render_passes_info.promoted_rpdqs.end(),
        });
      }
    }
  }

  DCLayerOverlayProcessor::RenderPassOverlayDataMap
      surface_content_render_passes;

  for (auto render_pass : promoted_render_passes_info.promoted_render_passes) {
    render_pass->will_backing_be_read_by_viz =
        BackingWillBeReadInViz(render_pass.get(), embedders[render_pass->id]);

    // If we're in partial delegation, we want to promote video quads out of
    // e.g. web contents surfaces as if they were the root surface.
    if (render_pass->is_from_surface_root_pass) {
      DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
      overlay_data.damage_rect = render_pass->damage_rect;
      surface_content_render_passes.insert(
          {&render_pass.get(), std::move(overlay_data)});
    } else {
      render_pass->needs_synchronous_dcomp_commit = true;
    }
  }

  return surface_content_render_passes;
}

// static
gfx::Rect OverlayProcessorWin::InsertSurfaceContentOverlaysAndSetPlaneZOrder(
    DCLayerOverlayProcessor::RenderPassOverlayDataMap
        surface_content_render_passes,
    OverlayCandidateList& candidates) {
  gfx::Rect overlay_union_rect;

  // Returns the entry in |surface_content_render_passes| corresponding to
  // |candidate| if it is a RPDQ candidate that had child overlays promoted from
  // it. Returns nullptr otherwise.
  const auto TryGetSurfaceContentOverlayData =
      [](DCLayerOverlayProcessor::RenderPassOverlayDataMap&
             surface_content_render_passes,
         const OverlayCandidate& candidate)
      -> DCLayerOverlayProcessor::RenderPassOverlayDataMap::value_type* {
    if (candidate.rpdq) {
      if (auto it = std::ranges::find(
              surface_content_render_passes, candidate.rpdq->render_pass_id,
              [](const auto& kv) { return kv.first->id; });
          it != surface_content_render_passes.end()) {
        if (!it->second.promoted_overlays.empty()) {
          return &*it;
        } else {
          // If the surface had no promoted overlays, we don't need to process
          // them.
        }
      } else {
        // RPDQ was not from a surface quad and therefore not a candidate for
        // overlay promotion.
      }
    }

    return nullptr;
  };

  // Assign properties on |child| that can be inherited from |rpdq_parent|, such
  // as clip rect(s).
  const auto InheritOverlayPropertiesFromParent =
      [](const gfx::Rect& surface_bounds_in_root,
         const OverlayCandidate& rpdq_parent, OverlayCandidate& child) {
        // Ensure that the candidate is contained by the surface it was promoted
        // from.
        gfx::Rect candidate_clip = surface_bounds_in_root;

        if (rpdq_parent.clip_rect.has_value()) {
          // If the parent has a clip rect, let this candidate inherit that clip
          // rect.
          candidate_clip.Intersect(rpdq_parent.clip_rect.value());
        }

        if (child.clip_rect) {
          child.clip_rect->Intersect(candidate_clip);
        } else {
          child.clip_rect = candidate_clip;
        }

        // If the parent has rounded corners, let this candidate inherit that
        // rounded corner clip so it will be correctly clipped if it's
        // positioned at one of the corners.
        if (!rpdq_parent.rounded_corners.IsEmpty()) {
          // We don't expect |DCLayerOverlayProcessor| to set the rounded
          // corners of its candidates.
          CHECK(child.rounded_corners.IsEmpty());

          // The rounded corners from the original quad are painted into its
          // parent surface, making it safe for us to use the candidates'
          // rounded corners to store its parent's rounded corners.
          child.rounded_corners = rpdq_parent.rounded_corners;
        }
      };

  int current_z_index = 1;
  // We don't use an iterator since we're pushing to the end of |candidates|
  // during our iteration, which may invalidate iterators.
  const size_t size_before_surface_content_overlays = candidates.size();
  for (size_t rpdq_index = 0; rpdq_index < size_before_surface_content_overlays;
       rpdq_index++) {
    auto* surface_content_overlay_data = TryGetSurfaceContentOverlayData(
        surface_content_render_passes, candidates[rpdq_index]);
    if (!surface_content_overlay_data) {
      // This is a regular delegated overlay candidate, assign it the next
      // z-index and move on.
      candidates[rpdq_index].plane_z_order = current_z_index++;
      continue;
    }

    // |candidates[rpdq_index]| is a RPDQ candidate with child overlays (e.g.
    // videos, canvas, etc). We need to add the child overlays to |candidates|
    // and assign them z-indexes relative to their parent RPDQ candidate. In
    // back-to-front order, we will assign:
    //   1. z-indexes for the underlays
    //   2. a z-index for the RPDQ candidate itself
    //   3. and z-indexes for the overlays.

    auto& [render_pass, overlay_data] = *surface_content_overlay_data;

    // Sort the child overlays so we can iterate them back-to-front.
    std::ranges::sort(overlay_data.promoted_overlays, std::ranges::less(),
                      &OverlayCandidate::plane_z_order);

    const gfx::Rect surface_bounds_in_root = gfx::ToRoundedRect(
        OverlayCandidate::DisplayRectInTargetSpace(candidates[rpdq_index]));

    bool rpdq_handled = false;
    candidates.reserve(candidates.size() +
                       overlay_data.promoted_overlays.size());
    for (auto& overlay : overlay_data.promoted_overlays) {
      CHECK_NE(overlay.plane_z_order, 0);
      if (!rpdq_handled && overlay.plane_z_order > 0) {
        // Assign the current z-index to the RPDQ candidate to place it between
        // its underlays and overlays.
        candidates[rpdq_index].plane_z_order = current_z_index++;
        rpdq_handled = true;
      }

      candidates.push_back(std::move(overlay));
      // Overwrite the previous |plane_z_order| that was relative to the RPDQ
      // candidate with a z-index relative to the full candidates list.
      candidates.back().plane_z_order = current_z_index++;

      InheritOverlayPropertiesFromParent(surface_bounds_in_root,
                                         /*rpdq_parent=*/candidates[rpdq_index],
                                         /*child=*/candidates.back());

      overlay_union_rect.Union(gfx::ToEnclosingRect(
          OverlayCandidate::DisplayRectInTargetSpace(overlay)));
    }
    if (!rpdq_handled) {
      // Handle fencepost problem: insert the RPDQ candidate in the case that
      // there were only child underlays.
      candidates[rpdq_index].plane_z_order = current_z_index++;
    }
  }

  return overlay_union_rect;
}

// static
gfx::Rect
OverlayProcessorWin::InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
    DCLayerOverlayProcessor::RenderPassOverlayDataMap
        surface_content_render_passes,
    OverlayCandidateList& candidates) {
  CHECK_IS_TEST();
  return InsertSurfaceContentOverlaysAndSetPlaneZOrder(
      std::move(surface_content_render_passes), candidates);
}

}  // namespace viz
