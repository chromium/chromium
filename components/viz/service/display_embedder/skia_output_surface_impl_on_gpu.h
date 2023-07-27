// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_

#include <memory>
#include <string>
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
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "ui/gfx/gpu_fence_handle.h"

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
class DawnContextProvider;
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
    : public gpu::ImageTransportSurfaceDelegate,
      public gpu::SharedContextState::ContextLostObserver {
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

  // |gpu_vsync_callback| must be safe to call on any thread. The other
  // callbacks will only be called via |deps->PostTaskToClientThread|.
  static std::unique_ptr<SkiaOutputSurfaceImplOnGpu> Create(
      SkiaOutputSurfaceDependency* deps,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      ScheduleGpuTaskCallback schedule_gpu_task,
      GpuVSyncCallback gpu_vsync_callback,
      AddChildWindowToBrowserCallback parent_child_Window_to_browser_callback);

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
      GpuVSyncCallback gpu_vsync_callback,
      AddChildWindowToBrowserCallback parent_child_window_to_browser_callback);

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

  void Reshape(const SkImageInfo& image_info,
               const gfx::ColorSpace& color_space,
               int sample_count,
               float device_scale_factor,
               gfx::OverlayTransform transform);
  void FinishPaintCurrentFrame(
      sk_sp<GrDeferredDisplayList> ddl,
      sk_sp<GrDeferredDisplayList> overdraw_ddl,
      std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
      std::vector<ImageContextImpl*> image_contexts,
      std::vector<gpu::SyncToken> sync_tokens,
      base::OnceClosure on_finished,
      base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
      absl::optional<gfx::Rect> draw_rectangle);
  void ScheduleOutputSurfaceAsOverlay(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane&
          output_surface_plane);
  void SwapBuffers(OutputSurfaceFrame frame);
  void EnsureMinNumberOfBuffers(int n);

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
      std::vector<ImageContextImpl*> image_contexts,
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

  void BeginAccessImages(const std::vector<ImageContextImpl*>& image_contexts,
                         std::vector<GrBackendSemaphore>* begin_semaphores,
                         std::vector<GrBackendSemaphore>* end_semaphores);
  void ResetStateOfImages();
  void EndAccessImages(const base::flat_set<ImageContextImpl*>& image_contexts);

  size_t max_resource_cache_bytes() const { return max_resource_cache_bytes_; }
  void ReleaseImageContexts(
      std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
          image_contexts);
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays);

  void SetEnableDCLayers(bool enable);

  void SetGpuVSyncEnabled(bool enabled);

  void SetVSyncDisplayID(int64_t display_id);

  void SetFrameRate(float frame_rate);

  bool was_context_lost() { return context_state_->context_lost(); }

  void SetCapabilitiesForTesting(
      const OutputSurface::Capabilities& capabilities);

  bool IsDisplayedAsOverlay();

  // gpu::SharedContextState::ContextLostObserver implementation:
  void OnContextLost() override;

  // gpu::ImageTransportSurfaceDelegate implementation:
#if BUILDFLAG(IS_WIN)
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) override;
#endif
  const gpu::gles2::FeatureInfo* GetFeatureInfo() const override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  GpuVSyncCallback GetGpuVSyncCallback() override;
  base::TimeDelta GetGpuBlockedTimeSinceLastSwap() override;

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
                         uint32_t usage,
                         std::string debug_label,
                         gpu::SurfaceHandle surface_handle);
  void CreateSolidColorSharedImage(gpu::Mailbox mailbox,
                                   const SkColor4f& color,
                                   const gfx::ColorSpace& color_space);
  void DestroySharedImage(gpu::Mailbox mailbox);

  // Called on the viz thread!
  base::ScopedClosureRunner GetCacheBackBufferCb();

 private:
  struct MailboxAccessData {
    MailboxAccessData();
    MailboxAccessData(MailboxAccessData&& other);
    MailboxAccessData& operator=(MailboxAccessData&& other);
    ~MailboxAccessData();

    SkISize size;
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

  // Provided as a callback to |device_|.
  void DidSwapBuffersCompleteInternal(gpu::SwapBuffersCompleteParams params,
                                      const gfx::Size& pixel_size,
                                      gfx::GpuFenceHandle release_fence);

  DidSwapBufferCompleteCallback GetDidSwapBuffersCompleteCallback();

  void MarkContextLost(ContextLostReason reason);

  void DestroyCopyOutputResourcesOnGpuThread(const gpu::Mailbox& mailbox);

  void SwapBuffersInternal(absl::optional<OutputSurfaceFrame> frame);
  void PostSubmit(absl::optional<OutputSurfaceFrame> frame);

  GrDirectContext* gr_context() const { return context_state_->gr_context(); }

  skgpu::graphite::Context* graphite_context() const {
    return context_state_->graphite_context();
  }

  bool is_using_vulkan() const {
    return !!vulkan_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kVulkan;
  }

  bool is_using_gl() const {
    return gpu_preferences_.gr_context_type == gpu::GrContextType::kGL;
  }

  bool is_using_graphite_dawn() const {
    return !!dawn_context_provider_ && gpu_preferences_.gr_context_type ==
                                           gpu::GrContextType::kGraphiteDawn;
  }

  // Helper for `FlushSurface()` & `FlushContext()` methods, flushes writes
  // to either the surface if it is non-null or to the context otherwise, using
  // |end_semaphores| and |end_state|.
  bool FlushInternal(
      SkSurface* surface,
      std::vector<GrBackendSemaphore>& end_semaphores,
      gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
      GrGpuFinishedProc finished_proc = nullptr,
      GrGpuFinishedContext finished_context = nullptr);

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
                                      base::StringPiece debug_label);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods, renders
  // |surface| into |dest_surface|'s canvas, cropping and scaling the results
  // appropriately. |source_selection| is the area of the |surface| that will be
  // rendered to the destination.
  void RenderSurface(SkSurface* surface,
                     const SkIRect& source_selection,
                     absl::optional<SkVector> scaling,
                     bool is_downscale_or_identity_in_both_dimensions,
                     SkSurface* dest_surface);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods, flushes writes
  // to |surface| with |end_semaphores| and |end_state|.
  bool FlushSurface(
      SkSurface* surface,
      std::vector<GrBackendSemaphore>& end_semaphores,
      gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
      GrGpuFinishedProc finished_proc = nullptr,
      GrGpuFinishedContext finished_context = nullptr);

  // Helper for `CopyOutputNV12()` & `CopyOutputRGBA()` methods, flushes writes
  // to the Skia context with |end_semaphores| and |end_state|.
  bool FlushContext(
      std::vector<GrBackendSemaphore>& end_semaphores,
      gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
      GrGpuFinishedProc finished_proc = nullptr,
      GrGpuFinishedContext finished_context = nullptr);

  // Creates surfaces needed to store the data in NV12 format.
  // |mailbox_access_datas| will be populated with information needed to access
  // the NV12 planes.
  bool CreateSurfacesForNV12Planes(
      const SkYUVAInfo& yuva_info,
      const gfx::ColorSpace& color_space,
      std::array<MailboxAccessData, CopyOutputResult::kNV12MaxPlanes>&
          mailbox_access_datas,
      bool is_multiplane);

  // Imports surfaces needed to store the data in NV12 format from a blit
  // request. |mailbox_access_datas| will be populated with information needed
  // to access the NV12 planes.
  bool ImportSurfacesForNV12Planes(
      const BlitRequest& blit_request,
      std::array<MailboxAccessData, CopyOutputResult::kNV12MaxPlanes>&
          mailbox_access_datas,
      bool is_multiplane);

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
  absl::optional<ReleaseCurrent> release_current_last_;

  const raw_ptr<SkiaOutputSurfaceDependency> dependency_;
  raw_ptr<gpu::DisplayCompositorMemoryAndTaskControllerOnGpu> shared_gpu_deps_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<gpu::SharedImageFactory> shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  const raw_ptr<VulkanContextProvider> vulkan_context_provider_;
  const raw_ptr<gpu::DawnContextProvider> dawn_context_provider_;
  const RendererSettings renderer_settings_;

  // Should only be run on the client thread with PostTaskToClientThread().
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  BufferPresentedCallback buffer_presented_callback_;
  ContextLostCallback context_lost_callback_;
  ScheduleGpuTaskCallback schedule_gpu_task_;
  GpuVSyncCallback gpu_vsync_callback_;
  AddChildWindowToBrowserCallback add_child_window_to_browser_callback_;

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

  gpu::GpuPreferences gpu_preferences_;
  gfx::Size size_;
  // Only one of GLSurface of Presenter exists at the time.
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::Presenter> presenter_;
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

    void BeginAccess(std::vector<ImageContextImpl*> image_contexts,
                     std::vector<GrBackendSemaphore>* begin_semaphores,
                     std::vector<GrBackendSemaphore>* end_semaphores);
    void EndAccess();

   private:
    const raw_ptr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu_;
    base::flat_set<ImageContextImpl*> image_contexts_;
  };
  PromiseImageAccessHelper promise_image_access_helper_{this};
  base::flat_set<ImageContextImpl*> image_contexts_to_apply_end_state_;

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

  absl::optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>
      output_surface_plane_;
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
  base::flat_set<AsyncReadResultHelper*> async_read_result_helpers_;

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

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImplOnGpu> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
