// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_INTERFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_INTERFACE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gpu_task_scheduler_helper.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/swap_result.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

namespace gpu {
class SharedImageInterface;
}

namespace viz {
struct DebugRendererSettings;
class DisplayResourceProvider;
class OutputSurface;
class RendererSettings;

// This class is called inside the DirectRenderer to separate the contents that
// should be sent into the overlay system and the contents that requires
// compositing from the DirectRenderer. This class has different subclass
// implemented by different platforms. This class defines the minimal interface
// for overlay processing that each platform needs to implement.
class VIZ_SERVICE_EXPORT OverlayProcessorInterface {
 public:
  using PlatformOverlayCandidate = OverlayCandidate;
  using CandidateList = OverlayCandidateList;
  using FilterOperationsMap =
      base::flat_map<AggregatedRenderPassId,
                     raw_ptr<cc::FilterOperations, CtnExperimental>>;

  virtual bool DisableSplittingQuads() const;

  // Used by DCLayerOverlayProcessor and OverlayProcessorUsingStrategy.
  static void RecordOverlayDamageRectHistograms(
      bool is_overlay,
      bool has_occluding_surface_damage,
      bool zero_damage_rect);

  // Data needed to represent |OutputSurface| as an overlay plane. Due to the
  // default values for the primary plane, this is a partial list of
  // OverlayCandidate.
  struct VIZ_SERVICE_EXPORT OutputSurfaceOverlayPlane {
    OutputSurfaceOverlayPlane();
    OutputSurfaceOverlayPlane(const OutputSurfaceOverlayPlane&);
    OutputSurfaceOverlayPlane& operator=(const OutputSurfaceOverlayPlane&);
    ~OutputSurfaceOverlayPlane();
    // Display's rotation information.
    gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
    // Rect on the display to position to. This takes in account of Display's
    // rotation.
    gfx::RectF display_rect;
    // Specifies the region within the buffer to be cropped and (maybe)scaled to
    // place inside |display_rect|.
    gfx::RectF uv_rect;
    // Size of output surface in pixels.
    gfx::Size resource_size;
    // Format of the buffer to scanout.
    SharedImageFormat format = SinglePlaneFormat::kBGRA_8888;
    // ColorSpace of the buffer for scanout.
    gfx::ColorSpace color_space;
    // Enable blending when we have underlay.
    bool enable_blending = false;
    // Opacity of the overlay independent of buffer alpha. When rendered:
    // src-alpha = |opacity| * buffer-component-alpha.
    float opacity = 1.0f;
    // Mailbox corresponding to the buffer backing the primary plane.
    gpu::Mailbox mailbox;
    // Hints for overlay prioritization.
    gfx::OverlayPriorityHint priority_hint = gfx::OverlayPriorityHint::kNone;
    // Specifies the rounded corners.
    gfx::RRectF rounded_corners;
    // Optional damage rect. If none is provided the damage is assumed to be
    // |resource_size| (full damage).
    std::optional<gfx::Rect> damage_rect;
  };

  // TODO(weiliangc): Eventually the asymmetry between primary plane and
  // non-primary places should be internalized and should not have a special
  // API.
  static OutputSurfaceOverlayPlane ProcessOutputSurfaceAsOverlay(
      const gfx::Size& viewport_size,
      const gfx::Size& resource_size,
      const SharedImageFormat si_format,
      const gfx::ColorSpace& color_space,
      bool has_alpha,
      float opacity,
      const gpu::Mailbox& mailbox);

  static std::unique_ptr<OverlayProcessorInterface> CreateOverlayProcessor(
      OutputSurface* output_surface,
      gpu::SurfaceHandle surface_handle,
      const OutputSurface::Capabilities& capabilities,
      DisplayCompositorMemoryAndTaskController* display_controller,
      gpu::SharedImageInterface* shared_image_interface,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings);

  OverlayProcessorInterface(const OverlayProcessorInterface&) = delete;
  OverlayProcessorInterface& operator=(const OverlayProcessorInterface&) =
      delete;

  virtual ~OverlayProcessorInterface() = default;

  virtual bool IsOverlaySupported() const = 0;
  // Returns a bounding rectangle of the last set of overlay planes scheduled.
  // It's expected to be called after ProcessForOverlays at frame N-1 has been
  // called and before GetAndResetOverlayDamage at frame N.
  virtual gfx::Rect GetPreviousFrameOverlaysBoundingRect() const = 0;
  virtual gfx::Rect GetAndResetOverlayDamage() = 0;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  virtual bool NeedsSurfaceDamageRectList() const = 0;

  // Attempts to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  virtual void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      AggregatedRenderPassList* render_passes,
      const SkM44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      SurfaceDamageRectList surface_damage_rect_list,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) = 0;

  // If we successfully generated a candidates list for delegated compositing
  // during |ProcessForOverlays|, we no longer need the |output_surface_plane|.
  // This function takes a pointer to the std::optional instance so the instance
  // can be reset.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  virtual void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) = 0;

  // Before the overlay refactor to use OverlayProcessorOnGpu, overlay
  // candidates are stored inside DirectRenderer. Those overlay candidates are
  // later sent over to the GPU thread by SkiaRenderer. This helper function
  // will be called by DirectRenderer to take these overlay candidates inside
  // overlay processor to avoid sending over DirectRenderer implementation. This
  // is overridden by each platform that is ready to send overlay candidates
  // inside |OverlayProcessor|. Must be called before ScheduleOverlays().
  virtual void TakeOverlayCandidates(CandidateList* candidate_list) {}

  // TODO(weiliangc): Make it pure virtual after it is implemented by every
  // subclass.
  virtual void ScheduleOverlays(
      DisplayResourceProvider* display_resource_provider);
  // This is a signal from Display::DidReceiveSwapBuffersAck. This is used as
  // approximate signale for when the overlays are presented.
  virtual void OverlayPresentationComplete();

  // These two functions are used by Android SurfaceControl, and SetViewportSize
  // is also used for Windows DC layers.
  virtual void SetDisplayTransformHint(gfx::OverlayTransform transform) {}
  virtual void SetViewportSize(const gfx::Size& size) {}

  // Overlay processor uses a frame counter to determine the potential power
  // benefits of individual overlay candidates.
  virtual void SetFrameSequenceNumber(uint64_t frame_sequence_number) {}

  // If true, page fullscreen mode is enabled for this frame.
  virtual void SetIsPageFullscreen(bool enabled) {}

  virtual gfx::CALayerResult GetCALayerErrorCode() const;

  // For Lacros, get damage that was not assigned to any overlay candidates
  // during ProcessForOverlays.
  virtual gfx::RectF GetUnassignedDamage() const;

  // Supports gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90 and
  // gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270 transforms.
  virtual bool SupportsFlipRotateTransform() const;

  // This is used by the overlay processor on platforms that support delegated
  // ink. It marks the current frame as having delegated ink, and is cleared in
  // the next ProcessForOverlays call.
  virtual void SetFrameHasDelegatedInk() {}

  // Notifies the OverlayProcessor about the status of the last swap.
  virtual void OnSwapBuffersComplete(gfx::SwapResult swap_result) {}

 protected:
  OverlayProcessorInterface() = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_INTERFACE_H_
