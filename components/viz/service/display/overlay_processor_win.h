// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_delegated_support.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace viz {
class DisplayResourceProvider;
struct DebugRendererSettings;
class OverlayCandidateFactory;

class VIZ_SERVICE_EXPORT OverlayProcessorWin
    : public OverlayProcessorInterface {
 public:
  OverlayProcessorWin(
      OutputSurface::DCSupportLevel dc_support_level,
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

  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

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

  void SetFrameHasDelegatedInk() override;

  bool frame_has_delegated_ink_for_testing() const {
    CHECK_IS_TEST();
    return frame_has_delegated_ink_;
  }

  // Sets whether or not |render_pass_id| will be marked for a DComp surface
  // backing. If |value| is true, this also resets the frame count since
  // enabling DC layers.
  void SetUsingDCLayersForTesting(AggregatedRenderPassId render_pass_id,
                                  bool value);

  static gfx::Rect InsertSurfaceContentOverlaysAndSetPlaneZOrderForTesting(
      DCLayerOverlayProcessor::RenderPassOverlayDataMap
          surface_content_render_passes,
      OverlayCandidateList& candidates);

 protected:
  // For testing.
  DCLayerOverlayProcessor* GetOverlayProcessor() {
    return dc_layer_overlay_processor_.get();
  }

 private:
  void InsertDebugBorderDrawQuadsForOverlayCandidates(
      const OverlayCandidateList& dc_layer_overlays,
      AggregatedRenderPass* render_pass,
      gfx::Rect& damage_rect);

  // Promote a subset of quads from the root render pass using
  // |DCLayerOverlayProcessor| while still intending to schedule the primary
  // plane as its own overlay. This is the fallback for delegated compositing
  // that still allows quads to be promoted e.g. for protected content or for
  // performance reasons.
  void ProcessOverlaysFromOutputSurfacePlane(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const SurfaceDamageRectList& surface_damage_rect_list_in_root_space,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* candidates,
      gfx::Rect* root_damage_rect);

  // Try to promote all quads from the root render pass to overlay.
  // In partially delegated compositing, RPDQs that represent surfaces will have
  // quads promoted from their render passes using |DCLayerOverlayProcessor|.
  DelegationStatus ProcessOverlaysForDelegation(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const SurfaceDamageRectList& surface_damage_rect_list_in_root_space,
      CandidateList* candidates,
      gfx::Rect* root_damage_rect);

  // This struct holds information about the RPDQ overlays promoted during
  // delegated compositing. It references objects in the current frame and is
  // only valid while the render pass list's pointers are valid.
  // TODO(crbug.com/324460866): Used for partially delegated compositing.
  struct PromotedRenderPassesInfo {
    PromotedRenderPassesInfo();
    ~PromotedRenderPassesInfo();

    PromotedRenderPassesInfo(PromotedRenderPassesInfo&&);
    PromotedRenderPassesInfo& operator=(PromotedRenderPassesInfo&&);

    // List of render passes that were embedded by a promoted RPDQ overlay.
    base::flat_set<raw_ref<AggregatedRenderPass>> promoted_render_passes;
    // List of RPDQs that were promoted to overlay.
    std::vector<raw_ref<const AggregatedRenderPassDrawQuad>> promoted_rpdqs;
  };

  // Result of attempting delegated compositing.
  struct DelegatedCompositingResult {
    DelegatedCompositingResult();
    ~DelegatedCompositingResult();

    DelegatedCompositingResult(DelegatedCompositingResult&&);
    DelegatedCompositingResult& operator=(DelegatedCompositingResult&&);

    OverlayCandidateList candidates;
    PromotedRenderPassesInfo promoted_render_passes_info;
  };

  // Attempt to promote all the quads in |root_render_pass|. Promoted quads will
  // be placed in |out_candidates| in front-to-back order. Returns true if all
  // quads were successfully promoted.
  base::expected<DelegatedCompositingResult, DelegationStatus>
  TryDelegatedCompositing(
      bool is_full_delegated_compositing,
      const AggregatedRenderPassList& render_passes,
      const OverlayCandidateFactory& factory,
      const OverlayProcessorInterface::FilterOperationsMap&
          render_pass_backdrop_filters,
      const DisplayResourceProvider* resource_provider) const;

  // Modifies the properties of |promoted_render_passes| for passes that are
  // referenced by RPDQ overlays. This gives |SkiaRenderer| enough information
  // to decide whether or not a RPDQ overlay can skip the copy in
  // |PrepareRenderPassOverlay| and, if so, whether to allocate a swap chain or
  // DComp surface backing. Returns the set of surfaces we should use
  // |DCLayerOverlayProcessor| to promote overlays from.
  // TODO(crbug.com/324460866): Used for partially delegated compositing.
  static DCLayerOverlayProcessor::RenderPassOverlayDataMap
  UpdatePromotedRenderPassPropertiesAndGetSurfaceContentPasses(
      bool is_full_delegated_compositing,
      const AggregatedRenderPassList& render_passes,
      const PromotedRenderPassesInfo& promoted_render_passes_info);

  // Insert overlay candidates from |surface_content_render_passes| into
  // |candidates|, assigning correct plane z-order in the process. |candidates|
  // is assumed to be in front-to-back. The resulting candidates list is not
  // sorted. Returns the union rect of overlays in
  // |surface_content_render_passes|.
  // TODO(crbug.com/324460866): Used for partially delegated compositing.
  static gfx::Rect InsertSurfaceContentOverlaysAndSetPlaneZOrder(
      DCLayerOverlayProcessor::RenderPassOverlayDataMap
          surface_content_render_passes,
      OverlayCandidateList& candidates);

  const OutputSurface::DCSupportLevel dc_support_level_;

  // Reference to the global viz singleton.
  const raw_ptr<const DebugRendererSettings> debug_settings_;

  // Number of frames since the last time direct composition layers were used
  // for each render pass we promote overlays from in the frame. Presence in
  // this map indicates that the render pass is using a DComp surface.
  base::flat_map<AggregatedRenderPassId, int> frames_since_using_dc_layers_map_;

  // TODO(weiliangc): Eventually fold DCLayerOverlayProcessor into this class.
  std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor_;

  bool is_page_fullscreen_mode_ = false;

  bool delegation_succeeded_last_frame_ = false;

  // If true, causes the use of DComp surfaces as the backing image of a render
  // pass, given that UseDCompSurfacesForDelegatedInk is also enabled.
  bool frame_has_delegated_ink_ = false;

  // Returned and reset by |GetAndResetOverlayDamage| to fully damage the root
  // render pass when we drop out of delegated compositing. This is essentially
  // a binary of "full damage needed" or "no full damage needed" for the primary
  // plane.
  gfx::Rect overlay_damage_rect_;

  // The union of the current frame's overlays that were promoted from surface
  // content passes to be read on the subsequent frame. This is used to
  // correctly damage surface content backings if we had previously removed
  // damage due to overlay occlusion.
  gfx::Rect previous_frame_overlay_rect_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_WIN_H_
