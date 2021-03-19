// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "third_party/skia/include/core/SkDeferredDisplayListRecorder.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkSurfaceCharacterization.h"

namespace viz {

class ImageContextImpl;
class SkiaOutputSurfaceDependency;
class SkiaOutputSurfaceImplOnGpu;

// The SkiaOutputSurface implementation. It is the output surface for
// SkiaRenderer. It lives on the compositor thread, but it will post tasks
// to the GPU thread for initializing. Currently, SkiaOutputSurfaceImpl
// create a SkiaOutputSurfaceImplOnGpu on the GPU thread. It will be used
// for creating a SkSurface from the default framebuffer and providing the
// SkSurfaceCharacterization for the SkSurface. And then SkiaOutputSurfaceImpl
// will create SkDeferredDisplayListRecorder and SkCanvas for SkiaRenderer to
// render into. In SwapBuffers, it detaches a SkDeferredDisplayList from the
// recorder and plays it back on the framebuffer SkSurface on the GPU thread
// through SkiaOutputSurfaceImpleOnGpu.
class VIZ_SERVICE_EXPORT SkiaOutputSurfaceImpl : public SkiaOutputSurface {
 public:
  static std::unique_ptr<SkiaOutputSurface> Create(
      DisplayCompositorMemoryAndTaskController* display_controller,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings);

  SkiaOutputSurfaceImpl(
      base::PassKey<SkiaOutputSurfaceImpl> pass_key,
      DisplayCompositorMemoryAndTaskController* display_controller,
      const RendererSettings& renderer_settings,
      const DebugRendererSettings* debug_settings);
  ~SkiaOutputSurfaceImpl() override;

  // OutputSurface implementation:
  gpu::SurfaceHandle GetSurfaceHandle() const override;
  void BindToClient(OutputSurfaceClient* client) override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetEnableDCLayers(bool enable) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               bool use_stencil) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetGpuVSyncCallback(GpuVSyncCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gpu::Mailbox GetOverlayMailbox() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  unsigned UpdateGpuFence() override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  base::ScopedClosureRunner GetCacheBackBufferCb() override;
  gfx::Rect GetCurrentFramebufferDamage() const override;
  void SetFrameRate(float frame_rate) override;
  void SetNeedsMeasureNextDrawLatency() override;

  // SkiaOutputSurface implementation:
  SkCanvas* BeginPaintCurrentFrame() override;
  sk_sp<SkImage> MakePromiseSkImageFromYUV(
      const std::vector<ImageContext*>& contexts,
      sk_sp<SkColorSpace> image_color_space,
      SkYUVAInfo::PlaneConfig plane_config,
      SkYUVAInfo::Subsampling subsampling) override;
  void SwapBuffersSkipped(const gfx::Rect root_pass_damage_rect) override;
  void ScheduleOutputSurfaceAsOverlay(
      OverlayProcessorInterface::OutputSurfaceOverlayPlane output_surface_plane)
      override;

  SkCanvas* BeginPaintRenderPass(const AggregatedRenderPassId& id,
                                 const gfx::Size& surface_size,
                                 ResourceFormat format,
                                 bool mipmap,
                                 sk_sp<SkColorSpace> color_space) override;
  void EndPaint(base::OnceClosure on_finished) override;
  void MakePromiseSkImage(ImageContext* image_context) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const AggregatedRenderPassId& id,
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;

  void RemoveRenderPassResource(
      std::vector<AggregatedRenderPassId> ids) override;
  void ScheduleOverlays(OverlayList overlays,
                        std::vector<gpu::SyncToken> sync_tokens,
                        base::OnceClosure on_finished) override;

  void CopyOutput(AggregatedRenderPassId id,
                  const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;
  void PreserveChildSurfaceControls() override;
  gpu::SyncToken Flush() override;

#if defined(OS_APPLE)
  SkCanvas* BeginPaintRenderPassOverlay(
      const gfx::Size& size,
      ResourceFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) override;
  sk_sp<SkDeferredDisplayList> EndPaintRenderPassOverlay() override;
#endif

  // ExternalUseClient implementation:
  gpu::SyncToken ReleaseImageContexts(
      std::vector<std::unique_ptr<ImageContext>> image_contexts) override;
  std::unique_ptr<ExternalUseClient::ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      ResourceFormat format,
      bool maybe_concurrent_reads,
      const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space) override;

  // Set the fields of |capabilities_| and propagates to |impl_on_gpu_|. Should
  // be called after BindToClient().
  void SetCapabilitiesForTesting(gfx::SurfaceOrigin output_surface_origin);

  // Used in unit tests.
  void ScheduleGpuTaskForTesting(
      base::OnceClosure callback,
      std::vector<gpu::SyncToken> sync_tokens) override;

 private:
  bool Initialize();
  void InitializeOnGpuThread(GpuVSyncCallback vsync_callback_runner,
                             bool* result);
  SkSurfaceCharacterization CreateSkSurfaceCharacterization(
      const gfx::Size& surface_size,
      gfx::BufferFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space,
      bool is_root_render_pass);
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params,
                              const gfx::Size& pixel_size);
  void BufferPresented(const gfx::PresentationFeedback& feedback);

  // Provided as a callback for the GPU thread.
  void OnGpuVSync(base::TimeTicks timebase, base::TimeDelta interval);

  using GpuTask = base::OnceClosure;
  void EnqueueGpuTask(GpuTask task,
                      std::vector<gpu::SyncToken> sync_tokens,
                      bool make_current,
                      bool need_framebuffer);
  void FlushGpuTasks(bool wait_for_finish);
  GrBackendFormat GetGrBackendFormatForTexture(
      ResourceFormat resource_format,
      uint32_t gl_texture_target,
      const base::Optional<gpu::VulkanYCbCrInfo>& ycbcr_info);
  void ContextLost();

  void RecreateRootRecorder();

  OutputSurfaceClient* client_ = nullptr;
  bool needs_swap_size_notifications_ = false;

  // Images for current frame or render pass.
  std::vector<ImageContextImpl*> images_in_current_paint_;

  THREAD_CHECKER(thread_checker_);

  // Observers for context lost.
  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  uint64_t sync_fence_release_ = 0;
  SkiaOutputSurfaceDependency* dependency_;
  UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  GpuVSyncCallback gpu_vsync_callback_;
  bool is_displayed_as_overlay_ = false;
  gpu::Mailbox last_swapped_mailbox_;

  gfx::Size size_;
  gfx::ColorSpace color_space_;
  gfx::BufferFormat format_;
  bool is_hdr_ = false;
  SkSurfaceCharacterization characterization_;
  base::Optional<SkDeferredDisplayListRecorder> root_recorder_;

  class ScopedPaint {
   public:
    explicit ScopedPaint(SkDeferredDisplayListRecorder* root_recorder);
    explicit ScopedPaint(SkSurfaceCharacterization characterization);
    ScopedPaint(SkSurfaceCharacterization characterization,
                AggregatedRenderPassId render_pass_id);
    ~ScopedPaint();

    SkDeferredDisplayListRecorder* recorder() { return recorder_; }
    AggregatedRenderPassId render_pass_id() { return render_pass_id_; }

   private:
    // This is recorder being used for current paint
    SkDeferredDisplayListRecorder* recorder_;
    // If we need new recorder for this Paint (i.e it's not root render pass),
    // it's stored here
    base::Optional<SkDeferredDisplayListRecorder> recorder_storage_;
    const AggregatedRenderPassId render_pass_id_;
  };

  // This holds current paint info
  base::Optional<ScopedPaint> current_paint_;

  // The SkDDL recorder is used for overdraw feedback. It is created by
  // BeginPaintOverdraw, and FinishPaintCurrentFrame will turn it into a SkDDL
  // and play the SkDDL back on the GPU thread.
  base::Optional<SkDeferredDisplayListRecorder> overdraw_surface_recorder_;

  // |overdraw_canvas_| is used to record draw counts.
  base::Optional<SkOverdrawCanvas> overdraw_canvas_;

  // |nway_canvas_| contains |overdraw_canvas_| and root canvas.
  base::Optional<SkNWayCanvas> nway_canvas_;

  // The cache for promise image created from render passes.
  base::flat_map<AggregatedRenderPassId, std::unique_ptr<ImageContextImpl>>
      render_pass_image_cache_;

  // Sync tokens for resources which are used for the current frame or render
  // pass.
  std::vector<gpu::SyncToken> resource_sync_tokens_;

  const RendererSettings renderer_settings_;

  // Points to the viz-global singleton.
  const DebugRendererSettings* const debug_settings_;

  // For testing cases we would need to setup a SkiaOutputSurface without
  // OverlayProcessor and Display. For those cases, we hold the gpu task
  // scheduler inside this class by having a unique_ptr.
  // TODO(weiliangc): After changing to proper initialization order for Android
  // WebView, remove this holder.
  DisplayCompositorMemoryAndTaskController* display_compositor_controller_;

  // |gpu_task_scheduler_| holds a gpu::SingleTaskSequence, and helps schedule
  // tasks on GPU as a single sequence. It is shared with OverlayProcessor so
  // compositing and overlay processing are in order. A gpu::SingleTaskSequence
  // in regular Viz is implemented by SchedulerSequence. In Android WebView
  // gpu::SingleTaskSequence is implemented on top of WebView's task queue.
  gpu::GpuTaskSchedulerHelper* gpu_task_scheduler_;

  // The display transform relative to the hardware natural orientation,
  // applied to the frame content. The transform can be rotations in 90 degree
  // increments or flips.
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // |impl_on_gpu| is created and destroyed on the GPU thread by a posted task
  // from SkiaOutputSurfaceImpl::Initialize and SkiaOutputSurfaceImpl::dtor. So
  // it's safe to use base::Unretained for posting tasks during life time of
  // SkiaOutputSurfaceImpl.
  std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;

  sk_sp<GrContextThreadSafeProxy> gr_context_thread_safe_;

  bool has_set_draw_rectangle_for_frame_ = false;
  base::Optional<gfx::Rect> draw_rectangle_;

  bool should_measure_next_post_task_ = false;

  // GPU tasks pending for flush.
  std::vector<GpuTask> gpu_tasks_;
  // GPU sync tokens which are depended by |gpu_tasks_|.
  std::vector<gpu::SyncToken> gpu_task_sync_tokens_;
  // True if _any_ of |gpu_tasks_| need a GL context.
  bool make_current_ = false;
  // True if _any_ of |gpu_tasks_| need to access the framebuffer.
  bool need_framebuffer_ = false;

  bool use_damage_area_from_skia_output_device_ = false;
  // Damage area of the current buffer. Differ to the last submit buffer.
  base::Optional<gfx::Rect> damage_of_current_buffer_;
  // Current buffer index.
  size_t current_buffer_ = 0;
  // Accumulates framebuffer damage since last drawing to a particular buffer.
  // There is one gfx::Rect per framebuffer.
  std::vector<gfx::Rect> accumulated_buffer_damage_;
  // Track if the current buffer content is changed.
  bool current_buffer_modified_ = false;

  base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
