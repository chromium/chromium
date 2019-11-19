// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

#if defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class OutputSurface;
class OverlayCandidateList;
class OverlayCandidateValidator;
class RendererSettings;

class VIZ_SERVICE_EXPORT OverlayProcessor {
 public:
#if defined(OS_ANDROID)
  using CandidateList = OverlayCandidateList;
#elif defined(OS_MACOSX)
  using CandidateList = CALayerOverlayList;
#elif defined(OS_WIN)
  using CandidateList = DCLayerOverlayList;
#elif defined(USE_OZONE)
  using CandidateList = OverlayCandidateList;
#else
  // Default.
  using CandidateList = OverlayCandidateList;
#endif

  using FilterOperationsMap =
      base::flat_map<RenderPassId, cc::FilterOperations*>;

  static void RecordOverlayDamageRectHistograms(
      bool is_overlay,
      bool has_occluding_surface_damage,
      bool zero_damage_rect,
      bool occluding_damage_equal_to_damage_rect);

  // Data needed to represent |OutputSurface| as an overlay plane. Due to the
  // default values for the primary plane, this is a partial list of
  // OverlayCandidate.
  struct VIZ_SERVICE_EXPORT OutputSurfaceOverlayPlane {
    // Display's rotation information.
    gfx::OverlayTransform transform;
    // Rect on the display to position to. This takes in account of Display's
    // rotation.
    gfx::RectF display_rect;
    // Size of output surface in pixels.
    gfx::Size resource_size;
    // Format of the buffer to scanout.
    gfx::BufferFormat format;
    // ColorSpace of the buffer for scanout.
    gfx::ColorSpace color_space;
    // Enable blending when we have underlay.
    bool enable_blending;
    // Gpu fence to wait for before overlay is ready for display.
    unsigned gpu_fence_id;
  };

  class VIZ_SERVICE_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    using PrimaryPlane = OverlayProcessor::OutputSurfaceOverlayPlane;
    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_pass_list|. Most strategies should look at the primary
    // RenderPass, the last element.
    virtual bool Attempt(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        RenderPassList* render_pass_list,
        const PrimaryPlane* primary_plane,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) = 0;

    // Currently this is only overridden by the Underlay strategy: the underlay
    // strategy needs to enable blending for the primary plane in order to show
    // content underneath.
    virtual void AdjustOutputSurfaceOverlay(
        OutputSurfaceOverlayPlane* output_surface_plane) {}

    // Currently this is only overridden by the Fullscreen strategy: the
    // fullscreen strategy covers the entire screen and there is no need to use
    // the primary plane.
    virtual bool RemoveOutputSurfaceAsOverlay();

    virtual OverlayStrategy GetUMAEnum() const;
  };
  using StrategyList = std::vector<std::unique_ptr<Strategy>>;

  static std::unique_ptr<OverlayProcessor> CreateOverlayProcessor(
      SkiaOutputSurface* skia_output_surface,
      gpu::SurfaceHandle surface_handle,
      const OutputSurface::Capabilities& capabilities,
      const RendererSettings& renderer_settings);

  virtual ~OverlayProcessor();

  gfx::Rect GetAndResetOverlayDamage();
  void SetSoftwareMirrorMode(bool software_mirror_mode);

  const OverlayCandidateValidator* GetOverlayCandidateValidator() const {
    return overlay_validator_.get();
  }

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor (currently Windows only).
  bool NeedsSurfaceOccludingDamageRect() const;

  void SetDisplayTransformHint(gfx::OverlayTransform transform);
  void SetValidatorViewportSize(const gfx::Size& size);

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

  // TODO(weiliangc): Eventually the asymmetry between primary plane and
  // non-primary places should be internalized and should not have a special
  // API.
  OutputSurfaceOverlayPlane ProcessOutputSurfaceAsOverlay(
      const gfx::Size& viewport_size,
      const gfx::BufferFormat& buffer_format,
      const gfx::ColorSpace& color_space,
      bool has_alpha) const;

  // For Mac, if we successfully generated a candidate list for CALayerOverlay,
  // we no longer need the |output_surface_plane|. This function takes a pointer
  // to the base::Optional instance so the instance can be reset.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane);

 protected:
  // For testing.
  explicit OverlayProcessor(
      std::unique_ptr<OverlayCandidateValidator> overlay_validator);

  StrategyList strategies_;
  std::unique_ptr<OverlayCandidateValidator> overlay_validator_;

  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;
  bool previous_frame_underlay_was_unoccluded_ = false;

 private:
  OverlayProcessor(
      SkiaOutputSurface* skia_output_surface,
      std::unique_ptr<OverlayCandidateValidator> overlay_validator);

#if defined(OS_WIN)
  void InitializeDCOverlayProcessor(
      std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor);
#endif

  bool ProcessForCALayers(
      DisplayResourceProvider* resource_provider,
      RenderPass* render_pass,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect);
  bool ProcessForDCLayers(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect);
  // Update |damage_rect| by removing damage casued by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        const gfx::Rect& previous_frame_underlay_rect,
                        bool previous_frame_underlay_was_unoccluded,
                        const QuadList* quad_list,
                        gfx::Rect* damage_rect);

#if defined(OS_WIN)
  std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor_;
#endif

#if defined(OS_ANDROID)
  SkiaOutputSurface* skia_output_surface_;
#endif
  bool output_surface_already_handled_;
  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
