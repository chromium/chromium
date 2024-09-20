// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_lost_reason.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_render_copy_results.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "media/gpu/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "ui/gfx/gpu_fence_handle.h"

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/chromeos/vulkan_overlay_adaptor.h"
#endif

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class ColorSpace;
}  // namespace gfx

namespace gl {
class GLSurface;
class Presenter;
}  // namespace gl

namespace gpu {
class DisplayCompositorMemoryAndTaskControllerOnGpu;
class SharedImageRepresentationFactory;
class SharedImageFactory;
class SyncPointClientState;
}  // namespace gpu

namespace skgpu::graphite {
class Context;
class Recording;
}  // namespace skgpu::graphite

namespace ui {
#if BUILDFLAG(IS_OZONE)
class PlatformWindowSurface;
#endif
}  // namespace ui

namespace viz {

class AsyncReadResultHelper;
class AsyncReadResultLock;
class ImageContextImpl;
class SkiaOutputSurfaceDependency;
class VulkanContextProvider;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// The SkiaOutputSurface implementation running on the GPU thread. This class
// should be created, used and destroyed on the GPU thread.
class SkiaOutputSurfaceImplOnGpu
    : public gpu::SharedContextState::ContextLostObserver {
 public:
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size,
                                   gfx::GpuFenceHandle release_fence)>;
  using BufferPresentedCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback& feedback)>;
  using ContextLostCallback = base::OnceClosure;

  using ScheduleGpuTaskCallback =
      base::RepeatingCallback<void(base::OnceClosure,
                                   std::vector<gpu::SyncToken>)>;

  using AddChildWindowToBrowserCallback =
      base::RepeatingCallback<void(gpu::SurfaceHandle child_window)>;

  // Callbacks will only be called via |deps->PostTaskToClientThread|.
  static std::unique_ptr<SkiaOutputSurfaceImplOnGpu> Create(
      SkiaOutputSurfaceDependency* deps,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      ScheduleGpuTaskCallback schedule_gpu_task,
      AddChildWindowToBrowserCallback parent_child_Window_to_browser_callback,
      SkiaOutputDevice::ReleaseOverlaysCallback release_overlays_callback);

  SkiaOutputSurfaceImplOnGpu(
      base::PassKey<SkiaOutputSurfaceImplOnGpu> pass_key,
      SkiaOutputSurfaceDependency* deps,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      ScheduleGpuTaskCallback schedule_gpu_task,
      AddChildWindowToBrowserCallback parent_child_window_to_browser_callback,
      SkiaOutputDevice::ReleaseOverlaysCallback release_overlays_callback);

  SkiaOutputSurfaceImplOnGpu(const SkiaOutputSurfaceImplOnGpu&) = delete;
  SkiaOutputSurfaceImplOnGpu& operator=(const SkiaOutputSurfaceImplOnGpu&) =
      delete;

  ~SkiaOutputSurfaceImplOnGpu() override;

  gpu::CommandBufferId command_buffer_id() const {
    return shared_gpu_deps_->command_buffer_id();
  }

  const OutputSurface::Capabilities& capabilities() const {
    return output_device_->capabilities();
  }
  const base::WeakPtr<SkiaOutputSurfaceImplOnGpu>& weak_ptr() const {
    return weak_ptr_;
  }

  void Reshape(const SkiaOutputDevice::ReshapeParams& params);
  void FinishPaintCurrentFrame(
      sk_sp<GrDeferredDisplayList> ddl,
      sk_sp<GrDeferredDisplayList> overdraw_ddl,
      std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
      std::vector<raw_ptr<ImageContextImpl, VectorExperimental>> image_contexts,
      std::vector<gpu::SyncToken> sync_tokens,
      base::OnceClosure on_finished,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb);
  void SwapBuffers(OutputSurfaceFrame frame);

  void SetDependenciesResolvedTimings(base::TimeTicks task_ready);
  void SetDrawTimings(base::TimeTicks task_ready);

  // Runs |deferred_framebuffer_draw_closure| when SwapBuffers() or CopyOutput()
  // will not.
  void SwapBuffersSkipped();
  void EnsureBackbuffer();
  void DiscardBackbuffer();
  // |update_rect| is in buffer space.
  // If is |is_overlay| is true, the ScopedWriteAccess will be saved and kept
  // open until PostSubmit().
  void FinishPaintRenderPass(
      const gpu::Mailbox& mailbox,
      sk_sp<GrDeferredDisplayList> ddl,
      sk_sp<GrDeferredDisplayList> overdraw_ddl,
      std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
      std::vector<raw_ptr<ImageContextImpl, VectorExperimental>> image_contexts,
      std::vector<gpu::SyncToken> sync_tokens,
      base::OnceClosure on_finished,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
      const gfx::Rect& update_rect,
      bool is_overlay);
  // Deletes resources for RenderPasses in |ids|. Also takes ownership of
  // |images_contexts| and destroys them on GPU thread.
  void RemoveRenderPassResource(
      std::vector<AggregatedRenderPassId> ids,
      std::vector<std::unique_ptr<ImageContextImpl>> image_contexts);
  void CopyOutput(const copy_output::RenderPassGeometry& geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request,
                  const gpu::Mailbox& mailbox);

  void BeginAccessImages(
      const std::vector<raw_ptr<ImageContextImpl, VectorExperimental>>&
          image_contexts,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);
  void ResetStateOfImages();
  void EndAccessImages(
      const base::flat_set<raw_ptr<ImageContextImpl, CtnExperimental>>&
          image_contexts);

  size_t max_resource_cache_bytes() const { return max_resource_cache_bytes_; }
  void ReleaseImageContexts(
      std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
          image_contexts);
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays);

  void SetVSyncDisplayID(int64_t display_id);

  void SetFrameRate(float frame_rate);

  bool was_context_lost() { return context_state_->context_lost(); }

  void SetCapabilitiesForTesting(
      const OutputSurface::Capabilities& capabilities);

  // gpu::SharedContextState::ContextLostObserver implementation:
  void OnContextLost() override;

#if BUILDFLAG(IS_WIN)
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window);
#endif
  const gpu::gles2::FeatureInfo* GetFeatureInfo() const;

  void PostTaskToClientThread(base::OnceClosure closure) {
    dependency_->PostTaskToClientThread(std::move(closure));
  }

  void ReadbackDone() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK_GT(num_readbacks_pending_, 0);
    num_readbacks_pending_--;
  }

  // Make context current for GL, and return false if the context is lost.
  // It will do nothing when Vulkan is used.
  bool MakeCurrent(bool need_framebuffer);

  void ReleaseFenceSync(uint64_t sync_fence_release);

  void PreserveChildSurfaceControls();

  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver);

  const scoped_refptr<AsyncReadResultLock> GetAsyncReadResultLock() const;

  void AddAsyncReadResultHelperWithLock(AsyncReadResultHelper* helper);
  void RemoveAsyncReadResultHelperWithLock(AsyncReadResultHelper* helper);

  void CreateSharedImage(gpu::Mailbox mailbox,
                         SharedImageFormat format,
                         const gfx::Size& size,
                         const gfx::ColorSpace& color_space,
                         SkAlphaType alpha_type,
                         gpu::SharedImageUsageSet usage,
                         std::string debug_label,
                         gpu::SurfaceHandle surface_handle);
  void CreateSolidColorSharedImage(gpu::Mailbox mailbox,
                                   const SkColor4f& color,
                                   const gfx::ColorSpace& color_space);
  void DestroySharedImage(gpu::Mailbox mailbox);
  void SetSharedImagePurgeable(const gpu::Mailbox& mailbox, bool purgeable);

#if BUILDFLAG(IS_ANDROID)
  // Called on the viz thread!
  base::ScopedClosureRunner GetCacheBackBufferCb();
#endif

  // Checks the relevant context for completed tasks and, indirectly, causes
  // associated completion callbacks to run.
  void CheckAsyncWorkCompletion();

  gpu::SharedContextState* context_state() const {
    return context_state_.get();
  }

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  void DetileOverlay(gpu::Mailbox input,
                     const gfx::Size& input_visible_size,
                     gpu::Mailbox output,
                     const gfx::RectF& display_rect,
                     const gfx::RectF& crop_rect,
                     gfx::OverlayTransform transform,
                     bool is_10bit);

  void CleanupImageProcessor();
#endif

  void ReadbackForTesting(
      CopyOutputRequest::CopyOutputRequestCallback result_callback);

 private:
  struct MailboxAccessData {
    MailboxAccessData();
    MailboxAccessData(MailboxAccessData&& other);
    MailboxAccessData& operator=(MailboxAccessData&& other);
    ~MailboxAccessData();

    gpu::Mailbox mailbox;
    std::unique_ptr<gpu::SkiaImageRepresentation> representation;
    std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>
        scoped_write;

    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
  };

  bool Initialize();
  bool InitializeForGL();
  bool InitializeForVulkan();
  bool InitializeForDawn();
  bool InitializeForMetal();

  // Provided as a callback to |device_|.
  void DidSwapBuffersCompleteInternal(gpu::SwapBuffersCompleteParams params,
                                      const gfx::Size& pixel_size,
                                      gfx::GpuFenceHandle release_fence);
  void ReleaseOverlays(const std::vector<gpu::Mailbox> overlays);

  DidSwapBufferCompleteCallback GetDidSwapBuffersCompleteCallback();
  SkiaOutputDevice::ReleaseOverlaysCallback GetReleaseOverlaysCallback();

  void MarkContextLost(ContextLostReason reason);

  void DestroyCopyOutputResourcesOnGpuThread(const gpu::Mailbox& mailbox);

  void SwapBuffersInternal(std::optional<OutputSurfaceFrame> frame);
  void PostSubmit(std::optional<OutputSurfaceFrame> frame);

  GrDirectContext* gr_context() const { return context_state_->gr_context(); }

  skgpu::graphite::Context* graphite_context() const {
    return context_state_->graphite_context();
  }

  skgpu::graphite::Recorder* graphite_recorder() const {
    return context_state_->gpu_main_graphite_recorder();
  }

  bool is_using_vulkan() const {
    return !!vulkan_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kVulkan;
  }

  bool is_using_gl() const {
    return gpu_preferences_.gr_context_type == gpu::GrContextType::kGL;
  }

  // Helper for `CopyOutput()` method, handles the RGBA format.
  void CopyOutputRGBA(SkSurface* surface,
                      copy_output::RenderPassGeometry geometry,
                      const gfx::ColorSpace& color_space,
                      const SkIRect& src_rect,
                      SkSurface::RescaleMode rescale_mode,
                      bool is_downscale_or_identity_in_both_dimensions,
                      std::unique_ptr<CopyOutputRequest> request);

  void CopyOutputRGBAInMemory(SkSurface* surface,
                              copy_output::RenderPassGeometry geometry,
                              const gfx::ColorSpace& color_space,
                              const SkIRect& src_rect,
                              SkSurface::RescaleMode rescale_mode,
                              bool is_downscale_or_identity_in_both_dimensions,
                              std::unique_ptr<CopyOutputRequest> request);

  void CopyOutputRGBAInTexture(SkSurface* surface,
                               copy_output::RenderPassGeometry geometry,
                               const gfx::ColorSpace& color_space,
                               const SkIRect& src_rect,
                               SkSurface::RescaleMode rescale_mode,
                               bool is_downscale_or_identity_in_both_dimensions,
                               std::unique_ptr<CopyOutputRequest> request);

  void CopyOutputNV12(SkSurface* surface,
                      copy_output::RenderPassGeometry geometry,
                      const gfx::ColorSpace& color_space,
                      const SkIRect& src_rect,
                      SkSurface::RescaleMode rescale_mode,
                      bool is_downscale_or_identity_in_both_dimensions,
                      std::unique_ptr<CopyOutputRequest> request);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods:
  std::unique_ptr<gpu::SkiaImageRepresentation>
  CreateSharedImageRepresentationSkia(SharedImageFormat format,
                                      const gfx::Size& size,
                                      const gfx::ColorSpace& color_space,
                                      std::string_view debug_label);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods, renders
  // |surface| into |dest_surface|'s canvas, cropping and scaling the results
  // appropriately. |source_selection| is the area of the |surface| that will be
  // rendered to the destination.
  void RenderSurface(SkSurface* surface,
                     const SkIRect& source_selection,
                     std::optional<SkVector> scaling,
                     bool is_downscale_or_identity_in_both_dimensions,
                     SkSurface* dest_surface,
                     gfx::Point destination_origin);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods, flushes writes
  // to |surface| with |end_semaphores| and |end_state|.
  bool FlushSurface(
      SkSurface* surface,
      std::vector<GrBackendSemaphore>& end_semaphores,
      gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
      GrGpuFinishedProc ganesh_finished_proc = nullptr,
      skgpu::graphite::GpuFinishedProc graphite_finished_proc = nullptr,
      void* finished_context = nullptr);

  // Begins access to the CopyOutputRequest destination shared image. If request
  // has `BlitRequest` then specified mailbox will be accessed. Otherwise a new
  // shared image to store the result will be allocated. `mailbox_access_data`
  // will be populated with information needed to access the texture if function
  // returns true.
  bool CreateDestinationImageIfNeededAndBeginAccess(
      CopyOutputRequest* request,
      gfx::Size intermediate_dst_size,
      const gfx::ColorSpace& color_space,
      MailboxAccessData& mailbox_access_data);

  // Helper, blends `BlendBitmap`s set on the |blit_request| over the |canvas|.
  // Used to implement handling of `CopyOutputRequest`s that contain
  // `BlitRequest`s.
  void BlendBitmapOverlays(SkCanvas* canvas, const BlitRequest& blit_request);

  // Schedules a task to check if any skia readback requests have completed
  // after a short delay. Will not schedule a task if there is already a
  // scheduled task or no readback requests are pending.
  void ScheduleCheckReadbackCompletion();

  // Checks if any skia readback requests have completed. If there are still
  // pending readback requests after checking then it will reschedule itself
  // after a short delay.
  void CheckReadbackCompletion();

  void ReleaseAsyncReadResultHelpers();

#if BUILDFLAG(ENABLE_VULKAN)
  // Creates a release fence. The semaphore is an external semaphore created
  // by CreateAndStoreExternalSemaphoreVulkan(). May destroy VkSemaphore that
  // the |semaphore| stores if creation of a release fence fails. In this case,
  // invalid fence handle is returned.
  gfx::GpuFenceHandle CreateReleaseFenceForVulkan(
      const GrBackendSemaphore& semaphore);
  // Returns true if succeess.
  bool CreateAndStoreExternalSemaphoreVulkan(
      std::vector<GrBackendSemaphore>& end_semaphores);
#endif
  gfx::GpuFenceHandle CreateReleaseFenceForGL();

  // Draws `overdraw_ddl` to the target `canvas`.
  void DrawOverdraw(sk_sp<GrDeferredDisplayList> overdraw_ddl,
                    SkCanvas& canvas);

  // Gets the cached SkiaImageRepresentation for this mailbox if it exists, or
  // returns a newly produced one and caches it.
  gpu::SkiaImageRepresentation* GetSkiaRepresentation(gpu::Mailbox mailbox);

  class ReleaseCurrent {
   public:
    ReleaseCurrent(scoped_refptr<gl::GLSurface> gl_surface,
                   scoped_refptr<gpu::SharedContextState> context_state);
    ~ReleaseCurrent();

   private:
    scoped_refptr<gl::GLSurface> gl_surface_;
    scoped_refptr<gpu::SharedContextState> context_state_;
  };

  // This must remain the first member variable to ensure that other member
  // dtors are called first.
  std::optional<ReleaseCurrent> release_current_last_;

  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  raw_ptr<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu> shared_gpu_deps_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  const raw_ptr<VulkanContextProvider> vulkan_context_provider_;
  const RendererSettings renderer_settings_;

  // Should only be run on the client thread with PostTaskToClientThread().
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  BufferPresentedCallback buffer_presented_callback_;
  ContextLostCallback context_lost_callback_;
  ScheduleGpuTaskCallback schedule_gpu_task_;
  AddChildWindowToBrowserCallback add_child_window_to_browser_callback_;
  SkiaOutputDevice::ReleaseOverlaysCallback release_overlays_callback_;

  // ImplOnGpu::CopyOutput can create SharedImages via ImplOnGpu's
  // SharedImageFactory. Clients can use these images via CopyOutputResult and
  // when done, release the resources by invoking the provided callback. If
  // ImplOnGpu is already destroyed, however, there is no way of running the
  // release callback from the client, so this vector holds all pending images
  // so resources can still be cleaned up in the dtor.
  std::vector<std::unique_ptr<gpu::SkiaImageRepresentation>>
      copy_output_images_;

  // Helper, creates a release callback for the passed in |representation|.
  ReleaseCallback CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
      std::unique_ptr<gpu::SkiaImageRepresentation> representation);

#if BUILDFLAG(IS_OZONE)
  // This should outlive gl_surface_ and vulkan_surface_.
  std::unique_ptr<ui::PlatformWindowSurface> window_surface_;
#endif

  const gpu::GpuPreferences gpu_preferences_;
  gfx::Size size_;
  // Only one of GLSurface of Presenter exists at the time.
  scoped_refptr<gl::GLSurface> gl_surface_;
  raw_ptr<gl::Presenter> presenter_ = nullptr;
  scoped_refptr<gpu::SharedContextState> context_state_;
  size_t max_resource_cache_bytes_ = 0u;

  bool context_is_lost_ = false;

  class PromiseImageAccessHelper {
   public:
    explicit PromiseImageAccessHelper(SkiaOutputSurfaceImplOnGpu* impl_on_gpu);

    PromiseImageAccessHelper(const PromiseImageAccessHelper&) = delete;
    PromiseImageAccessHelper& operator=(const PromiseImageAccessHelper&) =
        delete;

    ~PromiseImageAccessHelper();

    void BeginAccess(std::vector<raw_ptr<ImageContextImpl, VectorExperimental>>
                         image_contexts,
                     std::vector<GrBackendSemaphore>* begin_semaphores,
                     std::vector<GrBackendSemaphore>* end_semaphores);
    void EndAccess();

   private:
    const raw_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;
    base::flat_set<raw_ptr<ImageContextImpl, CtnExperimental>> image_contexts_;
  };
  PromiseImageAccessHelper promise_image_access_helper_{this};
  base::flat_set<raw_ptr<ImageContextImpl, CtnExperimental>>
      image_contexts_to_apply_end_state_;

  std::unique_ptr<SkiaOutputDevice> output_device_;
  std::unique_ptr<SkiaOutputDevice::ScopedPaint> scoped_output_device_paint_;

  // Cache of SkiaImageRepresentations for each render pass mailbox so we don't
  // need to recreate them if they are reused on future frames. Entries are
  // initialized to nullptr CreateSharedImage() and updated in
  // GetSkiaRepresentation(). They will be erased when calling
  // DestroySharedImage().
  base::flat_map<gpu::Mailbox, std::unique_ptr<gpu::SkiaImageRepresentation>>
      skia_representations_;

  // Overlayed render passes need to keep their write access open until after
  // submit. These will be set in FinishPaintRenderPass() if |is_overlay| is
  // true and destroyed in PostSubmit().
  base::flat_map<
      gpu::Mailbox,
      std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>>
      overlay_pass_accesses_;

  // Overlays are saved when ScheduleOverlays() is called, then passed to
  // |output_device_| in PostSubmit().
  SkiaOutputSurface::OverlayList overlays_;

  // Micro-optimization to get to issuing GPU SwapBuffers as soon as possible.
  std::vector<sk_sp<GrDeferredDisplayList>> destroy_after_swap_;

  bool waiting_for_full_damage_ = false;

  int num_readbacks_pending_ = 0;
  bool readback_poll_pending_ = false;

  // Lock for |async_read_result_helpers_|.
  scoped_refptr<AsyncReadResultLock> async_read_result_lock_;

  // Tracking for ongoing AsyncReadResults.
  base::flat_set<raw_ptr<AsyncReadResultHelper, CtnExperimental>>
      async_read_result_helpers_;

  // Pending release fence callbacks. These callbacks can be delayed if Vulkan
  // external semaphore type has copy transference, which means importing
  // semaphores has to be delayed until submission.
  base::circular_deque<std::pair<GrBackendSemaphore,
                       base::OnceCallback<void(gfx::GpuFenceHandle)>>>
      pending_release_fence_cbs_;

  // A cache of solid color image mailboxes so we can destroy them in the
  // destructor.
  base::flat_set<gpu::Mailbox> solid_color_images_;

  // The format that will be used to CreateSolidColorSharedImage(). This should
  // be either RGBA_8888 by default, or BGRA_8888 if the default is not
  // supported on Linux.
  SharedImageFormat solid_color_image_format_ = SinglePlaneFormat::kRGBA_8888;

  THREAD_CHECKER(thread_checker_);

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_CHROMEOS) && \
    BUILDFLAG(USE_V4L2_CODEC)
  std::unique_ptr<media::VulkanOverlayAdaptor> vulkan_overlay_adaptor_ =
      nullptr;
#endif

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImplOnGpu> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
