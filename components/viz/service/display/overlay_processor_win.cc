// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_win.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/trace_event.h"
#include "base/types/expected.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "components/viz/service/display/overlay_processor_delegated_support.h"
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
    const bool frame_has_delegated_ink) {
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
  if (!overlay_data.promoted_overlays.empty() ||
      (frame_has_delegated_ink &&
       features::ShouldUseDCompSurfacesForDelegatedInk())) {
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

}  // anonymous namespace

OverlayProcessorWin::OverlayProcessorWin(
    OutputSurface::DCSupportLevel dc_support_level,
    const DebugRendererSettings* debug_settings,
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor)
    : dc_support_level_(dc_support_level),
      debug_settings_(debug_settings),
      dc_layer_overlay_processor_(std::move(dc_layer_overlay_processor)) {
  DCHECK_GT(dc_support_level_, OutputSurface::DCSupportLevel::kNone);
}

OverlayProcessorWin::~OverlayProcessorWin() = default;

bool OverlayProcessorWin::IsOverlaySupported() const {
  return true;
}

gfx::Rect OverlayProcessorWin::GetPreviousFrameOverlaysBoundingRect() const {
  if (features::IsDelegatedCompositingEnabled()) {
    return previous_frame_overlay_rect_;
  }

  // TODO(dcastagna): Implement me.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect OverlayProcessorWin::GetAndResetOverlayDamage() {
  return std::exchange(overlay_damage_rect_, gfx::Rect());
}

void OverlayProcessorWin::AdjustOutputSurfaceOverlay(
    std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (delegation_succeeded_last_frame_) {
    output_surface_plane->reset();
  }
}

void OverlayProcessorWin::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    AggregatedRenderPassList* render_passes,
    const SkM44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list_in_root_space,
    OutputSurfaceOverlayPlane* output_surface_plane,
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
        surface_damage_rect_list_in_root_space, output_surface_plane,
        candidates, root_damage_rect);
  }

  DebugLogAfterDelegation(status, *candidates, *root_damage_rect);

  frame_has_delegated_ink_ = false;
  delegation_succeeded_last_frame_ =
      status == DelegationStatus::kFullDelegation;
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
  if (dc_support_level_ < OutputSurface::DCSupportLevel::kDCompTexture ||
      !features::IsDelegatedCompositingEnabled() || ForceDisableDelegation()) {
    return DelegationStatus::kCompositedFeatureDisabled;
  }

  const bool is_full_delegated_compositing =
      features::kDelegatedCompositingModeParam.Get() ==
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
    PromotedRenderPassesInfo promoted_render_passes_info =
        std::move(delegation_result.value().promoted_render_passes_info);

    DCLayerOverlayProcessor::RenderPassOverlayDataMap
        surface_content_render_passes =
            UpdatePromotedRenderPassPropertiesAndGetSurfaceContentPasses(
                is_full_delegated_compositing, *render_passes,
                promoted_render_passes_info);

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
          return base::ranges::none_of(surface_content_render_passes,
                                       [&pass_id](const auto& overlay_data_kv) {
                                         const auto& [pass, _data] =
                                             overlay_data_kv;
                                         return pass_id == pass->id;
                                       });
        });

    for (auto& [render_pass, overlay_data] : surface_content_render_passes) {
      render_pass->damage_rect = UpdateRenderPassFromOverlayData(
          overlay_data, render_pass, frames_since_using_dc_layers_map_,
          frame_has_delegated_ink_);

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

    // Set this to the full output rect unconditionally on success. This is
    // unioned with the next frame's damage (via |GetAndResetOverlayDamage|)
    // to fully damage the root surface if the next frame fails delegation.
    // Since delegated compositing succeeded here, the previous frame's
    // |overlay_damage_rect_| influence on |root_damage_rect| is cleared
    // below.
    // In the case of resize, we will be correctly damaged from another
    // source.
    overlay_damage_rect_ = render_passes->back()->output_rect;

    delegation_succeeded_last_frame_ = true;
    *candidates = std::move(delegated_candidates);
    *root_damage_rect = gfx::Rect();

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
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* root_damage_rect) {
  auto* root_render_pass = render_passes->back().get();
  if (render_passes->back()->is_color_conversion_pass) {
    DCHECK_GT(render_passes->size(), 1u);
    root_render_pass = (*render_passes)[render_passes->size() - 2].get();
  }

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
      frames_since_using_dc_layers_map_, frame_has_delegated_ink_);
  *candidates = std::move(root_render_pass_overlay_data.promoted_overlays);
  if (!root_render_pass->copy_requests.empty()) {
    // A DComp surface is not readable by viz.
    // |DCLayerOverlayProcessor::Process| should avoid overlay candidates if
    // there are e.g. copy output requests present.
    CHECK(!root_render_pass->needs_synchronous_dcomp_commit);
  }

  // |root_render_pass| will be promoted to overlay only if
  // |output_surface_plane| is present.
  DCHECK_NE(output_surface_plane, nullptr);
  output_surface_plane->enable_blending =
      root_render_pass->has_transparent_background;

  if (debug_settings_->show_dc_layer_debug_borders) {
    InsertDebugBorderDrawQuadsForOverlayCandidates(
        *candidates, root_render_pass, *root_damage_rect);
  }
}

void OverlayProcessorWin::SetFrameHasDelegatedInk() {
  frame_has_delegated_ink_ = true;
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

  if (root_render_pass->is_color_conversion_pass) {
    // We don't expect to handle a color conversion pass (e.g. for frames with
    // HDR content) with delegated compositing. See: crbug.com/41497086
    return base::unexpected(DelegationStatus::kCompositedOther);
  }

  DelegatedCompositingResult result;
  result.candidates.reserve(root_render_pass->quad_list.size());

  int draw_quad_rounded_corner_count = 0;

  // Try to promote all the quads in the root pass to overlay.
  for (auto it = root_render_pass->quad_list.begin();
       it != root_render_pass->quad_list.end(); ++it) {
    const DrawQuad* quad = *it;

    std::optional<OverlayCandidate> dc_layer;
    if (is_full_delegated_compositing) {
      // Try to promote videos like DCLayerOverlay does first, then fall back to
      // OverlayCandidateFactory. This is because Windows has some specific
      // details on how it promotes e.g. protected videos that we want to
      // preserve.
      dc_layer = dc_layer_overlay_processor_->FromTextureOrYuvQuad(
          resource_provider, root_render_pass, it, is_page_fullscreen_mode_);
    } else {
      // In the partial delegated compositing case, we don't expect
      // video/canvas/etc content in the UI.
    }

    if (!dc_layer.has_value()) {
      if (auto candidate_result =
              TryPromoteDrawQuadForDelegation(factory, quad);
          candidate_result.has_value()) {
        if (auto& candidate = candidate_result.value()) {
          dc_layer = std::move(candidate);
        } else {
          // This quad can be intentionally skipped.
          continue;
        }
      } else {
        return base::unexpected(candidate_result.error());
      }
    }

    if (factory.IsOccludedByFilteredQuad(
            dc_layer.value(), root_render_pass->quad_list.begin(),
            root_render_pass->quad_list.end(), render_pass_backdrop_filters)) {
      return base::unexpected(DelegationStatus::kCompositedBackdropFilter);
    }

    // Store metadata on RPDQ overlays for post-processing in
    // |UpdatePromotedRenderPassProperties| to support partially delegated
    // compositing.
    if (dc_layer->rpdq) {
      auto render_pass_it =
          base::ranges::find(render_passes, dc_layer->rpdq->render_pass_id,
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
  }

  return base::ok(std::move(result));
}

// static
DCLayerOverlayProcessor::RenderPassOverlayDataMap OverlayProcessorWin::
    UpdatePromotedRenderPassPropertiesAndGetSurfaceContentPasses(
        bool is_full_delegated_compositing,
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
        if (base::ranges::any_of(embedders, [](const auto& embedder) {
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
        auto it = base::ranges::find(
            promoted_render_passes_info.promoted_render_passes,
            rpdq->render_pass_id, &AggregatedRenderPass ::id);
        if (it == promoted_render_passes_info.promoted_render_passes.end()) {
          // We don't need to track embedders of render passes that are not
          // going to overlay since we can assume those will be read by viz.
          continue;
        }

        embedders[(*it)->id].push_back(Embedder{
            .rpdq = rpdq,
            .is_overlay = base::ranges::find(
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
    if (!is_full_delegated_compositing &&
        render_pass->is_from_surface_root_pass) {
      DCLayerOverlayProcessor::RenderPassOverlayData overlay_data;
      overlay_data.damage_rect = render_pass->damage_rect;
      surface_content_render_passes.insert(
          {&render_pass.get(), std::move(overlay_data)});
    } else {
      render_pass->needs_synchronous_dcomp_commit = true;
    }
  }

  // If we are not doing partial delegation, we don't expect any surface content
  // render passes.
  CHECK(!is_full_delegated_compositing ||
        surface_content_render_passes.empty());

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
      if (auto it = base::ranges::find(
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

  // We inserted into candidates in front-to-back order, but |plane_z_order|s
  // increment back-to-front, so we want to invert the iteration so we can
  // insert in ascending z-order.
  int current_z_index = 1;
  // We don't use an iterator since we're pushing to the end of |candidates|
  // during our iteration, which may invalidate iterators.
  for (int rpdq_index = candidates.size() - 1; rpdq_index >= 0; rpdq_index--) {
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
    base::ranges::sort(overlay_data.promoted_overlays, base::ranges::less(),
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
