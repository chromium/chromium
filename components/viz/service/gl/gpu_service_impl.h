// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/clang_profiling_buildflags.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/gpu_extra_info.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace arc {
class ProtectedBufferManager;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace gpu {
class GpuMemoryBufferFactory;
class GpuWatchdogThread;
class ImageDecodeAcceleratorWorker;
class Scheduler;
class SyncPointManager;
class SharedImageManager;
class VulkanImplementation;
}  // namespace gpu

namespace media {
class MediaGpuChannelManager;
}

namespace gpu {
class SharedContextState;
}  // namespace gpu

namespace viz {

class VulkanContextProvider;
class MetalContextProvider;
class DawnContextProvider;

enum class ExitCode {
  // Matches service_manager::ResultCode::RESULT_CODE_NORMAL_EXIT
  RESULT_CODE_NORMAL_EXIT = 0,
  // Matches chrome::ResultCode::RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST
  RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST = 34,
};

// This runs in the GPU process, and communicates with the gpu host (which is
// the window server) over the mojom APIs. This is responsible for setting up
// the connection to clients, allocating/free'ing gpu memory etc.
class VIZ_SERVICE_EXPORT GpuServiceImpl : public gpu::GpuChannelManagerDelegate,
                                          public mojom::GpuService {
 public:
  GpuServiceImpl(
      const gpu::GPUInfo& gpu_info,
      std::unique_ptr<gpu::GpuWatchdogThread> watchdog,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const gpu::GpuPreferences& gpu_preferences,
      const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const base::Optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gfx::GpuExtraInfo& gpu_extra_info,
      gpu::VulkanImplementation* vulkan_implementation,
      base::OnceCallback<void(base::Optional<ExitCode>)> exit_callback);

  ~GpuServiceImpl() override;

  void UpdateGPUInfo();
  void UpdateGPUInfoGL();

  void InitializeWithHost(
      mojo::PendingRemote<mojom::GpuHost> gpu_host,
      gpu::GpuProcessActivityFlags activity_flags,
      scoped_refptr<gl::GLSurface> default_offscreen_surface,
      gpu::SyncPointManager* sync_point_manager = nullptr,
      gpu::SharedImageManager* shared_image_manager = nullptr,
      base::WaitableEvent* shutdown_event = nullptr);
  void Bind(mojo::PendingReceiver<mojom::GpuService> pending_receiver);

  scoped_refptr<gpu::SharedContextState> GetContextState();

  // Notifies the GpuHost to stop using GPU compositing. This should be called
  // in response to an error in the GPU process that occurred after
  // InitializeWithHost() was called, otherwise GpuFeatureInfo should be set
  // accordingly. This can safely be called from any thread.
  void DisableGpuCompositing();

  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           bool cache_shaders_on_disk,
                           EstablishGpuChannelCallback callback) override;
  void CloseChannel(int32_t client_id) override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void CreateArcVideoDecodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver)
      override;
  void CreateArcVideoEncodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver)
      override;
  void CreateArcVideoProtectedBufferAllocator(
      mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
          pba_receiver) override;
  void CreateArcProtectedBufferManager(
      mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver)
      override;
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override;
  void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          vea_provider_receiver) override;
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
  void CopyGpuMemoryBuffer(gfx::GpuMemoryBufferHandle buffer_handle,
                           base::UnsafeSharedMemoryRegion shared_memory,
                           CopyGpuMemoryBufferCallback callback) override;
  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override;
  void StartPeakMemoryMonitor(uint32_t sequence_num) override;
  void GetPeakMemoryUsage(uint32_t sequence_num,
                          GetPeakMemoryUsageCallback callback) override;

  void RequestHDRStatus(RequestHDRStatusCallback callback) override;
  void LoadedShader(int32_t client_id,
                    const std::string& key,
                    const std::string& data) override;
  void WakeUpGpu() override;
  void GpuSwitched(gl::GpuPreference active_gpu_heuristic) override;
  void DisplayAdded() override;
  void DisplayRemoved() override;
  void DisplayMetricsChanged() override;
  void DestroyAllChannels() override;
  void OnBackgroundCleanup() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;
#if !defined(OS_ANDROID)
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) override;
#endif
#if defined(OS_APPLE)
  void BeginCATransaction() override;
  void CommitCATransaction(CommitCATransactionCallback callback) override;
#endif
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void WriteClangProfilingProfile(
      WriteClangProfilingProfileCallback callback) override;
#endif
  void Crash() override;
  void Hang() override;
  void ThrowJavaException() override;
  void Stop(StopCallback callback) override;

  // gpu::GpuChannelManagerDelegate:
  void RegisterDisplayContext(gpu::DisplayContext* display_context) override;
  void UnregisterDisplayContext(gpu::DisplayContext* display_context) override;
  void LoseAllContexts() override;
  void DidCreateContextSuccessfully() override;
  void DidCreateOffscreenContext(const GURL& active_url) override;
  void DidDestroyChannel(int client_id) override;
  void DidDestroyAllChannels() override;
  void DidDestroyOffscreenContext(const GURL& active_url) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
#if defined(OS_WIN)
  void DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) override;
  void DidUpdateHDRStatus(bool hdr_enabled) override;
#endif
  void StoreShaderToDisk(int client_id,
                         const std::string& key,
                         const std::string& shader) override;
  void MaybeExitOnContextLost() override;
  bool IsExiting() const override;
  gpu::Scheduler* GetGpuScheduler() override;

#if defined(OS_WIN)
  void SendCreatedChildWindow(gpu::SurfaceHandle parent_window,
                              gpu::SurfaceHandle child_window) override;
#endif

  // Installs a base::LogMessageHandlerFunction which ensures messages are sent
  // to the mojom::GpuHost once InitializeWithHost() completes.
  //
  // In the event of aborted initialization, FlushPreInitializeLogMessages() may
  // be called to flush the accumulated logs to the remote host.
  //
  // Note: ~GpuServiceImpl() will clear installed log handlers.
  static void InstallPreInitializeLogHandler();
  static void FlushPreInitializeLogMessages(mojom::GpuHost* gpu_host);

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

  gpu::raster::GrShaderCache* gr_shader_cache() {
    return gpu_channel_manager_->gr_shader_cache();
  }

  gpu::SyncPointManager* sync_point_manager() {
    return gpu_channel_manager_->sync_point_manager();
  }

  scoped_refptr<base::SingleThreadTaskRunner>& main_runner() {
    return main_runner_;
  }

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

#if BUILDFLAG(ENABLE_VULKAN)
  bool is_using_vulkan() const {
    return !!vulkan_context_provider_ &&
           gpu_preferences_.gr_context_type == gpu::GrContextType::kVulkan;
  }
  VulkanContextProvider* vulkan_context_provider() {
    return vulkan_context_provider_.get();
  }
#else
  bool is_using_vulkan() const { return false; }
  VulkanContextProvider* vulkan_context_provider() { return nullptr; }
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
  bool is_using_dawn() const { return !!dawn_context_provider_; }
  DawnContextProvider* dawn_context_provider() {
    return dawn_context_provider_.get();
  }
#else
  bool is_using_dawn() const { return false; }
  DawnContextProvider* dawn_context_provider() { return nullptr; }
#endif

 private:
  void RecordLogMessage(int severity,
                        const std::string& header,
                        const std::string& message);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void CreateArcVideoDecodeAcceleratorOnMainThread(
      mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver);
  void CreateArcVideoEncodeAcceleratorOnMainThread(
      mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver);
  void CreateArcVideoProtectedBufferAllocatorOnMainThread(
      mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
          pba_receiver);
  void CreateArcProtectedBufferManagerOnMainThread(
      mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void RequestHDRStatusOnMainThread(RequestHDRStatusCallback callback);

  void OnBackgroundedOnMainThread();

  // Ensure that all peak memory tracking occurs on the main thread as all
  // MemoryTracker are created on that thread. All requests made before
  // GpuServiceImpl::InitializeWithHost will be enqueued.
  void StartPeakMemoryMonitorOnMainThread(uint32_t sequence_num);
  void GetPeakMemoryUsageOnMainThread(uint32_t sequence_num,
                                      GetPeakMemoryUsageCallback callback);

  // Attempts to cleanly exit the process but only if not running in host
  // process. If |for_context_loss| is true an error message will be logged.
  void MaybeExit(bool for_context_loss);

  // Update overlay info and HDR status on the GPU process and send the updated
  // info back to the browser process if there is a change.
#if defined(OS_WIN)
  void UpdateOverlayAndHDRInfo();
#endif

  scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  // Do not change the class member order here. watchdog_thread_ should be the
  // last one to be destroyed before main_runner_ and io_runner_.
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;

  const gpu::GpuPreferences gpu_preferences_;

  // Information about the GPU, such as device and vendor ID.
  gpu::GPUInfo gpu_info_;

  // Information about general chrome feature support for the GPU.
  gpu::GpuFeatureInfo gpu_feature_info_;

  bool hdr_enabled_ = false;

  // What we would have gotten if we haven't fallen back to SwiftShader or
  // pure software (in the viz case).
  base::Optional<gpu::GPUInfo> gpu_info_for_hardware_gpu_;
  base::Optional<gpu::GpuFeatureInfo> gpu_feature_info_for_hardware_gpu_;

  // Information about the GPU process populated on creation.
  gfx::GpuExtraInfo gpu_extra_info_;

  mojo::SharedRemote<mojom::GpuHost> gpu_host_;
  std::unique_ptr<gpu::GpuChannelManager> gpu_channel_manager_;
  std::unique_ptr<media::MediaGpuChannelManager> media_gpu_channel_manager_;

  // On some platforms (e.g. android webview), the SyncPointManager and
  // SharedImageManager comes from external sources.
  std::unique_ptr<gpu::SyncPointManager> owned_sync_point_manager_;

  std::unique_ptr<gpu::SharedImageManager> owned_shared_image_manager_;

  std::unique_ptr<gpu::Scheduler> scheduler_;

#if BUILDFLAG(ENABLE_VULKAN)
  gpu::VulkanImplementation* vulkan_implementation_;
  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;
#endif
  std::unique_ptr<MetalContextProvider> metal_context_provider_;
#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<DawnContextProvider> dawn_context_provider_;
#endif

  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  // An event that will be signalled when we shutdown. On some platforms it
  // comes from external sources.
  std::unique_ptr<base::WaitableEvent> owned_shutdown_event_;
  base::WaitableEvent* shutdown_event_ = nullptr;

  // Callback that safely exits GPU process.
  base::OnceCallback<void(base::Optional<ExitCode>)> exit_callback_;
  base::AtomicFlag is_exiting_;

  // Used for performing hardware decode acceleration of images. This is shared
  // by all the GPU channels.
  std::unique_ptr<gpu::ImageDecodeAcceleratorWorker>
      image_decode_accelerator_worker_;

  base::Time start_time_;

  // Used to track the task to bind |receiver_| on the IO thread.
  base::CancelableTaskTracker bind_task_tracker_;
  // Should only be accessed on the IO thread after creation.
  mojo::Receiver<mojom::GpuService> receiver_{this};

#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Display compositor contexts that don't have a corresponding GPU channel.
  base::ObserverList<gpu::DisplayContext>::Unchecked display_contexts_;

  base::WeakPtr<GpuServiceImpl> weak_ptr_;
  base::WeakPtrFactory<GpuServiceImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuServiceImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_GPU_SERVICE_IMPL_H_
