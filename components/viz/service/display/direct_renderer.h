// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DIRECT_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DIRECT_RENDERER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/latency/latency_info.h"

namespace cc {
class FilterOperations;
}  // namespace cc

namespace gfx {
class ColorSpace;
}

namespace viz {
class BspWalkActionDrawPolygon;
class DrawPolygon;
class OutputSurface;
class RendererSettings;
class RenderPass;

// This is the base class for code shared between the GL and software
// renderer implementations. "Direct" refers to the fact that it does not
// delegate rendering to another compositor (see historical DelegatingRenderer
// for reference).
class VIZ_SERVICE_EXPORT DirectRenderer {
 public:
  DirectRenderer(const RendererSettings* settings,
                 OutputSurface* output_surface,
                 DisplayResourceProvider* resource_provider);
  virtual ~DirectRenderer();

  void Initialize();

  bool use_partial_swap() const { return use_partial_swap_; }

  void SetVisible(bool visible);
  void DecideRenderPassAllocationsForFrame(
      const RenderPassList& render_passes_in_draw_order);
  void DrawFrame(RenderPassList* render_passes_in_draw_order,
                 float device_scale_factor,
                 const gfx::Size& device_viewport_size);

  // Public interface implemented by subclasses.
  virtual void SwapBuffers(std::vector<ui::LatencyInfo> latency_info,
                           bool need_presentation_feedback) = 0;
  virtual void SwapBuffersComplete() {}
  virtual void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) {}

  // Public for tests that poke at internals.
  struct VIZ_SERVICE_EXPORT DrawingFrame {
    DrawingFrame();
    ~DrawingFrame();

    const RenderPassList* render_passes_in_draw_order = nullptr;
    const RenderPass* root_render_pass = nullptr;
    const RenderPass* current_render_pass = nullptr;

    gfx::Rect root_damage_rect;
    std::vector<gfx::Rect> root_content_bounds;
    gfx::Size device_viewport_size;

    gfx::Transform projection_matrix;
    gfx::Transform window_matrix;

    OverlayCandidateList overlay_list;
    CALayerOverlayList ca_layer_overlay_list;
    DCLayerOverlayList dc_layer_overlay_list;
  };

  void SetCurrentFrameForTesting(const DrawingFrame& frame);
  bool HasAllocatedResourcesForTesting(
      const RenderPassId& render_pass_id) const;
  // Allow tests to enlarge the texture size of non-root render passes to
  // verify cases where the texture doesn't match the render pass size.
  void SetEnlargePassTextureAmountForTesting(const gfx::Size& amount) {
    enlarge_pass_texture_amount_ = amount;
  }

 protected:
  friend class BspWalkActionDrawPolygon;

  enum SurfaceInitializationMode {
    SURFACE_INITIALIZATION_MODE_PRESERVE,
    SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR,
    SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR,
  };

  struct RenderPassRequirements {
    gfx::Size size;
    bool mipmap = false;
  };

  static gfx::RectF QuadVertexRect();
  static void QuadRectTransform(gfx::Transform* quad_rect_transform,
                                const gfx::Transform& quad_transform,
                                const gfx::RectF& quad_rect);
  // This function takes DrawingFrame as an argument because RenderPass drawing
  // code uses its computations for buffer sizing.
  void InitializeViewport(DrawingFrame* frame,
                          const gfx::Rect& draw_rect,
                          const gfx::Rect& viewport_rect,
                          const gfx::Size& surface_size);
  gfx::Rect MoveFromDrawToWindowSpace(const gfx::Rect& draw_rect) const;

  gfx::Rect DeviceViewportRectInDrawSpace() const;
  gfx::Rect OutputSurfaceRectInDrawSpace() const;
  void SetScissorStateForQuad(const DrawQuad& quad,
                              const gfx::Rect& render_pass_scissor,
                              bool use_render_pass_scissor);
  bool ShouldSkipQuad(const DrawQuad& quad,
                      const gfx::Rect& render_pass_scissor);
  void SetScissorTestRectInDrawSpace(const gfx::Rect& draw_space_rect);

  gfx::Size CalculateTextureSizeForRenderPass(const RenderPass* render_pass);

  void FlushPolygons(
      base::circular_deque<std::unique_ptr<DrawPolygon>>* poly_list,
      const gfx::Rect& render_pass_scissor,
      bool use_render_pass_scissor);
  void DrawRenderPassAndExecuteCopyRequests(RenderPass* render_pass);
  void DrawRenderPass(const RenderPass* render_pass);
  // Returns true if it detects that we do not need to draw the render pass.
  // This may be because the RenderPass is already cached, or because it is
  // entirely clipped out, for instance.
  bool CanSkipRenderPass(const RenderPass* render_pass) const;
  void UseRenderPass(const RenderPass* render_pass);
  gfx::Rect ComputeScissorRectForRenderPass(
      const RenderPass* render_pass) const;

  void DoDrawPolygon(const DrawPolygon& poly,
                     const gfx::Rect& render_pass_scissor,
                     bool use_render_pass_scissor);

  const cc::FilterOperations* FiltersForPass(RenderPassId render_pass_id) const;
  const cc::FilterOperations* BackgroundFiltersForPass(
      RenderPassId render_pass_id) const;

  // Private interface implemented by subclasses for use by DirectRenderer.
  virtual bool CanPartialSwap() = 0;
  virtual void UpdateRenderPassTextures(
      const RenderPassList& render_passes_in_draw_order,
      const base::flat_map<RenderPassId, RenderPassRequirements>&
          render_passes_in_frame) = 0;
  virtual void AllocateRenderPassResourceIfNeeded(
      const RenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) = 0;
  virtual bool IsRenderPassResourceAllocated(
      const RenderPassId& render_pass_id) const = 0;
  virtual gfx::Size GetRenderPassBackingPixelSize(
      const RenderPassId& render_pass_id) = 0;
  virtual void BindFramebufferToOutputSurface() = 0;
  virtual void BindFramebufferToTexture(const RenderPassId render_pass_id) = 0;
  virtual void SetScissorTestRect(const gfx::Rect& scissor_rect) = 0;
  virtual void PrepareSurfaceForPass(
      SurfaceInitializationMode initialization_mode,
      const gfx::Rect& render_pass_scissor) = 0;
  // |clip_region| is a (possibly null) pointer to a quad in the same
  // space as the quad. When non-null only the area of the quad that overlaps
  // with clip_region will be drawn.
  virtual void DoDrawQuad(const DrawQuad* quad,
                          const gfx::QuadF* clip_region) = 0;
  virtual void BeginDrawingFrame() = 0;
  virtual void FinishDrawingFrame() = 0;
  // If a pass contains a single tile draw quad and can be drawn without
  // a render pass (e.g. applying a filter directly to the tile quad)
  // return that quad, otherwise return null.
  static const TileDrawQuad* CanPassBeDrawnDirectly(
      const RenderPass* pass,
      bool is_using_vulkan,
      DisplayResourceProvider* const resource_provider);
  virtual const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass);
  virtual void FinishDrawingQuadList() {}
  virtual bool FlippedFramebuffer() const = 0;
  virtual void EnsureScissorTestEnabled() = 0;
  virtual void EnsureScissorTestDisabled() = 0;
  virtual void DidChangeVisibility() = 0;
  virtual void CopyDrawnRenderPass(
      std::unique_ptr<CopyOutputRequest> request) = 0;
  virtual void SetEnableDCLayers(bool enable) = 0;
  virtual void GenerateMipmap() = 0;

  gfx::Size surface_size_for_swap_buffers() const {
    return reshape_surface_size_;
  }

  const RendererSettings* const settings_;
  OutputSurface* const output_surface_;
  DisplayResourceProvider* const resource_provider_;
  // This can be replaced by test implementations.
  std::unique_ptr<OverlayProcessor> overlay_processor_;

  // Whether it's valid to SwapBuffers with an empty rect. Trivially true when
  // using partial swap.
  bool allow_empty_swap_ = false;
  // Whether partial swap can be used.
  bool use_partial_swap_ = false;
  // Whether overdraw feedback is enabled and can be used.
  bool overdraw_feedback_ = false;
  // Whether the SetDrawRectangle and EnableDCLayers commands are in
  // use.
  bool supports_dc_layers_ = false;
  // Whether the output surface is actually using DirectComposition.
  bool using_dc_layers_ = false;
  // This counts the number of draws since the last time
  // DirectComposition layers needed to be used.
  int frames_since_using_dc_layers_ = 0;

  // A map from RenderPass id to the single quad present in and replacing the
  // RenderPass.
  base::flat_map<RenderPassId, TileDrawQuad> render_pass_bypass_quads_;

  // A map from RenderPass id to the filters used when drawing the RenderPass.
  base::flat_map<RenderPassId, cc::FilterOperations*> render_pass_filters_;
  base::flat_map<RenderPassId, cc::FilterOperations*>
      render_pass_backdrop_filters_;

  bool visible_ = false;
  bool disable_color_checks_for_testing_ = false;

  // For use in coordinate conversion, this stores the output rect, viewport
  // rect (= unflipped version of glViewport rect), the size of target
  // framebuffer, and the current window space viewport. During a draw, this
  // stores the values for the current render pass; in between draws, they
  // retain the values for the root render pass of the last draw.
  gfx::Rect current_draw_rect_;
  gfx::Rect current_viewport_rect_;
  gfx::Size current_surface_size_;
  gfx::Rect current_window_space_viewport_;

  DrawingFrame* current_frame() {
    DCHECK(current_frame_valid_);
    return &current_frame_;
  }
  const DrawingFrame* current_frame() const {
    DCHECK(current_frame_valid_);
    return &current_frame_;
  }

 private:
  bool initialized_ = false;
#if DCHECK_IS_ON()
  bool overdraw_feedback_support_missing_logged_once_ = false;
#endif
  gfx::Size enlarge_pass_texture_amount_;

  // The current drawing frame is valid only during the duration of the
  // DrawFrame function. Use the accessor current_frame() to ensure that use
  // is valid;
  DrawingFrame current_frame_;
  bool current_frame_valid_ = false;

  // Cached values given to Reshape().
  gfx::Size reshape_surface_size_;
  float reshape_device_scale_factor_ = 0.f;
  gfx::ColorSpace reshape_device_color_space_;
  bool reshape_has_alpha_ = false;
  bool reshape_use_stencil_ = false;

  DISALLOW_COPY_AND_ASSIGN(DirectRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DIRECT_RENDERER_H_
