// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/gl_renderer_copier.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/texture_deleter.h"
#include "components/viz/service/display_embedder/direct_context_provider.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_passthrough.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/private/SkDeferredDisplayList.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"
#endif

#if (BUILDFLAG(ENABLE_VULKAN) || BUILDFLAG(SKIA_USE_DAWN)) && defined(USE_X11)
#include "components/viz/service/display_embedder/skia_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "components/viz/common/gpu/dawn_context_provider.h"
#include "components/viz/service/display_embedder/skia_output_device_dawn.h"
#endif

namespace viz {

namespace {

struct ReadPixelsContext {
  ReadPixelsContext(std::unique_ptr<CopyOutputRequest> request,
                    const gfx::Rect& result_rect)
      : request(std::move(request)), result_rect(result_rect) {}

  std::unique_ptr<CopyOutputRequest> request;
  gfx::Rect result_rect;
};

class CopyOutputResultYUV : public CopyOutputResult {
 public:
  CopyOutputResultYUV(const gfx::Rect& rect,
                      std::unique_ptr<const SkSurface::AsyncReadResult> result)
      : CopyOutputResult(Format::I420_PLANES, rect),
        result_(std::move(result)) {
    DCHECK_EQ(3, result_->count());
    DCHECK_EQ(0, size().width() % 2);
    DCHECK_EQ(0, size().height() % 2);
  }

  // CopyOutputResult implementation.
  bool ReadI420Planes(uint8_t* y_out,
                      int y_out_stride,
                      uint8_t* u_out,
                      int u_out_stride,
                      uint8_t* v_out,
                      int v_out_stride) const override {
    const auto CopyPlane = [](const uint8_t* src, int src_stride, int width,
                              int height, uint8_t* out, int out_stride) {
      for (int i = 0; i < height; ++i, src += src_stride, out += out_stride) {
        memcpy(out, src, width);
      }
    };
    auto* data0 = static_cast<const uint8_t*>(result_->data(0));
    auto* data1 = static_cast<const uint8_t*>(result_->data(1));
    auto* data2 = static_cast<const uint8_t*>(result_->data(2));
    CopyPlane(data0, result_->rowBytes(0), width(0), height(0), y_out,
              y_out_stride);
    CopyPlane(data1, result_->rowBytes(1), width(1), height(1), u_out,
              u_out_stride);
    CopyPlane(data2, result_->rowBytes(2), width(2), height(2), v_out,
              v_out_stride);
    return true;
  }

 private:
  uint32_t width(int plane) const {
    if (plane == 0)
      return size().width();
    else
      return size().width() / 2;
  }

  uint32_t height(int plane) const {
    if (plane == 0)
      return size().height();
    else
      return size().height() / 2;
  }

  std::unique_ptr<const SkSurface::AsyncReadResult> result_;
};

void OnYUVReadbackDone(
    void* c,
    std::unique_ptr<const SkSurface::AsyncReadResult> async_result) {
  std::unique_ptr<ReadPixelsContext> context(
      static_cast<ReadPixelsContext*>(c));
  if (!async_result) {
    // This will automatically send an empty result.
    return;
  }
  std::unique_ptr<CopyOutputResult> result =
      std::make_unique<CopyOutputResultYUV>(context->result_rect,
                                            std::move(async_result));
  context->request->SendResult(std::move(result));
}

}  // namespace

class SkiaOutputSurfaceImplOnGpu::ScopedPromiseImageAccess {
 public:
  ScopedPromiseImageAccess(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           std::vector<ImageContextImpl*> image_contexts)
      : impl_on_gpu_(impl_on_gpu), image_contexts_(std::move(image_contexts)) {
    begin_semaphores_.reserve(image_contexts_.size());
    // We may need one more space for the swap buffer semaphore.
    end_semaphores_.reserve(image_contexts_.size() + 1);
    // TODO(penghuang): gather begin read access semaphores from shared images.
    // https://crbug.com/944194
    impl_on_gpu_->BeginAccessImages(image_contexts_, &begin_semaphores_,
                                    &end_semaphores_);
  }

  ~ScopedPromiseImageAccess() {
    // TODO(penghuang): end shared image access with meaningful semaphores.
    // https://crbug.com/944194
    impl_on_gpu_->EndAccessImages(image_contexts_);
  }

  std::vector<GrBackendSemaphore>& begin_semaphores() {
    return begin_semaphores_;
  }

  std::vector<GrBackendSemaphore>& end_semaphores() { return end_semaphores_; }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  std::vector<ImageContextImpl*> image_contexts_;
  std::vector<GrBackendSemaphore> begin_semaphores_;
  std::vector<GrBackendSemaphore> end_semaphores_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPromiseImageAccess);
};

// Skia gr_context() and |context_provider_| share an underlying GLContext.
// Each of them caches some GL state. Interleaving usage could make cached
// state inconsistent with GL state. Using a ScopedUseContextProvider whenever
// |context_provider_| could be accessed (e.g. processing completed queries),
// will keep cached state consistent with driver GL state.
class SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider {
 public:
  ScopedUseContextProvider(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           GLuint texture_client_id)
      : impl_on_gpu_(impl_on_gpu) {
    if (!impl_on_gpu_->MakeCurrent(true /* need_fbo0 */)) {
      valid_ = false;
      return;
    }

    // GLRendererCopier uses context_provider_->ContextGL(), which caches GL
    // state and removes state setting calls that it considers redundant. To get
    // to a known GL state, we first set driver GL state and then make client
    // side consistent with that.
    auto* api = impl_on_gpu_->api_;
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);

    auto* group = impl_on_gpu->context_provider_->decoder()->GetContextGroup();
    if (group->use_passthrough_cmd_decoder()) {
      // Passthrough decoding is not hooked into GLStateRestorer and we must
      // manually reset the context into a known state after Skia is finished.
      api->glUseProgramFn(0);
      api->glActiveTextureFn(GL_TEXTURE0);
      api->glBindBufferFn(GL_ARRAY_BUFFER, 0);
      api->glBindTextureFn(GL_TEXTURE_2D, 0);
    }
    impl_on_gpu_->context_provider_->SetGLRendererCopierRequiredState(
        texture_client_id);
  }

  ~ScopedUseContextProvider() {
    if (valid_)
      impl_on_gpu_->gr_context()->resetContext();
  }

  bool valid() { return valid_; }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  bool valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseContextProvider);
};

namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    SkiaOutputSurfaceDependency* deps,
    gpu::SequenceId sequence_id) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return deps->GetSyncPointManager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      sequence_id);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(SkiaOutputSurfaceDependency* deps) {
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      deps->GetSharedImageManager(), nullptr);
}

class ScopedSurfaceToTexture {
 public:
  ScopedSurfaceToTexture(scoped_refptr<DirectContextProvider> context_provider,
                         SkSurface* surface)
      : context_provider_(context_provider),
        client_id_(context_provider->GenClientTextureId()) {
    GrBackendTexture skia_texture =
        surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
    GrGLTextureInfo gl_texture_info;
    skia_texture.getGLTextureInfo(&gl_texture_info);

    auto* group = context_provider->decoder()->GetContextGroup();
    if (group->use_passthrough_cmd_decoder()) {
      group->passthrough_resources()->texture_id_map.SetIDMapping(
          client_id_, gl_texture_info.fID);

      auto texture = base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
          gl_texture_info.fID, gl_texture_info.fTarget, GL_RGBA,
          surface->width(), surface->height(),
          /*depth=*/1, /*border=*/0,
          /*format=*/GL_RGBA, /*type=*/GL_UNSIGNED_BYTE);

      group->passthrough_resources()->texture_object_map.SetIDMapping(
          client_id_, texture);
    } else {
      auto* texture_manager = context_provider_->texture_manager();
      texture_ref_ =
          texture_manager->CreateTexture(client_id_, gl_texture_info.fID);
      texture_manager->SetTarget(texture_ref_.get(), gl_texture_info.fTarget);
      texture_manager->SetLevelInfo(
          texture_ref_.get(), gl_texture_info.fTarget,
          /*level=*/0,
          /*internal_format=*/GL_RGBA, surface->width(), surface->height(),
          /*depth=*/1, /*border=*/0,
          /*format=*/GL_RGBA, /*type=*/GL_UNSIGNED_BYTE,
          /*cleared_rect=*/gfx::Rect(surface->width(), surface->height()));
    }
  }

  ~ScopedSurfaceToTexture() {
    auto* group = context_provider_->decoder()->GetContextGroup();

    // Skia owns the texture. It will delete it when it is done.
    if (group->use_passthrough_cmd_decoder()) {
      group->passthrough_resources()
          ->texture_object_map.GetServiceIDOrInvalid(client_id_)
          ->MarkContextLost();
    } else {
      texture_ref_->ForceContextLost();
    }

    context_provider_->DeleteClientTextureId(client_id());
  }

  GLuint client_id() { return client_id_; }

 private:
  scoped_refptr<DirectContextProvider> context_provider_;
  const GLuint client_id_;
  // This is only used with validating gles cmd decoder
  scoped_refptr<gpu::gles2::TextureRef> texture_ref_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSurfaceToTexture);
};

// This SingleThreadTaskRunner runs tasks on the GPU main thread, where
// DirectContextProvider can safely service calls. It wraps all posted tasks to
// ensure that |impl_on_gpu_->context_provider_| is made current and in a known
// state when the task is run. If |impl_on_gpu| is destructed, pending tasks are
// no-oped when they are run.
class ContextCurrentTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit ContextCurrentTaskRunner(SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
      : real_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        impl_on_gpu_(impl_on_gpu) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return real_task_runner_->PostDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return real_task_runner_->PostNonNestableDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return real_task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  base::OnceClosure WrapClosure(base::OnceClosure task) {
    return base::BindOnce(
        [](base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
           base::OnceClosure task) {
          if (!impl_on_gpu)
            return;
          SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider scoped_use(
              impl_on_gpu.get(), /*texture_client_id=*/0);
          if (!scoped_use.valid())
            return;

          std::move(task).Run();
        },
        impl_on_gpu_->weak_ptr(), std::move(task));
  }

  ~ContextCurrentTaskRunner() override = default;

  scoped_refptr<base::SingleThreadTaskRunner> real_task_runner_;
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;

  DISALLOW_COPY_AND_ASSIGN(ContextCurrentTaskRunner);
};

class DirectContextProviderDelegateImpl : public DirectContextProviderDelegate,
                                          public gpu::SharedImageInterface {
 public:
  DirectContextProviderDelegateImpl(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      gpu::SharedContextState* context_state,
      gpu::MailboxManager* mailbox_manager,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<gpu::SyncPointClientState> sync_point_client_state)
      : shared_image_manager_(shared_image_manager),
        shared_image_factory_(gpu_preferences,
                              workarounds,
                              gpu_feature_info,
                              context_state,
                              mailbox_manager,
                              shared_image_manager,
                              nullptr /* image_factory */,
                              nullptr /* memory_tracker */,
                              true /* is_using_skia_renderer */),
        sync_point_client_state_(sync_point_client_state) {}

  ~DirectContextProviderDelegateImpl() override {
    sync_point_client_state_->Destroy();
  }

  // SharedImageInterface implementation:
  gpu::Mailbox CreateSharedImage(ResourceFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 uint32_t usage) override {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (shared_image_factory_.CreateSharedImage(mailbox, format, size,
                                                color_space, usage))
      return mailbox;
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (shared_image_factory_.CreateSharedImage(mailbox, format, size,
                                                color_space, usage, pixel_data))
      return mailbox;
    return gpu::Mailbox();
  }

  gpu::Mailbox CreateSharedImage(
      gfx::GpuMemoryBuffer* gpu_memory_buffer,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override {
    // We do not support creating GMB backed SharedImages.
    NOTIMPLEMENTED();
    return gpu::Mailbox();
  }
  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         std::unique_ptr<gfx::GpuFence> acquire_fence,
                         const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }

  void UpdateSharedImage(const gpu::SyncToken& sync_token,
                         const gpu::Mailbox& mailbox) override {
    DCHECK(!ShouldWait(sync_token))
        << "Cannot UpdateSharedImage with SyncToken from different "
           "command buffer.";
    shared_image_factory_.UpdateSharedImage(mailbox);
  }

  void DestroySharedImage(const gpu::SyncToken& sync_token,
                          const gpu::Mailbox& mailbox) override {
    DCHECK(!ShouldWait(sync_token))
        << "Cannot DestroySharedImage with SyncToken from different "
           "command buffer.";
    shared_image_factory_.DestroySharedImage(mailbox);
  }

  SwapChainMailboxes CreateSwapChain(ResourceFormat format,
                                     const gfx::Size& size,
                                     const gfx::ColorSpace& color_space,
                                     uint32_t usage) override {
    NOTREACHED();
    return {};
  }

  void PresentSwapChain(const gpu::SyncToken& sync_token,
                        const gpu::Mailbox& mailbox) override {
    NOTREACHED();
  }

#if defined(OS_FUCHSIA)
  void RegisterSysmemBufferCollection(gfx::SysmemBufferCollectionId id,
                                      zx::channel token) override {
    NOTREACHED();
  }

  void ReleaseSysmemBufferCollection(
      gfx::SysmemBufferCollectionId id) override {
    NOTREACHED();
  }
#endif  // defined(OS_FUCHSIA)

  gpu::SyncToken GenUnverifiedSyncToken() override {
    return gpu::SyncToken(sync_point_client_state_->namespace_id(),
                          sync_point_client_state_->command_buffer_id(),
                          GenerateFenceSyncRelease());
  }

  gpu::SyncToken GenVerifiedSyncToken() override {
    gpu::SyncToken sync_token = GenUnverifiedSyncToken();
    sync_token.SetVerifyFlush();
    return sync_token;
  }

  void Flush() override {
    // No need to flush in this implementation.
  }

  // DirectContextProviderDelegate implementation.
  gpu::SharedImageManager* GetSharedImageManager() override {
    return shared_image_manager_;
  }

  gpu::SharedImageInterface* GetSharedImageInterface() override { return this; }

  gpu::CommandBufferNamespace GetNamespaceID() const override {
    return sync_point_client_state_->namespace_id();
  }

  gpu::CommandBufferId GetCommandBufferID() const override {
    return sync_point_client_state_->command_buffer_id();
  }

  uint64_t GenerateFenceSyncRelease() override {
    uint64_t release = ++sync_fence_release_;
    // Release fence immediately because the relevant GPU calls were already
    // issued.
    sync_point_client_state_->ReleaseFenceSync(release);
    return release;
  }

  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       base::OnceClosure callback) override {
    base::RepeatingClosure maybe_pass_callback =
        base::AdaptCallbackForRepeating(std::move(callback));
    if (!sync_point_client_state_->Wait(sync_token, maybe_pass_callback)) {
      maybe_pass_callback.Run();
    }
  }

 private:
  bool ShouldWait(const gpu::SyncToken& sync_token) {
    // Don't wait on an invalid SyncToken.
    if (!sync_token.HasData())
      return false;

    // Don't wait on SyncTokens our own sync tokens because we've already issued
    // the relevant calls to the GPU.
    return sync_point_client_state_->namespace_id() !=
               sync_token.namespace_id() ||
           sync_point_client_state_->command_buffer_id() !=
               sync_token.command_buffer_id();
  }

  gpu::SharedImageManager* const shared_image_manager_;
  gpu::SharedImageFactory shared_image_factory_;
  scoped_refptr<gpu::SyncPointClientState> sync_point_client_state_;
  uint64_t sync_fence_release_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DirectContextProviderDelegateImpl);
};

}  // namespace

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::~OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface(
    OffscreenSurface&& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface&
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::operator=(
    OffscreenSurface&& offscreen_surface) = default;

SkSurface* SkiaOutputSurfaceImplOnGpu::OffscreenSurface::surface() const {
  return surface_.get();
}

SkPromiseImageTexture* SkiaOutputSurfaceImplOnGpu::OffscreenSurface::fulfill() {
  DCHECK(surface_);
  if (!promise_texture_) {
    promise_texture_ = SkPromiseImageTexture::Make(
        surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess));
  }
  return promise_texture_.get();
}

void SkiaOutputSurfaceImplOnGpu::OffscreenSurface::set_surface(
    sk_sp<SkSurface> surface) {
  surface_ = std::move(surface);
  promise_texture_ = {};
}

// static
std::unique_ptr<SkiaOutputSurfaceImplOnGpu> SkiaOutputSurfaceImplOnGpu::Create(
    SkiaOutputSurfaceDependency* deps,
    const RendererSettings& renderer_settings,
    const gpu::SequenceId sequence_id,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    BufferPresentedCallback buffer_presented_callback,
    ContextLostCallback context_lost_callback,
    GpuVSyncCallback gpu_vsync_callback) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::Create");

  auto context_state = deps->GetSharedContextState();
  if (!context_state)
    return nullptr;

  auto impl_on_gpu = std::make_unique<SkiaOutputSurfaceImplOnGpu>(
      util::PassKey<SkiaOutputSurfaceImplOnGpu>(), deps,
      context_state->feature_info(), renderer_settings, sequence_id,
      std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(gpu_vsync_callback));
  if (!impl_on_gpu->Initialize())
    return nullptr;

  return impl_on_gpu;
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    util::PassKey<SkiaOutputSurfaceImplOnGpu> /* pass_key */,
    SkiaOutputSurfaceDependency* deps,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    const RendererSettings& renderer_settings,
    const gpu::SequenceId sequence_id,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    BufferPresentedCallback buffer_presented_callback,
    ContextLostCallback context_lost_callback,
    GpuVSyncCallback gpu_vsync_callback)
    : dependency_(std::move(deps)),
      feature_info_(std::move(feature_info)),
      sync_point_client_state_(
          CreateSyncPointClientState(dependency_, sequence_id)),
      shared_image_representation_factory_(
          CreateSharedImageRepresentationFactory(dependency_)),
      vulkan_context_provider_(dependency_->GetVulkanContextProvider()),
      dawn_context_provider_(dependency_->GetDawnContextProvider()),
      renderer_settings_(renderer_settings),
      sequence_id_(sequence_id),
      did_swap_buffer_complete_callback_(
          std::move(did_swap_buffer_complete_callback)),
      buffer_presented_callback_(std::move(buffer_presented_callback)),
      context_lost_callback_(std::move(context_lost_callback)),
      gpu_vsync_callback_(std::move(gpu_vsync_callback)),
      gpu_preferences_(dependency_->GetGpuPreferences()),
      copier_active_url_(GURL("chrome://gpu/SkiaRendererGLRendererCopier")) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  dependency_->RegisterDisplayContext(this);
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  dependency_->UnregisterDisplayContext(this);

  // |context_provider_| and clients want either the context to be lost or made
  // current on destruction.
  if (context_state_ && MakeCurrent(false /* need_fbo0 */)) {
    // This ensures any outstanding callbacks for promise images are performed.
    gr_context()->flush();
  }

  if (copier_) {
    context_provider_->FinishQueries();

    copier_ = nullptr;
    texture_deleter_ = nullptr;
    context_provider_ = nullptr;
    MakeCurrent(false /* need_fbo0 */);
  }

  sync_point_client_state_->Destroy();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil,
    gfx::OverlayTransform transform,
    SkSurfaceCharacterization* characterization,
    base::WaitableEvent* event) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::Reshape");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gr_context());

  base::ScopedClosureRunner scoped_runner;
  if (event) {
    scoped_runner.ReplaceClosure(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  if (!MakeCurrent(!dependency_->IsOffscreen() /* need_fbo0 */))
    return;

  size_ = size;
  color_space_ = color_space;
  if (!output_device_->Reshape(size_, device_scale_factor, color_space,
                               has_alpha, transform)) {
    MarkContextLost();
    return;
  }

  if (characterization) {
    // Start a paint temporarily for getting sk surface characterization.
    scoped_output_device_paint_.emplace(output_device_.get());
    output_sk_surface()->characterize(characterization);
    DCHECK(characterization->isValid());
    scoped_output_device_paint_.reset();
  }
}

bool SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release,
    base::OnceClosure on_finished,
    base::Optional<gfx::Rect> draw_rectangle) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);
  DCHECK(!scoped_output_device_paint_);

  if (!MakeCurrent(true /* need_fbo0 */))
    return false;

  if (draw_rectangle)
    output_device_->SetDrawRectangle(*draw_rectangle);

  // We do not reset scoped_output_device_paint_ after drawing the ddl until
  // SwapBuffers() is called, because we may need access to output_sk_surface()
  // for CopyOutput().
  scoped_output_device_paint_.emplace(output_device_.get());
  DCHECK(output_sk_surface());

  dependency_->ScheduleGrContextCleanup();

  PullTextureUpdates(std::move(sync_tokens));

  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }

    ScopedPromiseImageAccess scoped_promise_image_access(
        this, std::move(image_contexts));
    if (!scoped_promise_image_access.begin_semaphores().empty()) {
      auto result = output_sk_surface()->wait(
          scoped_promise_image_access.begin_semaphores().size(),
          scoped_promise_image_access.begin_semaphores().data());
      DCHECK(result);
    }

    // Draw will only fail if the SkSurface and SkDDL are incompatible.
    bool draw_success = output_sk_surface()->draw(ddl.get());
    DCHECK(draw_success);

    destroy_after_swap_.emplace_back(std::move(ddl));

    if (overdraw_ddl) {
      sk_sp<SkSurface> overdraw_surface = SkSurface::MakeRenderTarget(
          gr_context(), overdraw_ddl->characterization(), SkBudgeted::kNo);
      overdraw_surface->draw(overdraw_ddl.get());
      destroy_after_swap_.emplace_back(std::move(overdraw_ddl));

      SkPaint paint;
      sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

      sk_sp<SkColorFilter> colorFilter = SkiaHelper::MakeOverdrawColorFilter();
      paint.setColorFilter(colorFilter);
      // TODO(xing.xu): move below to the thread where skia record happens.
      output_sk_surface()->getCanvas()->drawImage(overdraw_image.get(), 0, 0,
                                                  &paint);
    }

    if (output_device_->need_swap_semaphore())
      scoped_promise_image_access.end_semaphores().emplace_back();

    GrFlushInfo flush_info = {
        .fFlags = kNone_GrFlushFlags,
        .fNumSemaphores = scoped_promise_image_access.end_semaphores().size(),
        .fSignalSemaphores =
            scoped_promise_image_access.end_semaphores().data(),
    };

    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_,
                                          &flush_info);
    if (on_finished)
      gpu::AddCleanupTaskForSkiaFlush(std::move(on_finished), &flush_info);

    auto result = output_sk_surface()->flush(
        SkSurface::BackendSurfaceAccess::kPresent, flush_info);
    if (result != GrSemaphoresSubmitted::kYes &&
        !(scoped_promise_image_access.begin_semaphores().empty() &&
          scoped_promise_image_access.end_semaphores().empty())) {
      // TODO(penghuang): handle vulkan device lost.
      DLOG(ERROR) << "output_sk_surface()->flush() failed.";
      return false;
    }
    if (output_device_->need_swap_semaphore()) {
      auto& semaphore = scoped_promise_image_access.end_semaphores().back();
      DCHECK(semaphore.isInitialized());
      scoped_output_device_paint_->set_semaphore(std::move(semaphore));
    }
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
  return true;
}

void SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay(
    const OverlayProcessor::OutputSurfaceOverlayPlane& output_surface_plane) {
  DCHECK(!output_surface_plane_);
  output_surface_plane_ = output_surface_plane;
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(
    OutputSurfaceFrame frame,
    base::OnceCallback<bool()> deferred_framebuffer_draw_closure,
    uint64_t sync_fence_release) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::SwapBuffers");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (deferred_framebuffer_draw_closure) {
    // Returns false if context not set to current, i.e lost
    if (!std::move(deferred_framebuffer_draw_closure).Run())
      return;
    DCHECK(context_state_->IsCurrent(nullptr /* surface */));
  } else {
    if (!MakeCurrent(!dependency_->IsOffscreen() /* need_fbo0 */))
      return;
  }
  DCHECK(output_device_);

  scoped_output_device_paint_.reset();

  if (output_surface_plane_) {
    DCHECK(!is_using_vulkan());
    if (gl::GLImage* image = output_device_->GetOverlayImage()) {
      std::unique_ptr<gfx::GpuFence> gpu_fence =
          output_device_->SubmitOverlayGpuFence();

      // Output surface is also z-order 0.
      int plane_z_order = 0;
      // Output surface always uses the full texture.
      gfx::RectF uv_rect(0.f, 0.f, 1.f, 1.f);

      gl_surface_->ScheduleOverlayPlane(
          plane_z_order, output_surface_plane_->transform, image,
          ToNearestRect(output_surface_plane_->display_rect), uv_rect,
          output_surface_plane_->enable_blending, std::move(gpu_fence));
    }
    output_surface_plane_.reset();
  }

  if (frame.sub_buffer_rect && frame.sub_buffer_rect->IsEmpty()) {
    // Call SwapBuffers() to present overlays.
    output_device_->SwapBuffers(buffer_presented_callback_,
                                std::move(frame.latency_info));
  } else if (capabilities().supports_post_sub_buffer && frame.sub_buffer_rect) {
    if (!capabilities().flipped_output_surface)
      frame.sub_buffer_rect->set_y(size_.height() - frame.sub_buffer_rect->y() -
                                   frame.sub_buffer_rect->height());
    output_device_->PostSubBuffer(*frame.sub_buffer_rect,
                                  buffer_presented_callback_,
                                  std::move(frame.latency_info));
  } else {
    output_device_->SwapBuffers(buffer_presented_callback_,
                                std::move(frame.latency_info));
  }

  if (sync_fence_release)
    ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);

  destroy_after_swap_.clear();
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    RenderPassId id,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  PullTextureUpdates(std::move(sync_tokens));

  auto& offscreen = offscreen_surfaces_[id];
  if (!offscreen.surface()) {
    offscreen.set_surface(SkSurface::MakeRenderTarget(
        gr_context(), ddl->characterization(), SkBudgeted::kNo));
    DCHECK(offscreen.surface());
  } else {
#if DCHECK_IS_ON()
    // TODO(crbug.com/1022304): Remove aliasing after figuring out how
    // characterizations are different.
    SkSurfaceCharacterization characterization;
    DCHECK(offscreen.surface()->characterize(&characterization));
    base::debug::Alias(&characterization);
    SkSurfaceCharacterization ddl_characterization = ddl->characterization();
    base::debug::Alias(&ddl_characterization);
    DCHECK(characterization == ddl_characterization);
#endif
  }
  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }
    ScopedPromiseImageAccess scoped_promise_image_access(
        this, std::move(image_contexts));
    if (!scoped_promise_image_access.begin_semaphores().empty()) {
      auto result = offscreen.surface()->wait(
          scoped_promise_image_access.begin_semaphores().size(),
          scoped_promise_image_access.begin_semaphores().data());
      DCHECK(result);
    }
    offscreen.surface()->draw(ddl.get());
    destroy_after_swap_.emplace_back(std::move(ddl));

    GrFlushInfo flush_info = {
        .fFlags = kNone_GrFlushFlags,
        .fNumSemaphores = scoped_promise_image_access.end_semaphores().size(),
        .fSignalSemaphores =
            scoped_promise_image_access.end_semaphores().data(),
    };

    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_,
                                          &flush_info);
    auto result = offscreen.surface()->flush(
        SkSurface::BackendSurfaceAccess::kNoAccess, flush_info);
    if (result != GrSemaphoresSubmitted::kYes &&
        !(scoped_promise_image_access.begin_semaphores().empty() &&
          scoped_promise_image_access.end_semaphores().empty())) {
      // TODO(penghuang): handle vulkan device lost.
      DLOG(ERROR) << "offscreen.surface()->flush() failed.";
      return;
    }
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource(
    std::vector<std::unique_ptr<ImageContextImpl>> image_contexts) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!image_contexts.empty());
  for (auto& image_context : image_contexts) {
    // It's possible that |offscreen_surfaces_| won't contain an entry for the
    // render pass if draw failed early.
    offscreen_surfaces_.erase(image_context->render_pass_id());
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    RenderPassId id,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    base::OnceCallback<bool()> deferred_framebuffer_draw_closure) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::CopyOutput");
  // TODO(crbug.com/898595): Do this on the GPU instead of CPU with Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Clear |destroy_after_swap_| if we CopyOutput without SwapBuffers.
  base::ScopedClosureRunner cleanup(
      base::BindOnce([](std::vector<std::unique_ptr<SkDeferredDisplayList>>) {},
                     std::move(destroy_after_swap_)));

  bool use_gl_renderer_copier = !is_using_vulkan() && !is_using_dawn() &&
                                !features::IsUsingSkiaForGLReadback();
  if (use_gl_renderer_copier)
    gpu::ContextUrl::SetActiveUrl(copier_active_url_);

  // Lazy initialize GLRendererCopier before draw because
  // DirectContextProvider ctor the backbuffer.
  if (use_gl_renderer_copier && !copier_) {
    if (!MakeCurrent(true /* need_fbo0 */))
      return;
    auto client = std::make_unique<DirectContextProviderDelegateImpl>(
        gpu_preferences_, dependency_->GetGpuDriverBugWorkarounds(),
        dependency_->GetGpuFeatureInfo(), context_state_.get(),
        dependency_->GetMailboxManager(), dependency_->GetSharedImageManager(),
        CreateSyncPointClientState(dependency_, sequence_id_));
    context_provider_ = base::MakeRefCounted<DirectContextProvider>(
        context_state_->context(), gl_surface_, supports_alpha_,
        gpu_preferences_, feature_info_.get(), std::move(client));
    auto result = context_provider_->BindToCurrentThread();
    if (result != gpu::ContextResult::kSuccess) {
      DLOG(ERROR) << "Couldn't initialize GLRendererCopier";
      context_provider_ = nullptr;
      return;
    }
    context_current_task_runner_ =
        base::MakeRefCounted<ContextCurrentTaskRunner>(this);
    texture_deleter_ =
        std::make_unique<TextureDeleter>(context_current_task_runner_);
    copier_ = std::make_unique<GLRendererCopier>(context_provider_,
                                                 texture_deleter_.get());
    copier_->set_async_gl_task_runner(context_current_task_runner_);

    // DirectContextProvider changed GL state. Reset Skia state tracking
    // for potential draw below.
    gr_context()->resetContext();
  }

  if (deferred_framebuffer_draw_closure) {
    // returns false if context not set to current, i.e lost
    if (!std::move(deferred_framebuffer_draw_closure).Run())
      return;
    DCHECK(context_state_->IsCurrent(nullptr /* surface */));
  } else {
    if (!MakeCurrent(true /* need_fbo0 */))
      return;
  }

  bool from_fbo0 = !id;
  DCHECK(scoped_output_device_paint_ || !from_fbo0);

  DCHECK(from_fbo0 ||
         offscreen_surfaces_.find(id) != offscreen_surfaces_.end());
  auto* surface =
      from_fbo0 ? output_sk_surface() : offscreen_surfaces_[id].surface();

  // If a platform doesn't support RGBX_8888 format, we will use RGBA_8888
  // instead. In this case, we need discard alpha channel (modify the alpha
  // value to 0xff, but keep other channel not changed).
  bool need_discard_alpha = from_fbo0 && (output_device_->is_emulated_rgbx());
  if (need_discard_alpha) {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }
    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    paint.setBlendMode(SkBlendMode::kDstATop);
    surface->getCanvas()->drawPaint(paint);
    surface->flush();
  }

  if (use_gl_renderer_copier) {
    surface->flush();

    GLuint gl_id = 0;
    GLenum internal_format = supports_alpha_ ? GL_RGBA : GL_RGB;
    bool flipped = from_fbo0 ? !capabilities().flipped_output_surface : false;
    // readback_offset is in window co-ordinate space and must take into account
    // flipping.
    if (flipped) {
      geometry.readback_offset.set_y(
          size_.height() -
          (geometry.readback_offset.y() + geometry.result_selection.height()));
    }

    base::Optional<ScopedSurfaceToTexture> texture_mapper;
    if (!from_fbo0 || dependency_->IsOffscreen()) {
      texture_mapper.emplace(context_provider_.get(), surface);
      gl_id = texture_mapper.value().client_id();
      internal_format = GL_RGBA;
    }

    gfx::Size surface_size(surface->width(), surface->height());
    ScopedUseContextProvider use_context_provider(this, gl_id);
    copier_->CopyFromTextureOrFramebuffer(std::move(request), geometry,
                                          internal_format, gl_id, surface_size,
                                          flipped, color_space);

    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries())
      ScheduleDelayedWork();

    return;
  }

  if (request->result_format() ==
      CopyOutputRequest::ResultFormat::I420_PLANES) {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kInProcessCommandBufferClientId);
    }
    // For downscaling, use the GOOD quality setting (appropriate for
    // thumbnailing); and, for upscaling, use the BEST quality.
    bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    SkFilterQuality filter_quality = is_downscale_in_both_dimensions
                                         ? kMedium_SkFilterQuality
                                         : kHigh_SkFilterQuality;
    SkIRect srcRect = SkIRect::MakeXYWH(
        geometry.sampling_bounds.x(), geometry.sampling_bounds.y(),
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height());
    std::unique_ptr<ReadPixelsContext> context =
        std::make_unique<ReadPixelsContext>(std::move(request),
                                            geometry.result_bounds);
    surface->asyncRescaleAndReadPixelsYUV420(
        kRec709_SkYUVColorSpace, SkColorSpace::MakeSRGB(), srcRect,
        {geometry.result_bounds.width(), geometry.result_bounds.height()},
        SkSurface::RescaleGamma::kSrc, filter_quality, OnYUVReadbackDone,
        context.release());
    return;
  }

  SkBitmap bitmap;
  if (request->is_scaled()) {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.sampling_bounds.x(),
                        geometry.sampling_bounds.y());

    // Execute the scaling: For downscaling, use the RESIZE_BETTER strategy
    // (appropriate for thumbnailing); and, for upscaling, use the RESIZE_BEST
    // strategy. Note that processing is only done on the subset of the
    // RenderPass output that contributes to the result.
    using skia::ImageOperations;
    const bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    const ImageOperations::ResizeMethod method =
        is_downscale_in_both_dimensions ? ImageOperations::RESIZE_BETTER
                                        : ImageOperations::RESIZE_BEST;
    bitmap = ImageOperations::Resize(
        bitmap, method, geometry.result_bounds.width(),
        geometry.result_bounds.height(),
        SkIRect{geometry.result_selection.x(), geometry.result_selection.y(),
                geometry.result_selection.right(),
                geometry.result_selection.bottom()});
  } else {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.result_selection.width(), geometry.result_selection.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.readback_offset.x(),
                        geometry.readback_offset.y());
  }

  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to the
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());

  // Deliver the result. SkiaRenderer supports RGBA_BITMAP and I420_PLANES
  // only. For legacy reasons, if a RGBA_TEXTURE request is being made, clients
  // are prepared to accept RGBA_BITMAP results.
  //
  // TODO(crbug/754872): Get rid of the legacy behavior and send empty results
  // for RGBA_TEXTURE requests once tab capture is moved into VIZ.
  const CopyOutputResult::Format result_format =
      (request->result_format() == CopyOutputResult::Format::RGBA_TEXTURE)
          ? CopyOutputResult::Format::RGBA_BITMAP
          : request->result_format();
  // Note: The CopyOutputSkBitmapResult automatically provides I420 format
  // conversion, if needed.
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      result_format, geometry.result_selection, bitmap));
}

gpu::DecoderContext* SkiaOutputSurfaceImplOnGpu::decoder() {
  return context_provider_->decoder();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PerformDelayedWork,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(2));
}

void SkiaOutputSurfaceImplOnGpu::PerformDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu::ContextUrl::SetActiveUrl(copier_active_url_);
  ScopedUseContextProvider use_context_provider(this, /*texture_client_id=*/0);

  delayed_work_pending_ = false;
  if (MakeCurrent(true /* need_fbo0 */)) {
    decoder()->PerformIdleWork();
    decoder()->ProcessPendingQueries(false);
    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries()) {
      ScheduleDelayedWork();
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::BeginAccessImages(
    const std::vector<ImageContextImpl*>& image_contexts,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::BeginAccessImages");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto* context : image_contexts) {
    // Prepare for accessing render pass.
    if (context->render_pass_id()) {
      // We don't cache promise image for render pass, so the it should always
      // be nullptr.
      auto it = offscreen_surfaces_.find(context->render_pass_id());
      DCHECK(it != offscreen_surfaces_.end());
      context->set_promise_image_texture(sk_ref_sp(it->second.fulfill()));
      if (!context->promise_image_texture()) {
        DLOG(ERROR) << "Failed to fulfill the promise texture created from "
                       "RenderPassId:"
                    << context->render_pass_id();
      }
    } else {
      context->BeginAccessIfNecessary(
          context_state_.get(), shared_image_representation_factory_.get(),
          dependency_->GetMailboxManager(), begin_semaphores, end_semaphores);
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::EndAccessImages(
    const std::vector<ImageContextImpl*>& image_contexts) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::EndAccessImages");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto* context : image_contexts)
    context->EndAccessIfNecessary();
}

sk_sp<GrContextThreadSafeProxy>
SkiaOutputSurfaceImplOnGpu::GetGrContextThreadSafeProxy() {
  return gr_context()->threadSafeProxy();
}

void SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts(
    std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
        image_contexts) {
  DCHECK(!image_contexts.empty());
  // The window could be destroyed already, and the MakeCurrent will fail with
  // an destroyed window, so MakeCurrent without requiring the fbo0.
  if (!MakeCurrent(false /* need_fbo0 */)) {
    for (const auto& context : image_contexts)
      context->OnContextLost();
  }

  // |image_contexts| goes out of scope here.
}

#if defined(OS_WIN)
void SkiaOutputSurfaceImplOnGpu::SetEnableDCLayers(bool enable) {
  if (!MakeCurrent(false /* need_fbo0 */))
    return;
  output_device_->SetEnableDCLayers(enable);
}

void SkiaOutputSurfaceImplOnGpu::ScheduleDCLayers(
    std::vector<DCLayerOverlay> dc_layers) {
  output_device_->ScheduleDCLayers(std::move(dc_layers));
}
#endif

void SkiaOutputSurfaceImplOnGpu::SetGpuVSyncEnabled(bool enabled) {
  output_device_->SetGpuVSyncEnabled(enabled);
}

void SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting(
    const OutputSurface::Capabilities& capabilities) {
  MakeCurrent(false /* need_fbo0 */);
  // Check that we're using an offscreen surface.
  DCHECK(dependency_->IsOffscreen());
  output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
      context_state_, capabilities.flipped_output_surface,
      renderer_settings_.requires_alpha_channel,
      did_swap_buffer_complete_callback_);
}

bool SkiaOutputSurfaceImplOnGpu::Initialize() {
  TRACE_EVENT1("viz", "SkiaOutputSurfaceImplOnGpu::Initialize",
               "is_using_vulkan", is_using_vulkan());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
#if defined(USE_OZONE)
  window_surface_ =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreatePlatformWindowSurface(dependency_->GetSurfaceHandle());
#endif

  if (is_using_vulkan()) {
    if (!InitializeForVulkan())
      return false;
  } else if (is_using_dawn()) {
    if (!InitializeForDawn())
      return false;
  } else {
    if (!InitializeForGL())
      return false;
  }
  max_resource_cache_bytes_ = context_state_->max_resource_cache_bytes();
  return true;
}

bool SkiaOutputSurfaceImplOnGpu::InitializeForGL() {
  context_state_ = dependency_->GetSharedContextState();
  if (!context_state_) {
    DLOG(ERROR) << "Failed to create GrContext";
    return false;
  }

  auto* context = context_state_->real_context();
  auto* current_gl = context->GetCurrentGL();
  api_ = current_gl->Api;
  gl_version_info_ = context->GetVersionInfo();

  if (dependency_->IsOffscreen()) {
    gl_surface_ = dependency_->CreateGLSurface(nullptr);
    if (!gl_surface_)
      return false;

    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, true /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  } else {
    gl_surface_ = dependency_->CreateGLSurface(weak_ptr_factory_.GetWeakPtr());

    if (!gl_surface_)
      return false;

    if (MakeCurrent(true /* need_fbo0 */)) {
      if (gl_surface_->IsSurfaceless()) {
        std::unique_ptr<SkiaOutputDeviceBufferQueue> onscreen_device =
            std::make_unique<SkiaOutputDeviceBufferQueue>(
                gl_surface_, dependency_, did_swap_buffer_complete_callback_);
        supports_alpha_ = onscreen_device->supports_alpha();
        output_device_ = std::move(onscreen_device);

      } else {
        std::unique_ptr<SkiaOutputDeviceGL> onscreen_device =
            std::make_unique<SkiaOutputDeviceGL>(
                dependency_->GetMailboxManager(), gl_surface_, feature_info_,
                did_swap_buffer_complete_callback_);

        onscreen_device->Initialize(gr_context(), context);
        supports_alpha_ = onscreen_device->supports_alpha();
        output_device_ = std::move(onscreen_device);
      }
    } else {
      gl_surface_ = nullptr;
      context_state_ = nullptr;
      LOG(FATAL) << "Failed to make current during initialization.";
      return false;
    }
  }
  DCHECK_EQ(gl_surface_->IsOffscreen(), dependency_->IsOffscreen());
  return true;
}

bool SkiaOutputSurfaceImplOnGpu::InitializeForVulkan() {
  context_state_ = dependency_->GetSharedContextState();
  DCHECK(context_state_);
#if BUILDFLAG(ENABLE_VULKAN)
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, false /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  } else {
#if defined(USE_X11)
    supports_alpha_ = true;
    if (gpu_preferences_.disable_vulkan_surface) {
      output_device_ = std::make_unique<SkiaOutputDeviceX11>(
          context_state_, dependency_->GetSurfaceHandle(),
          did_swap_buffer_complete_callback_);
    } else {
      output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
          vulkan_context_provider_, dependency_->GetSurfaceHandle(),
          did_swap_buffer_complete_callback_);
    }
#else
    output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
        vulkan_context_provider_, dependency_->GetSurfaceHandle(),
        did_swap_buffer_complete_callback_);
#endif
  }
#endif
  return true;
}

bool SkiaOutputSurfaceImplOnGpu::InitializeForDawn() {
  context_state_ = dependency_->GetSharedContextState();
  DCHECK(context_state_);
#if BUILDFLAG(SKIA_USE_DAWN)
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, false /* flipped */,
        renderer_settings_.requires_alpha_channel,
        did_swap_buffer_complete_callback_);
    supports_alpha_ = renderer_settings_.requires_alpha_channel;
  } else {
#if defined(USE_X11)
    // TODO(sgilhuly): Set up a Vulkan swapchain so that Linux can also use
    // SkiaOutputDeviceDawn.
    output_device_ = std::make_unique<SkiaOutputDeviceX11>(
        context_state_, dependency_->GetSurfaceHandle(),
        did_swap_buffer_complete_callback_);
#else
    output_device_ = std::make_unique<SkiaOutputDeviceDawn>(
        dawn_context_provider_, dependency_->GetSurfaceHandle(),
        did_swap_buffer_complete_callback_);
#endif
  }
#endif
  return true;
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent(bool need_fbo0) {
  if (!is_using_vulkan()) {
    if (context_state_->context_lost())
      return false;

    // Only make current with |gl_surface_|, if following operations will use
    // fbo0.
    if (!context_state_->MakeCurrent(need_fbo0 ? gl_surface_.get() : nullptr)) {
      LOG(ERROR) << "Failed to make current.";
      dependency_->DidLoseContext(
          !need_fbo0 /* offscreen */, gpu::error::kMakeCurrentFailed,
          GURL("chrome://gpu/SkiaOutputSurfaceImplOnGpu::MakeCurrent"));
      MarkContextLost();
      return false;
    }
    context_state_->set_need_context_state_reset(true);
  }
  return true;
}

void SkiaOutputSurfaceImplOnGpu::PullTextureUpdates(
    std::vector<gpu::SyncToken> sync_tokens) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (dependency_->GetMailboxManager()->UsesSync()) {
    for (auto& sync_token : sync_tokens)
      dependency_->GetMailboxManager()->PullTextureUpdates(sync_token);
  }
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSyncAndPushTextureUpdates(
    uint64_t sync_fence_release) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (dependency_->GetMailboxManager()->UsesSync()) {
    // If MailboxManagerSync is used, we are sharing textures between threads.
    // In this case, sync point can only guarantee GL commands are issued in
    // correct order across threads and GL contexts. However GPU driver may
    // execute GL commands out of the issuing order across GL contexts. So we
    // have to use PushTextureUpdates() and PullTextureUpdates() to ensure the
    // correct GL commands executing order.
    // PushTextureUpdates(token) will insert a GL fence into the current GL
    // context, PullTextureUpdates(token) will wait the GL fence associated with
    // the give token on the current GL context.
    // Reconstruct sync_token from sync_fence_release.
    gpu::SyncToken sync_token(
        gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
        command_buffer_id(), sync_fence_release);
    dependency_->GetMailboxManager()->PushTextureUpdates(sync_token);
  }
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

bool SkiaOutputSurfaceImplOnGpu::IsDisplayedAsOverlay() {
  return gl_surface_ ? gl_surface_->IsSurfaceless() : false;
}

#if defined(OS_WIN)
void SkiaOutputSurfaceImplOnGpu::DidCreateAcceleratedSurfaceChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  dependency_->DidCreateAcceleratedSurfaceChildWindow(parent_window,
                                                      child_window);
}
#endif

const gpu::gles2::FeatureInfo* SkiaOutputSurfaceImplOnGpu::GetFeatureInfo()
    const {
  return feature_info_.get();
}

const gpu::GpuPreferences& SkiaOutputSurfaceImplOnGpu::GetGpuPreferences()
    const {
  return gpu_preferences_;
}

GpuVSyncCallback SkiaOutputSurfaceImplOnGpu::GetGpuVSyncCallback() {
  return gpu_vsync_callback_;
}

base::TimeDelta SkiaOutputSurfaceImplOnGpu::GetGpuBlockedTimeSinceLastSwap() {
  return dependency_->GetGpuBlockedTimeSinceLastSwap();
}

void SkiaOutputSurfaceImplOnGpu::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SkiaOutputSurfaceImplOnGpu::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SkiaOutputSurfaceImplOnGpu::SendOverlayPromotionNotification(
    base::flat_set<gpu::Mailbox> promotion_denied,
    base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions) {
#if defined(OS_ANDROID)
  for (auto& denied : promotion_denied) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(denied);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(false, gfx::Rect());
  }
  for (auto& possible : possible_promotions) {
    auto shared_image_overlay =
        shared_image_representation_factory_->ProduceOverlay(possible.first);
    // When display is re-opened, the first few frames might not have video
    // resource ready. Possible investigation crbug.com/1023971.
    if (!shared_image_overlay)
      continue;

    shared_image_overlay->NotifyOverlayPromotion(true, possible.second);
  }
#endif
}

void SkiaOutputSurfaceImplOnGpu::RenderToOverlay(
    gpu::Mailbox overlay_candidate_mailbox,
    const gfx::Rect& bounds) {
#if defined(OS_ANDROID)
  auto shared_image_overlay =
      shared_image_representation_factory_->ProduceOverlay(
          overlay_candidate_mailbox);
  // When display is re-opened, the first few frames might not have video
  // resource ready. Possible investigation crbug.com/1023971.
  if (!shared_image_overlay)
    return;

  // In current implementation, the BeginReadAccess will ends up calling
  // CodecImage::RenderToOverlay. Currently this code path is only used for
  // Android Classic video overlay, where update of the overlay plane is within
  // media code. Since we are not actually passing an overlay plane to the
  // display controller here, we are able to call EndReadAccess directly after
  // BeginReadAccess.
  shared_image_overlay->NotifyOverlayPromotion(true, bounds);
  shared_image_overlay->BeginReadAccess();
  shared_image_overlay->EndReadAccess();
#endif
}

void SkiaOutputSurfaceImplOnGpu::MarkContextLost() {
  context_state_->MarkContextLost();
  if (context_lost_callback_) {
    std::move(context_lost_callback_).Run();
    if (context_provider_)
      context_provider_->MarkContextLost();
  }
}

}  // namespace viz
