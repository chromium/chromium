// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/display_compositor_memory_and_task_controller.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayListRecorder.h"
#include "third_party/skia/include/private/chromium/GrSurfaceCharacterization.h"
#include "ui/gfx/presentation_feedback.h"

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
}  // namespace gfx

namespace gpu {
class SharedImageRepresentationFactory;
struct SwapBuffersCompleteParams;
}  // namespace gpu

namespace gpu::raster {
class GraphiteCacheController;
}  // namespace gpu::raster

namespace skgpu::graphite {
class Recorder;
class Recording;
}  // namespace skgpu::graphite

namespace viz {

class ImageContextImpl;
class SkiaOutputSurfaceDependency;
class SkiaOutputSurfaceImplOnGpu;

// The SkiaOutputSurface implementation. It is the output surface for
// SkiaRenderer. It lives on the compositor thread, but it will post tasks
// to the GPU thread for initializing. Currently, SkiaOutputSurfaceImpl
// create a SkiaOutputSurfaceImplOnGpu on the GPU thread. It will be used
// for creating a SkSurface from the default framebuffer and providing the
// GrSurfaceCharacterization for the SkSurface. And then SkiaOutputSurfaceImpl
// will create GrDeferredDisplayListRecorder and SkCanvas for SkiaRenderer to
// render into. In SwapBuffers, it detaches a GrDeferredDisplayList from the
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

  SkiaOutputSurfaceImpl(const SkiaOutputSurfaceImpl&) = delete;
  SkiaOutputSurfaceImpl& operator=(const SkiaOutputSurfaceImpl&) = delete;

  // OutputSurface implementation:
  gpu::SurfaceHandle GetSurfaceHandle() const override;
  void BindToClient(OutputSurfaceClient* client) override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SetEnableDCLayers(bool enable) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const ReshapeParams& params) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetGpuVSyncEnabled(bool enabled) override;
  void SetGpuVSyncCallback(GpuVSyncCallback callback) override;
  void SetVSyncDisplayID(int64_t display_id) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  bool IsDisplayedAsOverlayPlane() const override;
  gpu::Mailbox GetOverlayMailbox() const override;
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
                                 SharedImageFormat format,
                                 bool mipmap,
                                 bool scanout_dcomp_surface,
                                 sk_sp<SkColorSpace> color_space,
                                 bool is_overlay,
                                 const gpu::Mailbox& mailbox) override;
  SkCanvas* RecordOverdrawForCurrentPaint() override;
  void EndPaint(
      base::OnceClosure on_finished,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
      const gfx::Rect& update_rect,
      bool is_overlay) override;
  void MakePromiseSkImage(ImageContext* image_context,
                          const gfx::ColorSpace& yuv_color_space) override;
  sk_sp<SkImage> MakePromiseSkImageFromRenderPass(
      const AggregatedRenderPassId& id,
      const gfx::Size& size,
      SharedImageFormat format,
      bool mipmap,
      sk_sp<SkColorSpace> color_space,
      const gpu::Mailbox& mailbox) override;

  void RemoveRenderPassResource(
      std::vector<AggregatedRenderPassId> ids) override;
  void ScheduleOverlays(OverlayList overlays,
                        std::vector<gpu::SyncToken> sync_tokens) override;

  void CopyOutput(const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request,
                  const gpu::Mailbox& mailbox) override;
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;
  void PreserveChildSurfaceControls() override;
  gpu::SyncToken Flush() override;
  bool EnsureMinNumberOfBuffers(int n) override;
  gpu::Mailbox CreateSharedImage(SharedImageFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 uint32_t usage,
                                 base::StringPiece debug_label,
                                 gpu::SurfaceHandle surface_handle) override;
  gpu::Mailbox CreateSolidColorSharedImage(
      const SkColor4f& color,
      const gfx::ColorSpace& color_space) override;
  void DestroySharedImage(const gpu::Mailbox& mailbox) override;
  bool SupportsBGRA() const override;

  // ExternalUseClient implementation:
  gpu::SyncToken ReleaseImageContexts(
      std::vector<std::unique_ptr<ImageContext>> image_contexts) override;
  std::unique_ptr<ExternalUseClient::ImageContext> CreateImageContext(
      const gpu::MailboxHolder& holder,
      const gfx::Size& size,
      SharedImageFormat format,
      bool maybe_concurrent_reads,
      const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
      sk_sp<SkColorSpace> color_space,
      bool raw_draw_if_possible) override;

  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver) override;

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
  GrSurfaceCharacterization CreateGrSurfaceCharacterizationRenderPass(
      const gfx::Size& surface_size,
      SkColorType color_type,
      SkAlphaType alpha_type,
      bool mipmap,
      sk_sp<SkColorSpace> color_space,
      bool is_overlay,
      bool scanout_dcomp_surface) const;
  GrSurfaceCharacterization CreateGrSurfaceCharacterizationCurrentFrame(
      const gfx::Size& surface_size,
      SkColorType color_type,
      SkAlphaType alpha_type,
      bool mipmap,
      sk_sp<SkColorSpace> color_space) const;
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params,
                              const gfx::Size& pixel_size,
                              gfx::GpuFenceHandle release_fence);
  void BufferPresented(const gfx::PresentationFeedback& feedback);
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window);

  // Provided as a callback for the GPU thread.
  void OnGpuVSync(base::TimeTicks timebase, base::TimeDelta interval);

  using GpuTask = base::OnceClosure;
  void EnqueueGpuTask(GpuTask task,
                      std::vector<gpu::SyncToken> sync_tokens,
                      bool make_current,
                      bool need_framebuffer);
  void ScheduleOrRetainGpuTask(base::OnceClosure callback,
                               std::vector<gpu::SyncToken> tokens);

  enum class SyncMode {
    kNoWait = 0,
    kWaitForTasksStarted = 1,
    kWaitForTasksFinished = 2,
  };
  void FlushGpuTasks(SyncMode sync_mode);
  // When flushing the final task to destroy |impl_on_gpu_| we need to pass in a
  // copy of that pointer for any tasks that were already enqueued and will run
  // before the destructor.
  void FlushGpuTasksWithImpl(SyncMode sync_mode,
                             SkiaOutputSurfaceImplOnGpu* impl_on_gpu);
  GrBackendFormat GetGrBackendFormatForTexture(
      SharedImageFormat si_format,
      int plane_index,
      uint32_t gl_texture_target,
      const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info);
  void MakePromiseSkImageSinglePlane(ImageContextImpl* image_context,
                                     bool mipmapped);
  void MakePromiseSkImageMultiPlane(ImageContextImpl* image_context,
                                    const gfx::ColorSpace& yuv_color_space);
  void ContextLost();
  void RecreateRootDDLRecorder();

  raw_ptr<OutputSurfaceClient> client_ = nullptr;
  bool needs_swap_size_notifications_ = false;

  // Images for current frame or render pass.
  std::vector<ImageContextImpl*> images_in_current_paint_;

  THREAD_CHECKER(thread_checker_);

  // Observers for context lost.
  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  uint64_t sync_fence_release_ = 0;
  raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  UpdateVSyncParametersCallback update_vsync_parameters_callback_;
  GpuVSyncCallback gpu_vsync_callback_;
  bool is_displayed_as_overlay_ = false;
  gpu::Mailbox last_swapped_mailbox_;

  gfx::Size size_;
  SharedImageFormat format_;
  int sample_count_ = 1;
  SkColorType color_type_ = kUnknown_SkColorType;
  SkAlphaType alpha_type_ = kUnknown_SkAlphaType;
  sk_sp<SkColorSpace> sk_color_space_;
  bool reset_ddl_recorder_on_swap_ = false;
  absl::optional<GrDeferredDisplayListRecorder> root_ddl_recorder_;

  class ScopedPaint {
   public:
    // Ganesh root surface
    explicit ScopedPaint(GrDeferredDisplayListRecorder* root_ddl_recorder);
    // Ganesh render pass (root or non-root)
    ScopedPaint(const GrSurfaceCharacterization& characterization,
                const gpu::Mailbox& mailbox);
    // Graphite (root or non-root)
    ScopedPaint(skgpu::graphite::Recorder* recorder,
                const SkImageInfo& image_info,
                skgpu::graphite::TextureInfo texture_info,
                const gpu::Mailbox& mailbox = gpu::Mailbox());
    ~ScopedPaint();

    // SkCanvas for the current paint, retrieved from the DDL recorder for
    // Ganesh, or from the Graphite recorder.
    SkCanvas* canvas() const { return canvas_; }

    // Mailbox for the render pass for the current paint (if present).
    const gpu::Mailbox& mailbox() const { return mailbox_; }

    // Detach DDL and reset the SkCanvas pointer.
    sk_sp<GrDeferredDisplayList> DetachDDL();

    // Snap Graphite recording and reset the SkCanvas pointer.
    std::unique_ptr<skgpu::graphite::Recording> SnapRecording();

   private:
    // This is the DDL recorder being used for current paint when using Ganesh.
    raw_ptr<GrDeferredDisplayListRecorder> ddl_recorder_ = nullptr;
    // If we need new recorder for this Paint (i.e. it's not root render pass),
    // it's stored here
    absl::optional<GrDeferredDisplayListRecorder> ddl_recorder_storage_;
    // Graphite recorder used for current paint.
    raw_ptr<skgpu::graphite::Recorder> graphite_recorder_ = nullptr;
    // SkCanvas for the current paint, retrieved from the DDL recorder for
    // Ganesh, or from the Graphite recorder.
    raw_ptr<SkCanvas> canvas_ = nullptr;
    // Mailbox for the render pass for the current paint (if present).
    const gpu::Mailbox mailbox_;
  };

  // Tracks damage across at most `number_of_buffers`. Note this implementation
  // only assumes buffers are used in a circular order, and does not require a
  // fixed number of frame buffers to be allocated.
  class FrameBufferDamageTracker {
   public:
    explicit FrameBufferDamageTracker(size_t number_of_buffers);
    ~FrameBufferDamageTracker();

    void FrameBuffersChanged(const gfx::Size& frame_buffer_size);
    void SwappedWithDamage(const gfx::Rect& damage);
    void SkippedSwapWithDamage(const gfx::Rect& damage);
    gfx::Rect GetCurrentFrameBufferDamage() const;

   private:
    gfx::Rect ComputeCurrentFrameBufferDamage() const;

    size_t number_of_buffers_;
    gfx::Size frame_buffer_size_;
    // This deque should contains the incremental damage of the last N swapped
    // frames where N is at most `number_of_buffers_`. Each rect represents
    // from the incremental damage from the previous frame; note if there is no
    // previous frame (eg first swap after a `Reshape`), the damage should be
    // the full frame buffer.
    base::circular_deque<gfx::Rect> damage_between_frames_;
    // Result of `GetCurrentFramebufferDamage` to optimize consecutive calls.
    mutable absl::optional<gfx::Rect> cached_current_damage_;
  };

  // This holds current paint info
  absl::optional<ScopedPaint> current_paint_;

  // The SkDDL recorder is used for overdraw feedback. It is created by
  // BeginPaintOverdraw, and FinishPaintCurrentFrame will turn it into a SkDDL
  // and play the SkDDL back on the GPU thread.
  absl::optional<GrDeferredDisplayListRecorder> overdraw_surface_ddl_recorder_;

  // |overdraw_canvas_| is used to record draw counts.
  absl::optional<SkOverdrawCanvas> overdraw_canvas_;

  // |nway_canvas_| contains |overdraw_canvas_| and root canvas.
  absl::optional<SkNWayCanvas> nway_canvas_;

  // The cache for promise image created from render passes.
  base::flat_map<AggregatedRenderPassId, std::unique_ptr<ImageContextImpl>>
      render_pass_image_cache_;

  // Sync tokens for resources which are used for the current frame or render
  // pass.
  std::vector<gpu::SyncToken> resource_sync_tokens_;

  const RendererSettings renderer_settings_;

  // Points to the viz-global singleton.
  const raw_ptr<const DebugRendererSettings> debug_settings_;

  // For testing cases we would need to setup a SkiaOutputSurface without
  // OverlayProcessor and Display. For those cases, we hold the gpu task
  // scheduler inside this class by having a unique_ptr.
  // TODO(weiliangc): After changing to proper initialization order for Android
  // WebView, remove this holder.
  const raw_ptr<DisplayCompositorMemoryAndTaskController>
      display_compositor_controller_;

  // |gpu_task_scheduler_| holds a gpu::SingleTaskSequence, and helps schedule
  // tasks on GPU as a single sequence. It is shared with OverlayProcessor so
  // compositing and overlay processing are in order. A gpu::SingleTaskSequence
  // in regular Viz is implemented by SchedulerSequence. In Android WebView
  // gpu::SingleTaskSequence is implemented on top of WebView's task queue.
  const raw_ptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler_;

  // True if raw draw is being used.
  const bool is_using_raw_draw_;

  // True if raw draw is using MSAA output surface.
  const bool is_raw_draw_using_msaa_;

  // The display transform relative to the hardware natural orientation,
  // applied to the frame content. The transform can be rotations in 90 degree
  // increments or flips.
  gfx::OverlayTransform display_transform_ = gfx::OVERLAY_TRANSFORM_NONE;

  // |impl_on_gpu| is created and destroyed on the GPU thread by a posted task
  // from SkiaOutputSurfaceImpl::Initialize and SkiaOutputSurfaceImpl::dtor. So
  // it's safe to use base::Unretained for posting tasks during life time of
  // SkiaOutputSurfaceImpl.
  std::unique_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;

  gpu::GrContextType gr_context_type_ = gpu::GrContextType::kGL;
  sk_sp<GrContextThreadSafeProxy> gr_context_thread_safe_;
  raw_ptr<skgpu::graphite::Recorder> graphite_recorder_ = nullptr;
  scoped_refptr<gpu::raster::GraphiteCacheController>
      graphite_cache_controller_;

  bool has_set_draw_rectangle_for_frame_ = false;
  absl::optional<gfx::Rect> draw_rectangle_;

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
  absl::optional<gfx::Rect> damage_of_current_buffer_;

  // Used when `use_damage_area_from_skia_output_device_` is false and keeps
  // track of across multiple frame buffers. Can be nullptr.
  absl::optional<FrameBufferDamageTracker> frame_buffer_damage_tracker_;

  // Track if the current buffer content is changed.
  bool current_buffer_modified_ = false;

  // Last number sent to `SetNumberOfFrameBuffers` on the GPU.
  int cached_number_of_buffers_ = 0;

  // For accessing tile shared image backings from compositor thread.
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      representation_factory_;
  // The refresh interval from presentation feedback.
  base::TimeDelta refresh_interval_;

  base::WeakPtr<SkiaOutputSurfaceImpl> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImpl> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_H_
