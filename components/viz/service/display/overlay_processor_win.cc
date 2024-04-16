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
#include "ui/gl/gl_switches.h"

namespace viz {
namespace {

constexpr int kDCLayerDebugBorderWidth = 4;
constexpr gfx::Insets kDCLayerDebugBorderInsets = gfx::Insets(-2);

// Switching between enabling DC layers and not is expensive, so only
// switch away after a large number of frames not needing DC layers have
// been produced.
constexpr int kNumberOfFramesBeforeDisablingDCLayers = 60;

gfx::Rect UpdateRenderPassFromOverlayData(
    const DCLayerOverlayProcessor::RenderPassOverlayData& overlay_data,
    AggregatedRenderPass* render_pass,
    base::flat_map<AggregatedRenderPassId, int>&
        frames_since_using_dc_layers_map) {
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
  if (!overlay_data.promoted_overlays.empty()) {
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
  context.allow_non_overlay_resources = base::FeatureList::IsEnabled(
      features::kCopyNonOverlayResourcesToDCompSurfaces);
  return context;
}

}  // anonymous namespace

OverlayProcessorWin::OverlayProcessorWin(
    OutputSurface* output_surface,
    const DebugRendererSettings* debug_settings,
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor)
    : output_surface_(output_surface),
      debug_settings_(debug_settings),
      dc_layer_overlay_processor_(std::move(dc_layer_overlay_processor)) {
  DCHECK(output_surface_->capabilities().supports_dc_layers);
}

OverlayProcessorWin::~OverlayProcessorWin() = default;

bool OverlayProcessorWin::IsOverlaySupported() const {
  return true;
}

gfx::Rect OverlayProcessorWin::GetPreviousFrameOverlaysBoundingRect() const {
  if (features::IsDelegatedCompositingEnabled()) {
    return gfx::Rect();
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
  if (!features::IsDelegatedCompositingEnabled() || ForceDisableDelegation()) {
    return DelegationStatus::kCompositedFeatureDisabled;
  }

  const bool is_full_delegated_compositing =
      !base::FeatureList::IsEnabled(features::kDelegatedCompositingLimitToUi);

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

    UpdatePromotedRenderPassProperties(*render_passes,
                                       promoted_render_passes_info);

    // We are not promoting videos from any render pass so this map should be
    // empty.
    frames_since_using_dc_layers_map_.clear();

    // Set the z-order of the candidates, noting that |delegated_candidates|
    // was pushed in front-to-back order.
    for (size_t i = 0u; i < delegated_candidates.size(); i++) {
      delegated_candidates[i].plane_z_order = delegated_candidates.size() - i;
    }

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
  if (!frames_since_using_dc_layers_map_.contains(root_render_pass->id)) {
    // The root render pass ID has changed and we only expect
    // |UpdateRenderPassFromOverlayData| to insert a single entry for the root
    // pass, so we can remove all other entries.
    frames_since_using_dc_layers_map_.clear();
  }
  *root_damage_rect = UpdateRenderPassFromOverlayData(
      root_render_pass_overlay_data, root_render_pass,
      frames_since_using_dc_layers_map_);
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

    // Mark the entire output as damaged because the border quads might not be
    // inside the current damage rect.  It's far simpler to mark the entire
    // output as damaged instead of accounting for individual border quads which
    // can change positions across frames.
    *root_damage_rect = root_render_pass->output_rect;
  }
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
    AggregatedRenderPass* root_render_pass,
    const gfx::Rect& damage_rect) {
  auto* shared_quad_state = root_render_pass->CreateAndAppendSharedQuadState();
  auto& quad_list = root_render_pass->quad_list;

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

  // Add debug borders for overlays/underlays
  for (const auto& dc_layer : dc_layer_overlays) {
    gfx::Rect overlay_rect = gfx::ToEnclosingRect(
        OverlayCandidate::DisplayRectInTargetSpace(dc_layer));
    if (dc_layer.clip_rect) {
      overlay_rect.Intersect(*dc_layer.clip_rect);
    }

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
}

bool OverlayProcessorWin::NeedsSurfaceDamageRectList() const {
  return true;
}

void OverlayProcessorWin::SetIsPageFullscreen(bool enabled) {
  is_page_fullscreen_mode_ = enabled;
}

void OverlayProcessorWin::ProcessOnDCLayerOverlayProcessorForTesting(
    const DisplayResourceProvider* resource_provider,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    bool is_page_fullscreen_mode,
    DCLayerOverlayProcessor::RenderPassOverlayDataMap&
        render_pass_overlay_data_map) {
  CHECK_IS_TEST();
  dc_layer_overlay_processor_->Process(
      resource_provider, render_pass_filters, render_pass_backdrop_filters,
      surface_damage_rect_list, is_page_fullscreen_mode,
      render_pass_overlay_data_map);
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
    const bool is_full_delegated_compositing,
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

  if (root_render_pass->is_color_conversion_pass) {
    // We don't expect to handle a color conversion pass (e.g. for frames with
    // HDR content) with delegated compositing. See: crbug.com/41497086
    return base::unexpected(DelegationStatus::kCompositedOther);
  }

  DelegatedCompositingResult result;
  result.candidates.reserve(root_render_pass->quad_list.size());

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
              dc_layer->rpdq));
    }

    result.candidates.push_back(std::move(dc_layer).value());
  }

  return base::ok(std::move(result));
}

// static
void OverlayProcessorWin::UpdatePromotedRenderPassProperties(
    const AggregatedRenderPassList& render_passes,
    const PromotedRenderPassesInfo& promoted_render_passes_info) {
  struct Embedder {
    raw_ptr<const AggregatedRenderPassDrawQuad> rpdq = nullptr;
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
    return;
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

  for (auto render_pass : promoted_render_passes_info.promoted_render_passes) {
    render_pass->will_backing_be_read_by_viz =
        BackingWillBeReadInViz(render_pass.get(), embedders[render_pass->id]);
  }
}

}  // namespace viz
