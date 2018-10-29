// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_config.h"
#include "gpu/ipc/service/x_util.h"
#include "gpu/vulkan/buildflags.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/viz/privileged/interfaces/gl/gpu_host.mojom.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/native_widget_types.h"

class GrContext;

#if defined(OS_CHROMEOS)
namespace arc {
class ProtectedBufferManager;
}
#endif  // OS_CHROMEOS

namespace gpu {
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class Scheduler;
class SyncPointManager;
class VulkanImplementation;
}  // namespace gpu

namespace media {
class MediaGpuChannelManager;
}

namespace viz {

class VulkanContextProvider;

// This runs in the GPU process, and communicates with the gpu host (which is
// the window server) over the mojom APIs. This is responsible for setting up
// the connection to clients, allocating/free'ing gpu memory etc.
class VIZ_SERVICE_EXPORT GpuServiceImpl : public gpu::GpuChannelManagerDelegate,
                                          public mojom::GpuService {
 public:
  GpuServiceImpl(const gpu::GPUInfo& gpu_info,
                 std::unique_ptr<gpu::GpuWatchdogThread> watchdog,
                 scoped_refptr<base::SingleThreadTaskRunner> io_runner,
                 const gpu::GpuFeatureInfo& gpu_feature_info,
                 const gpu::GpuPreferences& gpu_preferences,
                 const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
                 const base::Optional<gpu::GpuFeatureInfo>&
                     gpu_feature_info_for_hardware_gpu,
                 gpu::VulkanImplementation* vulkan_implementation,
                 base::OnceClosure exit_callback);

  ~GpuServiceImpl() override;

  void UpdateGPUInfo();

  void InitializeWithHost(
      mojom::GpuHostPtr gpu_host,
      gpu::GpuProcessActivityFlags activity_flags,
      scoped_refptr<gl::GLSurface> default_offscreen_surface,
      gpu::SyncPointManager* sync_point_manager = nullptr,
      base::WaitableEvent* shutdown_event = nullptr);
  void Bind(mojom::GpuServiceRequest request);

  // Get a GrContext and a GLContext for a given GL surface.
  bool GetGrContextForGLSurface(gl::GLSurface* surface,
                                GrContext** gr_context,
                                gl::GLContext** gl_context);

  GrContext* GetGrContextForVulkan();

  // Notifies the GpuHost to stop using GPU compositing. This should be called
  // in response to an error in the GPU process that occurred after
  // InitializeWithHost() was called, otherwise GpuFeatureInfo should be set
  // accordingly. This can safely be called from any thread.
  void DisableGpuCompositing();

  bool is_initialized() const { return !!gpu_host_; }

  media::MediaGpuChannelManager* media_gpu_channel_manager() {
    return media_gpu_channel_manager_.get();
  }

  gpu::GpuChannelManager* gpu_channel_manager() {
    return gpu_channel_manager_.get();
  }

  gpu::ImageFactory* gpu_image_factory();
  gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory() {
    return gpu_memory_buffer_factory_.get();
  }

  gpu::MailboxManager* mailbox_manager() {
    return gpu_channel_manager_->mailbox_manager();
  }

  gpu::SharedImageManager* shared_image_manager() {
    return gpu_channel_manager_->shared_image_manager();
  }

  gl::GLShareGroup* share_group() {
    return gpu_channel_manager_->share_group();
  }

  gpu::SyncPointManager* sync_point_manager() { return sync_point_manager_; }
  gpu::Scheduler* scheduler() { return scheduler_.get(); }

  gpu::GpuWatchdogThread* watchdog_thread() { return watchdog_thread_.get(); }

  const gpu::GpuFeatureInfo& gpu_feature_info() const {
    return gpu_feature_info_;
  }

  bool in_host_process() const { return gpu_info_.in_process_gpu; }

  void set_start_time(base::Time start_time) { start_time_ = start_time; }

  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }
  const gpu::GpuPreferences& gpu_preferences() const {
    return gpu_preferences_;
  }

  gpu::SequenceId skia_output_surface_sequence_id() const {
    return skia_output_surface_sequence_id_;
  }

#if BUILDFLAG(ENABLE_VULKAN)
  bool is_using_vulkan() const { return !!vulkan_context_provider_; }
  VulkanContextProvider* vulkan_context_provider() {
    return vulkan_context_provider_.get();
  }
#else
  bool is_using_vulkan() const { return false; }
  VulkanContextProvider* vulkan_context_provider() { return nullptr; }
#endif

  void set_oopd_enabled() { oopd_enabled_ = true; }

 private:
  void RecordLogMessage(int severity,
                        size_t message_start,
                        const std::string& message);

  void UpdateGpuInfoPlatform(base::OnceClosure on_gpu_info_updated);

  // gpu::GpuChannelManagerDelegate:
  void DidCreateContextSuccessfully() override;
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void StoreShaderToDisk(int client_id,
                         const std::string& key,
                         const std::string& shader) override;
  void ExitProcess() override;
#if defined(OS_WIN)
  void SendCreatedChildWindow(gpu::SurfaceHandle parent_window,
                              gpu::SurfaceHandle child_window) override;
#endif
  void SetActiveURL(const GURL& url) override;

  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           bool cache_shaders_on_disk,
                           EstablishGpuChannelCallback callback) override;
  void CloseChannel(int32_t client_id) override;
#if defined(OS_CHROMEOS)
  void CreateArcVideoDecodeAccelerator(
      arc::mojom::VideoDecodeAcceleratorRequest vda_request) override;
  void CreateArcVideoEncodeAccelerator(
      arc::mojom::VideoEncodeAcceleratorRequest vea_request) override;
  void CreateArcVideoProtectedBufferAllocator(
      arc::mojom::VideoProtectedBufferAllocatorRequest pba_request) override;
  void CreateArcProtectedBufferManager(
      arc::mojom::ProtectedBufferManagerRequest pbm_request) override;
#endif  // defined(OS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override;
  void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest jea_request) override;
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request)
      override;
  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override;
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override;
  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override;
#if defined(OS_WIN)
  void RequestCompleteGpuInfo(RequestCompleteGpuInfoCallback callback) override;
  void GetGpuSupportedRuntimeVersion(
      GetGpuSupportedRuntimeVersionCallback callback) override;
#endif
  void RequestHDRStatus(RequestHDRStatusCallback callback) override;
  void LoadedShader(int32_t client_id,
                    const std::string& key,
                    const std::string& data) override;
  void WakeUpGpu() override;
  void GpuSwitched() override;
  void DestroyAllChannels() override;
  void OnBackgroundCleanup() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;
#if defined(OS_MACOSX)
  void BeginCATransaction() override;
  void CommitCATransaction(CommitCATransactionCallback callback) override;
#endif
  void Crash() override;
  void Hang() override;
  void ThrowJavaException() override;
  void Stop(StopCallback callback) override;

#if defined(OS_CHROMEOS)
  void CreateArcVideoDecodeAcceleratorOnMainThread(
      arc::mojom::VideoDecodeAcceleratorRequest vda_request);
  void CreateArcVideoEncodeAcceleratorOnMainThread(
      arc::mojom::VideoEncodeAcceleratorRequest vea_request);
  void CreateArcVideoProtectedBufferAllocatorOnMainThread(
      arc::mojom::VideoProtectedBufferAllocatorRequest pba_request);
  void CreateArcProtectedBufferManagerOnMainThread(
      arc::mojom::ProtectedBufferManagerRequest pbm_request);
#endif  // defined(OS_CHROMEOS)

  void RequestHDRStatusOnMainThread(RequestHDRStatusCallback callback);

  scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  const gpu::GpuPreferences gpu_preferences_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  // Information about general chrome feature support for the GPU.
  gpu::GpuFeatureInfo gpu_feature_info_;

  // What we would have gotten if we haven't fallen back to SwiftShader or
  // pure software (in the viz case).
  base::Optional<gpu::GPUInfo> gpu_info_for_hardware_gpu_;
  base::Optional<gpu::GpuFeatureInfo> gpu_feature_info_for_hardware_gpu_;

  scoped_refptr<mojom::ThreadSafeGpuHostPtr> gpu_host_;
  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;
  std::unique_ptr<media::MediaGpuChannelManager> media_gpu_channel_manager_;

  // On some platforms (e.g. android webview), the SyncPointManager comes from
  // external sources.
  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;
  gpu::SyncPointManager* sync_point_manager_ = nullptr;

  std::unique_ptr<gpu::Scheduler> scheduler_;

  // sequence id for running tasks from SkiaOutputSurface;
  gpu::SequenceId skia_output_surface_sequence_id_;

  // GL and Gr contexts used by Skia only.
  struct GrContextAndGLContext;
  base::flat_map<unsigned long, GrContextAndGLContext> contexts_for_gl_;

#if BUILDFLAG(ENABLE_VULKAN)
  gpu::VulkanImplementation* vulkan_implementation_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
#endif

  // An event that will be signalled when we shutdown. On some platforms it
  // comes from external sources.
  std::unique_ptr<base::WaitableEvent> owned_shutdown_event_;
  base::WaitableEvent* shutdown_event_ = nullptr;

  // Callback that safely exits GPU process.
  base::OnceClosure exit_callback_;

  base::Time start_time_;

  // Used to track the task to bind a GpuServiceRequest on the io thread.
  base::CancelableTaskTracker bind_task_tracker_;
  std::unique_ptr<mojo::BindingSet<mojom::GpuService>> bindings_;

#if defined(OS_CHROMEOS)
  scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager_;
#endif  // defined(OS_CHROMEOS)

  bool oopd_enabled_ = false;

  base::WeakPtr<GpuServiceImpl> weak_ptr_;
  base::WeakPtrFactory<GpuServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_
