// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "cc/cc_export.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/sync_query_collection.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "ui/latency/latency_info.h"

class SkColorFilter;
class SkNWayCanvas;
class SkPictureRecorder;
class SkRuntimeColorFilterFactory;

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
class YUVVideoDrawQuad;

// TODO(795132): SkColorSpace is only a subset comparing to gfx::ColorSpace.
// Need to figure out support for color space that is not covered by
// SkColorSpace.
class VIZ_SERVICE_EXPORT SkiaRenderer : public DirectRenderer {
 public:
  // Different draw modes that are supported by SkiaRenderer right now.
  enum DrawMode { DDL, SKPRECORD };

  // TODO(penghuang): Remove skia_output_surface when DDL is used everywhere.
  SkiaRenderer(const RendererSettings* settings,
               OutputSurface* output_surface,
               DisplayResourceProvider* resource_provider,
               SkiaOutputSurface* skia_output_surface,
               DrawMode mode);
  ~SkiaRenderer() override;

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info) override;

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
  void CopyDrawnRenderPass(const copy_output::RenderPassGeometry& geometry,
                           std::unique_ptr<CopyOutputRequest> request) override;
#if defined(OS_WIN)
  void SetEnableDCLayers(bool enable) override;
#endif
  void DidChangeVisibility() override;
  void FinishDrawingQuadList() override;
  void GenerateMipmap() override;

 private:
  enum class BypassMode;
  struct DrawQuadParams;
  struct DrawRPDQParams;
  class ScopedSkImageBuilder;
  class ScopedYUVSkImageBuilder;

  void ClearCanvas(SkColor color);
  void ClearFramebuffer();

  // Callers should init an SkAutoCanvasRestore before calling this function.
  // |scissor_rect| and |rounded_corner_bounds| should be in device space,
  // i.e. same space that |cdt| will transform subsequent draws into.
  void PrepareCanvas(const base::Optional<gfx::Rect>& scissor_rect,
                     const base::Optional<gfx::RRectF>& rounded_corner_bounds,
                     const gfx::Transform* cdt);
  // Further modify the canvas as needed to apply the effects represented by
  // |rpdq_params|. Call Prepare[Paint|Color]OrCanvasForRPDQ when possible,
  // in order apply the RPDQ effects into a more efficient format.
  void PrepareCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                            DrawQuadParams* params);
  // Attempt to apply the effects in |rpdq_params| to the paint used to draw
  // the quad; otherwise modify the current canvas instead.
  void PreparePaintOrCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                   DrawQuadParams* params,
                                   SkPaint* paint);
  // Attempt to apply the effects in |rpdq_params| to the color used to draw
  // the quad; otherwise modify the current canvas as a fallback.
  void PrepareColorOrCanvasForRPDQ(const DrawRPDQParams& rpdq_params,
                                   DrawQuadParams* params,
                                   SkColor* color);

  // The returned DrawQuadParams can be modified by the DrawX calls that accept
  // params so that they can apply explicit data transforms before sending to
  // Skia in a consistent manner.
  DrawQuadParams CalculateDrawQuadParams(const gfx::Transform& target_to_device,
                                         const gfx::Rect* scissor_rect,
                                         const DrawQuad* quad,
                                         const gfx::QuadF* draw_region) const;
  DrawRPDQParams CalculateRPDQParams(const RenderPassDrawQuad* quad,
                                     DrawQuadParams* params);
  // Modifies |params| and |rpdq_params| to apply correctly when drawing the
  // RenderPass directly via |bypass_quad|.
  BypassMode CalculateBypassParams(const DrawQuad* bypass_quad,
                                   DrawRPDQParams* rpdq_params,
                                   DrawQuadParams* params) const;

  SkCanvas::ImageSetEntry MakeEntry(const SkImage* image,
                                    int matrix_index,
                                    const DrawQuadParams& params) const;
  // Returns overall constraint to pass to Skia, and modifies |params| to
  // emulate content area clamping different from the provided texture coords.
  SkCanvas::SrcRectConstraint ResolveTextureConstraints(
      const SkImage* image,
      const gfx::RectF& valid_texel_bounds,
      DrawQuadParams* params) const;

  bool MustFlushBatchedQuads(const DrawQuad* new_quad,
                             const DrawRPDQParams* rpdq_params,
                             const DrawQuadParams& params) const;
  void AddQuadToBatch(const SkImage* image,
                      const gfx::RectF& valid_texel_bounds,
                      DrawQuadParams* params);
  void FlushBatchedQuads();

  // Utility function that calls appropriate draw function based on quad
  // material. If |rpdq_params| is not null, then |quad| is assumed to be the
  // bypass quad associated with the RenderPass that defined the |rpdq_params|.
  void DrawQuadInternal(const DrawQuad* quad,
                        const DrawRPDQParams* rpdq_params,
                        DrawQuadParams* params);

  // Utility to draw a single quad as a filled color, and optionally apply the
  // effects defined in |rpdq_params| when the quad is bypassing the render pass
  void DrawColoredQuad(SkColor color,
                       const DrawRPDQParams* rpdq_params,
                       DrawQuadParams* params);
  // Utility to make a single ImageSetEntry and draw it with the complex paint,
  // and optionally apply the effects defined in |rpdq_params| when the quad is
  // bypassing the render pass
  void DrawSingleImage(const SkImage* image,
                       const gfx::RectF& valid_texel_bounds,
                       const DrawRPDQParams* rpdq_params,
                       SkPaint* paint,
                       DrawQuadParams* params);

  // RPDQ, DebugBorder and picture quads cannot be batched. They
  // either are not textures (debug, picture), or it's very likely
  // the texture will have advanced paint effects (rpdq). Additionally, they do
  // not support being drawn directly for a pass-through RenderPass.
  void DrawRenderPassQuad(const RenderPassDrawQuad* quad,
                          DrawQuadParams* params);
  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                           DrawQuadParams* params);
  void DrawPictureQuad(const PictureDrawQuad* quad, DrawQuadParams* params);

  // Solid-color quads are not batchable, but can be drawn directly in place of
  // a RenderPass (hence it takes the optional DrawRPDQParams).
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                          const DrawRPDQParams* rpdq_params,
                          DrawQuadParams* params);

  void DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                           const DrawRPDQParams* rpdq_params,
                           DrawQuadParams* params);
  void DrawTextureQuad(const TextureDrawQuad* quad,
                       const DrawRPDQParams* rpdq_params,
                       DrawQuadParams* params);
  void DrawTileDrawQuad(const TileDrawQuad* quad,
                        const DrawRPDQParams* rpdq_params,
                        DrawQuadParams* params);
  void DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                        const DrawRPDQParams* rpdq_params,
                        DrawQuadParams* params);

  void DrawUnsupportedQuad(const DrawQuad* quad,
                           const DrawRPDQParams* rpdq_params,
                           DrawQuadParams* params);

#if defined(OS_WIN)
  // Schedule overlay candidates for presentation at next SwapBuffers().
  void ScheduleDCLayers();
#endif

  // skia_renderer can draw most single-quad passes directly, regardless of
  // blend mode or image filtering.
  const DrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass) override;

  // Get corresponding GrContext. Returns nullptr when there is no GrContext.
  // TODO(weiliangc): This currently only returns nullptr. If SKPRecord isn't
  // going to use this later, it should be removed.
  GrContext* GetGrContext();
  bool is_using_ddl() const { return draw_mode_ == DrawMode::DDL; }

  sk_sp<SkColorFilter> GetColorFilter(const gfx::ColorSpace& src,
                                      const gfx::ColorSpace& dst,
                                      float resource_offset,
                                      float resource_multiplier);
  // A map from RenderPass id to the texture used to draw the RenderPass from.
  struct RenderPassBacking {
    sk_sp<SkSurface> render_pass_surface;
    gfx::Size size;
    bool generate_mipmap;
    gfx::ColorSpace color_space;
    ResourceFormat format;

    // Specific for SkPictureRecorder.
    std::unique_ptr<SkPictureRecorder> recorder;
    sk_sp<SkPicture> picture;

    RenderPassBacking(GrContext* gr_context,
                      const gpu::Capabilities& caps,
                      const gfx::Size& size,
                      bool generate_mipmap,
                      const gfx::ColorSpace& color_space);
    RenderPassBacking(const gfx::Size& size,
                      bool generate_mipmap,
                      const gfx::ColorSpace& color_space);
    ~RenderPassBacking();
    RenderPassBacking(RenderPassBacking&&);
    RenderPassBacking& operator=(RenderPassBacking&&);
  };
  base::flat_map<RenderPassId, RenderPassBacking> render_pass_backings_;

  const DrawMode draw_mode_;

  // Interface used for drawing. Common among different draw modes.
  sk_sp<SkSurface> root_surface_;
  SkCanvas* root_canvas_ = nullptr;
  SkCanvas* current_canvas_ = nullptr;
  SkSurface* current_surface_ = nullptr;
  class FrameResourceFence;
  scoped_refptr<FrameResourceFence> current_frame_resource_fence_;

  bool disable_picture_quad_image_filtering_ = false;
  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  // Specific for overdraw.
  sk_sp<SkSurface> overdraw_surface_;
  std::unique_ptr<SkCanvas> overdraw_canvas_;
  std::unique_ptr<SkNWayCanvas> nway_canvas_;

  // TODO(crbug.com/920344): Use partial swap for SkDDL.
  bool use_swap_with_bounds_ = false;
  gfx::Rect swap_buffer_rect_;
  std::vector<gfx::Rect> swap_content_bounds_;

  // State common to all quads in a batch. Draws that require an SkPaint not
  // captured by this state cannot be batched.
  struct BatchedQuadState {
    base::Optional<gfx::Rect> scissor_rect;
    base::Optional<gfx::RRectF> rounded_corner_bounds;
    SkBlendMode blend_mode;
    SkFilterQuality filter_quality;
    SkCanvas::SrcRectConstraint constraint;

    BatchedQuadState();
  };
  BatchedQuadState batched_quad_state_;
  std::vector<SkCanvas::ImageSetEntry> batched_quads_;
  // Same order as batched_quads_, but only includes draw regions for the
  // entries that have fHasClip == true. Each draw region is 4 consecutive pts
  std::vector<SkPoint> batched_draw_regions_;
  // Each entry of batched_quads_ will have an index into this vector; multiple
  // entries may point to the same matrix.
  std::vector<SkMatrix> batched_cdt_matrices_;

  // Specific for SkDDL.
  SkiaOutputSurface* const skia_output_surface_ = nullptr;

  // Lock set for resources that are used for the current frame. All resources
  // in this set will be unlocked with a sync token when the frame is done in
  // the compositor thread. And the sync token will be released when the DDL
  // for the current frame is replayed on the GPU thread.
  // It is only used with DDL.
  base::Optional<DisplayResourceProvider::LockSetForExternalUse>
      lock_set_for_external_use_;

  bool has_locked_overlay_resources_ = false;

  base::circular_deque<
      base::Optional<DisplayResourceProvider::ScopedReadLockSharedImage>>
      overlay_resource_locks_;

  // Specific for SkPRecord.
  std::unique_ptr<SkPictureRecorder> root_recorder_;
  sk_sp<SkPicture> root_picture_;
  sk_sp<SkPicture>* current_picture_;
  SkPictureRecorder* current_recorder_;
  ContextProvider* context_provider_ = nullptr;
  base::Optional<SyncQueryCollection> sync_queries_;

  std::map<
      gfx::ColorSpace,
      std::map<gfx::ColorSpace, std::unique_ptr<SkRuntimeColorFilterFactory>>>
      color_filter_cache_;

  DISALLOW_COPY_AND_ASSIGN(SkiaRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
