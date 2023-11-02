// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_

#include <memory>
#include <vector>

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace viz {
class DisplayResourceProvider;
struct DebugRendererSettings;

class VIZ_SERVICE_EXPORT OverlayProcessorWin
    : public OverlayProcessorInterface {
 public:
  OverlayProcessorWin(
      OutputSurface* output_surface,
      const DebugRendererSettings* debug_settings,
      std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor);

  OverlayProcessorWin(const OverlayProcessorWin&) = delete;
  OverlayProcessorWin& operator=(const OverlayProcessorWin&) = delete;

  ~OverlayProcessorWin() override;

  bool IsOverlaySupported() const override;
  gfx::Rect GetPreviousFrameOverlaysBoundingRect() const override;
  gfx::Rect GetAndResetOverlayDamage() override;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  bool NeedsSurfaceDamageRectList() const override;

  // Sets |is_page_fullscreen_mode_|.
  void SetIsPageFullscreen(bool enabled) override;

  void AdjustOutputSurfaceOverlay(absl::optional<OutputSurfaceOverlayPlane>*
                                      output_surface_plane) override {}

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list_in_root_space,
      OutputSurfaceOverlayPlane* output_surface_plane,
      OverlayCandidateList* overlay_candidates,
      gfx::Rect* root_damage_rect,
      std::vector<gfx::Rect>* content_bounds) override;

  // Sets whether or not |render_pass_id| will be marked for a DComp surface
  // backing. If |value| is true, this also resets the frame count since
  // enabling DC layers.
  void SetUsingDCLayersForTesting(AggregatedRenderPassId render_pass_id,
                                  bool value);

  void set_frames_since_last_qualified_multi_overlays_for_testing(int value) {
    CHECK_IS_TEST();
    GetOverlayProcessor()
        ->set_frames_since_last_qualified_multi_overlays_for_testing(value);
  }
  void set_system_hdr_enabled_for_testing(int value) {
    GetOverlayProcessor()->set_system_hdr_enabled_for_testing(value);
  }
  void set_has_p010_video_processor_support_for_testing(int value) {
    GetOverlayProcessor()->set_has_p010_video_processor_support_for_testing(
        value);
  }
  size_t get_previous_frame_render_pass_count() {
    CHECK_IS_TEST();
    return GetOverlayProcessor()->get_previous_frame_render_pass_count();
  }
  std::vector<AggregatedRenderPassId> get_previous_frame_render_pass_ids() {
    CHECK_IS_TEST();
    return GetOverlayProcessor()->get_previous_frame_render_pass_ids();
  }

  void ProcessOnDCLayerOverlayProcessorForTesting(
      DisplayResourceProvider* resource_provider,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      bool is_page_fullscreen_mode,
      DCLayerOverlayProcessor::RenderPassOverlayDataMap&
          render_pass_overlay_data_map);

 protected:
  // For testing.
  DCLayerOverlayProcessor* GetOverlayProcessor() {
    return dc_layer_overlay_processor_.get();
  }

 private:
  void InsertDebugBorderDrawQuadsForOverlayCandidates(
      const OverlayCandidateList& dc_layer_overlays,
      AggregatedRenderPass* root_render_pass,
      const gfx::Rect& damage_rect);

  const raw_ptr<OutputSurface> output_surface_;

  // Reference to the global viz singleton.
  const raw_ptr<const DebugRendererSettings> debug_settings_;

  // Number of frames since the last time direct composition layers were used
  // for each render pass we promote overlays from in the frame. Presence in
  // this map indicates that the render pass is using a DComp surface.
  base::flat_map<AggregatedRenderPassId, int> frames_since_using_dc_layers_map_;

  // TODO(weiliangc): Eventually fold DCLayerOverlayProcessor into this class.
  std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor_;

  bool is_page_fullscreen_mode_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_
