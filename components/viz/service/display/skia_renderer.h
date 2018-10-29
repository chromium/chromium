// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include <tuple>

#include "base/macros.h"
#include "cc/cc_export.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/sync_query_collection.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/latency/latency_info.h"

class SkNWayCanvas;
class SkPictureRecorder;

namespace gpu {
struct Capabilities;
}

namespace viz {
class DebugBorderDrawQuad;
class PictureDrawQuad;
class SkiaOutputSurface;
class SolidColorDrawQuad;
class TextureDrawQuad;
class TileDrawQuad;
class VulkanContextProvider;
class YUVVideoDrawQuad;

class VIZ_SERVICE_EXPORT SkiaRenderer : public DirectRenderer {
 public:
  // Different draw modes that are supported by SkiaRenderer right now.
  enum DrawMode { GL, DDL, VULKAN, SKPRECORD };

  // TODO(penghuang): Remove skia_output_surface when DDL is used everywhere.
  SkiaRenderer(const RendererSettings* settings,
               OutputSurface* output_surface,
               DisplayResourceProvider* resource_provider,
               SkiaOutputSurface* skia_output_surface,
               DrawMode mode);
  ~SkiaRenderer() override;

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info,
                   bool need_presentation_feedback) override;

  void SetDisablePictureQuadImageFiltering(bool disable) {
    disable_picture_quad_image_filtering_ = disable;
  }

 protected:
  bool CanPartialSwap() override;
  void UpdateRenderPassTextures(
      const RenderPassList& render_passes_in_draw_order,
      const base::flat_map<RenderPassId, RenderPassRequirements>&
          render_passes_in_frame) override;
  void AllocateRenderPassResourceIfNeeded(
      const RenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) override;
  bool IsRenderPassResourceAllocated(
      const RenderPassId& render_pass_id) const override;
  gfx::Size GetRenderPassBackingPixelSize(
      const RenderPassId& render_pass_id) override;
  void BindFramebufferToOutputSurface() override;
  void BindFramebufferToTexture(const RenderPassId render_pass_id) override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void PrepareSurfaceForPass(SurfaceInitializationMode initialization_mode,
                             const gfx::Rect& render_pass_scissor) override;
  void DoDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  bool FlippedFramebuffer() const override;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(std::unique_ptr<CopyOutputRequest> request) override;
  void SetEnableDCLayers(bool enable) override;
  void DidChangeVisibility() override;
  void FinishDrawingQuadList() override;
  void GenerateMipmap() override;

 private:
  struct DrawRenderPassDrawQuadParams;
  class ScopedSkImageBuilder;
  class ScopedYUVSkImageBuilder;

  void ClearCanvas(SkColor color);
  void ClearFramebuffer();

  void PrepareCanvasForDrawQuads(
      const SharedQuadState* shared_quad_state,
      const gfx::QuadF* draw_region,
      const gfx::Rect* scissor_rect,
      base::Optional<SkAutoCanvasRestore>* auto_canvas_restore);
  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad);
  void DrawPictureQuad(const PictureDrawQuad* quad);
  void DrawRenderPassQuad(const RenderPassDrawQuad* quad);
  void DrawRenderPassQuadInternal(const RenderPassDrawQuad* quad,
                                  sk_sp<SkImage> content_image);

  void DrawSolidColorQuad(const SolidColorDrawQuad* quad);
  void DrawTextureQuad(const TextureDrawQuad* quad);
  bool MustDrawBatchedTileQuadsBeforeQuad(const DrawQuad* new_quad,
                                          const gfx::QuadF* draw_region);
  void AddTileQuadToBatch(const TileDrawQuad* quad,
                          const gfx::QuadF* draw_region);
  void DrawBatchedTileQuads();
  void DrawYUVVideoQuad(const YUVVideoDrawQuad* quad);
  void DrawUnsupportedQuad(const DrawQuad* quad);
  bool CalculateRPDQParams(sk_sp<SkImage> src_image,
                           const RenderPassDrawQuad* quad,
                           DrawRenderPassDrawQuadParams* params);
  bool ShouldApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const cc::FilterOperations* backdrop_filters) const;
  bool IsUsingVulkan() const;
  const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass) override;

  // A map from RenderPass id to the texture used to draw the RenderPass from.
  struct RenderPassBacking {
    sk_sp<SkSurface> render_pass_surface;
    gfx::Size size;
    bool mipmap;
    gfx::ColorSpace color_space;
    ResourceFormat format;

    // Specific for SkPictureRecorder.
    std::unique_ptr<SkPictureRecorder> recorder;
    sk_sp<SkPicture> picture;

    RenderPassBacking(GrContext* gr_context,
                      const gpu::Capabilities& caps,
                      const gfx::Size& size,
                      bool mipmap,
                      const gfx::ColorSpace& color_space);
    RenderPassBacking(const gfx::Size& size,
                      bool mipmap,
                      const gfx::ColorSpace& color_space);
    ~RenderPassBacking();
    RenderPassBacking(RenderPassBacking&&);
    RenderPassBacking& operator=(RenderPassBacking&&);
  };
  base::flat_map<RenderPassId, RenderPassBacking> render_pass_backings_;

  const DrawMode draw_mode_;

  // Get corresponding GrContext in DrawMode::GL or DrawMode::VULKAN. Returns
  // nullptr when there is no GrContext.
  GrContext* GetGrContext();
  bool is_using_ddl() const { return draw_mode_ == DrawMode::DDL; }
  bool is_using_vulkan() const { return draw_mode_ == DrawMode::VULKAN; }

  // Interface used for drawing. Common among different draw modes.
  sk_sp<SkSurface> root_surface_;
  sk_sp<SkSurface> non_root_surface_;
  SkCanvas* root_canvas_ = nullptr;
  SkCanvas* current_canvas_ = nullptr;
  SkSurface* current_surface_ = nullptr;

  bool disable_picture_quad_image_filtering_ = false;
  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;
  SkPaint current_paint_;

  // Specific for overdraw.
  sk_sp<SkSurface> overdraw_surface_;
  std::unique_ptr<SkCanvas> overdraw_canvas_;
  std::unique_ptr<SkNWayCanvas> nway_canvas_;

  // Specific for GL.
  ContextProvider* context_provider_ = nullptr;
  base::Optional<SyncQueryCollection> sync_queries_;
  bool use_swap_with_bounds_ = false;
  gfx::Rect swap_buffer_rect_;
  std::vector<gfx::Rect> swap_content_bounds_;

  // State common to all tile quads in a batch
  struct BatchedTileState {
    const SharedQuadState* shared_quad_state;
    gfx::Rect scissor_rect;
    gfx::QuadF draw_region;
    bool is_nearest_neighbor;
    bool has_scissor_rect;
    bool has_draw_region;
  };
  BatchedTileState batched_tile_state_;
  std::vector<SkCanvas::ImageSetEntry> batched_tiles_;

// Specific for Vulkan.
#if BUILDFLAG(ENABLE_VULKAN)
  VulkanContextProvider* vulkan_context_provider_ = nullptr;
#endif

  // Specific for SkDDL.
  SkiaOutputSurface* const skia_output_surface_ = nullptr;

  // Lock set for resources that are used for the current frame. All resources
  // in this set will be unlocked with a sync token when the frame is done in
  // the compositor thread. And the sync token will be released when the DDL
  // for the current frame is replayed on the GPU thread.
  // It is only used with DDL.
  DisplayResourceProvider::LockSetForExternalUse lock_set_for_external_use_;

  // Promise images created from resources used in the current frame. This map
  // will be cleared when the frame is done and before all resources in
  // |lock_set_for_external_use_| are unlocked on the compositor thread.
  // It is only used with DDL.
  base::flat_map<ResourceId, sk_sp<SkImage>> promise_images_;
  using YUVIds = std::tuple<ResourceId, ResourceId, ResourceId, ResourceId>;
  base::flat_map<YUVIds, sk_sp<SkImage>> yuv_promise_images_;

  // Specific for SkPRecord.
  std::unique_ptr<SkPictureRecorder> root_recorder_;
  sk_sp<SkPicture> root_picture_;
  sk_sp<SkPicture>* current_picture_;
  SkPictureRecorder* current_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SkiaRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
