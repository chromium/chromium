// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_win.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "gpu/config/gpu_finch_features.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gl/gl_utils.h"

namespace viz {
namespace {

constexpr int kDCLayerDebugBorderWidth = 4;
constexpr gfx::Insets kDCLayerDebugBorderInsets = gfx::Insets(-2);

// Switching between enabling DC layers and not is expensive, so only
// switch away after a large number of frames not needing DC layers have
// been produced.
constexpr int kNumberOfFramesBeforeDisablingDCLayers = 60;

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
  // TODO(dcastagna): Implement me.
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect OverlayProcessorWin::GetAndResetOverlayDamage() {
  return gfx::Rect();
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
      surface_damage_rect_list_in_root_space, is_video_capture_enabled_,
      is_page_fullscreen_mode_, render_pass_overlay_data_map);
  *root_damage_rect = root_render_pass_overlay_data.damage_rect;
  *candidates = root_render_pass_overlay_data.promoted_overlays;

  // Force a swap chain when there is a copy request, since read back is
  // impossible with a DComp surface.
  //
  // Normally, |DCLayerOverlayProcessor::Process| prevents
  // overlays (and thus forces a swap chain) when there is a copy request, but
  // |frames_since_using_dc_layers_| implements a one-sided hysteresis that
  // keeps us on DComp surfaces a little after we stop having overlays. If a
  // client issues a copy request while we're in this timeout, we end up asking
  // read back from a DComp surface, which fails later in
  // |SkiaOutputSurfaceImplOnGpu::CopyOutput|.
  const bool force_swap_chain_due_to_copy_request =
      is_video_capture_enabled_ || !root_render_pass->copy_requests.empty();
  bool was_using_dc_layers = using_dc_layers_;
  if (!candidates->empty()) {
    using_dc_layers_ = true;
    frames_since_using_dc_layers_ = 0;
  } else if (++frames_since_using_dc_layers_ >=
                 kNumberOfFramesBeforeDisablingDCLayers ||
             force_swap_chain_due_to_copy_request) {
    using_dc_layers_ = false;
  }

  if (was_using_dc_layers != using_dc_layers_) {
    // The entire surface has to be redrawn if switching from or to direct
    // composition layers, because the previous contents are discarded and some
    // contents would otherwise be undefined.
    *root_damage_rect = root_render_pass->output_rect;
  }

  if (base::FeatureList::IsEnabled(features::kDCompPresenter)) {
    if (!root_render_pass->copy_requests.empty()) {
      // A DComp surface is not readable by viz.
      // |DCLayerOverlayProcessor::Process| should avoid overlay candidates if
      // there are e.g. copy output requests present.
      CHECK(!using_dc_layers_);
    }

    // We have overlays, so our root surface requires a backing that
    // synchronizes with DComp commit. A swap chain's Present does not
    // synchronize with the DComp tree updates and would result in minor desync
    // during e.g. scrolling videos.
    root_render_pass->needs_synchronous_dcomp_commit = using_dc_layers_;

    // We only need to have a transparent backing if there's underlays, but we
    // unconditionally ask for transparency to avoid thrashing allocations if a
    // video alternated between overlay and underlay.
    root_render_pass->has_transparent_background = using_dc_layers_;

    // |root_render_pass| will be promoted to overlay only if
    // |output_surface_plane| is present.
    DCHECK_NE(output_surface_plane, nullptr);
    output_surface_plane->enable_blending =
        root_render_pass->has_transparent_background;
  } else {
    if (was_using_dc_layers != using_dc_layers_) {
      output_surface_->SetEnableDCLayers(using_dc_layers_);
    }
  }

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

void OverlayProcessorWin::SetIsVideoCaptureEnabled(bool enabled) {
  is_video_capture_enabled_ = enabled;
}

void OverlayProcessorWin::SetIsPageFullscreen(bool enabled) {
  is_page_fullscreen_mode_ = enabled;
}

void OverlayProcessorWin::ProcessOnDCLayerOverlayProcessorForTesting(
    DisplayResourceProvider* resource_provider,
    const FilterOperationsMap& render_pass_filters,
    const FilterOperationsMap& render_pass_backdrop_filters,
    SurfaceDamageRectList surface_damage_rect_list,
    bool is_video_capture_enabled,
    bool is_page_fullscreen_mode,
    DCLayerOverlayProcessor::RenderPassOverlayDataMap&
        render_pass_overlay_data_map) {
  CHECK_IS_TEST();
  dc_layer_overlay_processor_->Process(
      resource_provider, render_pass_filters, render_pass_backdrop_filters,
      surface_damage_rect_list, is_video_capture_enabled,
      is_page_fullscreen_mode, render_pass_overlay_data_map);
}

}  // namespace viz
