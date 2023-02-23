// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display_embedder/buffer_queue.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/latency/latency_info.h"

class SkColorFilter;

namespace viz {
class AggregatedRenderPassDrawQuad;
class DebugBorderDrawQuad;
class DelegatedInkPointRendererBase;
class DelegatedInkHandler;
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
  // TODO(penghuang): Remove skia_output_surface when DDL is used everywhere.
  SkiaRenderer(const RendererSettings* settings,
               const DebugRendererSettings* debug_settings,
               OutputSurface* output_surface,
               DisplayResourceProviderSkia* resource_provider,
               OverlayProcessorInterface* overlay_processor,
               SkiaOutputSurface* skia_output_surface);

  SkiaRenderer(const SkiaRenderer&) = delete;
  SkiaRenderer& operator=(const SkiaRenderer&) = delete;

  ~SkiaRenderer() override;

  void SwapBuffers(SwapFrameData swap_frame_data) override;
  void SwapBuffersSkipped() override;
  void SwapBuffersComplete(const gpu::SwapBuffersCompleteParams& params,
                           gfx::GpuFenceHandle release_fence) override;
  void BuffersPresented() override;
  void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) override;

  void SetDisablePictureQuadImageFiltering(bool disable) {
    disable_picture_quad_image_filtering_ = disable;
  }

  DelegatedInkPointRendererBase* GetDelegatedInkPointRenderer(
      bool create_if_necessary) override;
  void SetDelegatedInkMetadata(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) override;
  gfx::Rect GetCurrentFramebufferDamage() const override;
  void Reshape(const OutputSurface::ReshapeParams& reshape_params) override;
  void EnsureMinNumberOfBuffers(int n) override;
  gpu::Mailbox GetPrimaryPlaneOverlayTestingMailbox() override;

 protected:
  bool CanPartialSwap() override;
  void UpdateRenderPassTextures(
      const AggregatedRenderPassList& render_passes_in_draw_order,
      const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
          render_passes_in_frame) override;
  void AllocateRenderPassResourceIfNeeded(
      const AggregatedRenderPassId& render_pass_id,
      const RenderPassRequirements& requirements) override;
  bool IsRenderPassResourceAllocated(
      const AggregatedRenderPassId& render_pass_id) const override;
  gfx::Size GetRenderPassBackingPixelSize(
      const AggregatedRenderPassId& render_pass_id) override;
  void BindFramebufferToOutputSurface() override;
  void BindFramebufferToTexture(
      const AggregatedRenderPassId render_pass_id) override;
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
  void DidChangeVisibility() override;
  void FinishDrawingQuadList() override;
  void GenerateMipmap() override;
  void SetDelegatedInkPointRendererSkiaForTest(
      std::unique_ptr<DelegatedInkPointRendererSkia> renderer) override;

  std::unique_ptr<DelegatedInkHandler> delegated_ink_handler_;

 private:
  enum class BypassMode;
  struct DrawQuadParams;
  struct DrawRPDQParams;
  struct RenderPassOverlayParams;
  struct OverlayLock;
  class ScopedSkImageBuilder;
  class ScopedYUVSkImageBuilder;

  void ClearCanvas(SkColor4f color);
  void ClearFramebuffer();

  // Callers should init an SkAutoCanvasRestore before calling this function.
  // |scissor_rect| and |mask_filter_info| should be in device space,
  // i.e. same space that |cdt| will transform subsequent draws into.
  void PrepareCanvas(
      const absl::optional<gfx::Rect>& scissor_rect,
      const absl::optional<gfx::MaskFilterInfo>& mask_filter_info,
      const gfx::Transform* cdt);
  void PrepareGradient(
      const absl::optional<gfx::MaskFilterInfo>& mask_filter_info);

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
                                   SkColor4f* color);

  // The returned DrawQuadParams can be modified by the DrawX calls that accept
  // params so that they can apply explicit data transforms before sending to
  // Skia in a consistent manner.
  DrawQuadParams CalculateDrawQuadParams(
      const gfx::AxisTransform2d& target_to_device,
      const gfx::Rect* scissor_rect,
      const DrawQuad* quad,
      const gfx::QuadF* draw_region) const;

  DrawRPDQParams CalculateRPDQParams(const AggregatedRenderPassDrawQuad* quad,
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
  void DrawColoredQuad(SkColor4f color,
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

  void DrawPaintOpBuffer(const cc::PaintOpBuffer* buffer,
                         const absl::optional<SkColor4f>& clear_color,
                         const TileDrawQuad* quad,
                         const DrawQuadParams* params);

  // RPDQ, DebugBorder and picture quads cannot be batched. They
  // either are not textures (debug, picture), or it's very likely
  // the texture will have advanced paint effects (rpdq). Additionally, they do
  // not support being drawn directly for a pass-through RenderPass.
  void DrawRenderPassQuad(const AggregatedRenderPassDrawQuad* quad,
                          DrawQuadParams* params);
  void DrawDebugBorderQuad(const DebugBorderDrawQuad* quad,
                           DrawQuadParams* params);
  void DrawPictureQuad(const PictureDrawQuad* quad, DrawQuadParams* params);

  // Solid-color quads are not batchable, but can be drawn directly in place of
  // a RenderPass (hence it takes the optional DrawRPDQParams).
  void DrawSolidColorQuad(const SolidColorDrawQuad* quad,
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

  // Schedule overlay candidates for presentation at next SwapBuffers().
  void ScheduleOverlays();

  // skia_renderer can draw most single-quad passes directly, regardless of
  // blend mode or image filtering.
  const DrawQuad* CanPassBeDrawnDirectly(
      const AggregatedRenderPass* pass) override;

  const DrawQuad* CanPassBeDrawnDirectlyInternal(
      const AggregatedRenderPass* pass,
      bool* is_directly_drawable_with_single_rpdq);

  void DrawDelegatedInkTrail() override;

  // Get a color filter that converts from |src| color space to |dst| color
  // space using a shader constructed from gfx::ColorTransform.  The color
  // filters are cached in |color_filter_cache_|.  Resource offset and
  // multiplier are used to adjust the RGB output of the shader for YUV video
  // quads. The default values perform no adjustment.
  sk_sp<SkColorFilter> GetColorSpaceConversionFilter(
      const gfx::ColorSpace& src,
      absl::optional<uint32_t> src_bit_depth,
      absl::optional<gfx::HDRMetadata> src_hdr_metadata,
      const gfx::ColorSpace& dst,
      float resource_offset = 0.0f,
      float resource_multiplier = 1.0f);
  // Returns the color filter that should be applied to the current canvas.
  sk_sp<SkColorFilter> GetContentColorFilter();

  // Flush SkiaOutputSurface, so all pending GPU tasks in SkiaOutputSurface will
  // be sent to GPU scheduler.
  void FlushOutputSurface();

  struct RenderPassBacking {
    gfx::Size size;
    bool generate_mipmap;
    gfx::ColorSpace color_space;
    ResourceFormat format;
    gpu::Mailbox mailbox;
    bool is_root;
  };

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  bool CanSkipRenderPassOverlay(
      AggregatedRenderPassId render_pass_id,
      const AggregatedRenderPassDrawQuad* rpdq,
      RenderPassOverlayParams** output_render_pass_overlay);

  RenderPassOverlayParams* GetOrCreateRenderPassOverlayBacking(
      AggregatedRenderPassId render_pass_id,
      const AggregatedRenderPassDrawQuad* rpdq,
      ResourceFormat buffer_format,
      gfx::ColorSpace color_space,
      const gfx::Size& buffer_size);

  void PrepareRenderPassOverlay(
      OverlayProcessorInterface::PlatformOverlayCandidate* overlay);
#endif

  // Sets up callbacks for frame resource fences and passes them to
  // SkiaOutputSurface by calling EndPaint on that. If |failed|,
  // SkiaOutputSurface::EndPaint will be called with null callbacks.
  // |is_overlay| should be true for render passes that are scheduled as
  // overlays.
  void EndPaint(bool failed, bool is_overlay);

  DisplayResourceProviderSkia* resource_provider() {
    return static_cast<DisplayResourceProviderSkia*>(resource_provider_);
  }

#if BUILDFLAG(IS_OZONE)
  // Gets a cached or new mailbox for a 1x1 shared image of the specified color.
  // There will only be one allocated image for a given color at any time which
  // can be reused for same-colored quads in the same frame or across frames.
  const gpu::Mailbox GetImageMailboxForColor(const SkColor4f& color);

  // Append a viewport sized transparent solid color overlay to overlay_list if
  // capabilities().needs_background_image = true.
  void MaybeScheduleBackgroundImage(
      OverlayProcessorInterface::CandidateList& candidate_list);

  // Given locks that have either been swapped or skipped, if any correspond to
  // solid color mailboxes, decrement their use_count in |solid_color_buffers_|.
  // If capabilities().supports_non_backed_solid_color_overlays = true, there is
  // nothing to be done.
  void MaybeDecrementSolidColorBuffers(
      std::vector<OverlayLock>& finished_locks);
#endif

  // A map from RenderPass id to the texture used to draw the RenderPass from.
  base::flat_map<AggregatedRenderPassId, RenderPassBacking>
      render_pass_backings_;
  sk_sp<SkColorSpace> RenderPassBackingSkColorSpace(
      const RenderPassBacking& backing) {
    return backing.color_space.ToSkColorSpace(CurrentFrameSDRWhiteLevel());
  }

  // Interface used for drawing. Common among different draw modes.
  raw_ptr<SkCanvas> current_canvas_ = nullptr;

  class FrameResourceGpuCommandsCompletedFence;
  scoped_refptr<FrameResourceGpuCommandsCompletedFence>
      current_gpu_commands_completed_fence_;
  class FrameResourceReleaseFence;
  scoped_refptr<FrameResourceReleaseFence> current_release_fence_;

  bool disable_picture_quad_image_filtering_ = false;
  bool is_scissor_enabled_ = false;
  gfx::Rect scissor_rect_;

  gfx::Rect swap_buffer_rect_;

  // State common to all quads in a batch. Draws that require an SkPaint not
  // captured by this state cannot be batched.
  struct BatchedQuadState {
    absl::optional<gfx::Rect> scissor_rect;
    absl::optional<gfx::MaskFilterInfo> mask_filter_info;
    SkBlendMode blend_mode;
    SkSamplingOptions sampling;
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
  const raw_ptr<SkiaOutputSurface> skia_output_surface_;

  // Lock set for resources that are used for the current frame. All resources
  // in this set will be unlocked with a sync token when the frame is done in
  // the compositor thread. And the sync token will be released when the DDL
  // for the current frame is replayed on the GPU thread.
  // It is only used with DDL.
  absl::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;

  struct OverlayLock {
    OverlayLock(DisplayResourceProvider* resource_provider,
                ResourceId resource_id);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
    explicit OverlayLock(gpu::Mailbox mailbox);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

    ~OverlayLock();

    OverlayLock(OverlayLock&& other);
    OverlayLock& operator=(OverlayLock&& other);

    OverlayLock(const OverlayLock&) = delete;
    OverlayLock& operator=(const OverlayLock&) = delete;

    const gpu::Mailbox& mailbox() const {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
      if (render_pass_lock.has_value()) {
        return *render_pass_lock;
      }
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

      DCHECK(resource_lock.has_value());
      return resource_lock->mailbox();
    }

    const gpu::SyncToken& sync_token() const {
      DCHECK(resource_lock.has_value());
      return resource_lock->sync_token();
    }

    void SetReleaseFence(gfx::GpuFenceHandle release_fence) {
      if (resource_lock.has_value()) {
        resource_lock->SetReleaseFence(std::move(release_fence));
      }
    }

    bool HasReadLockFence() {
      if (resource_lock.has_value()) {
        return resource_lock->HasReadLockFence();
      }
      return false;
    }

    // Either resource_lock is set for non render pass overlays (i.e. videos),
    // or render_pass_lock is set for render pass overlays.
    absl::optional<DisplayResourceProviderSkia::ScopedReadLockSharedImage>
        resource_lock;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
    absl::optional<gpu::Mailbox> render_pass_lock;
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  };

  // Locks for overlays that are pending for SwapBuffers().
  base::circular_deque<std::vector<OverlayLock>> pending_overlay_locks_;

  // Locks for overlays that have been committed. |pending_overlay_locks_| will
  // be moved to |committed_overlay_locks_| after SwapBuffers() is completed.
  std::vector<OverlayLock> committed_overlay_locks_;

  // Locks for overlays that have release fences and read lock fences.
  base::circular_deque<std::vector<OverlayLock>>
      read_lock_release_fence_overlay_locks_;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)
  class OverlayLockComparator {
   public:
    using is_transparent = void;
    bool operator()(const OverlayLock& lhs, const OverlayLock& rhs) const;
  };

  // A set for locks of overlays which are waiting to be released, using
  // mailbox() as the unique key.
  base::flat_set<OverlayLock, OverlayLockComparator>
      awaiting_release_overlay_locks_;

  // Tracks RenderPassDrawQuad and render pass overlay backings that are
  // currently in use and available for re-using via mailboxes.
  // RenderPassBacking.generate_mipmap is not used.
  std::vector<RenderPassOverlayParams> in_flight_render_pass_overlay_backings_;
  std::vector<RenderPassOverlayParams> available_render_pass_overlay_backings_;

  // A feature flag that allows unchanged render pass draw quad in the overlay
  // list to skip.
  const bool can_skip_render_pass_overlay_;
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE)

  const bool is_using_raw_draw_;

  gfx::ColorConversionSkFilterCache color_filter_cache_;

  bool UsingSkiaForDelegatedInk() const;
  uint32_t debug_tint_modulate_count_ = 0;
  bool use_real_color_space_for_stream_video_ = false;

  // Used to get mailboxes for the root render pass when
  // capabilities().renderer_allocates_images = true.
  std::unique_ptr<BufferQueue> buffer_queue_;

#if BUILDFLAG(IS_OZONE)
  struct SolidColorBuffer {
    gpu::Mailbox mailbox;
    int use_count;
  };

  // Solid color buffers allocated on necessary platforms. The same image
  // can be reused for multiple same-color quads, and use count is tracked.
  // Entries will be erased and their SharedImages destroyed in the next
  // SwapBuffers() if their use_count reaches 0.
  // TODO(crbug.com/1342015): Move this to SkColor4f.
  base::flat_map<SkColor, SolidColorBuffer> solid_color_buffers_;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
