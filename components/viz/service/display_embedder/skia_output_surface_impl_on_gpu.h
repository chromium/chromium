// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/context_url.h"
#include "gpu/ipc/service/display_context.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

class SkDeferredDisplayList;

namespace base {
class WaitableEvent;
}

namespace gfx {
class ColorSpace;
}

namespace gl {
class GLApi;
class GLSurface;
}

namespace gpu {
class SyncPointClientState;
}

namespace ui {
#if defined(USE_OZONE)
class PlatformWindowSurface;
#endif
}  // namespace ui

namespace viz {

class DawnContextProvider;
class DirectContextProvider;
class GLRendererCopier;
class ImageContextImpl;
class SkiaOutputSurfaceDependency;
class TextureDeleter;
class VulkanContextProvider;

namespace copy_output {
struct RenderPassGeometry;
}  // namespace copy_output

// The SkiaOutputSurface implementation running on the GPU thread. This class
// should be created, used and destroyed on the GPU thread.
class SkiaOutputSurfaceImplOnGpu : public gpu::ImageTransportSurfaceDelegate,
                                   public gpu::DisplayContext {
 public:
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  using BufferPresentedCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback& feedback)>;
  using ContextLostCallback = base::OnceClosure;

  static std::unique_ptr<SkiaOutputSurfaceImplOnGpu> Create(
      SkiaOutputSurfaceDependency* deps,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      GpuVSyncCallback gpu_vsync_callback);

  SkiaOutputSurfaceImplOnGpu(
      util::PassKey<SkiaOutputSurfaceImplOnGpu> pass_key,
      SkiaOutputSurfaceDependency* deps,
      scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
      const RendererSettings& renderer_settings,
      const gpu::SequenceId sequence_id,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
      BufferPresentedCallback buffer_presented_callback,
      ContextLostCallback context_lost_callback,
      GpuVSyncCallback gpu_vsync_callback);
  ~SkiaOutputSurfaceImplOnGpu() override;

  gpu::CommandBufferId command_buffer_id() const {
    return sync_point_client_state_->command_buffer_id();
  }
  const OutputSurface::Capabilities capabilities() const {
    return output_device_->capabilities();
  }
  const base::WeakPtr<SkiaOutputSurfaceImplOnGpu>& weak_ptr() const {
    return weak_ptr_;
  }

  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil,
               gfx::OverlayTransform transform,
               SkSurfaceCharacterization* characterization,
               base::WaitableEvent* event);
  bool FinishPaintCurrentFrame(
      std::unique_ptr<SkDeferredDisplayList> ddl,
      std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
      std::vector<ImageContextImpl*> image_contexts,
      std::vector<gpu::SyncToken> sync_tokens,
      uint64_t sync_fence_release,
      base::OnceClosure on_finished,
      base::Optional<gfx::Rect> draw_rectangle);
  void ScheduleOutputSurfaceAsOverlay(
      const OverlayProcessor::OutputSurfaceOverlayPlane& output_surface_plane);
  void SwapBuffers(OutputSurfaceFrame frame,
                   base::OnceCallback<bool()> deferred_framebuffer_draw_closure,
                   uint64_t sync_fence_release);
  void EnsureBackbuffer() { output_device_->EnsureBackbuffer(); }
  void DiscardBackbuffer() { output_device_->DiscardBackbuffer(); }
  void FinishPaintRenderPass(RenderPassId id,
                             std::unique_ptr<SkDeferredDisplayList> ddl,
                             std::vector<ImageContextImpl*> image_contexts,
                             std::vector<gpu::SyncToken> sync_tokens,
                             uint64_t sync_fence_release);
  void RemoveRenderPassResource(
      std::vector<std::unique_ptr<ImageContextImpl>> image_contexts);
  void CopyOutput(RenderPassId id,
                  copy_output::RenderPassGeometry geometry,
                  const gfx::ColorSpace& color_space,
                  std::unique_ptr<CopyOutputRequest> request,
                  base::OnceCallback<bool()> deferred_framebuffer_draw_closure);

  void BeginAccessImages(const std::vector<ImageContextImpl*>& image_contexts,
                         std::vector<GrBackendSemaphore>* begin_semaphores,
                         std::vector<GrBackendSemaphore>* end_semaphores);
  void EndAccessImages(const std::vector<ImageContextImpl*>& image_contexts);

  sk_sp<GrContextThreadSafeProxy> GetGrContextThreadSafeProxy();
  const gl::GLVersionInfo* gl_version_info() const { return gl_version_info_; }
  size_t max_resource_cache_bytes() const { return max_resource_cache_bytes_; }
  void ReleaseImageContexts(
      std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
          image_contexts);
#if defined(OS_WIN)
  void SetEnableDCLayers(bool enable);
  void ScheduleDCLayers(std::vector<DCLayerOverlay> dc_layers);
#endif
  void SetGpuVSyncEnabled(bool enabled);

  bool was_context_lost() { return context_state_->context_lost(); }

  class ScopedUseContextProvider;

  void SetCapabilitiesForTesting(
      const OutputSurface::Capabilities& capabilities);

  bool IsDisplayedAsOverlay();

  // gpu::ImageTransportSurfaceDelegate implementation:
#if defined(OS_WIN)
  void DidCreateAcceleratedSurfaceChildWindow(
      gpu::SurfaceHandle parent_window,
      gpu::SurfaceHandle child_window) override;
#endif
  const gpu::gles2::FeatureInfo* GetFeatureInfo() const override;
  const gpu::GpuPreferences& GetGpuPreferences() const override;
  void DidSwapBuffersComplete(gpu::SwapBuffersCompleteParams params) override;
  void BufferPresented(const gfx::PresentationFeedback& feedback) override;
  GpuVSyncCallback GetGpuVSyncCallback() override;
  base::TimeDelta GetGpuBlockedTimeSinceLastSwap() override;

  void SendOverlayPromotionNotification(
      base::flat_set<gpu::Mailbox> promotion_denied,
      base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions);

  void RenderToOverlay(gpu::Mailbox overlay_candidate_mailbox,
                       const gfx::Rect& bounds);

  // gpu::DisplayContext implementation:
  void MarkContextLost() override;

 private:
  class ScopedPromiseImageAccess;

  bool Initialize();
  bool InitializeForGL();
  bool InitializeForVulkan();
  bool InitializeForDawn();

  // Make context current for GL, and return false if the context is lost.
  // It will do nothing when Vulkan is used.
  bool MakeCurrent(bool need_fbo0);

  void PullTextureUpdates(std::vector<gpu::SyncToken> sync_token);

  void ReleaseFenceSyncAndPushTextureUpdates(uint64_t sync_fence_release);

  GrContext* gr_context() { return context_state_->gr_context(); }
  gpu::DecoderContext* decoder();

  void ScheduleDelayedWork();
  void PerformDelayedWork();

  bool is_using_vulkan() const {
    return !!vulkan_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kVulkan;
  }
  bool is_using_dawn() const {
    return !!dawn_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kDawn;
  }

  SkSurface* output_sk_surface() const {
    return scoped_output_device_paint_->sk_surface();
  }

  SkiaOutputSurfaceDependency* const dependency_;
  scoped_refptr<gpu::gles2::FeatureInfo> feature_info_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  VulkanContextProvider* const vulkan_context_provider_;
  DawnContextProvider* const dawn_context_provider_;
  const RendererSettings renderer_settings_;
  // This is only used to lazily create DirectContextProviderDelegate for
  // readback using GLRendererCopier.
  // TODO(samans): Remove |sequence_id| once readback always uses Skia.
  const gpu::SequenceId sequence_id_;
  const DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;
  const BufferPresentedCallback buffer_presented_callback_;
  ContextLostCallback context_lost_callback_;
  const GpuVSyncCallback gpu_vsync_callback_;

#if defined(USE_OZONE)
  // This should outlive gl_surface_ and vulkan_surface_.
  std::unique_ptr<ui::PlatformWindowSurface> window_surface_;
#endif

  gpu::GpuPreferences gpu_preferences_;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  const gl::GLVersionInfo* gl_version_info_ = nullptr;
  size_t max_resource_cache_bytes_ = 0u;

  std::unique_ptr<SkiaOutputDevice> output_device_;
  base::Optional<SkiaOutputDevice::ScopedPaint> scoped_output_device_paint_;

  base::Optional<OverlayProcessor::OutputSurfaceOverlayPlane>
      output_surface_plane_;

  // Offscreen surfaces for render passes. It can only be accessed on GPU
  // thread.
  class OffscreenSurface {
   public:
    OffscreenSurface();
    OffscreenSurface(const OffscreenSurface& offscreen_surface) = delete;
    OffscreenSurface(OffscreenSurface&& offscreen_surface);
    OffscreenSurface& operator=(const OffscreenSurface& offscreen_surface) =
        delete;
    OffscreenSurface& operator=(OffscreenSurface&& offscreen_surface);
    ~OffscreenSurface();
    SkSurface* surface() const;
    SkPromiseImageTexture* fulfill();
    void set_surface(sk_sp<SkSurface> surface);

   private:
    sk_sp<SkSurface> surface_;
    sk_sp<SkPromiseImageTexture> promise_texture_;
  };
  base::flat_map<RenderPassId, OffscreenSurface> offscreen_surfaces_;

  scoped_refptr<base::SingleThreadTaskRunner> context_current_task_runner_;
  scoped_refptr<DirectContextProvider> context_provider_;
  std::unique_ptr<TextureDeleter> texture_deleter_;
  std::unique_ptr<GLRendererCopier> copier_;

  bool delayed_work_pending_ = false;

  gl::GLApi* api_ = nullptr;
  bool supports_alpha_ = false;

  // Micro-optimization to get to issuing GPU SwapBuffers as soon as possible.
  std::vector<std::unique_ptr<SkDeferredDisplayList>> destroy_after_swap_;

  const gpu::ContextUrl copier_active_url_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtr<SkiaOutputSurfaceImplOnGpu> weak_ptr_;
  base::WeakPtrFactory<SkiaOutputSurfaceImplOnGpu> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceImplOnGpu);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_IMPL_ON_GPU_H_
