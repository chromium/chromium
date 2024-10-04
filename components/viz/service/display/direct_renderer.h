// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DIRECT_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DIRECT_RENDERER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/render_pass_alpha_type.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/ca_layer_result.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/latency/latency_info.h"

namespace cc {
class FilterOperations;
}  // namespace cc

namespace gfx {
class ColorSpace;
class RRectF;
}  // namespace gfx

namespace gpu {
struct SwapBuffersCompleteParams;
}

namespace viz {
class BspWalkActionDrawPolygon;
class DrawPolygon;
class OutputSurface;
struct DebugRendererSettings;
class RendererSettings;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// This is the base class for code shared between the GL and software
// renderer implementations. "Direct" refers to the fact that it does not
// delegate rendering to another compositor (see historical DelegatingRenderer
// for reference).
class VIZ_SERVICE_EXPORT DirectRenderer {
 public:
  DirectRenderer(const RendererSettings* settings,
                 const DebugRendererSettings* debug_settings,
                 OutputSurface* output_surface,
                 DisplayResourceProvider* resource_provider,
                 OverlayProcessorInterface* overlay_processor);

  DirectRenderer(const DirectRenderer&) = delete;
  DirectRenderer& operator=(const DirectRenderer&) = delete;

  virtual ~DirectRenderer();

  void Initialize();

  bool use_partial_swap() const { return use_partial_swap_; }

  void SetOutputSurfaceClipRect(const gfx::Rect& clip_rect);
  void SetVisible(bool visible);
  void ReallocatedFrameBuffers();
  void DecideRenderPassAllocationsForFrame(
      const AggregatedRenderPassList& render_passes_in_draw_order);
  void DrawFrame(AggregatedRenderPassList* render_passes_in_draw_order,
                 float device_scale_factor,
                 const gfx::Size& device_viewport_size,
                 const gfx::DisplayColorSpaces& display_color_spaces,
                 SurfaceDamageRectList surface_damage_rect_list);

  // The renderer might expand the damage (e.g: HW overlays were used,
  // invalidation rects on previous buffers). This function returns a
  // bounding rect of the area that might need to be recomposited.
  gfx::Rect GetTargetDamageBoundingRect() const;

  // Public interface implemented by subclasses.
  struct VIZ_SERVICE_EXPORT SwapFrameData {
    SwapFrameData();
    ~SwapFrameData();

    SwapFrameData& operator=(SwapFrameData&&);
    SwapFrameData(SwapFrameData&&);

    SwapFrameData(const SwapFrameData&) = delete;
    SwapFrameData& operator=(const SwapFrameData&) = delete;

    std::vector<ui::LatencyInfo> latency_info;
    int64_t seq = -1;

    // The HDR headroom of the display that this frame is being swapped to.
    // Propagated to the gl::Presenter via gfx::FrameData.
    float display_hdr_headroom = 1.f;

#if BUILDFLAG(IS_APPLE)
    // The result of CoreAnimation delegated compositing from the overlay
    // processor. Propagated to the gl::Presenter via gfx::FrameData for
    // integration testing.
    gfx::CALayerResult ca_layer_error_code = gfx::kCALayerSuccess;
#endif
    std::optional<int64_t> choreographer_vsync_id;
    int64_t swap_trace_id = -1;
  };
  virtual void SwapBuffers(SwapFrameData swap_frame_data) = 0;
  virtual void SwapBuffersSkipped() {}
  virtual void SwapBuffersComplete(const gpu::SwapBuffersCompleteParams& params,
                                   gfx::GpuFenceHandle release_fence) {}
  virtual void BuffersPresented() {}
  virtual void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) {}

  // Public for tests that poke at internals.
  struct VIZ_SERVICE_EXPORT DrawingFrame {
    DrawingFrame();
    ~DrawingFrame();

    raw_ptr<const AggregatedRenderPassList> render_passes_in_draw_order =
        nullptr;
    raw_ptr<const AggregatedRenderPass> root_render_pass = nullptr;
    raw_ptr<const AggregatedRenderPass> current_render_pass = nullptr;

    gfx::Rect root_damage_rect;
    std::vector<gfx::Rect> root_content_bounds;
    gfx::Size device_viewport_size;
    gfx::DisplayColorSpaces display_color_spaces;

    gfx::AxisTransform2d target_to_device_transform;

    OverlayProcessorInterface::CandidateList overlay_list;
    // When we have a buffer queue, the output surface could be treated as an
    // overlay plane, and the struct to store that information is in
    // |output_surface_plane|.
    std::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
        output_surface_plane;
  };

  void SetCurrentFrameForTesting(const DrawingFrame& frame);
  bool HasAllocatedResourcesForTesting(
      const AggregatedRenderPassId& render_pass_id) const;
  // Allow tests to enlarge the texture size of non-root render passes to
  // verify cases where the texture doesn't match the render pass size.
  void SetEnlargePassTextureAmountForTesting(const gfx::Size& amount) {
    enlarge_pass_texture_amount_ = amount;
  }

  gfx::Rect GetLastRootScissorRectForTesting() const {
    return last_root_render_pass_scissor_rect_;
  }

  base::flat_set<AggregatedRenderPassId>*
  GetLastSkippedRenderPassIdsForTesting() {
    return &skipped_render_pass_ids_;
  }

  virtual DelegatedInkPointRendererBase* GetDelegatedInkPointRenderer(
      bool create_if_necessary);
  virtual void SetDelegatedInkMetadata(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) {}

  // Returns true if composite time tracing is enabled. This measures a detailed
  // trace log for draw time spent per quad.
  virtual bool CompositeTimeTracingEnabled();

  // Puts the draw time wall in trace file relative to the |ready_timestamp|.
  virtual void AddCompositeTimeTraces(base::TimeTicks ready_timestamp);

  // Returns the current frame buffer damage.
  virtual gfx::Rect GetCurrentFramebufferDamage() const;

  // Reshapes the output surface.
  virtual void Reshape(const OutputSurface::ReshapeParams& reshape_params);

  // Set the number of frame buffers to use when
  // `supports_dynamic_frame_buffer_allocation` is true. `n` must satisfy
  // 0 < n <= capabilities_.number_of_buffers.
  virtual void EnsureMinNumberOfBuffers(int n) {}

  // Gets a mailbox that can be used for overlay testing the primary plane. This
  // does not need to be the next mailbox that will be swapped.
  virtual gpu::Mailbox GetPrimaryPlaneOverlayTestingMailbox();

  // Return the bounding rect of previously drawn delegated ink trail.
  gfx::Rect GetDelegatedInkTrailDamageRect();

 protected:
  friend class BspWalkActionDrawPolygon;
  friend class SkiaDelegatedInkRendererTest;
  friend class DelegatedInkPointPixelTestHelper;
  friend class DelegatedInkDisplayTest;

  struct RenderPassRequirements {
    gfx::Size size;
    bool generate_mipmap = false;
    SharedImageFormat format;
    gfx::ColorSpace color_space;
    RenderPassAlphaType alpha_type = RenderPassAlphaType::kPremul;
    // Render pass wants scanout
    bool is_scanout = false;
    // Render pass wants to synchronize updates with other overlays, on Windows
    // this means a DComp surface.
    bool scanout_dcomp_surface = false;
  };

  static gfx::RectF QuadVertexRect();
  static void QuadRectTransform(gfx::Transform* quad_rect_transform,
                                const gfx::Transform& quad_transform,
                                const gfx::RectF& quad_rect);
  // Returns a transform that maps the the draw rect (i.e. the render pass
  // output rect) to the device space (i.e. buffer space).
  gfx::AxisTransform2d CalculateTargetToDeviceTransform(
      const gfx::Rect& draw_rect,
      const gfx::Size& viewport_size);
  gfx::Rect MoveFromDrawToWindowSpace(const gfx::Rect& draw_rect) const;

  gfx::Rect DeviceViewportRectInDrawSpace() const;
  gfx::Rect OutputSurfaceRectInDrawSpace() const;
  void SetScissorStateForQuad(const DrawQuad& quad,
                              const gfx::Rect& render_pass_scissor,
                              bool use_render_pass_scissor);
  bool ShouldSkipQuad(const DrawQuad& quad,
                      const gfx::Rect& render_pass_scissor);
  void SetScissorTestRectInDrawSpace(const gfx::Rect& draw_space_rect);

  gfx::Size CalculateTextureSizeForRenderPass(
      const AggregatedRenderPass* render_pass) const;
  gfx::Size CalculateSizeForOutputSurface(
      const gfx::Size& device_viewport_size);
  RenderPassRequirements CalculateRenderPassRequirements(
      const AggregatedRenderPass* render_pass) const;

  void FlushPolygons(
      base::circular_deque<std::unique_ptr<DrawPolygon>>* poly_list,
      const gfx::Rect& render_pass_scissor,
      bool use_render_pass_scissor);
  void DrawRenderPassAndExecuteCopyRequests(AggregatedRenderPass* render_pass);
  void DrawRenderPass(const AggregatedRenderPass* render_pass);
  // Returns true if it detects that we do not need to draw the render pass.
  // This may be because the RenderPass is already cached, or because it is
  // entirely clipped out, for instance.
  bool CanSkipRenderPass(const AggregatedRenderPass* render_pass) const;
  void EnsureRenderPassAllocated(const AggregatedRenderPass* render_pass);
  gfx::Rect ComputeScissorRectForRenderPass(
      const AggregatedRenderPass* render_pass) const;

  void DoDrawPolygon(const DrawPolygon& poly,
                     const gfx::Rect& render_pass_scissor,
                     bool use_render_pass_scissor);

  const cc::FilterOperations* FiltersForPass(
      AggregatedRenderPassId render_pass_id) const;
  const cc::FilterOperations* BackdropFiltersForPass(
      AggregatedRenderPassId render_pass_id) const;
  const std::optional<gfx::RRectF> BackdropFilterBoundsForPass(
      AggregatedRenderPassId render_pass_id) const;

  virtual void SetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id,
      const gfx::Rect& drawn_rect) = 0;

  virtual gfx::Rect GetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id) const = 0;

  // Private interface implemented by subclasses for use by DirectRenderer.
  virtual bool CanPartialSwap() = 0;
  virtual void UpdateRenderPassTextures(
      const AggregatedRenderPassList& render_passes_in_draw_order,
      const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
          render_passes_in_frame) = 0;
  virtual void AllocateRenderPassResourceIfNeeded(
      const AggregatedRenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) = 0;
  virtual bool IsRenderPassResourceAllocated(
      const AggregatedRenderPassId& render_pass_id) const = 0;
  virtual gfx::Size GetRenderPassBackingPixelSize(
      const AggregatedRenderPassId& render_pass_id) = 0;

  virtual void SetScissorTestRect(const gfx::Rect& scissor_rect) = 0;
  // |render_pass_update_rect| is in render pass backing buffer space.
  virtual void BeginDrawingRenderPass(const AggregatedRenderPass* render_pass,
                                      bool needs_clear,
                                      const gfx::Rect& render_pass_update_rect,
                                      const gfx::Size& viewport_size) = 0;
  // |clip_region| is a (possibly null) pointer to a quad in the same
  // space as the quad. When non-null only the area of the quad that overlaps
  // with clip_region will be drawn.
  virtual void DoDrawQuad(const DrawQuad* quad,
                          const gfx::QuadF* clip_region) = 0;
  virtual void FinishDrawingRenderPass() {}
  virtual void BeginDrawingFrame() = 0;
  virtual void FinishDrawingFrame() = 0;
  // If a pass contains a single tile draw quad and can be drawn without
  // a render pass (e.g. applying a filter directly to the tile quad)
  // return that quad, otherwise return null.
  virtual const DrawQuad* CanPassBeDrawnDirectly(
      const AggregatedRenderPass* pass,
      const RenderPassRequirements& requirements);
  virtual void EnsureScissorTestDisabled() = 0;
  virtual void DidChangeVisibility() = 0;
  virtual void CopyDrawnRenderPass(
      const copy_output::RenderPassGeometry& geometry,
      std::unique_ptr<CopyOutputRequest> request) = 0;
  virtual bool SupportsBGRA() const;

  gfx::Size surface_size_for_swap_buffers() const {
    return reshape_params_ ? reshape_params_->size : gfx::Size();
  }
  gfx::Size viewport_size_for_swap_buffers() const {
    return device_viewport_size_;
  }

  bool ShouldApplyRoundedCorner(const DrawQuad* quad) const;
  bool ShouldApplyGradientMask(const DrawQuad* quad) const;

  float CurrentFrameSDRWhiteLevel() const;
  SharedImageFormat GetColorSpaceSharedImageFormat(
      gfx::ColorSpace color_space) const;
  // Return the SkColorSpace for rendering to the current render pass. Unlike
  // CurrentRenderPassColorSpace, this color space has the value of
  // CurrentFrameSDRWhiteLevel incorporated into it.
  sk_sp<SkColorSpace> CurrentRenderPassSkColorSpace() const {
    return RenderPassColorSpace(current_frame()->current_render_pass)
        .ToSkColorSpace(CurrentFrameSDRWhiteLevel());
  }

  gfx::ColorSpace RenderPassColorSpace(
      const AggregatedRenderPass* render_pass) const;

  const raw_ptr<const RendererSettings> settings_;
  // Points to the viz-global singleton.
  const raw_ptr<const DebugRendererSettings> debug_settings_;
  const raw_ptr<OutputSurface> output_surface_;
  const raw_ptr<DisplayResourceProvider> resource_provider_;
  // This can be replaced by test implementations.
  // TODO(weiliangc): For SoftwareRenderer and tests where overlay is not used,
  // use OverlayProcessorStub so this pointer is never null.
  raw_ptr<OverlayProcessorInterface> overlay_processor_;

  // If the non-root render pass and its embedded child render passes are not
  // damaged, skip the rendering.
  const bool allow_undamaged_nonroot_render_pass_to_skip_;

  // Whether partial swap can be used.
  bool use_partial_swap_ = false;

  // Whether render pass drawn rect functionality can be used. This means we
  // will be tracking the drawn area of a render pass to determine what needs to
  // be redrawn every frame.
  bool use_render_pass_drawn_rect_ = false;

  // A map from RenderPass id to the single quad present in and replacing the
  // RenderPass. The DrawQuads are owned by their RenderPasses, which outlive
  // the drawn frame, so it is safe to store these pointers until the end of
  // DrawFrame().
  base::flat_map<AggregatedRenderPassId,
                 raw_ptr<const DrawQuad, CtnExperimental>>
      render_pass_bypass_quads_;

  // A map from RenderPass id to the filters used when drawing the RenderPass.
  base::flat_map<AggregatedRenderPassId,
                 raw_ptr<cc::FilterOperations, CtnExperimental>>
      render_pass_filters_;
  base::flat_map<AggregatedRenderPassId,
                 raw_ptr<cc::FilterOperations, CtnExperimental>>
      render_pass_backdrop_filters_;
  base::flat_map<AggregatedRenderPassId, std::optional<gfx::RRectF>>
      render_pass_backdrop_filter_bounds_;
  base::flat_map<AggregatedRenderPassId, gfx::Rect>
      backdrop_filter_output_rects_;

  // Whether a render pass with foreground filters that move pixels is found in
  // this frame.
  bool has_pixel_moving_foreground_filters_ = false;

  // Track skipped non-root render passes in DrawRenderPass.
  base::flat_set<AggregatedRenderPassId> skipped_render_pass_ids_;

  bool visible_ = false;
  bool disable_color_checks_for_testing_ = false;

  DrawingFrame* current_frame() {
    DCHECK(current_frame_valid_);
    return &current_frame_;
  }
  const DrawingFrame* current_frame() const {
    DCHECK(current_frame_valid_);
    return &current_frame_;
  }
  SharedImageFormat reshape_si_format() const {
    DCHECK(reshape_params_);
    return reshape_params_->format;
  }
  gfx::ColorSpace reshape_color_space() const {
    DCHECK(reshape_params_);
    return reshape_params_->color_space;
  }
  RenderPassAlphaType reshape_alpha_type() const {
    DCHECK(reshape_params_);
    return reshape_params_->alpha_type;
  }

  // Sets a DelegatedInkPointRendererSkiaForTest to be used for testing only, in
  // order to save delegated ink metadata values that would otherwise be reset.
  virtual void SetDelegatedInkPointRendererSkiaForTest(
      std::unique_ptr<DelegatedInkPointRendererSkia> renderer) {}

 private:
  // Update the damage rect of the render pass that will contain the drawn ink
  // trail, or had drawn the ink trail in the previous frame.
  void AddInkDamageToRenderPass(const AggregatedRenderPass* render_pass,
                                gfx::Rect& output_damage_rect);

  bool initialized_ = false;

  // Track the id of the render pass that received delegated ink in the latest
  // frame. This will be used when the the ink trail is cleared and damage
  // needs to be applied to the correct render pass.
  std::optional<AggregatedRenderPassId> last_pass_with_delegated_ink_;

  gfx::Rect last_root_render_pass_scissor_rect_;
  gfx::Size enlarge_pass_texture_amount_;

  // The current drawing frame is valid only during the duration of the
  // DrawFrame function. Use the accessor current_frame() to ensure that use
  // is valid;
  DrawingFrame current_frame_;
  bool current_frame_valid_ = false;

  // Time of most recent reshape that ended up with |device_viewport_size_| !=
  // |reshape_params->size|.
  base::TimeTicks last_viewport_resize_time_;

  bool next_frame_needs_full_frame_redraw_ = false;

  // Cached values given to Reshape(). The `reshape_params_` is optional
  // to prevent use of uninitialized values. The size in these parameters
  // may be larger than the `device_viewport_size_` that users see.
  std::optional<OutputSurface::ReshapeParams> reshape_params_;

  // If present additionally restricts drawing to the OutputSurface. WebView
  // gets this rect from the HWUI.
  std::optional<gfx::Rect> output_surface_clip_rect_;
  gfx::Size device_viewport_size_;
  gfx::OverlayTransform reshape_display_transform_ =
      gfx::OVERLAY_TRANSFORM_INVALID;
  uint64_t total_pixels_rendered_this_frame_ = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DIRECT_RENDERER_H_
