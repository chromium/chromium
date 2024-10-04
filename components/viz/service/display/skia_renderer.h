// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_

#include <memory>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display_embedder/buffer_queue.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/vulkan/buildflags.h"
#include "media/gpu/buildflags.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
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

// TODO(crbug.com/40554816): SkColorSpace is only a subset comparing to
// gfx::ColorSpace. Need to figure out support for color space that is not
// covered by SkColorSpace.
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

  void SetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id,
      const gfx::Rect& drawn_rect) override;

  gfx::Rect GetRenderPassBackingDrawnRect(
      const AggregatedRenderPassId& render_pass_id) const override;
  void SetScissorTestRect(const gfx::Rect& scissor_rect) override;
  void BeginDrawingRenderPass(const AggregatedRenderPass* render_pass,
                              bool needs_clear,
                              const gfx::Rect& render_pass_update_rect,
                              const gfx::Size& viewport_size) override;
  void DoDrawQuad(const DrawQuad* quad, const gfx::QuadF* draw_region) override;
  void FinishDrawingRenderPass() override;
  void BeginDrawingFrame() override;
  void FinishDrawingFrame() override;
  void EnsureScissorTestDisabled() override;
  void CopyDrawnRenderPass(const copy_output::RenderPassGeometry& geometry,
                           std::unique_ptr<CopyOutputRequest> request) override;
  void DidChangeVisibility() override;
  void SetDelegatedInkPointRendererSkiaForTest(
      std::unique_ptr<DelegatedInkPointRendererSkia> renderer) override;
  bool SupportsBGRA() const override;

  std::unique_ptr<DelegatedInkHandler> delegated_ink_handler_;

 private:
  enum class BypassMode;
  struct DrawQuadParams;
  struct DrawRPDQParams;
  struct RenderPassOverlayParams;
  struct OverlayLock;
  class ScopedSkImageBuilder;
  class VizDebuggerLog;

  void ClearCanvas(SkColor4f color);
  void ClearFramebuffer();

  // Callers should init an SkAutoCanvasRestore before calling this function.
  // |scissor_rect| and |mask_filter_info| should be in device space,
  // i.e. same space that |cdt| will transform subsequent draws into.
  void PrepareCanvas(const std::optional<gfx::Rect>& scissor_rect,
                     const std::optional<gfx::MaskFilterInfo>& mask_filter_info,
                     const gfx::Transform* cdt);
  void PrepareGradient(
      const std::optional<gfx::MaskFilterInfo>& mask_filter_info);

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
      const std::optional<gfx::Rect>& scissor_rect,
      const DrawQuad* quad,
      const gfx::QuadF* draw_region) const;

  DrawRPDQParams CalculateRPDQParams(
      const gfx::AxisTransform2d& target_to_device,
      const AggregatedRenderPassDrawQuad* quad,
      const DrawQuadParams* params);
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
                         const std::optional<SkColor4f>& clear_color,
                         const TileDrawQuad* quad,
                         const DrawQuadParams* params);

  // RenderPass draw quads can only be batch when they aren't bypassed and
  // don't have any advanced effects (eg. filter).
  void DrawRenderPassQuad(const AggregatedRenderPassDrawQuad* quad,
                          const DrawRPDQParams* bypassed_rpdq_params,
                          DrawQuadParams* params);

  // DebugBorder and picture quads cannot be batched since they are not
  // textures. Additionally, they do not support being drawn directly for a
  // pass-through RenderPass.
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

  void DrawUnsupportedQuad(const DrawQuad* quad,
                           const DrawRPDQParams* rpdq_params,
                           DrawQuadParams* params);

  // Schedule overlay candidates for presentation at next SwapBuffers().
  void ScheduleOverlays();

  // skia_renderer can draw most single-quad passes directly, regardless of
  // blend mode or image filtering.
  const DrawQuad* CanPassBeDrawnDirectly(
      const AggregatedRenderPass* pass,
      const RenderPassRequirements& requirements) override;

  void DrawDelegatedInkTrail(
      const gfx::Transform& root_target_to_render_pass_transform);

  // Get a color filter that converts from |src| color space to |dst| color
  // space using a shader constructed from gfx::ColorTransform.  The color
  // filters are cached in |color_filter_cache_|.
  sk_sp<SkColorFilter> GetColorSpaceConversionFilter(
      const gfx::ColorSpace& src,
      std::optional<uint32_t> src_bit_depth,
      std::optional<gfx::HDRMetadata> src_hdr_metadata,
      const gfx::ColorSpace& dst,
      bool is_video_frame);
  // Returns the color filter that should be applied to the current canvas.
  sk_sp<SkColorFilter> GetContentColorFilter();

  // Flush SkiaOutputSurface, so all pending GPU tasks in SkiaOutputSurface will
  // be sent to GPU scheduler.
  void FlushOutputSurface();

  struct RenderPassBacking {
    RenderPassBacking();
    RenderPassBacking(gfx::Size size,
                      bool generate_mipmap,
                      gfx::ColorSpace color_space,
                      RenderPassAlphaType alpha_type,
                      SharedImageFormat format,
                      gpu::Mailbox mailbox,
                      bool is_root,
                      bool is_scanout,
                      bool scanout_dcomp_surface);
    RenderPassBacking(const RenderPassBacking&);
    RenderPassBacking& operator=(const RenderPassBacking&);

    gfx::Size size;
    bool generate_mipmap = false;
    gfx::ColorSpace color_space;
    RenderPassAlphaType alpha_type = RenderPassAlphaType::kPremul;
    SharedImageFormat format;
    gpu::Mailbox mailbox;
    bool is_root = false;
    bool is_scanout = false;
    bool scanout_dcomp_surface = false;
    // This is the rect that has been drawn to this backing. It starts out as
    // empty and is expanded as drawing operations are made to this backing.
    gfx::Rect drawn_rect;
  };

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  bool CanSkipRenderPassOverlay(
      AggregatedRenderPassId render_pass_id,
      const AggregatedRenderPassDrawQuad* rpdq,
      RenderPassOverlayParams** output_render_pass_overlay);

  // Returns a |RenderPassBacking| whose mailbox can be scheduled directly as an
  // overlay.
  std::optional<SkiaRenderer::RenderPassBacking>
  GetRenderPassBackingForDirectScanout(
      const AggregatedRenderPassId& render_pass_id) const;

  RenderPassOverlayParams* GetOrCreateRenderPassOverlayBacking(
      AggregatedRenderPassId render_pass_id,
      const AggregatedRenderPassDrawQuad* rpdq,
      SharedImageFormat buffer_format,
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
  void EndPaint(const gfx::Rect& update_rect, bool failed, bool is_overlay);

  DisplayResourceProviderSkia* resource_provider() {
    return static_cast<DisplayResourceProviderSkia*>(resource_provider_);
  }

#if BUILDFLAG(IS_OZONE)
  // Append a viewport sized transparent solid color overlay to overlay_list if
  // capabilities().needs_background_image = true.
  void MaybeScheduleBackgroundImage(
      OverlayProcessorInterface::CandidateList& candidate_list);
#endif

  // A map from RenderPass id to the texture used to draw the RenderPass from.
  base::flat_map<AggregatedRenderPassId, RenderPassBacking>
      render_pass_backings_;
  sk_sp<SkColorSpace> RenderPassBackingSkColorSpace(
      const RenderPassBacking& backing) {
    return backing.color_space.GetWithSdrWhiteLevel(CurrentFrameSDRWhiteLevel())
        .ToSkColorSpace();
  }

  // Contains every render pass ID that this renderer has allocated. Values are
  // never evicted-- every 1 million entries takes up about 8MB space.
  // TODO(crbug.com/347909405): Remove this
  base::flat_set<AggregatedRenderPassId> seen_render_pass_ids_;

  // Interface used for drawing. Common among different draw modes.
  raw_ptr<SkCanvas> current_canvas_ = nullptr;

  class FrameResourceGpuCommandsCompletedFence;
  scoped_refptr<FrameResourceGpuCommandsCompletedFence>
      current_gpu_commands_completed_fence_;
  class FrameResourceReleaseFence;
  scoped_refptr<FrameResourceReleaseFence> current_release_fence_;

  // The rect for the current render pass containing pixels that we intend to
  // update this frame.
  // In the current render pass' backing's buffer space, contained by
  // |current_viewport_rect_|.
  gfx::Rect current_render_pass_update_rect_;

  // The scissor rect for the current draw. In the same coordinate space as and
  // contained by |current_render_pass_update_rect_|.
  std::optional<gfx::Rect> scissor_rect_;

  gfx::Rect swap_buffer_rect_;

  // State common to all quads in a batch. Draws that require an SkPaint not
  // captured by this state cannot be batched.
  struct BatchedQuadState {
    std::optional<gfx::Rect> scissor_rect;
    std::optional<gfx::MaskFilterInfo> mask_filter_info;
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

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  // Tracks RenderPassDrawQuad and render pass overlay backings that are
  // currently in use and available for re-using via mailboxes.
  // RenderPassBacking.generate_mipmap is not used.
  // Since OverlayLocks for render passes can refer to these, they must be
  // declared before anything owning OverlayLocks to ensure a safe destruction
  // order.
  std::vector<RenderPassOverlayParams> in_flight_render_pass_overlay_backings_;
  std::vector<RenderPassOverlayParams> available_render_pass_overlay_backings_;

  // A feature flag that allows unchanged render pass draw quad in the overlay
  // list to skip.
  const bool can_skip_render_pass_overlay_;
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

  // Lock set for resources that are used for the current frame. All resources
  // in this set will be unlocked with a sync token when the frame is done in
  // the compositor thread. And the sync token will be released when the DDL
  // for the current frame is replayed on the GPU thread.
  // It is only used with DDL.
  DisplayResourceProviderSkia::LockSetForExternalUse lock_set_for_external_use_;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  // A reference to an entry in |in_flight_render_pass_overlay_backings_|. If
  // this is the last reference, the backing will be moved to
  // |available_render_pass_overlay_backings_| on destruction.
  class ScopedInFlightRenderPassOverlayBackingRef {
   public:
    ScopedInFlightRenderPassOverlayBackingRef(SkiaRenderer* renderer,
                                              const gpu::Mailbox& mailbox);
    ~ScopedInFlightRenderPassOverlayBackingRef();

    ScopedInFlightRenderPassOverlayBackingRef(
        ScopedInFlightRenderPassOverlayBackingRef&& other);
    ScopedInFlightRenderPassOverlayBackingRef& operator=(
        ScopedInFlightRenderPassOverlayBackingRef&& other);

    ScopedInFlightRenderPassOverlayBackingRef(
        const ScopedInFlightRenderPassOverlayBackingRef&) = delete;
    ScopedInFlightRenderPassOverlayBackingRef& operator=(
        const ScopedInFlightRenderPassOverlayBackingRef&) = delete;

    const gpu::Mailbox& mailbox() const { return mailbox_; }

   private:
    void Reset();

    raw_ptr<SkiaRenderer> renderer_ = nullptr;

    // The mailbox of the |RenderPassOverlayParams|'s backing.
    gpu::Mailbox mailbox_;
  };
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

  struct OverlayLock {
    OverlayLock(DisplayResourceProvider* resource_provider,
                ResourceId resource_id);

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
    OverlayLock(SkiaRenderer* renderer, const gpu::Mailbox& mailbox);
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

    ~OverlayLock();

    OverlayLock(OverlayLock&& other);
    OverlayLock& operator=(OverlayLock&& other);

    OverlayLock(const OverlayLock&) = delete;
    OverlayLock& operator=(const OverlayLock&) = delete;

    const gpu::Mailbox& mailbox() const {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
      if (render_pass_lock.has_value()) {
        return render_pass_lock->mailbox();
      }
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)

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
    std::optional<DisplayResourceProviderSkia::ScopedReadLockSharedImage>
        resource_lock;

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
    std::optional<ScopedInFlightRenderPassOverlayBackingRef> render_pass_lock;
#endif  // BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_WIN)
  };

  // Locks for overlays that are pending for SwapBuffers().
  base::circular_deque<std::vector<OverlayLock>> pending_overlay_locks_;

  // Locks for overlays that have been committed. |pending_overlay_locks_| will
  // be moved to |committed_overlay_locks_| after SwapBuffers() is completed.
  std::vector<OverlayLock> committed_overlay_locks_;

  // Locks for overlays that have release fences and read lock fences.
  base::circular_deque<std::vector<OverlayLock>>
      read_lock_release_fence_overlay_locks_;

#if BUILDFLAG(IS_APPLE)
  struct OverlayLockHash {
    using is_transparent = void;
    std::size_t operator()(const OverlayLock& o) const;
    std::size_t operator()(const gpu::Mailbox& m) const;
  };

  struct OverlayLockKeyEqual {
    using is_transparent = void;
    bool operator()(const OverlayLock& lhs, const OverlayLock& rhs) const;
    bool operator()(const OverlayLock& lhs, const gpu::Mailbox& rhs) const;
  };

  // A set for locks of overlays which are waiting to be released, using
  // the mailbox() as the unique key.
  std::unordered_set<OverlayLock, OverlayLockHash, OverlayLockKeyEqual>
      awaiting_release_overlay_locks_;
#endif  // BUILDFLAG(IS_APPLE)

  const bool is_using_raw_draw_;

  gfx::ColorConversionSkFilterCache color_filter_cache_;

  // Returns true if we need to push a color conversion layer to correctly draw
  // |render_pass|'s contents.
  bool NeedsLayerForColorConversion(const AggregatedRenderPass* render_pass);

  // Returns the color space of the current draw layer, which may differ from
  // the render pass' color space.
  gfx::ColorSpace CurrentDrawLayerColorSpace() const;

  // A layer that may be pushed at the start of |BeginDrawingRenderPass| that is
  // the size of the pass' update rect. Drawing done inside this layer is in a
  // blend-friendly color space.
  std::optional<SkAutoCanvasRestore> hdr_color_conversion_layer_reset_;

  bool UsingSkiaForDelegatedInk() const;
  uint32_t debug_tint_modulate_count_ = 0;

  // Used to get mailboxes for the root render pass when
  // capabilities().renderer_allocates_images = true.
  std::unique_ptr<BufferQueue> buffer_queue_;

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  bool is_protected_pool_idle_ = true;
  std::unique_ptr<BufferQueue> protected_buffer_queue_ = nullptr;

  gpu::Mailbox GetProtectedSharedImage(bool is_10bit);
  void MaybeFreeProtectedPool();
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SKIA_RENDERER_H_
