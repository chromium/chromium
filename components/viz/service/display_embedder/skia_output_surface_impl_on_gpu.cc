// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/blit_request.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/image_context_impl.h"
#include "components/viz/service/display_embedder/output_presenter_gl.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_buffer_queue.h"
#include "components/viz/service/display_embedder/skia_output_device_gl.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/display_embedder/skia_output_device_webview.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu_debug_capture.h"
#include "components/viz/service/display_embedder/skia_render_copy_results.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/display_compositor_memory_and_task_controller_on_gpu.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/buildflags.h"
#include "skia/ext/rgba_to_yuva.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/core/SkYUVAInfo.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/presenter.h"
#include "ui/gl/progress_reporter.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display_embedder/skia_output_device_dcomp.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/viz/service/display_embedder/skia_output_device_vulkan_secondary_cb.h"
#endif
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#if BUILDFLAG(OZONE_PLATFORM_X11)
#define USE_OZONE_PLATFORM_X11
#endif
#endif

#if (BUILDFLAG(ENABLE_VULKAN) || BUILDFLAG(SKIA_USE_DAWN)) && \
    defined(USE_OZONE_PLATFORM_X11)
#include "components/viz/service/display_embedder/skia_output_device_x11.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display_embedder/skia_output_device_dawn.h"
#endif
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "components/viz/service/display_embedder/output_presenter_fuchsia.h"
#endif

namespace viz {

namespace {

template <typename... Args>
void PostAsyncTaskRepeatedly(
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
    const base::RepeatingCallback<void(Args...)>& callback,
    Args... args) {
  // Callbacks generated by this function may be executed asynchronously
  // (e.g. by presentation feedback) after |impl_on_gpu| has been destroyed.
  if (impl_on_gpu)
    impl_on_gpu->PostTaskToClientThread(base::BindOnce(callback, args...));
}

template <typename... Args>
base::RepeatingCallback<void(Args...)> CreateSafeRepeatingCallback(
    base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
    const base::RepeatingCallback<void(Args...)>& callback) {
  return base::BindRepeating(&PostAsyncTaskRepeatedly<Args...>, impl_on_gpu,
                             callback);
}

void FailedSkiaFlush(base::StringPiece msg) {
  static auto* kCrashKey = base::debug::AllocateCrashKeyString(
      "sk_flush_failed", base::debug::CrashKeySize::Size64);
  base::debug::SetCrashKeyString(kCrashKey, msg);
  LOG(ERROR) << msg;
}

#if BUILDFLAG(ENABLE_VULKAN)
// Returns whether SkiaOutputDeviceX11 can be instantiated on this platform.
bool MayFallBackToSkiaOutputDeviceX11() {
#if BUILDFLAG(IS_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .skia_can_fall_back_to_x11;
#else
  return false;
#endif  // BUILDFLAG(IS_OZONE)
}
#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace

SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::PromiseImageAccessHelper(
    SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
    : impl_on_gpu_(impl_on_gpu) {}

SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::
    ~PromiseImageAccessHelper() {
  DCHECK(image_contexts_.empty() || impl_on_gpu_->was_context_lost());
}

void SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::BeginAccess(
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  // Only Vulkan needs semaphores.
  if (impl_on_gpu_->context_state_->GrContextIsVulkan()) {
    CHECK(begin_semaphores);
    CHECK(end_semaphores);
    begin_semaphores->reserve(image_contexts.size());
    // We may need one more space for the swap buffer semaphore.
    end_semaphores->reserve(image_contexts.size() + 1);
  }
  image_contexts_.reserve(image_contexts.size() + image_contexts_.size());
  image_contexts_.insert(image_contexts.begin(), image_contexts.end());
  impl_on_gpu_->BeginAccessImages(std::move(image_contexts), begin_semaphores,
                                  end_semaphores);
}

void SkiaOutputSurfaceImplOnGpu::PromiseImageAccessHelper::EndAccess() {
  impl_on_gpu_->EndAccessImages(image_contexts_);
  image_contexts_.clear();
}

namespace {

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    SkiaOutputSurfaceDependency* deps,
    gpu::CommandBufferId command_buffer_id,
    gpu::SequenceId sequence_id) {
  return deps->GetSyncPointManager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      sequence_id);
}

std::unique_ptr<gpu::SharedImageFactory> CreateSharedImageFactory(
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker) {
  return std::make_unique<gpu::SharedImageFactory>(
      deps->GetGpuPreferences(), deps->GetGpuDriverBugWorkarounds(),
      deps->GetGpuFeatureInfo(), deps->GetSharedContextState().get(),
      deps->GetSharedImageManager(), memory_tracker,
      /*is_for_display_compositor=*/true);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(SkiaOutputSurfaceDependency* deps,
                                       gpu::MemoryTracker* memory_tracker) {
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      deps->GetSharedImageManager(), memory_tracker);
}

}  // namespace

SkiaOutputSurfaceImplOnGpu::ReleaseCurrent::ReleaseCurrent(
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::SharedContextState> context_state)
    : gl_surface_(gl_surface), context_state_(context_state) {}

SkiaOutputSurfaceImplOnGpu::ReleaseCurrent::~ReleaseCurrent() {
  if (context_state_ && gl_surface_)
    context_state_->ReleaseCurrent(gl_surface_.get());
}

// static
std::unique_ptr<SkiaOutputSurfaceImplOnGpu> SkiaOutputSurfaceImplOnGpu::Create(
    SkiaOutputSurfaceDependency* deps,
    const RendererSettings& renderer_settings,
    const gpu::SequenceId sequence_id,
    gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* shared_gpu_deps,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    BufferPresentedCallback buffer_presented_callback,
    ContextLostCallback context_lost_callback,
    ScheduleGpuTaskCallback schedule_gpu_task,
    GpuVSyncCallback gpu_vsync_callback,
    AddChildWindowToBrowserCallback add_child_window_to_browser_callback) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::Create");

  auto context_state = deps->GetSharedContextState();
  if (!context_state)
    return nullptr;

  // Even with Vulkan/Dawn compositing, the SharedImageFactory constructor
  // always initializes a GL-backed SharedImage factory to fall back on.
  // Creating the GLTextureImageBackingFactory invokes GL API calls, so
  // we need to ensure there is a current GL context.
  if (!context_state->MakeCurrent(nullptr, true /* need_gl */)) {
    LOG(ERROR) << "Failed to make current during initialization.";
    return nullptr;
  }
  context_state->set_need_context_state_reset(true);

  auto impl_on_gpu = std::make_unique<SkiaOutputSurfaceImplOnGpu>(
      base::PassKey<SkiaOutputSurfaceImplOnGpu>(), deps,
      context_state->feature_info(), renderer_settings, sequence_id,
      shared_gpu_deps, std::move(did_swap_buffer_complete_callback),
      std::move(buffer_presented_callback), std::move(context_lost_callback),
      std::move(schedule_gpu_task), std::move(gpu_vsync_callback),
      std::move(add_child_window_to_browser_callback));
  if (!impl_on_gpu->Initialize())
    return nullptr;

  return impl_on_gpu;
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    base::PassKey<SkiaOutputSurfaceImplOnGpu> /* pass_key */,
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
    AddChildWindowToBrowserCallback add_child_window_to_browser_callback)
    : dependency_(std::move(deps)),
      shared_gpu_deps_(shared_gpu_deps),
      feature_info_(std::move(feature_info)),
      sync_point_client_state_(
          CreateSyncPointClientState(dependency_,
                                     shared_gpu_deps_->command_buffer_id(),
                                     sequence_id)),
      shared_image_factory_(
          CreateSharedImageFactory(dependency_,
                                   shared_gpu_deps_->memory_tracker())),
      shared_image_representation_factory_(
          CreateSharedImageRepresentationFactory(
              dependency_,
              shared_gpu_deps_->memory_tracker())),
      vulkan_context_provider_(dependency_->GetVulkanContextProvider()),
      dawn_context_provider_(dependency_->GetDawnContextProvider()),
      renderer_settings_(renderer_settings),
      did_swap_buffer_complete_callback_(
          std::move(did_swap_buffer_complete_callback)),
      context_lost_callback_(std::move(context_lost_callback)),
      schedule_gpu_task_(std::move(schedule_gpu_task)),
      gpu_vsync_callback_(std::move(gpu_vsync_callback)),
      add_child_window_to_browser_callback_(
          std::move(add_child_window_to_browser_callback)),
      gpu_preferences_(dependency_->GetGpuPreferences()),
      async_read_result_lock_(base::MakeRefCounted<AsyncReadResultLock>()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
  buffer_presented_callback_ = CreateSafeRepeatingCallback(
      weak_ptr_, std::move(buffer_presented_callback));
}

void SkiaOutputSurfaceImplOnGpu::ReleaseAsyncReadResultHelpers() {
  base::AutoLock auto_lock(async_read_result_lock_->lock());
  for (auto* helper : async_read_result_helpers_)
    helper->reset();
  async_read_result_helpers_.clear();
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  TRACE_EVENT0("cc", __PRETTY_FUNCTION__);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We need to have context current or lost during the destruction.
  bool has_context = false;
  if (context_state_) {
    context_state_->RemoveContextLostObserver(this);
    has_context = MakeCurrent(/*need_framebuffer=*/false);
    if (has_context) {
      release_current_last_.emplace(gl_surface_, context_state_);
    }
  }

  DCHECK(copy_output_images_.empty() || context_state_)
      << "We must have a valid context if copy requests were serviced";
  copy_output_images_.clear();

  // |scoped_output_device_paint_| needs |output_device_|, so release it first.
  scoped_output_device_paint_.reset();

  // |output_device_| may still need |shared_image_factory_|, so release it
  // first.
  output_device_.reset();

  // Clear any open accesses before destroying the skia representations.
  overlay_pass_accesses_.clear();
  skia_representations_.clear();

  // Destroy shared images created by this class.
  shared_image_factory_->DestroyAllSharedImages(has_context);

  // Since SharedImageFactory also has a reference to ImplOnGpu's member
  // SharedContextState, we need to explicitly invoke the factory's destructor
  // before deleting ImplOnGpu's other member variables.
  shared_image_factory_.reset();
  if (has_context && gr_context()) {
    TRACE_EVENT0("viz", "Cleanup");
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }
    // This ensures any outstanding callbacks for promise images are
    // performed.
    GrFlushInfo flush_info = {};
    gpu::AddVulkanCleanupTaskForSkiaFlush(context_state_->vk_context_provider(),
                                          &flush_info);
    gl::ScopedProgressReporter scoped_process_reporter(
        context_state_->progress_reporter());
    gr_context()->flush(flush_info);
    gr_context()->submit(true);

#if BUILDFLAG(ENABLE_VULKAN)
    // No frame will come for us, make sure that all the cleanup is done.
    if (base::FeatureList::IsEnabled(features::kGpuCleanupInBackground) &&
        context_state_->GrContextIsVulkan()) {
      DCHECK(context_state_->vk_context_provider());
      auto* fence_helper = context_state_->vk_context_provider()
                               ->GetDeviceQueue()
                               ->GetFenceHelper();
      fence_helper->PerformImmediateCleanup();
    }
#endif
  }

  sync_point_client_state_->Destroy();

  // Release all ongoing AsyncReadResults.
  ReleaseAsyncReadResultHelpers();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(const SkImageInfo& image_info,
                                         const gfx::ColorSpace& color_space,
                                         int sample_count,
                                         float device_scale_factor,
                                         gfx::OverlayTransform transform) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::Reshape");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gr_context() || graphite_context());

  if (context_is_lost_)
    return;

  size_ = gfx::SkISizeToSize(image_info.dimensions());
  if (!output_device_->Reshape(image_info, color_space, sample_count,
                               device_scale_factor, transform)) {
    MarkContextLost(CONTEXT_LOST_RESHAPE_FAILED);
  }
}

void SkiaOutputSurfaceImplOnGpu::DrawOverdraw(
    sk_sp<GrDeferredDisplayList> overdraw_ddl,
    SkCanvas& canvas) {
  DCHECK(overdraw_ddl);

  sk_sp<SkSurface> overdraw_surface = SkSurfaces::RenderTarget(
      gr_context(), overdraw_ddl->characterization(), skgpu::Budgeted::kNo);
  skgpu::ganesh::DrawDDL(overdraw_surface, overdraw_ddl);
  destroy_after_swap_.push_back(std::move(overdraw_ddl));

  SkPaint paint;
  sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

  paint.setColorFilter(SkiaHelper::MakeOverdrawColorFilter());
  // TODO(xing.xu): move below to the thread where skia record happens.
  canvas.drawImage(overdraw_image.get(), 0, 0, SkSamplingOptions(), &paint);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    sk_sp<GrDeferredDisplayList> ddl,
    sk_sp<GrDeferredDisplayList> overdraw_ddl,
    std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
    absl::optional<gfx::Rect> draw_rectangle) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!scoped_output_device_paint_);

  if (context_is_lost_)
    return;

  if (!ddl && !graphite_recording) {
    MarkContextLost(CONTEXT_LOST_UNKNOWN);
    return;
  }

  if (draw_rectangle) {
    if (!output_device_->SetDrawRectangle(*draw_rectangle)) {
      MarkContextLost(
          ContextLostReason::CONTEXT_LOST_SET_DRAW_RECTANGLE_FAILED);
      return;
    }
  }

  // We do not reset scoped_output_device_paint_ after drawing the ddl until
  // SwapBuffers() is called, because we may need access to output_sk_surface()
  // for CopyOutput().
  scoped_output_device_paint_ = output_device_->BeginScopedPaint();
  if (!scoped_output_device_paint_) {
    // For debugging: http://crbug.com/1364756
    // We want to figure out why beginning a write access can fail.
    base::debug::DumpWithoutCrashing();
    MarkContextLost(ContextLostReason::CONTEXT_LOST_BEGIN_PAINT_FAILED);
    return;
  }

  if (graphite_recording) {
    CHECK(return_release_fence_cb.is_null());
    promise_image_access_helper_.BeginAccess(std::move(image_contexts),
                                             /*begin_semaphores=*/nullptr,
                                             /*end_semaphores=*/nullptr);
    if (!scoped_output_device_paint_->Draw(std::move(graphite_recording),
                                           std::move(on_finished))) {
      FailedSkiaFlush("Graphite insertRecording failed.");
    }
    return;
  }

  {
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }
    // TODO(crbug.com/1434131): Implement resource cleanup for Graphite.
    dependency_->ScheduleGrContextCleanup();

    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    promise_image_access_helper_.BeginAccess(
        std::move(image_contexts), &begin_semaphores, &end_semaphores);
    if (!begin_semaphores.empty()) {
      auto result = scoped_output_device_paint_->Wait(
          begin_semaphores.size(), begin_semaphores.data(),
          /*delete_semaphores_after_wait=*/false);
      DCHECK(result);
    }

    // Draw will only fail if the SkSurface and SkDDL are incompatible.
    bool draw_success = scoped_output_device_paint_->Draw(ddl);
    DCHECK(draw_success);

    destroy_after_swap_.emplace_back(std::move(ddl));

    if (overdraw_ddl) {
      DrawOverdraw(std::move(overdraw_ddl),
                   *scoped_output_device_paint_->GetCanvas());
    }

    auto end_paint_semaphores =
        scoped_output_device_paint_->TakeEndPaintSemaphores();
    end_semaphores.insert(end_semaphores.end(), end_paint_semaphores.begin(),
                          end_paint_semaphores.end());

#if BUILDFLAG(ENABLE_VULKAN)
    // Semaphores for release fences for vulkan should be created before flush.
    if (!return_release_fence_cb.is_null() && is_using_vulkan()) {
      const bool result = CreateAndStoreExternalSemaphoreVulkan(end_semaphores);
      // A release fence will be created on submit as some platforms may use
      // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT handle types for their
      // external semaphore. That handle type has COPY transference. Vulkan spec
      // says that semaphore has to be signaled, or have an associated semaphore
      // signal operation pending execution. Thus, delay importing the handle
      // and creating the fence until commands are submitted.
      pending_release_fence_cbs_.emplace_back(
          result ? end_semaphores.back() : GrBackendSemaphore(),
          std::move(return_release_fence_cb));
      return_release_fence_cb.Reset();
    }
#endif

    const bool end_semaphores_empty = end_semaphores.empty();
    auto result = scoped_output_device_paint_->Flush(vulkan_context_provider_,
                                                     std::move(end_semaphores),
                                                     std::move(on_finished));

    if (result != GrSemaphoresSubmitted::kYes &&
        !(begin_semaphores.empty() && end_semaphores_empty)) {
      if (!return_release_fence_cb.is_null()) {
        std::move(return_release_fence_cb).Run(gfx::GpuFenceHandle());
        return_release_fence_cb.Reset();
      }
      // TODO(penghuang): handle vulkan device lost.
      FailedSkiaFlush("output_sk_surface()->flush() failed.");
      return;
    }

    gfx::GpuFenceHandle release_fence;
    if (!return_release_fence_cb.is_null() && is_using_gl()) {
      DCHECK(release_fence.is_null());
      release_fence = CreateReleaseFenceForGL();
    }

    if (!return_release_fence_cb.is_null()) {
      // Returning fences for Vulkan is delayed. See the comment above.
      DCHECK(!is_using_vulkan());
      std::move(return_release_fence_cb).Run(std::move(release_fence));
      return_release_fence_cb.Reset();
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::ScheduleOutputSurfaceAsOverlay(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane&
        output_surface_plane) {
  DCHECK(!output_surface_plane_);
  output_surface_plane_ = output_surface_plane;
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(OutputSurfaceFrame frame) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::SwapBuffers");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SwapBuffersInternal(std::move(frame));
}

void SkiaOutputSurfaceImplOnGpu::EnsureMinNumberOfBuffers(int n) {
  if (!output_device_->EnsureMinNumberOfBuffers(n)) {
    MarkContextLost(CONTEXT_LOST_ALLOCATE_FRAME_BUFFERS_FAILED);
  }
}

void SkiaOutputSurfaceImplOnGpu::SetDependenciesResolvedTimings(
    base::TimeTicks task_ready) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  output_device_->SetDependencyTimings(task_ready);
}

void SkiaOutputSurfaceImplOnGpu::SetDrawTimings(base::TimeTicks task_posted) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  output_device_->SetDrawTimings(task_posted, base::TimeTicks::Now());
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffersSkipped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SwapBuffersInternal(absl::nullopt);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    const gpu::Mailbox& mailbox,
    sk_sp<GrDeferredDisplayList> ddl,
    sk_sp<GrDeferredDisplayList> overdraw_ddl,
    std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
    std::vector<ImageContextImpl*> image_contexts,
    std::vector<gpu::SyncToken> sync_tokens,
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb,
    const gfx::Rect& update_rect,
    bool is_overlay) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (context_is_lost_)
    return;

  if (!ddl && !graphite_recording) {
    MarkContextLost(CONTEXT_LOST_UNKNOWN);
    return;
  }

  gpu::SkiaImageRepresentation* skia_representation =
      GetSkiaRepresentation(mailbox);
  if (!skia_representation) {
    MarkContextLost(CONTEXT_LOST_RESHAPE_FAILED);
    return;
  }

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  int sample_count = 0;
  SkSurfaceProps surface_props;
  if (ddl) {
    sample_count = ddl->characterization().sampleCount();
    surface_props = ddl->characterization().surfaceProps();
  }
  auto local_scoped_access = skia_representation->BeginScopedWriteAccess(
      sample_count, surface_props, update_rect, &begin_semaphores,
      &end_semaphores,
      gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!local_scoped_access) {
    MarkContextLost(CONTEXT_LOST_UNKNOWN);
    return;
  }

  absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
  if (gr_context() && dependency_->GetGrShaderCache()) {
    // TODO(crbug.com/1434131): Implement pipeline caching for Graphite.
    cache_use.emplace(dependency_->GetGrShaderCache(),
                      gpu::kDisplayCompositorClientId);
  }

  // When CompositorGpuThread is disabled, this cleanup for gpu main
  // thread context already happens in raster decoder and hence we do not want
  // to do additional cleanup here on same context. That results in more skia
  // reported memory on mac - crbug.com/1396279.
  // When CompositorGpuThread is enabled, we want to do cleanup here for every
  // render pass instead of once per frame as it results in less outstanding
  // allocated memory.
  if (dependency_->IsUsingCompositorGpuThread())
    dependency_->ScheduleGrContextCleanup();

  // Only overlayed images require end_semaphore synchronization.
  DCHECK(is_overlay || end_semaphores.empty());

  // If this render pass is an overlay we need to hang onto the scoped write
  // access until PostSubmit(), so we'll transfer ownership to a member
  // variable. This is necessary because in Vulkan on Android we need to wait
  // until submit is called before ending the ScopedWriteAccess. We'll also
  // create a raw pointer to it first for use within this function.
  gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_access =
      local_scoped_access.get();
  // DComp only allows drawing to a single surface at a time and does not
  // require us to keep the write accesses open through submit.
  const bool is_dcomp_surface =
      (local_scoped_access->representation()->usage() &
       gpu::SHARED_IMAGE_USAGE_SCANOUT_DCOMP_SURFACE) != 0;
  if (is_overlay && !is_dcomp_surface) {
    DCHECK(!overlay_pass_accesses_.contains(mailbox));
    overlay_pass_accesses_.emplace(mailbox, std::move(local_scoped_access));
  }

  DLOG_IF(WARNING, update_rect.IsEmpty() && !skia_representation->IsCleared())
      << "FinishPaintRenderPass called with empty update_rect on an "
         "uninitialized backing.";

  SkSurface* surface = scoped_access->surface();
  DCHECK(surface);

  promise_image_access_helper_.BeginAccess(std::move(image_contexts),
                                           &begin_semaphores, &end_semaphores);

  if (graphite_recording) {
    skgpu::graphite::InsertRecordingInfo info;
    info.fRecording = graphite_recording.get();
    info.fTargetSurface = surface;
    if (on_finished) {
      gpu::AddCleanupTaskForGraphiteRecording(std::move(on_finished), &info);
    }
    graphite_context()->insertRecording(info);
    graphite_context()->submit();
    skia_representation->SetCleared();
    return;
  }

  if (!begin_semaphores.empty()) {
    auto result =
        surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                      /*deleteSemaphoresAfterWait=*/false);
    DCHECK(result);
  }
  skgpu::ganesh::DrawDDL(surface, ddl);
  skia_representation->SetCleared();
  destroy_after_swap_.emplace_back(std::move(ddl));

  if (overdraw_ddl) {
    DrawOverdraw(std::move(overdraw_ddl), *surface->getCanvas());
  }

#if BUILDFLAG(ENABLE_VULKAN)
  // Semaphores for release fences for vulkan should be created before flush.
  if (!return_release_fence_cb.is_null() && is_using_vulkan()) {
    const bool result = CreateAndStoreExternalSemaphoreVulkan(end_semaphores);
    // A release fence will be created on submit as some platforms may use
    // VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT handle types for their
    // external semaphore. That handle type has COPY transference. Vulkan spec
    // says that semaphore has to be signaled, or have an associated semaphore
    // signal operation pending execution. Thus, delay importing the handle
    // and creating the fence until commands are submitted.
    pending_release_fence_cbs_.emplace_back(
        result ? end_semaphores.back() : GrBackendSemaphore(),
        std::move(return_release_fence_cb));
    return_release_fence_cb.Reset();
  }
#endif
  GrFlushInfo flush_info = {
      .fNumSemaphores = end_semaphores.size(),
      .fSignalSemaphores = end_semaphores.data(),
  };
  gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_, &flush_info);
  if (on_finished) {
    gpu::AddCleanupTaskForSkiaFlush(std::move(on_finished), &flush_info);
  }

  gl::ScopedProgressReporter scoped_process_reporter(
      context_state_->progress_reporter());

  // This flushes paint ops first, then applies Vulkan transition layouts and
  // then submit semaphores to signal.
  gr_context()->flush(surface, {});
  scoped_access->ApplyBackendSurfaceEndState();
  auto result = gr_context()->flush(surface, flush_info, nullptr);

  if (result != GrSemaphoresSubmitted::kYes &&
      !(begin_semaphores.empty() && end_semaphores.empty())) {
    if (!return_release_fence_cb.is_null()) {
      std::move(return_release_fence_cb).Run(gfx::GpuFenceHandle());
      return_release_fence_cb.Reset();
    }
    // TODO(penghuang): handle vulkan device lost.
    FailedSkiaFlush("offscreen.surface()->flush() failed.");
    return;
  }

  // If GL is used, create the release fence after flush.
  gfx::GpuFenceHandle release_fence;
  if (!return_release_fence_cb.is_null() && is_using_gl()) {
    DCHECK(release_fence.is_null());
    release_fence = CreateReleaseFenceForGL();
  }

  if (!return_release_fence_cb.is_null()) {
    // Returning fences for Vulkan is delayed. See the comment above.
    DCHECK(!is_using_vulkan());
    std::move(return_release_fence_cb).Run(std::move(release_fence));
    return_release_fence_cb.Reset();
  }

  bool sync_cpu =
      gpu::ShouldVulkanSyncCpuForSkiaSubmit(vulkan_context_provider_);
  if (sync_cpu) {
    gr_context()->submit(true);
  }
}

std::unique_ptr<gpu::SkiaImageRepresentation>
SkiaOutputSurfaceImplOnGpu::CreateSharedImageRepresentationSkia(
    SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    base::StringPiece debug_label) {
  constexpr uint32_t kUsage = gpu::SHARED_IMAGE_USAGE_GLES2 |
                              gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
                              gpu::SHARED_IMAGE_USAGE_RASTER |
                              gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                              gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE;

  gpu::Mailbox mailbox = gpu::Mailbox::GenerateForSharedImage();
  bool result = shared_image_factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, gpu::kNullSurfaceHandle, kUsage,
      std::string(debug_label));
  if (!result) {
    DLOG(ERROR) << "Failed to create shared image.";
    return nullptr;
  }

  auto representation = dependency_->GetSharedImageManager()->ProduceSkia(
      mailbox, context_state_->memory_type_tracker(), context_state_);
  shared_image_factory_->DestroySharedImage(mailbox);

  return representation;
}

void SkiaOutputSurfaceImplOnGpu::CopyOutputRGBAInMemory(
    SkSurface* surface,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    const SkIRect& src_rect,
    SkSurface::RescaleMode rescale_mode,
    bool is_downscale_or_identity_in_both_dimensions,
    std::unique_ptr<CopyOutputRequest> request) {
  // If we can't convert |color_space| to a SkColorSpace (e.g. PIECEWISE_HDR),
  // request a sRGB destination color space for the copy result instead.
  gfx::ColorSpace dest_color_space = color_space;
  sk_sp<SkColorSpace> sk_color_space = color_space.ToSkColorSpace();
  if (!sk_color_space) {
    dest_color_space = gfx::ColorSpace::CreateSRGB();
  }

  // TODO(https://bugs.chromium.org/p/skia/issues/detail?id=14389):
  // BGRA is not supported on iOS, so explicitly request RGBA here. This should
  // not prevent readback, however, so once that is fixed, this code could be
  // removed.
  auto color_type =
#if BUILDFLAG(IS_IOS)
      kRGBA_8888_SkColorType;
#else
      kN32_SkColorType;
#endif  // BUILDFLAG(IS_IOS)

  SkImageInfo dst_info = SkImageInfo::Make(
      geometry.result_selection.width(), geometry.result_selection.height(),
      color_type, kPremul_SkAlphaType, sk_color_space);
  std::unique_ptr<ReadPixelsContext> context =
      std::make_unique<ReadPixelsContext>(std::move(request),
                                          geometry.result_selection,
                                          dest_color_space, weak_ptr_);
  // Skia readback could be synchronous. Incremement counter in case
  // ReadbackCompleted is called immediately.
  num_readbacks_pending_++;
  if (auto* graphite_context = context_state_->graphite_context()) {
    // SkImage/SkSurface asyncRescaleAndReadPixels methods won't be implemented
    // for Graphite. Instead the equivalent methods will be on Graphite Context.
    graphite_context->asyncRescaleAndReadPixels(
        surface, dst_info, src_rect, SkSurface::RescaleGamma::kSrc,
        rescale_mode, &CopyOutputResultSkiaRGBA::OnReadbackDone,
        context.release());
  } else {
    surface->asyncRescaleAndReadPixels(
        dst_info, src_rect, SkSurface::RescaleGamma::kSrc, rescale_mode,
        &CopyOutputResultSkiaRGBA::OnReadbackDone, context.release());
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutputRGBA(
    SkSurface* surface,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    const SkIRect& src_rect,
    SkSurface::RescaleMode rescale_mode,
    bool is_downscale_or_identity_in_both_dimensions,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_EQ(request->result_format(), CopyOutputRequest::ResultFormat::RGBA);

  switch (request->result_destination()) {
    case CopyOutputRequest::ResultDestination::kSystemMemory:
      CopyOutputRGBAInMemory(
          surface, geometry, color_space, src_rect, rescale_mode,
          is_downscale_or_identity_in_both_dimensions, std::move(request));
      break;
    case CopyOutputRequest::ResultDestination::kNativeTextures: {
      auto representation = CreateSharedImageRepresentationSkia(
          SinglePlaneFormat::kRGBA_8888,
          gfx::Size(geometry.result_bounds.width(),
                    geometry.result_bounds.height()),
          color_space, "CopyOutputInMemory");

      if (!representation) {
        DLOG(ERROR) << "Failed to create shared image.";
        return;
      }

      SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;

      auto scoped_write = representation->BeginScopedWriteAccess(
          /*final_msaa_count=*/1, surface_props, gfx::Rect(), &begin_semaphores,
          &end_semaphores,
          gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

      absl::optional<SkVector> scaling;
      if (request->is_scaled()) {
        scaling =
            SkVector::Make(static_cast<SkScalar>(request->scale_to().x()) /
                               request->scale_from().x(),
                           static_cast<SkScalar>(request->scale_to().y()) /
                               request->scale_from().y());
      }

      scoped_write->surface()->wait(begin_semaphores.size(),
                                    begin_semaphores.data());

      RenderSurface(surface, src_rect, scaling,
                    is_downscale_or_identity_in_both_dimensions,
                    scoped_write->surface());

      bool should_submit = !end_semaphores.empty();

      if (!FlushSurface(scoped_write->surface(), end_semaphores,
                        scoped_write.get())) {
        // TODO(penghuang): handle vulkan device lost.
        FailedSkiaFlush("CopyOutputRGBA dest_surface->flush()");
        return;
      }

      if (should_submit && !gr_context()->submit()) {
        DLOG(ERROR) << "CopyOutputRGBA gr_context->submit() failed";
        return;
      }

      representation->SetCleared();

      // Grab the mailbox before we transfer `representation`'s ownership:
      gpu::Mailbox mailbox = representation->mailbox();

      CopyOutputResult::ReleaseCallbacks release_callbacks;
      release_callbacks.push_back(
          CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
              std::move(representation)));

      request->SendResult(std::make_unique<CopyOutputTextureResult>(
          CopyOutputResult::Format::RGBA, geometry.result_bounds,
          CopyOutputResult::TextureResult(mailbox, gpu::SyncToken(),
                                          color_space),
          std::move(release_callbacks)));
      break;
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::RenderSurface(
    SkSurface* surface,
    const SkIRect& source_selection,
    absl::optional<SkVector> scaling,
    bool is_downscale_or_identity_in_both_dimensions,
    SkSurface* dest_surface) {
  SkCanvas* dest_canvas = dest_surface->getCanvas();
  int state_depth = dest_canvas->save();

  if (scaling.has_value()) {
    dest_canvas->scale(scaling->x(), scaling->y());
  }

  dest_canvas->clipRect(SkRect::MakeXYWH(0, 0, source_selection.width(),
                                         source_selection.height()));
  // TODO(b/197353769): Ideally, we should simply use a kSrc blending mode,
  // but for some reason, this triggers some antialiasing code that causes
  // various Vulkan tests to fail. We should investigate this and replace
  // this clear with blend mode.
  if (surface->imageInfo().alphaType() != kOpaque_SkAlphaType) {
    dest_canvas->clear(SK_ColorTRANSPARENT);
  }

  auto sampling =
      is_downscale_or_identity_in_both_dimensions
          ? SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear)
          : SkSamplingOptions({1.0f / 3, 1.0f / 3});
  surface->draw(dest_canvas, -source_selection.x(), -source_selection.y(),
                sampling, /*paint=*/nullptr);

  dest_canvas->restoreToCount(state_depth);
}

bool SkiaOutputSurfaceImplOnGpu::FlushSurface(
    SkSurface* surface,
    std::vector<GrBackendSemaphore>& end_semaphores,
    gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
    GrGpuFinishedProc finished_proc,
    GrGpuFinishedContext finished_context) {
  return FlushInternal(surface, end_semaphores, scoped_write_access,
                       finished_proc, finished_context);
}

bool SkiaOutputSurfaceImplOnGpu::FlushContext(
    std::vector<GrBackendSemaphore>& end_semaphores,
    gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
    GrGpuFinishedProc finished_proc,
    GrGpuFinishedContext finished_context) {
  return FlushInternal(/*surface=*/nullptr, end_semaphores, scoped_write_access,
                       finished_proc, finished_context);
}

bool SkiaOutputSurfaceImplOnGpu::FlushInternal(
    SkSurface* surface,
    std::vector<GrBackendSemaphore>& end_semaphores,
    gpu::SkiaImageRepresentation::ScopedWriteAccess* scoped_write_access,
    GrGpuFinishedProc finished_proc,
    GrGpuFinishedContext finished_context) {
  gl::ScopedProgressReporter scoped_process_reporter(
      context_state_->progress_reporter());
  if (gr_context()) {
    GrFlushInfo flush_info;
    flush_info.fNumSemaphores = end_semaphores.size();
    flush_info.fSignalSemaphores = end_semaphores.data();
    flush_info.fFinishedProc = finished_proc;
    flush_info.fFinishedContext = finished_context;
    gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider_,
                                          &flush_info);
    GrSemaphoresSubmitted flush_result =
        surface ? gr_context()->flush(surface, flush_info)
                : gr_context()->flush(flush_info);
    if (scoped_write_access) {
      scoped_write_access->ApplyBackendSurfaceEndState();
    }
    return flush_result == GrSemaphoresSubmitted::kYes ||
           end_semaphores.empty();
  }

  CHECK(graphite_recorder());
  auto recording = graphite_recorder()->snap();
  if (recording) {
    skgpu::graphite::InsertRecordingInfo info = {};
    info.fRecording = recording.get();
    return graphite_context()->insertRecording(info);
  }
  return false;
}

SkiaOutputSurfaceImplOnGpu::MailboxAccessData::MailboxAccessData() = default;
SkiaOutputSurfaceImplOnGpu::MailboxAccessData::~MailboxAccessData() = default;

bool SkiaOutputSurfaceImplOnGpu::CreateSurfacesForNV12Planes(
    const SkYUVAInfo& yuva_info,
    const gfx::ColorSpace& color_space,
    std::array<MailboxAccessData, CopyOutputResult::kNV12MaxPlanes>&
        mailbox_access_datas,
    bool is_multiplane) {
  std::array<SkISize, SkYUVAInfo::kMaxPlanes> plane_dimensions;
  int plane_number = yuva_info.planeDimensions(plane_dimensions.data());

  DCHECK_EQ(CopyOutputResult::kNV12MaxPlanes, static_cast<size_t>(plane_number))
      << "We expect SkYUVAInfo to describe an NV12 data, which contains 2 "
         "planes!";

  int num_mailboxes = is_multiplane ? 1 : plane_number;

  for (int i = 0; i < num_mailboxes; ++i) {
    MailboxAccessData& mailbox_access_data = mailbox_access_datas[i];
    const SkISize& plane_size = plane_dimensions[i];

    const auto format = is_multiplane ? MultiPlaneFormat::kNV12
                        : (i == 0)    ? SinglePlaneFormat::kR_8
                                      : SinglePlaneFormat::kRG_88;
    auto representation = CreateSharedImageRepresentationSkia(
        format, gfx::SkISizeToSize(plane_size), color_space,
        "SurfacesForNV12Planes");
    if (!representation) {
      return false;
    }

    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

    std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>
        scoped_write = representation->BeginScopedWriteAccess(
            /*final_msaa_count=*/1, surface_props,
            &mailbox_access_data.begin_semaphores,
            &mailbox_access_data.end_semaphores,
            gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

    if (is_multiplane) {
      // NOTE: For multiplanar SharedImage there is only one set of semaphores
      // for all of the planes. Rather than waiting on one of the planes we wait
      // on the context, which facilitates flushing later: we first flush the
      // individual surfaces without signaling followed by flushing+signaling
      // the context.
      gr_context()->wait(mailbox_access_data.begin_semaphores.size(),
                         mailbox_access_data.begin_semaphores.data());
    } else {
      SkSurface* dest_surface = scoped_write->surface();
      dest_surface->wait(mailbox_access_data.begin_semaphores.size(),
                         mailbox_access_data.begin_semaphores.data());
    }

    // Semaphores have already been populated in `mailbox_access_data`.
    // Set the remaining fields.
    mailbox_access_data.mailbox = representation->mailbox();
    mailbox_access_data.representation = std::move(representation);
    mailbox_access_data.scoped_write = std::move(scoped_write);
    mailbox_access_data.size = plane_size;
  }

  return true;
}

bool SkiaOutputSurfaceImplOnGpu::ImportSurfacesForNV12Planes(
    const BlitRequest& blit_request,
    std::array<MailboxAccessData, CopyOutputResult::kNV12MaxPlanes>&
        mailbox_access_datas,
    bool is_multiplane) {
  size_t num_mailboxes = is_multiplane ? 1 : CopyOutputResult::kNV12MaxPlanes;
  for (size_t i = 0; i < num_mailboxes; ++i) {
    const gpu::MailboxHolder& mailbox_holder = blit_request.mailbox(i);

    // Should never happen, mailboxes are validated when setting blit request on
    // a CopyOutputResult and we only access `kNV12MaxPlanes` mailboxes.
    DCHECK(!mailbox_holder.mailbox.IsZero());

    MailboxAccessData& mailbox_access_data = mailbox_access_datas[i];

    auto representation = dependency_->GetSharedImageManager()->ProduceSkia(
        mailbox_holder.mailbox, context_state_->memory_type_tracker(),
        context_state_);
    if (!representation) {
      return false;
    }

    SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

    std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>
        scoped_write = representation->BeginScopedWriteAccess(
            /*final_msaa_count=*/1, surface_props,
            &mailbox_access_data.begin_semaphores,
            &mailbox_access_data.end_semaphores,
            gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);

    if (is_multiplane) {
      // NOTE: For multiplanar SharedImage there is only one set of semaphores
      // for all of the planes. Rather than waiting on one of the planes we wait
      // on the context, which facilitates flushing later: we first flush the
      // individual surfaces without signaling followed by flushing+signaling
      // the context.
      gr_context()->wait(mailbox_access_data.begin_semaphores.size(),
                         mailbox_access_data.begin_semaphores.data());
    } else {
      SkSurface* dest_surface = scoped_write->surface();
      dest_surface->wait(mailbox_access_data.begin_semaphores.size(),
                         mailbox_access_data.begin_semaphores.data());
    }

    // Semaphores have already been populated in `mailbox_access_data`.
    // Set the remaining fields.
    mailbox_access_data.size = gfx::SizeToSkISize(representation->size());
    mailbox_access_data.mailbox = representation->mailbox();
    mailbox_access_data.representation = std::move(representation);
    mailbox_access_data.scoped_write = std::move(scoped_write);
  }

  return true;
}

void SkiaOutputSurfaceImplOnGpu::BlendBitmapOverlays(
    SkCanvas* canvas,
    const BlitRequest& blit_request) {
  for (const BlendBitmap& blend_bitmap : blit_request.blend_bitmaps()) {
    SkPaint paint;
    paint.setBlendMode(SkBlendMode::kSrcOver);

    canvas->drawImageRect(blend_bitmap.image(),
                          gfx::RectToSkRect(blend_bitmap.source_region()),
                          gfx::RectToSkRect(blend_bitmap.destination_region()),
                          SkSamplingOptions(SkFilterMode::kLinear), &paint,
                          SkCanvas::kFast_SrcRectConstraint);
  }
}

// TODO(crbug.com/1452092): Make this path work with Graphite.
void SkiaOutputSurfaceImplOnGpu::CopyOutputNV12(
    SkSurface* surface,
    copy_output::RenderPassGeometry geometry,
    const gfx::ColorSpace& color_space,
    const SkIRect& src_rect,
    SkSurface::RescaleMode rescale_mode,
    bool is_downscale_or_identity_in_both_dimensions,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK(!request->has_blit_request() ||
         request->result_destination() ==
             CopyOutputRequest::ResultDestination::kNativeTextures)
      << "Only CopyOutputRequests that hand out native textures support blit "
         "requests!";
  DCHECK(!request->has_blit_request() || request->has_result_selection())
      << "Only CopyOutputRequests that specify result selection support blit "
         "requests!";

  // Overview:
  // 1. Try to create surfaces for NV12 planes (we know the needed size in
  // advance). If this fails, send an empty result. For requests that have a
  // blit request appended, the surfaces should be backed by caller-provided
  // textures.
  // 2. Render the desired region into a new SkSurface, taking into account
  // desired scaling and clipping.
  // 3. If blitting, honor the blend bitmap requests set by blending them onto
  // the surface produced in step 2.
  // 4. Grab an SkImage and convert it into multiple SkSurfaces created by
  // step 1, one for each plane.
  // 5. Depending on the result destination of the request, either:
  // - pass ownership of the textures to the caller (native textures result)
  // - schedule a read-back & expose its results to the caller (system memory
  // result)
  //
  // Note: in case the blit request populates the GMBs, the flow stays the same,
  // but we need to ensure that the results are only sent out after the
  // GpuMemoryBuffer is safe to map into system memory.

  // The size of the destination is passed in via `geometry.result_selection` -
  // it already takes into account the rect of the render pass that is being
  // copied, as well as area, scaling & result_selection of the `request`.
  // This represents the size of the intermediate texture that will be then
  // blitted to the destination textures.
  const gfx::Size intermediate_dst_size = geometry.result_selection.size();

  bool is_multiplane = request->result_format() ==
                       CopyOutputRequest::ResultFormat::NV12_MULTIPLANE;

  std::array<MailboxAccessData, CopyOutputResult::kNV12MaxPlanes>
      mailbox_access_datas;

  SkYUVAInfo yuva_info;

  bool destination_surfaces_ready = false;
  if (request->has_blit_request()) {
    if (request->result_selection().size() != intermediate_dst_size) {
      DLOG(WARNING)
          << __func__
          << ": result selection is different than render pass output, "
             "geometry="
          << geometry.ToString() << ", request=" << request->ToString();
      // Send empty result, we have a blit request that asks for a different
      // size than what we have available - the behavior in this case is
      // currently unspecified as we'd have to leave parts of the caller's
      // region unpopulated.
      return;
    }

    destination_surfaces_ready = ImportSurfacesForNV12Planes(
        request->blit_request(), mailbox_access_datas, is_multiplane);

    // The entire destination image size is the same as the size of the luma
    // plane of the image that was just imported:
    yuva_info = SkYUVAInfo(
        mailbox_access_datas[0].size, SkYUVAInfo::PlaneConfig::kY_UV,
        SkYUVAInfo::Subsampling::k420, kRec709_Limited_SkYUVColorSpace);

    // Check if the destination will fit in the blit target:
    const gfx::Rect blit_destination_rect(
        request->blit_request().destination_region_offset(),
        intermediate_dst_size);
    const gfx::Rect blit_target_image_rect(
        gfx::SkISizeToSize(mailbox_access_datas[0].size));

    if (!blit_target_image_rect.Contains(blit_destination_rect)) {
      // Send empty result, the blit target image is not large enough to fit the
      // results.
      return;
    }
  } else {
    yuva_info = SkYUVAInfo(gfx::SizeToSkISize(intermediate_dst_size),
                           SkYUVAInfo::PlaneConfig::kY_UV,
                           SkYUVAInfo::Subsampling::k420,
                           kRec709_Limited_SkYUVColorSpace);

    destination_surfaces_ready = CreateSurfacesForNV12Planes(
        yuva_info, color_space, mailbox_access_datas, is_multiplane);
  }

  if (!destination_surfaces_ready) {
    DVLOG(1) << "failed to create / import destination surfaces";
    // Send empty result.
    return;
  }

  // Create a destination for the scaled & clipped result:
  auto intermediate_surface = SkSurfaces::RenderTarget(
      gr_context(), skgpu::Budgeted::kYes,
      SkImageInfo::Make(gfx::SizeToSkISize(intermediate_dst_size),
                        SkColorType::kRGBA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType,
                        color_space.ToSkColorSpace()));

  if (!intermediate_surface) {
    DVLOG(1) << "failed to create surface for the intermediate texture";
    // Send empty result.
    return;
  }

  absl::optional<SkVector> scaling;
  if (request->is_scaled()) {
    scaling = SkVector::Make(static_cast<SkScalar>(request->scale_to().x()) /
                                 request->scale_from().x(),
                             static_cast<SkScalar>(request->scale_to().y()) /
                                 request->scale_from().y());
  }

  RenderSurface(surface, src_rect, scaling,
                is_downscale_or_identity_in_both_dimensions,
                intermediate_surface.get());

  if (request->has_blit_request()) {
    BlendBitmapOverlays(intermediate_surface->getCanvas(),
                        request->blit_request());
  }

  gr_context()->flush(intermediate_surface);

  auto intermediate_image = intermediate_surface->makeImageSnapshot();
  if (!intermediate_image) {
    DLOG(ERROR) << "failed to retrieve `intermediate_image`.";
    return;
  }

  // `skia::BlitRGBAToYUVA()` requires a buffer with 4 SkSurface* elements,
  // let's allocate it and populate its first 2 entries with the surfaces
  // obtained from |mailbox_access_datas|.
  std::array<SkSurface*, SkYUVAInfo::kMaxPlanes> plane_surfaces;

  // When using multiplanar SharedImage, the information for both planes is
  // contained within the single SharedImage. Otherwise, each plane is accessed
  // via its own SharedImage.
  if (is_multiplane) {
    plane_surfaces = {mailbox_access_datas[0].scoped_write->surface(0),
                      mailbox_access_datas[0].scoped_write->surface(1), nullptr,
                      nullptr};
  } else {
    plane_surfaces = {mailbox_access_datas[0].scoped_write->surface(),
                      mailbox_access_datas[1].scoped_write->surface(), nullptr,
                      nullptr};
  }

  // The region to be populated in caller's textures is derived from blit
  // request's |destination_region_offset()|, and from COR's
  // |result_selection()|. If we have a blit request, use it. Otherwise, use an
  // empty rect (which means that entire image will be used as the target of the
  // blit - this will not result in rescaling since w/o blit request present,
  // the destination image size matches the |geometry.result_selection|).
  const SkRect dst_region =
      request->has_blit_request()
          ? gfx::RectToSkRect(
                gfx::Rect(request->blit_request().destination_region_offset(),
                          intermediate_dst_size))
          : SkRect::MakeEmpty();

  // We should clear destination if BlitRequest asked to letterbox everything
  // outside of intended destination region:
  const bool clear_destination =
      request->has_blit_request()
          ? request->blit_request().letterboxing_behavior() ==
                LetterboxingBehavior::kLetterbox
          : false;
  skia::BlitRGBAToYUVA(intermediate_image.get(), plane_surfaces.data(),
                       yuva_info, dst_region, clear_destination);

  // Collect mailbox holders for the destination textures. They will be needed
  // in case the result is kNativeTextures. It happens here in order to simplify
  // the code in case we are populating the GpuMemoryBuffer-backed textures.
  // NOTE: When using multiplanar SharedImage, there is only one mailbox (rather
  // than one for each plane).
  auto second_mailbox_holder =
      is_multiplane ? gpu::MailboxHolder()
                    : gpu::MailboxHolder(mailbox_access_datas[1].mailbox,
                                         gpu::SyncToken(), GL_TEXTURE_2D);
  std::array<gpu::MailboxHolder, CopyOutputResult::kMaxPlanes> mailbox_holders =
      {
          gpu::MailboxHolder(mailbox_access_datas[0].mailbox, gpu::SyncToken(),
                             GL_TEXTURE_2D),
          second_mailbox_holder,
          gpu::MailboxHolder(),
      };

  // If we are not the ones allocating the textures, they may come from a GMB,
  // in which case we need to delay sending the results until we receive a
  // callback that the GPU work has completed - otherwise, memory-mapping the
  // GMB may not yield the latest version of the contents.
  const bool should_wait_for_gpu_work =
      request->result_destination() ==
          CopyOutputRequest::ResultDestination::kNativeTextures &&
      request->has_blit_request() &&
      request->blit_request().populates_gpu_memory_buffer();

  scoped_refptr<NV12PlanesReadyContext> nv12_planes_ready = nullptr;
  if (should_wait_for_gpu_work) {
    // Prepare a per-CopyOutputRequest context that will be responsible for
    // sending the CopyOutputResult:
    nv12_planes_ready = base::MakeRefCounted<NV12PlanesReadyContext>(
        weak_ptr_, std::move(request), geometry.result_selection,
        mailbox_holders, color_space, is_multiplane);
  }

  bool should_submit = false;
  size_t num_mailboxes = is_multiplane ? 1 : CopyOutputResult::kNV12MaxPlanes;
  for (size_t i = 0; i < num_mailboxes; ++i) {
    mailbox_access_datas[i].representation->SetCleared();

    should_submit |= !mailbox_access_datas[i].end_semaphores.empty();

    // Prepare a per-mailbox context that will notify the per-request context
    // that the GPU-side work for this mailbox has completed.
    std::unique_ptr<NV12SingleMailboxReadyContext> nv12_plane_ready =
        should_wait_for_gpu_work
            ? std::make_unique<NV12SingleMailboxReadyContext>(nv12_planes_ready)
            : nullptr;

    if (should_wait_for_gpu_work) {
      // Treat the fact that we're waiting for GPU work to finish the same way
      // as a readback request. This would allow us to nudge Skia to fire the
      // callbacks. See `SkiaOutputSurfaceImplOnGpu::CheckReadbackCompletion()`.
      ++num_readbacks_pending_;
    }

    bool flush_succeeded = false;
    if (is_multiplane) {
      // Flush the individual surfaces followed by flushing the context and
      // signaling.
      gr_context()->flush(plane_surfaces[0], GrFlushInfo());
      gr_context()->flush(plane_surfaces[1], GrFlushInfo());
      flush_succeeded = FlushContext(
          mailbox_access_datas[i].end_semaphores,
          mailbox_access_datas[i].scoped_write.get(),
          should_wait_for_gpu_work
              ? &NV12SingleMailboxReadyContext::OnMailboxReady
              : nullptr,
          should_wait_for_gpu_work ? nv12_plane_ready.release() : nullptr);
    } else {
      flush_succeeded = FlushSurface(
          plane_surfaces[i], mailbox_access_datas[i].end_semaphores,
          mailbox_access_datas[i].scoped_write.get(),
          should_wait_for_gpu_work
              ? &NV12SingleMailboxReadyContext::OnMailboxReady
              : nullptr,
          should_wait_for_gpu_work ? nv12_plane_ready.release() : nullptr);
    }

    if (!flush_succeeded) {
      // TODO(penghuang): handle vulkan device lost.
      FailedSkiaFlush("CopyOutputNV12 plane_surfaces[i]->flush()");
      return;
    }
  }

  if (should_submit && !gr_context()->submit()) {
    DLOG(ERROR) << "CopyOutputNV12 gr_context->submit() failed";
    return;
  }

  if (should_wait_for_gpu_work) {
    // Flow will continue after GPU work is done - see
    // `NV12PlanesReadyContext::OnMailboxReady()` that eventually gets called.
    return;
  }

  // We conditionally move from request (if `should_wait_for_gpu_work` is true),
  // DCHECK that we don't accidentally enter this codepath after the request was
  // moved from.
  DCHECK(request);

  switch (request->result_destination()) {
    case CopyOutputRequest::ResultDestination::kNativeTextures: {
      CopyOutputResult::ReleaseCallbacks release_callbacks;

      if (!request->has_blit_request()) {
        // In blit requests, we are not responsible for releasing the textures
        // (the issuer of the request owns them), create the callbacks only if
        // we don't have blit request:
        auto num_planes = is_multiplane ? 1 : CopyOutputResult::kNV12MaxPlanes;
        for (size_t i = 0; i < num_planes; ++i) {
          release_callbacks.push_back(
              CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
                  std::move(mailbox_access_datas[i].representation)));
        }
      }

      auto format = is_multiplane ? CopyOutputResult::Format::NV12_MULTIPLANE
                                  : CopyOutputResult::Format::NV12_PLANES;
      request->SendResult(std::make_unique<CopyOutputTextureResult>(
          format, geometry.result_selection,
          CopyOutputResult::TextureResult(mailbox_holders, color_space),
          std::move(release_callbacks)));
      break;
    }
    case CopyOutputRequest::ResultDestination::kSystemMemory: {
      auto nv12_readback = base::MakeRefCounted<NV12PlanesReadbackContext>(
          weak_ptr_, std::move(request), geometry.result_selection);

      // Issue readbacks from the surfaces:
      for (size_t i = 0; i < CopyOutputResult::kNV12MaxPlanes; ++i) {
        SkISize size(plane_surfaces[i]->width(), plane_surfaces[i]->height());
        SkImageInfo dst_info = SkImageInfo::Make(
            size, (i == 0) ? kAlpha_8_SkColorType : kR8G8_unorm_SkColorType,
            kUnpremul_SkAlphaType);

        auto context =
            std::make_unique<NV12PlanePixelReadContext>(nv12_readback, i);

        num_readbacks_pending_++;
        plane_surfaces[i]->asyncRescaleAndReadPixels(
            dst_info, SkIRect::MakeSize(size), SkSurface::RescaleGamma::kSrc,
            SkSurface::RescaleMode::kRepeatedLinear,
            &CopyOutputResultSkiaNV12::OnNV12PlaneReadbackDone,
            context.release());
      }

      break;
    }
  }
}

ReleaseCallback
SkiaOutputSurfaceImplOnGpu::CreateDestroyCopyOutputResourcesOnGpuThreadCallback(
    std::unique_ptr<gpu::SkiaImageRepresentation> representation) {
  copy_output_images_.push_back(std::move(representation));

  auto closure_on_gpu_thread = base::BindOnce(
      &SkiaOutputSurfaceImplOnGpu::DestroyCopyOutputResourcesOnGpuThread,
      weak_ptr_, copy_output_images_.back()->mailbox());

  // The destruction sequence for the textures cached by |copy_output_images_|
  // is as follows:
  // 1) The ReleaseCallback returned here can be invoked on any thread. When
  //    invoked, we post a task to the client thread with sync token
  //    dependencies that must be met before the texture can be released.
  // 2) When this task runs on the Viz thread, it will retain the closure above
  //    until the next draw (for WebView). At the next draw, the Viz thread
  //    synchronously waits to satisfy the sync token dependencies.
  // 3) Once the step above finishes, the closure is dispatched on the GPU
  //    thread (or render thread on WebView).
  ReleaseCallback release_callback = base::BindOnce(
      [](ScheduleGpuTaskCallback schedule_gpu_task, base::OnceClosure callback,
         const gpu::SyncToken& sync_token,
         bool) { schedule_gpu_task.Run(std::move(callback), {sync_token}); },
      schedule_gpu_task_, std::move(closure_on_gpu_thread));

  return base::BindPostTask(dependency_->GetClientTaskRunner(),
                            std::move(release_callback));
}

void SkiaOutputSurfaceImplOnGpu::DestroyCopyOutputResourcesOnGpuThread(
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (size_t i = 0; i < copy_output_images_.size(); ++i) {
    if (copy_output_images_[i]->mailbox() == mailbox) {
      context_state_->MakeCurrent(nullptr);
      copy_output_images_.erase(copy_output_images_.begin() + i);
      return;
    }
  }
  NOTREACHED() << "The Callback returned by GetDeleteCallback() was called "
               << "more than once.";
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    const gpu::Mailbox& mailbox) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::CopyOutput");
  // TODO(https://crbug.com/898595): Do this on the GPU instead of CPU with
  // Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (context_is_lost_)
    return;

  bool from_framebuffer = mailbox.IsZero();
  DCHECK(scoped_output_device_paint_ || !from_framebuffer);

  SkSurface* surface;
  std::unique_ptr<gpu::SkiaImageRepresentation> backing_representation;
  std::unique_ptr<gpu::SkiaImageRepresentation::ScopedWriteAccess>
      scoped_access;
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  if (from_framebuffer) {
    surface = scoped_output_device_paint_->sk_surface();
  } else {
    auto overlay_pass_access = overlay_pass_accesses_.find(mailbox);
    if (overlay_pass_access != overlay_pass_accesses_.end()) {
      surface = overlay_pass_access->second->surface();
    } else {
      backing_representation =
          shared_image_representation_factory_->ProduceSkia(
              mailbox, context_state_.get());
      DCHECK(backing_representation);

      SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};
      // TODO(https://crbug.com/1226672): Use BeginScopedReadAccess instead
      scoped_access = backing_representation->BeginScopedWriteAccess(
          /*final_msaa_count=*/1, surface_props, gfx::Rect(), &begin_semaphores,
          &end_semaphores,
          gpu::SharedImageRepresentation::AllowUnclearedAccess::kNo);
      surface = scoped_access->surface();
      if (!begin_semaphores.empty()) {
        auto result =
            surface->wait(begin_semaphores.size(), begin_semaphores.data(),
                          /*deleteSemaphoresAfterWait=*/false);
        DCHECK(result);
      }
    }
  }

  // Do not support reading back from vulkan secondary command buffer.
  if (!surface) {
    return;
  }

  // If a platform doesn't support RGBX_8888 format, we will use RGBA_8888
  // instead. In this case, we need discard alpha channel (modify the alpha
  // value to 0xff, but keep other channel not changed).
  bool need_discard_alpha =
      from_framebuffer && (output_device_->is_emulated_rgbx());
  if (need_discard_alpha) {
    absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (dependency_->GetGrShaderCache()) {
      cache_use.emplace(dependency_->GetGrShaderCache(),
                        gpu::kDisplayCompositorClientId);
    }
    SkPaint paint;
    paint.setColor(SK_ColorBLACK);
    paint.setBlendMode(SkBlendMode::kDstATop);
    surface->getCanvas()->drawPaint(paint);
    if (!FlushSurface(surface, end_semaphores, scoped_access.get())) {
      FailedSkiaFlush("CopyOutputRGBA need_discard_alpha flush");
      return;
    }
  }

  absl::optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
  if (dependency_->GetGrShaderCache()) {
    cache_use.emplace(dependency_->GetGrShaderCache(),
                      gpu::kDisplayCompositorClientId);
  }

  // For downscaling, use the GOOD quality setting (appropriate for
  // thumbnailing); and, for upscaling, use the BEST quality.
  const bool is_downscale_or_identity_in_both_dimensions =
      request->scale_to().x() <= request->scale_from().x() &&
      request->scale_to().y() <= request->scale_from().y();
  const SkSurface::RescaleMode rescale_mode =
      is_downscale_or_identity_in_both_dimensions
          ? SkSurface::RescaleMode::kRepeatedLinear
          : SkSurface::RescaleMode::kRepeatedCubic;

  // Compute |source_selection| as a workaround to support |result_selection|
  // with Skia readback. |result_selection| is a clip rect specified in the
  // destination pixel space. By transforming |result_selection| back to the
  // source pixel space we can compute what rectangle to sample from.
  //
  // This might introduce some rounding error if destination pixel space is
  // scaled up from the source pixel space. When scaling |result_selection| back
  // down it might not be pixel aligned.
  gfx::Rect source_selection = geometry.sampling_bounds;
  if (request->has_result_selection()) {
    gfx::Rect sampling_selection = request->result_selection();
    if (request->is_scaled()) {
      // Invert the scaling.
      sampling_selection = copy_output::ComputeResultRect(
          sampling_selection, request->scale_to(), request->scale_from());
    }
    sampling_selection.Offset(source_selection.OffsetFromOrigin());
    source_selection.Intersect(sampling_selection);
  }

  const SkIRect src_rect =
      SkIRect::MakeXYWH(source_selection.x(), source_selection.y(),
                        source_selection.width(), source_selection.height());
  switch (request->result_format()) {
    case CopyOutputRequest::ResultFormat::I420_PLANES: {
      DCHECK_EQ(geometry.result_selection.width() % 2, 0)
          << "SkSurface::asyncRescaleAndReadPixelsYUV420() requires "
             "destination width to be even!";
      DCHECK_EQ(geometry.result_selection.height() % 2, 0)
          << "SkSurface::asyncRescaleAndReadPixelsYUV420() requires "
             "destination height to be even!";

      const SkISize dst_size =
          SkISize::Make(geometry.result_selection.width(),
                        geometry.result_selection.height());
      auto context = std::make_unique<ReadPixelsContext>(
          std::move(request), geometry.result_selection, color_space,
          weak_ptr_);
      // Skia readback could be synchronous. Incremement counter in case
      // ReadbackCompleted is called immediately.
      num_readbacks_pending_++;
      if (auto* graphite_context = context_state_->graphite_context()) {
        // SkImage/SkSurface asyncRescaleAndReadPixels methods won't be
        // implemented for Graphite. Instead the equivalent methods will be on
        // Graphite Context.
        graphite_context->asyncRescaleAndReadPixelsYUV420(
            surface, kRec709_SkYUVColorSpace, SkColorSpace::MakeSRGB(),
            src_rect, dst_size, SkSurface::RescaleGamma::kSrc, rescale_mode,
            &CopyOutputResultSkiaYUV::OnReadbackDone, context.release());
      } else {
        surface->asyncRescaleAndReadPixelsYUV420(
            kRec709_SkYUVColorSpace, SkColorSpace::MakeSRGB(), src_rect,
            dst_size, SkSurface::RescaleGamma::kSrc, rescale_mode,
            &CopyOutputResultSkiaYUV::OnReadbackDone, context.release());
      }
      break;
    }
    case CopyOutputRequest::ResultFormat::NV12_PLANES:
    case CopyOutputRequest::ResultFormat::NV12_MULTIPLANE: {
      CopyOutputNV12(surface, geometry, color_space, src_rect, rescale_mode,
                     is_downscale_or_identity_in_both_dimensions,
                     std::move(request));
      break;
    }
    case CopyOutputRequest::ResultFormat::RGBA: {
      CopyOutputRGBA(surface, geometry, color_space, src_rect, rescale_mode,
                     is_downscale_or_identity_in_both_dimensions,
                     std::move(request));
      break;
    }
  }

  if (!FlushSurface(surface, end_semaphores, scoped_access.get())) {
    // TODO(penghuang): handle vulkan device lost.
    FailedSkiaFlush("surface->flush() failed.");
    return;
  }

  ScheduleCheckReadbackCompletion();
}

DBG_FLAG_FBOOL("skia_gpu.buffer_capture.enable", buffer_capture)

void SkiaOutputSurfaceImplOnGpu::BeginAccessImages(
    const std::vector<ImageContextImpl*>& image_contexts,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::BeginAccessImages");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  bool is_gl = gpu_preferences_.gr_context_type == gpu::GrContextType::kGL;
  for (auto* context : image_contexts) {
    if (buffer_capture()) {
      AttemptDebuggerBufferCapture(context, context_state_.get(),
                                   shared_image_representation_factory_.get());
    }

    // Prepare for accessing render pass.
    context->BeginAccessIfNecessary(
        context_state_.get(), shared_image_representation_factory_.get(),
        dependency_->GetMailboxManager(), begin_semaphores, end_semaphores);
    if (context->HasAccessEndState()) {
      image_contexts_to_apply_end_state_.emplace(context);
    }

    // Texture parameters can be modified by concurrent reads so reset them
    // before compositing from the texture. See https://crbug.com/1092080.
    if (is_gl && context->maybe_concurrent_reads()) {
      for (GrPromiseImageTexture* promise_texture :
           context->promise_image_textures()) {
        GrBackendTexture backend_texture = promise_texture->backendTexture();
        GrBackendTextures::GLTextureParametersModified(&backend_texture);
      }
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::ResetStateOfImages() {
  for (auto* context : image_contexts_to_apply_end_state_) {
    context->ApplyAccessEndState();
  }
  image_contexts_to_apply_end_state_.clear();
}

void SkiaOutputSurfaceImplOnGpu::EndAccessImages(
    const base::flat_set<ImageContextImpl*>& image_contexts) {
  TRACE_EVENT0("viz", "SkiaOutputSurfaceImplOnGpu::EndAccessImages");
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(image_contexts_to_apply_end_state_.empty());
  for (auto* context : image_contexts)
    context->EndAccessIfNecessary();
}

void SkiaOutputSurfaceImplOnGpu::ReleaseImageContexts(
    std::vector<std::unique_ptr<ExternalUseClient::ImageContext>>
        image_contexts) {
  DCHECK(!image_contexts.empty());
  // The window could be destroyed already, and the MakeCurrent will fail with
  // an destroyed window, so MakeCurrent without requiring the fbo0.
  if (context_is_lost_) {
    for (const auto& context : image_contexts)
      context->OnContextLost();
  }

  image_contexts.clear();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  overlays_ = std::move(overlays);
}

void SkiaOutputSurfaceImplOnGpu::SetEnableDCLayers(bool enable) {
  if (context_is_lost_)
    return;
  output_device_->SetEnableDCLayers(enable);
}

void SkiaOutputSurfaceImplOnGpu::SetGpuVSyncEnabled(bool enabled) {
  output_device_->SetGpuVSyncEnabled(enabled);
}

void SkiaOutputSurfaceImplOnGpu::SetVSyncDisplayID(int64_t display_id) {
  output_device_->SetVSyncDisplayID(display_id);
}

void SkiaOutputSurfaceImplOnGpu::SetFrameRate(float frame_rate) {
  if (gl_surface_)
    gl_surface_->SetFrameRate(frame_rate);
  if (presenter_) {
    presenter_->SetFrameRate(frame_rate);
  }
}

void SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting(
    const OutputSurface::Capabilities& capabilities) {
  // Check that we're using an offscreen surface.
  DCHECK(dependency_->IsOffscreen());
  output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
      context_state_, capabilities.output_surface_origin,
      renderer_settings_.requires_alpha_channel,
      shared_gpu_deps_->memory_tracker(), GetDidSwapBuffersCompleteCallback());
}

bool SkiaOutputSurfaceImplOnGpu::Initialize() {
  TRACE_EVENT1("viz", "SkiaOutputSurfaceImplOnGpu::Initialize",
               "is_using_vulkan", is_using_vulkan());
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_OZONE)
  gpu::SurfaceHandle surface_handle = dependency_->GetSurfaceHandle();
  if (surface_handle != gpu::kNullSurfaceHandle) {
    window_surface_ = ui::OzonePlatform::GetInstance()
                          ->GetSurfaceFactoryOzone()
                          ->CreatePlatformWindowSurface(surface_handle);
  }
#endif

  context_state_ = dependency_->GetSharedContextState();
  DCHECK(context_state_);
  if (!context_state_->gr_context() && !context_state_->graphite_context()) {
    DLOG(ERROR) << "Failed to create GrContext or GraphiteContext";
    return false;
  }

  if (is_using_vulkan()) {
    if (!InitializeForVulkan()) {
      return false;
    }
  } else if (is_using_graphite_dawn()) {
    if (!InitializeForDawn()) {
      return false;
    }
  } else {
    if (!InitializeForGL()) {
      return false;
    }
  }

  if (context_state_->gr_context()) {
    max_resource_cache_bytes_ =
        context_state_->gr_context()->getResourceCacheLimit();
  }

  if (context_state_)
    context_state_->AddContextLostObserver(this);

  return true;
}

bool SkiaOutputSurfaceImplOnGpu::InitializeForGL() {
  gl::GLSurfaceFormat format;
  if (PreferRGB565ResourcesForDisplay() &&
      !renderer_settings_.requires_alpha_channel) {
    format.SetRGB565();
  }

  if (dependency_->IsOffscreen()) {
    gl_surface_ = dependency_->CreateGLSurface(nullptr, format);
    if (!gl_surface_)
      return false;

    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, gfx::SurfaceOrigin::kTopLeft,
        renderer_settings_.requires_alpha_channel,
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
  } else {
    presenter_ =
        dependency_->CreatePresenter(weak_ptr_factory_.GetWeakPtr(), format);
    if (!presenter_) {
      gl_surface_ =
          dependency_->CreateGLSurface(weak_ptr_factory_.GetWeakPtr(), format);
      if (!gl_surface_) {
        return false;
      }
    }

#if BUILDFLAG(IS_MAC)
    if (base::FeatureList::IsEnabled(
            features::kCVDisplayLinkBeginFrameSource)) {
      presenter_->SetVSyncDisplayID(renderer_settings_.display_id);
    }
#endif

    if (MakeCurrent(/*need_framebuffer=*/true)) {
      if (presenter_) {
#if !BUILDFLAG(IS_WIN)
        output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
            std::make_unique<OutputPresenterGL>(
                presenter_, dependency_, shared_image_factory_.get(),
                shared_image_representation_factory_.get()),
            dependency_, shared_image_representation_factory_.get(),
            shared_gpu_deps_->memory_tracker(),
            GetDidSwapBuffersCompleteCallback());
#else   // !BUILDFLAG(IS_WIN)
        output_device_ = std::make_unique<SkiaOutputDeviceDCompPresenter>(
            shared_image_representation_factory_.get(), context_state_.get(),
            presenter_, feature_info_, shared_gpu_deps_->memory_tracker(),
            GetDidSwapBuffersCompleteCallback());
#endif  // BUILDFLAG(IS_WIN)
      } else {
        if (dependency_->NeedsSupportForExternalStencil()) {
          output_device_ = std::make_unique<SkiaOutputDeviceWebView>(
              context_state_.get(), gl_surface_,
              shared_gpu_deps_->memory_tracker(),
              GetDidSwapBuffersCompleteCallback());
        } else {
#if BUILDFLAG(IS_WIN)
          if (gl_surface_->SupportsDCLayers()) {
            output_device_ = std::make_unique<SkiaOutputDeviceDCompGLSurface>(
                shared_image_representation_factory_.get(),
                context_state_.get(), gl_surface_, feature_info_,
                shared_gpu_deps_->memory_tracker(),
                GetDidSwapBuffersCompleteCallback());
          }
#endif
          if (!output_device_) {
            // Used by Android, Linux, and Windows (when there is no DComp
            // support, e.g. Win7)
            output_device_ = std::make_unique<SkiaOutputDeviceGL>(
                context_state_.get(), gl_surface_, feature_info_,
                shared_gpu_deps_->memory_tracker(),
                GetDidSwapBuffersCompleteCallback());
          }
        }
      }
    } else {
      presenter_ = nullptr;
      gl_surface_ = nullptr;
      context_state_ = nullptr;
      LOG(ERROR) << "Failed to make current during initialization.";
      return false;
    }
  }

  if (dependency_->IsOffscreen()) {
    DCHECK(gl_surface_);
    DCHECK_EQ(gl_surface_->IsOffscreen(), true);
  } else if (gl_surface_) {
    // OnScreen GLSurfaces are never Surfaceless except on windows where a bit
    // of work needed to make it use Presenter.
#if !BUILDFLAG(IS_WIN)
    DCHECK(!gl_surface_->IsSurfaceless());
#endif
  } else {
    // If there is no gl_surface there must be presenter.
    DCHECK(presenter_);
  }

  return true;
}

#if BUILDFLAG(ENABLE_VULKAN)
bool SkiaOutputSurfaceImplOnGpu::InitializeForVulkan() {
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, gfx::SurfaceOrigin::kBottomLeft,
        renderer_settings_.requires_alpha_channel,
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
    return true;
  }

#if BUILDFLAG(IS_ANDROID)
  if (vulkan_context_provider_->GetGrSecondaryCBDrawContext()) {
    output_device_ = std::make_unique<SkiaOutputDeviceVulkanSecondaryCB>(
        vulkan_context_provider_, shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
    return true;
  }
#endif

#if !BUILDFLAG(IS_WIN)
  std::unique_ptr<OutputPresenter> output_presenter;
#if BUILDFLAG(IS_FUCHSIA)
  output_presenter =
      OutputPresenterFuchsia::Create(window_surface_.get(), dependency_);
#else
  presenter_ = dependency_->CreatePresenter(weak_ptr_factory_.GetWeakPtr(),
                                            gl::GLSurfaceFormat());
  if (presenter_) {
    output_presenter = std::make_unique<OutputPresenterGL>(
        presenter_, dependency_, shared_image_factory_.get(),
        shared_image_representation_factory_.get());
  }
#endif
  if (output_presenter) {
    output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
        std::move(output_presenter), dependency_,
        shared_image_representation_factory_.get(),
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
    return true;
  }
#endif  // !BUILDFLAG(IS_WIN)

  std::unique_ptr<SkiaOutputDeviceVulkan> output_device;
  if (!gpu_preferences_.disable_vulkan_surface) {
    output_device = SkiaOutputDeviceVulkan::Create(
        vulkan_context_provider_, dependency_->GetSurfaceHandle(),
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
  }
  if (MayFallBackToSkiaOutputDeviceX11()) {
#if defined(USE_OZONE_PLATFORM_X11)
    if (output_device) {
      output_device_ = std::move(output_device);
    } else {
      output_device_ = SkiaOutputDeviceX11::Create(
          context_state_, dependency_->GetSurfaceHandle(),
          shared_gpu_deps_->memory_tracker(),
          GetDidSwapBuffersCompleteCallback());
    }
    if (output_device_)
      return true;
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
  }
  if (!output_device)
    return false;

#if BUILDFLAG(IS_WIN)
  gpu::SurfaceHandle child_window = output_device->GetChildSurfaceHandle();
  if (child_window != gpu::kNullSurfaceHandle) {
    AddChildWindowToBrowser(child_window);
  }
#endif  // BUILDFLAG(IS_WIN)
  output_device_ = std::move(output_device);
  return true;
}
#else   // BUILDFLAG(ENABLE_VULKAN)
bool SkiaOutputSurfaceImplOnGpu::InitializeForVulkan() {
  return false;
}
#endif  // !BUILDFLAG(ENABLE_VULKAN)

bool SkiaOutputSurfaceImplOnGpu::InitializeForDawn() {
#if BUILDFLAG(SKIA_USE_DAWN)
  if (dependency_->IsOffscreen()) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        context_state_, gfx::SurfaceOrigin::kBottomLeft,
        renderer_settings_.requires_alpha_channel,
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
  } else {
#if defined(USE_OZONE_PLATFORM_X11)
    // TODO(rivr): Set up a Vulkan swapchain so that Linux can also use
    // SkiaOutputDeviceDawn.
    if (MayFallBackToSkiaOutputDeviceX11()) {
      output_device_ = SkiaOutputDeviceX11::Create(
          context_state_, dependency_->GetSurfaceHandle(),
          shared_gpu_deps_->memory_tracker(),
          GetDidSwapBuffersCompleteCallback());
    }
#elif BUILDFLAG(IS_WIN)
    presenter_ = dependency_->CreatePresenter(weak_ptr_factory_.GetWeakPtr(),
                                              gl::GLSurfaceFormat());
    if (presenter_) {
      output_device_ = std::make_unique<SkiaOutputDeviceDCompPresenter>(
          shared_image_representation_factory_.get(), context_state_.get(),
          presenter_, feature_info_, shared_gpu_deps_->memory_tracker(),
          GetDidSwapBuffersCompleteCallback());
    } else {
      auto output_device = std::make_unique<SkiaOutputDeviceDawn>(
          context_state_, gfx::SurfaceOrigin::kTopLeft,
          shared_gpu_deps_->memory_tracker(),
          GetDidSwapBuffersCompleteCallback());
      AddChildWindowToBrowser(output_device->GetChildSurfaceHandle());
      output_device_ = std::move(output_device);
    }
#elif BUILDFLAG(IS_APPLE)
    presenter_ = dependency_->CreatePresenter(weak_ptr_factory_.GetWeakPtr(),
                                              gl::GLSurfaceFormat());
#if BUILDFLAG(IS_MAC)
    if (features::UseGpuVsync()) {
      presenter_->SetVSyncDisplayID(renderer_settings_.display_id);
    }
#endif
    output_device_ = std::make_unique<SkiaOutputDeviceBufferQueue>(
        std::make_unique<OutputPresenterGL>(
            presenter_, dependency_, shared_image_factory_.get(),
            shared_image_representation_factory_.get()),
        dependency_, shared_image_representation_factory_.get(),
        shared_gpu_deps_->memory_tracker(),
        GetDidSwapBuffersCompleteCallback());
#else
    NOTREACHED_NORETURN();
#endif
  }
#endif
  return !!output_device_;
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent(bool need_framebuffer) {
// Windows still uses gl_surface for DComp presentation. Once that's switched
// over to presenter, these DCHECKs will be actual on all platforms and code can
// be simplified.
#if !BUILDFLAG(IS_WIN)
  if (gl_surface_) {
    DCHECK(context_state_->GrContextIsGL());
    DCHECK(!gl_surface_->IsSurfaceless() || gl_surface_->IsOffscreen());
  }
#endif

  // If GL is not being used or GLSurface is not surfaceless, we can ignore
  // making current the GLSurface for better performance.
  bool need_fbo0 = need_framebuffer && context_state_->GrContextIsGL() &&
                   gl_surface_ && !gl_surface_->IsSurfaceless();

  // need_fbo0 implies need_gl too.
  bool need_gl = need_fbo0;

  // Only make current with |gl_surface_|, if following operations will use
  // fbo0.
  auto* gl_surface = need_fbo0 ? gl_surface_.get() : nullptr;
  if (!context_state_->MakeCurrent(gl_surface, need_gl)) {
    LOG(ERROR) << "Failed to make current.";
    dependency_->DidLoseContext(
        *context_state_->context_lost_reason(),
        GURL("chrome://gpu/SkiaOutputSurfaceImplOnGpu::MakeCurrent"));
    MarkContextLost(GetContextLostReason(
        gpu::error::kLostContext, *context_state_->context_lost_reason()));
    return false;
  }

  // Some GLSurface implements OnMakeCurrent() to tracing current GLContext,
  // even if framebuffer is not needed, we still call OnMakeCurrent() so
  // GLSurface implementation will know the current GLContext.
  if (!need_fbo0) {
    if (gl_surface_) {
      gl_surface_->OnMakeCurrent(context_state_->context());
    }
  }

  context_state_->set_need_context_state_reset(true);
  return true;
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSync(uint64_t sync_fence_release) {
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffersInternal(
    absl::optional<OutputSurfaceFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(output_device_);

  if (context_is_lost_)
    return;

  if (frame) {
    if (gl_surface_) {
      if (frame->delegated_ink_metadata) {
        gl_surface_->SetDelegatedInkTrailStartPoint(
            std::move(frame->delegated_ink_metadata));
      }
    }
    if (presenter_) {
      presenter_->SetChoreographerVsyncIdForNextFrame(
          frame->choreographer_vsync_id);
#if BUILDFLAG(IS_WIN)
      if (frame->delegated_ink_metadata) {
        presenter_->SetDelegatedInkTrailStartPoint(
            std::move(frame->delegated_ink_metadata));
      }
#endif
    }
  }

  bool sync_cpu =
      gpu::ShouldVulkanSyncCpuForSkiaSubmit(vulkan_context_provider_);

  ResetStateOfImages();
  gl::ScopedProgressReporter scoped_process_reporter(
      context_state_->progress_reporter());
  output_device_->Submit(
      sync_cpu, base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PostSubmit,
                               base::Unretained(this), std::move(frame)));
}

void SkiaOutputSurfaceImplOnGpu::PostSubmit(
    absl::optional<OutputSurfaceFrame> frame) {
  promise_image_access_helper_.EndAccess();
  scoped_output_device_paint_.reset();
  overlay_pass_accesses_.clear();

#if BUILDFLAG(ENABLE_VULKAN)
  std::vector<VkSemaphore> semaphores;
  semaphores.reserve(pending_release_fence_cbs_.size());

  while (!pending_release_fence_cbs_.empty()) {
    auto& item = pending_release_fence_cbs_.front();
    auto release_fence = CreateReleaseFenceForVulkan(item.first);
    if (release_fence.is_null())
      LOG(ERROR) << "Unable to create a release fence for Vulkan.";
    else
      semaphores.emplace_back(item.first.vkSemaphore());
    std::move(item.second).Run(std::move(release_fence));
    pending_release_fence_cbs_.pop_front();
  }

  if (!semaphores.empty()) {
    gpu::VulkanFenceHelper* fence_helper = context_state_->vk_context_provider()
                                               ->GetDeviceQueue()
                                               ->GetFenceHelper();
    fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
        std::move(semaphores));
  }
#else
  DCHECK(pending_release_fence_cbs_.empty());
#endif

  if (frame) {
    if (waiting_for_full_damage_) {
      // If we're using partial swap, we need to check whether the sub-buffer
      // rect is actually the entire screen, but otherwise, the damage is
      // always the full surface.
      if (frame->sub_buffer_rect &&
          capabilities().supports_post_sub_buffer &&
          frame->sub_buffer_rect->size() != size_) {
        output_device_->SwapBuffersSkipped(buffer_presented_callback_,
                                           std::move(*frame));
        output_surface_plane_.reset();
        destroy_after_swap_.clear();
        return;
      }
      waiting_for_full_damage_ = false;
    }

    if (output_surface_plane_)
      DCHECK(output_device_->IsPrimaryPlaneOverlay());

    if (frame->sub_buffer_rect) {
      if (capabilities().supports_post_sub_buffer) {
        if (capabilities().output_surface_origin ==
            gfx::SurfaceOrigin::kBottomLeft) {
          frame->sub_buffer_rect->set_y(size_.height() -
                                        frame->sub_buffer_rect->y() -
                                        frame->sub_buffer_rect->height());
        }
      }

      if (output_surface_plane_)
        output_surface_plane_->damage_rect = frame->sub_buffer_rect;
    }

    if (overlays_.size()) {
      for (auto& each : overlays_) {
        DBG_DRAW_RECT("output.overlay.rect", each.display_rect);
        DBG_DRAW_RECT("output.overlay.damage", each.damage_rect);
      }
      TRACE_EVENT1("viz", "SkiaOutputDevice->ScheduleOverlays()",
                   "num_overlays", overlays_.size());
      constexpr base::TimeDelta kHistogramMinTime = base::Microseconds(5);
      constexpr base::TimeDelta kHistogramMaxTime = base::Milliseconds(16);
      constexpr int kHistogramTimeBuckets = 50;
      base::TimeTicks start_time = base::TimeTicks::Now();

      output_device_->ScheduleOverlays(std::move(overlays_));

      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Gpu.OutputSurface.ScheduleOverlaysUs",
          base::TimeTicks::Now() - start_time, kHistogramMinTime,
          kHistogramMaxTime, kHistogramTimeBuckets);
    }

    output_device_->SetViewportSize(frame->size);
    output_device_->SchedulePrimaryPlane(output_surface_plane_);

    DCHECK(!frame->sub_buffer_rect || capabilities().supports_post_sub_buffer);
    output_device_->Present(frame->sub_buffer_rect, buffer_presented_callback_,
                            std::move(*frame));
  }

  // Reset the overlay plane information even on skipped swap.
  output_surface_plane_.reset();
  overlays_.clear();

  destroy_after_swap_.clear();
  context_state_->UpdateSkiaOwnedMemorySize();
}

bool SkiaOutputSurfaceImplOnGpu::IsDisplayedAsOverlay() {
  return output_device_->IsPrimaryPlaneOverlay();
}

#if BUILDFLAG(IS_WIN)
void SkiaOutputSurfaceImplOnGpu::AddChildWindowToBrowser(
    gpu::SurfaceHandle child_window) {
  PostTaskToClientThread(
      base::BindOnce(add_child_window_to_browser_callback_, child_window));
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

void SkiaOutputSurfaceImplOnGpu::DidSwapBuffersCompleteInternal(
    gpu::SwapBuffersCompleteParams params,
    const gfx::Size& pixel_size,
    gfx::GpuFenceHandle release_fence) {
  if (params.swap_response.result == gfx::SwapResult::SWAP_FAILED) {
    DLOG(ERROR) << "Context lost on SWAP_FAILED";
    if (!context_state_->IsCurrent(nullptr) ||
        !context_state_->CheckResetStatus(false)) {
      // Mark the context lost if not already lost.
      MarkContextLost(ContextLostReason::CONTEXT_LOST_SWAP_FAILED);
    }
  } else if (params.swap_response.result ==
             gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // We shouldn't present newly reallocated buffers until we have fully
    // initialized their contents. SWAP_NAK_RECREAETE_BUFFERS should trigger a
    // full-screen damage in DirectRenderer, but there is no guarantee that it
    // will happen immediately since the SwapBuffersComplete task gets posted
    // back to the Viz thread and will race with the next invocation of
    // DrawFrame. To ensure we do not display uninitialized memory, we hold
    // off on submitting new frames until we have received a full damage.
    waiting_for_full_damage_ = true;
  }

  PostTaskToClientThread(base::BindOnce(did_swap_buffer_complete_callback_,
                                        params, pixel_size,
                                        std::move(release_fence)));
}

SkiaOutputSurfaceImplOnGpu::DidSwapBufferCompleteCallback
SkiaOutputSurfaceImplOnGpu::GetDidSwapBuffersCompleteCallback() {
  return base::BindRepeating(
      &SkiaOutputSurfaceImplOnGpu::DidSwapBuffersCompleteInternal, weak_ptr_);
}

void SkiaOutputSurfaceImplOnGpu::OnContextLost() {
  MarkContextLost(ContextLostReason::CONTEXT_LOST_UNKNOWN);
}

void SkiaOutputSurfaceImplOnGpu::MarkContextLost(ContextLostReason reason) {
  // This function potentially can be re-entered during from
  // SharedContextState::MarkContextLost(). This guards against it.
  if (context_is_lost_)
    return;
  context_is_lost_ = true;

  UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.DisplayCompositor", reason);

  // Release all ongoing AsyncReadResults.
  ReleaseAsyncReadResultHelpers();

  for (auto& [mailbox, representation] : skia_representations_) {
    if (representation) {
      representation->OnContextLost();
    }
  }

  context_state_->MarkContextLost();
  if (context_lost_callback_) {
    PostTaskToClientThread(std::move(context_lost_callback_));
  }
}

void SkiaOutputSurfaceImplOnGpu::ScheduleCheckReadbackCompletion() {
  if (num_readbacks_pending_ > 0 && !readback_poll_pending_) {
    dependency_->ScheduleDelayedGPUTaskFromGPUThread(
        base::BindOnce(&SkiaOutputSurfaceImplOnGpu::CheckReadbackCompletion,
                       weak_ptr_factory_.GetWeakPtr()));
    readback_poll_pending_ = true;
  }
}

void SkiaOutputSurfaceImplOnGpu::CheckReadbackCompletion() {
  readback_poll_pending_ = false;

  // If there are no pending readback requests or we can't make the context
  // current then exit. There is no thing to do here.
  if (num_readbacks_pending_ == 0 || !MakeCurrent(/*need_framebuffer=*/false))
    return;

  if (auto* graphite_context = context_state_->graphite_context()) {
    graphite_context->checkAsyncWorkCompletion();
  } else {
    CHECK(gr_context());
    gr_context()->checkAsyncWorkCompletion();
  }
  ScheduleCheckReadbackCompletion();
}

void SkiaOutputSurfaceImplOnGpu::PreserveChildSurfaceControls() {
  if (presenter_) {
    presenter_->PreserveChildSurfaceControls();
  }
}

void SkiaOutputSurfaceImplOnGpu::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  if (gl_surface_) {
    DCHECK(!presenter_);
    gl_surface_->InitDelegatedInkPointRendererReceiver(
        std::move(pending_receiver));
  } else if (presenter_) {
#if BUILDFLAG(IS_WIN)
    presenter_->InitDelegatedInkPointRendererReceiver(
        std::move(pending_receiver));
#endif
  }
}

const scoped_refptr<AsyncReadResultLock>
SkiaOutputSurfaceImplOnGpu::GetAsyncReadResultLock() const {
  return async_read_result_lock_;
}

void SkiaOutputSurfaceImplOnGpu::AddAsyncReadResultHelperWithLock(
    AsyncReadResultHelper* helper) {
  async_read_result_lock_->lock().AssertAcquired();
  DCHECK(helper);
  async_read_result_helpers_.insert(helper);
}

void SkiaOutputSurfaceImplOnGpu::RemoveAsyncReadResultHelperWithLock(
    AsyncReadResultHelper* helper) {
  async_read_result_lock_->lock().AssertAcquired();
  DCHECK(helper);
  DCHECK(async_read_result_helpers_.count(helper));
  async_read_result_helpers_.erase(helper);
}

void SkiaOutputSurfaceImplOnGpu::EnsureBackbuffer() {
  // We call GLSurface::SetBackbuffferAllocation in Ensure/Discard backbuffer,
  // so technically need framebuffer. In reality no GLSurface implements it, but
  // until it's removed we should keep true here.
  MakeCurrent(/*need_framebuffer=*/true);
  output_device_->EnsureBackbuffer();
}
void SkiaOutputSurfaceImplOnGpu::DiscardBackbuffer() {
  // We call GLSurface::SetBackbuffferAllocation in Ensure/Discard backbuffer,
  // so technically need framebuffer. In reality no GLSurface implements it, but
  // until it's removed we should keep true here.
  MakeCurrent(/*need_framebuffer=*/true);
  output_device_->DiscardBackbuffer();
}

#if BUILDFLAG(ENABLE_VULKAN)
gfx::GpuFenceHandle SkiaOutputSurfaceImplOnGpu::CreateReleaseFenceForVulkan(
    const GrBackendSemaphore& semaphore) {
  DCHECK(is_using_vulkan());

  if (semaphore.vkSemaphore() == VK_NULL_HANDLE)
    return {};

  auto* implementation = vulkan_context_provider_->GetVulkanImplementation();
  VkDevice device =
      vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice();

  auto handle =
      implementation->GetSemaphoreHandle(device, semaphore.vkSemaphore());
  if (!handle.is_valid()) {
    vkDestroySemaphore(device, semaphore.vkSemaphore(),
                       /*pAllocator=*/nullptr);
    LOG(ERROR) << "Failed to create a release fence for Vulkan.";
    return {};
  }
  return std::move(handle).ToGpuFenceHandle();
}

bool SkiaOutputSurfaceImplOnGpu::CreateAndStoreExternalSemaphoreVulkan(
    std::vector<GrBackendSemaphore>& end_semaphores) {
  DCHECK(is_using_vulkan());

  auto* implementation = vulkan_context_provider_->GetVulkanImplementation();
  VkDevice device =
      vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice();

  VkSemaphore semaphore = implementation->CreateExternalSemaphore(device);
  if (semaphore == VK_NULL_HANDLE) {
    LOG(ERROR)
        << "Creation of an external semaphore for a release fence failed.";
    return false;
  }

  end_semaphores.emplace_back();
  end_semaphores.back().initVulkan(semaphore);
  return true;
}
#endif

gfx::GpuFenceHandle SkiaOutputSurfaceImplOnGpu::CreateReleaseFenceForGL() {
  if (gl::GLFence::IsGpuFenceSupported()) {
    auto fence = gl::GLFence::CreateForGpuFence();
    if (fence)
      return fence->GetGpuFence()->GetGpuFenceHandle().Clone();
  }
  return {};
}

void SkiaOutputSurfaceImplOnGpu::CreateSharedImage(
    gpu::Mailbox mailbox,
    SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    std::string debug_label,
    gpu::SurfaceHandle surface_handle) {
  shared_image_factory_->CreateSharedImage(
      mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
      format.HasAlpha() ? kPremul_SkAlphaType : kOpaque_SkAlphaType,
      surface_handle, usage, std::move(debug_label));
  skia_representations_.emplace(mailbox, nullptr);
}

void SkiaOutputSurfaceImplOnGpu::CreateSolidColorSharedImage(
    gpu::Mailbox mailbox,
    const SkColor4f& color,
    const gfx::ColorSpace& color_space) {
#if BUILDFLAG(IS_OZONE)
  auto preferred_solid_color_format = ui::OzonePlatform::GetInstance()
                                          ->GetSurfaceFactoryOzone()
                                          ->GetPreferredFormatForSolidColor();
  if (preferred_solid_color_format)
    solid_color_image_format_ =
        GetSinglePlaneSharedImageFormat(preferred_solid_color_format.value());
#endif
  DCHECK(solid_color_image_format_ == SinglePlaneFormat::kRGBA_8888 ||
         solid_color_image_format_ == SinglePlaneFormat::kBGRA_8888);
  // Create a 1x1 pixel span of the colour in |solid_color_image_format_|.
  gfx::Size size(1, 1);
  // Premultiply the SkColor4f to support transparent quads.
  SkColor4f premul{color[0] * color[3], color[1] * color[3],
                   color[2] * color[3], color[3]};
  const uint32_t premul_rgba_bytes = premul.toBytes_RGBA();
  uint32_t premul_bytes = premul_rgba_bytes;
  if (solid_color_image_format_ == SinglePlaneFormat::kBGRA_8888) {
    SkSwapRB(&premul_bytes, &premul_rgba_bytes, 1);
  }
  auto pixel_span = base::make_span(
      reinterpret_cast<const uint8_t*>(&premul_bytes), sizeof(uint32_t));

  // TODO(crbug.com/1360538) Some work is needed to properly support F16 format.
  shared_image_factory_->CreateSharedImage(
      mailbox, solid_color_image_format_, size, color_space,
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_SCANOUT | gpu::SHARED_IMAGE_USAGE_DISPLAY_READ,
      "SkiaSolidColor", pixel_span);
  solid_color_images_.insert(mailbox);
}

void SkiaOutputSurfaceImplOnGpu::DestroySharedImage(gpu::Mailbox mailbox) {
  shared_image_factory_->DestroySharedImage(mailbox);
  // Under normal circumstances the write access should be destroyed already in
  // PostSubmit(), but if context was lost then SwapBuffersInternal will no-op
  // and PostSubmit() will not be called.
  DCHECK(!overlay_pass_accesses_.contains(mailbox) || context_is_lost_);
  overlay_pass_accesses_.erase(mailbox);
  skia_representations_.erase(mailbox);
  solid_color_images_.erase(mailbox);
}

gpu::SkiaImageRepresentation* SkiaOutputSurfaceImplOnGpu::GetSkiaRepresentation(
    gpu::Mailbox mailbox) {
  auto it = skia_representations_.find(mailbox);
  // The cache entry should already have been created in CreateSharedImage().
  DCHECK(it != skia_representations_.end());

  if (!it->second) {
    it->second = shared_image_representation_factory_->ProduceSkia(
        mailbox, context_state_.get());
  }
  return it->second.get();
}

base::ScopedClosureRunner SkiaOutputSurfaceImplOnGpu::GetCacheBackBufferCb() {
  if (gl_surface_) {
    DCHECK(!presenter_);
    return dependency_->CacheGLSurface(gl_surface_.get());
  }

  if (presenter_) {
    return dependency_->CachePresenter(presenter_.get());
  }

  return base::ScopedClosureRunner();
}

}  // namespace viz
