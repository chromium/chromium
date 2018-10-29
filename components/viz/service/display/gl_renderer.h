// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_H_

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/service/display/color_lut_cache.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/gl_renderer_copier.h"
#include "components/viz/service/display/gl_renderer_draw_cache.h"
#include "components/viz/service/display/program_binding.h"
#include "components/viz/service/display/scoped_gpu_memory_buffer_texture.h"
#include "components/viz/service/display/sync_query_collection.h"
#include "components/viz/service/display/texture_deleter.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/latency/latency_info.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {

class DynamicGeometryBinding;
class GLRendererShaderTest;
class OutputSurface;
class ScopedRenderPassTexture;
class StaticGeometryBinding;
class StreamVideoDrawQuad;
class TextureDrawQuad;

// Class that handles drawing of composited render layers using GL.
class VIZ_SERVICE_EXPORT GLRenderer : public DirectRenderer {
 public:
  class ScopedUseGrContext;

  GLRenderer(const RendererSettings* settings,
             OutputSurface* output_surface,
             DisplayResourceProvider* resource_provider,
             scoped_refptr<base::SingleThreadTaskRunner> current_task_runner);
  ~GLRenderer() override;

  bool use_swap_with_bounds() const { return use_swap_with_bounds_; }

  void SwapBuffers(std::vector<ui::LatencyInfo> latency_info,
                   bool need_presentation_feedback) override;
  void SwapBuffersComplete() override;

  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override;

  virtual bool IsContextLost();

 protected:
  void DidChangeVisibility() override;

  bool stencil_enabled() const { return stencil_shadow_; }

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
  void DoDrawQuad(const class DrawQuad*,
                  const gfx::QuadF* draw_region) override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  bool FlippedFramebuffer() const override;
  bool FlippedRootFramebuffer() const;
  void EnsureScissorTestEnabled() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(std::unique_ptr<CopyOutputRequest> request) override;
  void SetEnableDCLayers(bool enable) override;
  void FinishDrawingQuadList() override;
  void GenerateMipmap() override;

  // Returns true if quad requires antialiasing and false otherwise.
  static bool ShouldAntialiasQuad(const gfx::QuadF& device_layer_quad,
                                  bool clipped,
                                  bool force_aa);

  // Inflate the quad and fill edge array for fragment shader.
  // |local_quad| is set to inflated quad. |edge| array is filled with
  // inflated quad's edge data.
  static void SetupQuadForClippingAndAntialiasing(
      const gfx::Transform& device_transform,
      const DrawQuad* quad,
      const gfx::QuadF* device_layer_quad,
      const gfx::QuadF* clip_region,
      gfx::QuadF* local_quad,
      float edge[24]);
  static void SetupRenderPassQuadForClippingAndAntialiasing(
      const gfx::Transform& device_transform,
      const RenderPassDrawQuad* quad,
      const gfx::QuadF* device_layer_quad,
      const gfx::QuadF* clip_region,
      gfx::QuadF* local_quad,
      float edge[24]);

 private:
  friend class GLRendererCopierPixelTest;
  friend class GLRendererShaderPixelTest;
  friend class GLRendererShaderTest;
  friend class GLRendererTest;

  using OverlayResourceLock =
      std::unique_ptr<DisplayResourceProvider::ScopedReadLockGL>;
  using OverlayResourceLockList = std::vector<OverlayResourceLock>;

  // If a RenderPass is used as an overlay, we render the RenderPass with any
  // effects into a texture for overlay use. We must keep the texture alive past
  // the execution of SwapBuffers, and such textures are more expensive to make
  // so we want to reuse them.
  struct OverlayTexture {
    RenderPassId render_pass_id;
    ScopedGpuMemoryBufferTexture texture;
    int frames_waiting_for_reuse = 0;
  };

  struct DrawRenderPassDrawQuadParams;

  // Returns the format to use for storage if copying from the current
  // framebuffer. If the root renderpass is current, it uses the best matching
  // format from the OutputSurface, otherwise it uses the best matching format
  // from the texture being drawn to as the backbuffer.
  GLenum GetFramebufferCopyTextureFormat();
  void ReleaseRenderPassTextures();
  enum BoundGeometry { NO_BINDING, SHARED_BINDING, CLIPPED_BINDING };
  void PrepareGeometry(BoundGeometry geometry_to_bind);
  void SetStencilEnabled(bool enabled);
  void SetBlendEnabled(bool enabled);
  bool blend_enabled() const { return blend_shadow_; }

  // If any of the following functions returns false, then it means that drawing
  // is not possible.
  bool InitializeRPDQParameters(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQShadersForBlending(DrawRenderPassDrawQuadParams* params);
  bool UpdateRPDQWithSkiaFilters(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQTexturesForSampling(DrawRenderPassDrawQuadParams* params);
  void UpdateRPDQBlendMode(DrawRenderPassDrawQuadParams* params);
  void ChooseRPDQProgram(DrawRenderPassDrawQuadParams* params,
                         const gfx::ColorSpace& target_color_space);
  void UpdateRPDQUniforms(DrawRenderPassDrawQuadParams* params);
  void DrawRPDQ(const DrawRenderPassDrawQuadParams& params);

  static void ToGLMatrix(float* gl_matrix, const gfx::Transform& transform);

  void DiscardPixels();
  void ClearFramebuffer();
  void SetViewport();

  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad);
  static bool IsDefaultBlendMode(SkBlendMode blend_mode) {
    return blend_mode == SkBlendMode::kSrcOver;
  }
  bool CanApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode);
  void ApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode);
  void RestoreBlendFuncToDefault(SkBlendMode blend_mode);

  gfx::Rect GetBackdropBoundingBoxForRenderPassQuad(
      const RenderPassDrawQuad* quad,
      const gfx::Transform& contents_device_transform,
      const cc::FilterOperations* filters,
      const cc::FilterOperations* backdrop_filters,
      const gfx::QuadF* clip_region,
      bool use_aa,
      gfx::Rect* unclipped_rect);
  // Allocates and returns a texture id that contains a copy of the contents
  // of the current RenderPass being drawn.
  uint32_t GetBackdropTexture(const gfx::Rect& window_rect);

  static bool ShouldApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const cc::FilterOperations* backdrop_filters);
  sk_sp<SkImage> ApplyBackgroundFilters(
      const RenderPassDrawQuad* quad,
      const cc::FilterOperations& backdrop_filters,
      uint32_t background_texture,
      const gfx::Rect& rect,
      const gfx::Rect& unclipped_rect);

  const TileDrawQuad* CanPassBeDrawnDirectly(const RenderPass* pass) override;

  void DrawRenderPassQuad(const RenderPassDrawQuad* quadi,
                          const gfx::QuadF* clip_region);
  void DrawRenderPassQuadInternal(DrawRenderPassDrawQuadParams* params);
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                          const gfx::QuadF* clip_region);
  void DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                           const gfx::QuadF* clip_region);
  void EnqueueTextureQuad(const TextureDrawQuad* quad,
                          const gfx::QuadF* clip_region);
  void FlushTextureQuadCache(BoundGeometry flush_binding);
  void DrawTileQuad(const TileDrawQuad* quad, const gfx::QuadF* clip_region);
  void DrawContentQuad(const ContentDrawQuadBase* quad,
                       ResourceId resource_id,
                       const gfx::QuadF* clip_region);
  void DrawContentQuadAA(const ContentDrawQuadBase* quad,
                         ResourceId resource_id,
                         const gfx::Transform& device_transform,
                         const gfx::QuadF& aa_quad,
                         const gfx::QuadF* clip_region);
  void DrawContentQuadNoAA(const ContentDrawQuadBase* quad,
                           ResourceId resource_id,
                           const gfx::QuadF* clip_region);
  void DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                        const gfx::QuadF* clip_region);

  void SetShaderOpacity(float opacity);
  void SetShaderQuadF(const gfx::QuadF& quad);
  void SetShaderMatrix(const gfx::Transform& transform);
  void SetShaderColor(SkColor color, float opacity);
  void DrawQuadGeometryClippedByQuadF(const gfx::Transform& draw_transform,
                                      const gfx::RectF& quad_rect,
                                      const gfx::QuadF& clipping_region_quad,
                                      const float uv[8]);
  void DrawQuadGeometry(const gfx::Transform& projection_matrix,
                        const gfx::Transform& draw_transform,
                        const gfx::RectF& quad_rect);
  void DrawQuadGeometryWithAA(const DrawQuad* quad,
                              gfx::QuadF* local_quad,
                              const gfx::Rect& tile_rect);

  const gfx::QuadF& SharedGeometryQuad() const { return shared_geometry_quad_; }
  const StaticGeometryBinding* SharedGeometry() const {
    return shared_geometry_.get();
  }

  // If |dst_color_space| is invalid, then no color conversion (apart from
  // YUV to RGB conversion) is performed. This explicit argument is available
  // so that video color conversion can be enabled separately from general color
  // conversion.
  void SetUseProgram(const ProgramKey& program_key,
                     const gfx::ColorSpace& src_color_space,
                     const gfx::ColorSpace& dst_color_space);

  bool MakeContextCurrent();

  void InitializeSharedObjects();
  void CleanupSharedObjects();

  void ReinitializeGLState();
  void RestoreGLState();

  void ScheduleCALayers();
  void ScheduleDCLayers();
  void ScheduleOverlays();

  // Copies the contents of the render pass draw quad, including filter effects,
  // to a GL texture, returned in |overlay_texture|. The resulting texture may
  // be larger than the RenderPassDrawQuad's output, in order to reuse existing
  // textures. The new size and position is placed in |new_bounds|.
  void CopyRenderPassDrawQuadToOverlayResource(
      const CALayerOverlay* ca_layer_overlay,
      std::unique_ptr<OverlayTexture>* overlay_texture,
      gfx::RectF* new_bounds);
  std::unique_ptr<OverlayTexture> FindOrCreateOverlayTexture(
      const RenderPassId& render_pass_id,
      int width,
      int height,
      const gfx::ColorSpace& color_space);
  void ReduceAvailableOverlayTextures(
      const std::vector<std::unique_ptr<OverlayTexture>>& most_recent);

  // Schedules the |ca_layer_overlay|, which is guaranteed to have a non-null
  // |rpdq| parameter. Returns ownership of a GL texture that contains the
  // output of the RenderPassDrawQuad.
  std::unique_ptr<OverlayTexture> ScheduleRenderPassDrawQuad(
      const CALayerOverlay* ca_layer_overlay);

  // Setup/flush all pending overdraw feedback to framebuffer.
  void SetupOverdrawFeedback();
  void FlushOverdrawFeedback(const gfx::Rect& output_rect);
  // Process overdraw feedback from query.
  using OverdrawFeedbackCallback = base::Callback<void(unsigned, int)>;
  void ProcessOverdrawFeedback(std::vector<int>* overdraw,
                               size_t num_expected_results,
                               int max_result,
                               unsigned query,
                               int multiplier);

  ResourceFormat BackbufferFormat() const;

  // A map from RenderPass id to the texture used to draw the RenderPass from.
  base::flat_map<RenderPassId, ScopedRenderPassTexture> render_pass_textures_;

  // OverlayTextures that are free to be used in the next frame.
  std::vector<std::unique_ptr<OverlayTexture>> available_overlay_textures_;
  // OverlayTextures that have been set up for use but are waiting for
  // SwapBuffers.
  std::vector<std::unique_ptr<OverlayTexture>> awaiting_swap_overlay_textures_;
  // OverlayTextures that have been swapped for display on the gpu. Each vector
  // represents a single frame, and may be empty if none were used in that
  // frame. Ordered from oldest to most recent frame.
  std::vector<std::vector<std::unique_ptr<OverlayTexture>>>
      displayed_overlay_textures_;
  // OverlayTextures that we have replaced on the gpu but are awaiting
  // confirmation that we can reuse them.
  std::vector<std::unique_ptr<OverlayTexture>>
      awaiting_release_overlay_textures_;

  // Resources that have been sent to the GPU process, but not yet swapped.
  OverlayResourceLockList pending_overlay_resources_;
  // Resources that should be shortly swapped by the GPU process.
  base::circular_deque<OverlayResourceLockList> swapping_overlay_resources_;
  // Resources that the GPU process has finished swapping. The key is the
  // texture id of the resource.
  std::map<unsigned, OverlayResourceLock> swapped_and_acked_overlay_resources_;

  unsigned offscreen_framebuffer_id_ = 0u;

  std::unique_ptr<StaticGeometryBinding> shared_geometry_;
  std::unique_ptr<DynamicGeometryBinding> clipped_geometry_;
  gfx::QuadF shared_geometry_quad_;

  // This will return nullptr if the requested program has not yet been
  // initialized.
  const Program* GetProgramIfInitialized(const ProgramKey& key) const;

  std::unordered_map<ProgramKey, std::unique_ptr<Program>, ProgramKeyHash>
      program_cache_;

  const gfx::ColorTransform* GetColorTransform(const gfx::ColorSpace& src,
                                               const gfx::ColorSpace& dst);
  std::map<gfx::ColorSpace,
           std::map<gfx::ColorSpace, std::unique_ptr<gfx::ColorTransform>>>
      color_transform_cache_;

  gpu::gles2::GLES2Interface* gl_;
  gpu::ContextSupport* context_support_;
  std::unique_ptr<ContextCacheController::ScopedVisibility> context_visibility_;

  TextureDeleter texture_deleter_;
  GLRendererCopier copier_;

  gfx::Rect swap_buffer_rect_;
  std::vector<gfx::Rect> swap_content_bounds_;
  gfx::Rect scissor_rect_;
  bool is_scissor_enabled_ = false;
  bool stencil_shadow_ = false;
  bool blend_shadow_ = false;
  const Program* current_program_ = nullptr;
  TexturedQuadDrawCache draw_cache_;
  int highp_threshold_cache_ = 0;

  ScopedRenderPassTexture* current_framebuffer_texture_;

  SyncQueryCollection sync_queries_;
  bool use_discard_framebuffer_ = false;
  bool use_sync_query_ = false;
  bool use_blend_equation_advanced_ = false;
  bool use_blend_equation_advanced_coherent_ = false;
  bool use_occlusion_query_ = false;
  bool use_swap_with_bounds_ = false;

  // If true, tints all the composited content to red.
  bool tint_gl_composited_content_ = true;

  // The method FlippedFramebuffer determines whether the framebuffer associated
  // with a DrawingFrame is flipped. It makes the assumption that the
  // DrawingFrame is being used as part of a render pass. If a DrawingFrame is
  // not being used as part of a render pass, setting it here forces
  // FlippedFramebuffer to return |true|.
  bool force_drawing_frame_framebuffer_unflipped_ = false;

  BoundGeometry bound_geometry_;
  ColorLUTCache color_lut_cache_;

  unsigned offscreen_stencil_renderbuffer_id_ = 0;
  gfx::Size offscreen_stencil_renderbuffer_size_;

  unsigned num_triangles_drawn_ = 0;

  // This may be null if the compositor is run on a thread without a
  // MessageLoop.
  scoped_refptr<base::SingleThreadTaskRunner> current_task_runner_;
  base::WeakPtrFactory<GLRenderer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GLRenderer);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_GL_RENDERER_H_
