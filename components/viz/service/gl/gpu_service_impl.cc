// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/allocator/partition_alloc_support.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/startup_metric_utils/gpu/startup_metric_utils.h"
#include "components/version_info/version_info.h"
#include "components/viz/common/features.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "gpu/vulkan/buildflags.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/webnn_context_provider_impl.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gpu_switching_manager.h"
#include "ui/gl/init/create_gr_gl_interface.h"
#include "ui/gl/init/gl_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_image_decode_accelerator_worker.h"
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "components/viz/service/gl/throw_uncaught_exception.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/chromeos_camera/gpu_mjpeg_decode_accelerator_factory.h"
#include "components/chromeos_camera/mojo_jpeg_encode_accelerator_service.h"
#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "ash/components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
#include "ash/components/arc/video_accelerator/gpu_arc_video_decoder.h"
#endif
#include "ash/components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager_proxy.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#include "media/base/win/mf_feature_checks.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gl/dcomp_surface_registry.h"
#include "ui/gl/direct_composition_support.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "ui/base/cocoa/quartz_util.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

#if BUILDFLAG(SKIA_USE_METAL)
#include "components/viz/common/gpu/metal_context_provider.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace viz {

namespace {

// The names emitted for GPU initialization trace events.
// This code may be removed after the following investigation:
// crbug.com/1350257
constexpr char kGpuInitializationEventCategory[] = "latency";
constexpr char kGpuInitializationEvent[] = "GpuInitialization";

using LogCallback = base::RepeatingCallback<
    void(int severity, const std::string& header, const std::string& message)>;

struct LogMessage {
  LogMessage(int severity,
             const std::string& header,
             const std::string& message)
      : severity(severity),
        header(std::move(header)),
        message(std::move(message)) {}
  const int severity;
  const std::string header;
  const std::string message;
};

// Forward declare log handlers so they can be used within LogMessageManager.
bool PreInitializeLogHandler(int severity,
                             const char* file,
                             int line,
                             size_t message_start,
                             const std::string& message);
bool PostInitializeLogHandler(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& message);

// Class which manages LOG() message forwarding before and after GpuServiceImpl
// InitializeWithHost(). Prior to initialize, log messages are deferred and kept
// within the class. During initialize, InstallPostInitializeLogHandler() will
// be called to flush deferred messages and route new ones directly to GpuHost.
class LogMessageManager {
 public:
  LogMessageManager() = default;
  ~LogMessageManager() = delete;

  // Queues a deferred LOG() message into |deferred_messages_| unless
  // |log_callback_| has been set -- in which case RouteMessage() is called.
  void AddDeferredMessage(int severity,
                          const std::string& header,
                          const std::string& message) {
    base::AutoLock lock(message_lock_);
    // During InstallPostInitializeLogHandler() there's a brief window where a
    // call into this function may be waiting on |message_lock_|, so we need to
    // check if |log_callback_| was set once we get the lock.
    if (log_callback_) {
      RouteMessage(severity, std::move(header), std::move(message));
      return;
    }

    // Otherwise just queue the message for InstallPostInitializeLogHandler() to
    // forward later.
    deferred_messages_.emplace_back(severity, std::move(header),
                                    std::move(message));
  }

  // Used after InstallPostInitializeLogHandler() to route messages directly to
  // |log_callback_|; avoids the need for a global lock.
  void RouteMessage(int severity,
                    const std::string& header,
                    const std::string& message) {
    log_callback_.Run(severity, std::move(header), std::move(message));
  }

  // If InstallPostInitializeLogHandler() will never be called, this method is
  // called prior to process exit to ensure logs are forwarded.
  void FlushMessages(mojom::GpuHost* gpu_host) {
    base::AutoLock lock(message_lock_);
    for (auto& log : deferred_messages_) {
      gpu_host->RecordLogMessage(log.severity, std::move(log.header),
                                 std::move(log.message));
    }
    deferred_messages_.clear();
  }

  // Used prior to InitializeWithHost() during GpuMain startup to ensure logs
  // aren't lost before initialize.
  void InstallPreInitializeLogHandler() {
    DCHECK(!log_callback_);
    logging::SetLogMessageHandler(PreInitializeLogHandler);
  }

  // Called by InitializeWithHost() to take over logging from the
  // PostInitializeLogHandler(). Flushes all deferred messages.
  void InstallPostInitializeLogHandler(LogCallback log_callback) {
    base::AutoLock lock(message_lock_);
    DCHECK(!log_callback_);
    log_callback_ = std::move(log_callback);
    for (auto& log : deferred_messages_)
      RouteMessage(log.severity, std::move(log.header), std::move(log.message));
    deferred_messages_.clear();
    logging::SetLogMessageHandler(PostInitializeLogHandler);
  }

  // Called when it's no longer safe to invoke |log_callback_|.
  void ShutdownLogging() {
    logging::SetLogMessageHandler(nullptr);
    log_callback_.Reset();
  }

 private:
  base::Lock message_lock_;
  std::vector<LogMessage> deferred_messages_ GUARDED_BY(message_lock_);

  // Set once under |mesage_lock_|, but may be accessed without lock after that.
  LogCallback log_callback_;
};

LogMessageManager* GetLogMessageManager() {
  static base::NoDestructor<LogMessageManager> message_manager;
  return message_manager.get();
}

bool PreInitializeLogHandler(int severity,
                             const char* file,
                             int line,
                             size_t message_start,
                             const std::string& message) {
  GetLogMessageManager()->AddDeferredMessage(severity,
                                             message.substr(0, message_start),
                                             message.substr(message_start));
  return false;
}

bool PostInitializeLogHandler(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& message) {
  GetLogMessageManager()->RouteMessage(severity,
                                       message.substr(0, message_start),
                                       message.substr(message_start));
  return false;
}

bool IsAcceleratedJpegDecodeSupported() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return chromeos_camera::GpuMjpegDecodeAcceleratorFactory::
      IsAcceleratedJpegDecodeSupported();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool WillGetGmbConfigFromGpu() {
#if BUILDFLAG(IS_OZONE)
  // Ozone/X11 requires gpu initialization to be done before it can determine
  // what formats gmb can use. This limitation comes from the requirement to
  // have GLX bindings initialized. The buffer formats will be passed through
  // gpu extra info.
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .fetch_buffer_formats_for_gmb_on_gpu;
#else
  return false;
#endif
}

}  // namespace

GpuServiceImpl::GpuServiceImpl(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const std::optional<gpu::GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info,
    InitParams init_params)
    : main_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_runner_(std::move(init_params.io_runner)),
      watchdog_thread_(std::move(init_params.watchdog_thread)),
      gpu_preferences_(gpu_preferences),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      gpu_driver_bug_workarounds_(
          gpu_feature_info_.enabled_gpu_driver_bug_workarounds),
      gpu_info_for_hardware_gpu_(gpu_info_for_hardware_gpu),
      gpu_feature_info_for_hardware_gpu_(gpu_feature_info_for_hardware_gpu),
      gpu_extra_info_(gpu_extra_info),
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_(init_params.vulkan_implementation),
#endif
      exit_callback_(std::move(init_params.exit_callback)),
      clear_shader_cache_(base::FeatureList::IsEnabled(
          features::kClearGrShaderDiskCacheOnInvalidPrefix)) {
  DCHECK(!io_runner_->BelongsToCurrentThread());
  DCHECK(exit_callback_);

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  protected_buffer_manager_ = new arc::ProtectedBufferManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) &&
        // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(ENABLE_VULKAN)
  if (vulkan_implementation_) {
    bool is_native_vulkan =
        gpu_preferences_.use_vulkan == gpu::VulkanImplementationName::kNative ||
        gpu_preferences_.use_vulkan ==
            gpu::VulkanImplementationName::kForcedNative;
    // With swiftshader the vendor_id is 0xffff. For some tests gpu_info is not
    // initialized, so the vendor_id is 0.
    bool is_native_gl =
        gpu_info_.gpu.vendor_id != 0xffff && gpu_info_.gpu.vendor_id != 0;

    const bool is_thread_safe =
        features::IsDrDcEnabled() && !gpu_driver_bug_workarounds_.disable_drdc;
    // If GL is using a real GPU, the gpu_info will be passed in and vulkan will
    // use the same GPU.
    vulkan_context_provider_ = VulkanInProcessContextProvider::Create(
        vulkan_implementation_, gpu_preferences_.vulkan_heap_memory_limit,
        gpu_preferences_.vulkan_sync_cpu_memory_limit, is_thread_safe,
        (is_native_vulkan && is_native_gl) ? &gpu_info_ : nullptr);
    if (!vulkan_context_provider_) {
      DLOG(ERROR) << "Failed to create Vulkan context provider.";
    }
  }
#endif

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
  dawn_caching_interface_factory_ =
      std::make_unique<gpu::webgpu::DawnCachingInterfaceFactory>();
#endif

  if (gpu_preferences_.gr_context_type == gpu::GrContextType::kGraphiteDawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    dawn_context_provider_ = std::move(init_params.dawn_context_provider);

    if (dawn_context_provider_) {
      // GpuServiceImpl holds the instance of DawnContextProvider, so it
      // outlives the DawnContextProvider.
      auto cache_blob_callback = base::BindRepeating(
          [](GpuServiceImpl* self, gpu::GpuDiskCacheType type,
             const std::string& key, const std::string& blob) {
            self->StoreBlobToDisk(gpu::kGraphiteDawnGpuDiskCacheHandle, key,
                                  blob);
          },
          base::Unretained(this));
      auto caching_interface = dawn_caching_interface_factory_->CreateInstance(
          gpu::kGraphiteDawnGpuDiskCacheHandle, std::move(cache_blob_callback));
      dawn_context_provider_->SetCachingInterface(std::move(caching_interface));
    }
#endif  // BUILDFLAG(SKIA_USE_DAWN)
  } else if (gpu_preferences_.gr_context_type ==
             gpu::GrContextType::kGraphiteMetal) {
#if BUILDFLAG(SKIA_USE_METAL)
    metal_context_provider_ = MetalContextProvider::Create();
    if (!metal_context_provider_) {
      DLOG(ERROR) << "Failed to create Metal context provider for Graphite.";
    }
#endif  // BUILDFLAG(SKIA_USE_METAL)
  }

#if BUILDFLAG(USE_VAAPI_IMAGE_CODECS)
  image_decode_accelerator_worker_ =
      media::VaapiImageDecodeAcceleratorWorker::Create();
#endif  // BUILDFLAG(USE_VAAPI_IMAGE_CODECS)

#if BUILDFLAG(IS_WIN)
  if (media::SupportMediaFoundationClearPlayback()) {
    // Initialize the OverlayStateService using the GPUServiceImpl task
    // sequence.
    auto* overlay_state_service = OverlayStateService::GetInstance();
    overlay_state_service->Initialize(
        base::SequencedTaskRunner::GetCurrentDefault());
  }
#endif

  gpu_memory_buffer_factory_ = gpu::GpuMemoryBufferFactory::CreateNativeType(
      vulkan_context_provider(), io_runner_);

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuServiceImpl::~GpuServiceImpl() {
  DCHECK(main_runner_->BelongsToCurrentThread());

  // Ensure we don't try to exit when already in the process of exiting.
  is_exiting_.Set();

  bind_task_tracker_.TryCancelAll();

  if (!in_host_process())
    GetLogMessageManager()->ShutdownLogging();

#if BUILDFLAG(IS_WIN)
  gl::DirectCompositionOverlayCapsMonitor::GetInstance()->RemoveObserver(this);
#endif

  // Destroy the receiver on the IO thread.
  {
    base::WaitableEvent wait;
    auto destroy_receiver_task = base::BindOnce(
        [](mojo::Receiver<mojom::GpuService>* receiver,
           std::unordered_map<int, std::unique_ptr<ClientGmbInterfaceImpl>>
               client_gmb_interface_impl,
           base::WaitableEvent* wait) {
          receiver->reset();
          client_gmb_interface_impl.clear();
          wait->Signal();
        },
        &receiver_, std::move(gmb_clients_), base::Unretained(&wait));
    if (io_runner_->PostTask(FROM_HERE, std::move(destroy_receiver_task)))
      wait.Wait();
  }

  if (watchdog_thread_)
    watchdog_thread_->OnGpuProcessTearDown();

  compositor_gpu_thread_.reset();
  media_gpu_channel_manager_.reset();
  gpu_channel_manager_.reset();

  // Destroy |gpu_memory_buffer_factory_| on the IO thread since its weakptrs
  // are checked there.
  {
    base::WaitableEvent wait;
    auto destroy_gmb_factory = base::BindOnce(
        [](std::unique_ptr<gpu::GpuMemoryBufferFactory> gmb_factory,
           base::WaitableEvent* wait) {
          gmb_factory.reset();
          wait->Signal();
        },
        std::move(gpu_memory_buffer_factory_), base::Unretained(&wait));

    if (io_runner_->PostTask(FROM_HERE, std::move(destroy_gmb_factory))) {
      // |gpu_memory_buffer_factory_| holds a raw pointer to
      // |vulkan_context_provider_|. Waiting here enforces the correct order
      // of destruction.
      wait.Wait();
    }
  }

  // Scheduler must be destroyed before sync point manager is destroyed.
  owned_scheduler_.reset();
  owned_sync_point_manager_.reset();
  owned_shared_image_manager_.reset();

  // The image decode accelerator worker must outlive the GPU channel manager so
  // that it doesn't get any decode requests during/after destruction.
  DCHECK(!gpu_channel_manager_);
  image_decode_accelerator_worker_.reset();

  // Signal this event before destroying the child process. That way all
  // background threads can cleanup. For example, in the renderer the
  // RenderThread instances will be able to notice shutdown before the render
  // process begins waiting for them to exit.
  if (owned_shutdown_event_)
    owned_shutdown_event_->Signal();
}

void GpuServiceImpl::UpdateGPUInfo() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_host_);

  gpu_info_.jpeg_decode_accelerator_supported =
      IsAcceleratedJpegDecodeSupported();

  if (image_decode_accelerator_worker_) {
    gpu_info_.image_decode_accelerator_supported_profiles =
        image_decode_accelerator_worker_->GetSupportedProfiles();
  }

#if BUILDFLAG(IS_WIN)
  gpu_info_.shared_image_d3d =
      gpu::D3DImageBackingFactory::IsD3DSharedImageSupported(gpu_preferences_);
#endif

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  base::TimeTicks now = base::TimeTicks::Now();
  gpu_info_.initialization_time = now - start_time_;
  startup_metric_utils::GetGpu().RecordGpuInitialized(now);

  // This metric code may be removed after the following investigation:
  // crbug.com/1350257
  UMA_HISTOGRAM_CUSTOM_TIMES("GPU.GPUInitializationTime.V4",
                             gpu_info_.initialization_time,
                             base::Milliseconds(5), base::Seconds(90), 100);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN_WITH_TIMESTAMP0(
      kGpuInitializationEventCategory, kGpuInitializationEvent,
      TRACE_ID_LOCAL(this), start_time_);
  TRACE_EVENT_NESTABLE_ASYNC_END_WITH_TIMESTAMP0(
      kGpuInitializationEventCategory, kGpuInitializationEvent,
      TRACE_ID_LOCAL(this), now);
}

void GpuServiceImpl::UpdateGPUInfoGL() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu::CollectGraphicsInfoGL(&gpu_info_, GetContextState()->display());
  gpu_host_->DidUpdateGPUInfo(gpu_info_);
}

void GpuServiceImpl::InitializeWithHost(
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    gpu::GpuProcessShmCount use_shader_cache_shm_count,
    scoped_refptr<gl::GLSurface> default_offscreen_surface,
    gpu::SyncPointManager* sync_point_manager,
    gpu::SharedImageManager* shared_image_manager,
    gpu::Scheduler* scheduler,
    base::WaitableEvent* shutdown_event) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  mojo::Remote<mojom::GpuHost> gpu_host(std::move(pending_gpu_host));
  gpu_host->DidInitialize(gpu_info_, gpu_feature_info_,
                          gpu_info_for_hardware_gpu_,
                          gpu_feature_info_for_hardware_gpu_, gpu_extra_info_);
  gpu_host_ = mojo::SharedRemote<mojom::GpuHost>(gpu_host.Unbind(), io_runner_);
  if (!in_host_process()) {
    // The global callback is reset from the dtor. So Unretained() here is safe.
    // Note that the callback can be called from any thread. Consequently, the
    // callback cannot use a WeakPtr.
    GetLogMessageManager()->InstallPostInitializeLogHandler(base::BindRepeating(
        &GpuServiceImpl::RecordLogMessage, base::Unretained(this)));
  }

  if (!sync_point_manager) {
    owned_sync_point_manager_ = std::make_unique<gpu::SyncPointManager>();
    sync_point_manager = owned_sync_point_manager_.get();
  }

  if (!shared_image_manager) {
    // When using real buffers for testing overlay configurations, we need
    // access to SharedImageManager on the viz thread to obtain the buffer
    // corresponding to a mailbox.
    const bool display_context_on_another_thread =
        features::IsDrDcEnabled() && !gpu_driver_bug_workarounds_.disable_drdc;

    // |display_context_on_another_thread|, features::IsUsingRawDraw(),
    // kAlwaysUseRealBufferTestingOnOzone, and kSharedBitmapToSharedImage
    // requires |thread_safe_manager| to be true.
    bool thread_safe_manager = true;
    owned_shared_image_manager_ = std::make_unique<gpu::SharedImageManager>(
        thread_safe_manager, display_context_on_another_thread);
    shared_image_manager = owned_shared_image_manager_.get();
  }

  shutdown_event_ = shutdown_event;
  if (!shutdown_event_) {
    owned_shutdown_event_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    shutdown_event_ = owned_shutdown_event_.get();
  }

  if (scheduler) {
    scheduler_ = scheduler;
  } else {
    owned_scheduler_ = std::make_unique<gpu::Scheduler>(sync_point_manager);
    scheduler_ = owned_scheduler_.get();
  }

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_ = std::make_unique<gpu::GpuChannelManager>(
      gpu_preferences_, this, watchdog_thread_.get(), main_runner_, io_runner_,
      scheduler_, sync_point_manager, shared_image_manager,
      gpu_memory_buffer_factory_.get(), gpu_feature_info_,
      std::move(use_shader_cache_shm_count),
      std::move(default_offscreen_surface),
      image_decode_accelerator_worker_.get(), vulkan_context_provider(),
      metal_context_provider(), dawn_context_provider(),
      dawn_caching_interface_factory());

  media_gpu_channel_manager_ = std::make_unique<media::MediaGpuChannelManager>(
      gpu_channel_manager_.get());

  // Create and Initialize compositor gpu thread.
  {
    CompositorGpuThread::CreateParams params;
    params.gpu_channel_manager = gpu_channel_manager_.get();
    params.display =
        gpu_channel_manager_->default_offscreen_surface()
            ? gpu_channel_manager_->default_offscreen_surface()->GetGLDisplay()
            : nullptr;
    params.enable_watchdog = !!watchdog_thread_;

#if BUILDFLAG(ENABLE_VULKAN)
    params.vulkan_implementation = vulkan_implementation_;
    params.device_queue = vulkan_context_provider_
                              ? vulkan_context_provider_->GetDeviceQueue()
                              : nullptr;
#endif
#if BUILDFLAG(SKIA_USE_DAWN)
    params.dawn_context_provider = dawn_context_provider_.get();
#endif
    compositor_gpu_thread_ = CompositorGpuThread::MaybeCreate(params);
  }

#if BUILDFLAG(IS_WIN)
  // Add GpuServiceImpl to DirectCompositionOverlayCapsMonitor observer list for
  // overlay and DXGI info update. This should be added after |gpu_host_| is
  // initialized.
  gl::DirectCompositionOverlayCapsMonitor::GetInstance()->AddObserver(this);
#endif
}

void GpuServiceImpl::Bind(
    mojo::PendingReceiver<mojom::GpuService> pending_receiver) {
  if (main_runner_->BelongsToCurrentThread()) {
    bind_task_tracker_.PostTask(
        io_runner_.get(), FROM_HERE,
        base::BindOnce(&GpuServiceImpl::Bind, base::Unretained(this),
                       std::move(pending_receiver)));
    return;
  }
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(pending_receiver));
}

void GpuServiceImpl::DisableGpuCompositing() {
  // Can be called from any thread.
  gpu_host_->DisableGpuCompositing();
}

scoped_refptr<gpu::SharedContextState> GpuServiceImpl::GetContextState() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu::ContextResult result;
  return gpu_channel_manager_->GetSharedContextState(&result);
}

// static
void GpuServiceImpl::InstallPreInitializeLogHandler() {
  GetLogMessageManager()->InstallPreInitializeLogHandler();
}

// static
void GpuServiceImpl::FlushPreInitializeLogMessages(mojom::GpuHost* gpu_host) {
  GetLogMessageManager()->FlushMessages(gpu_host);
}

void GpuServiceImpl::SetVisibilityChangedCallback(
    VisibilityChangedCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  visibility_changed_callback_ = std::move(callback);
}

void GpuServiceImpl::RecordLogMessage(int severity,
                                      const std::string& header,
                                      const std::string& message) {
  // This can be run from any thread.
  gpu_host_->RecordLogMessage(severity, std::move(header), std::move(message));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
void GpuServiceImpl::CreateArcVideoDecodeAccelerator(
    mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoDecodeAcceleratorOnMainThread,
          weak_ptr_, std::move(vda_receiver)));
}

void GpuServiceImpl::CreateArcVideoDecoder(
    mojo::PendingReceiver<arc::mojom::VideoDecoder> vd_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::CreateArcVideoDecoderOnMainThread,
                     weak_ptr_, std::move(vd_receiver)));
}

void GpuServiceImpl::CreateArcVideoEncodeAccelerator(
    mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoEncodeAcceleratorOnMainThread,
          weak_ptr_, std::move(vea_receiver)));
}

void GpuServiceImpl::CreateArcVideoProtectedBufferAllocator(
    mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
        pba_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoProtectedBufferAllocatorOnMainThread,
          weak_ptr_, std::move(pba_receiver)));
}

void GpuServiceImpl::CreateArcProtectedBufferManager(
    mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcProtectedBufferManagerOnMainThread,
          weak_ptr_, std::move(pbm_receiver)));
}

void GpuServiceImpl::CreateArcVideoDecodeAcceleratorOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver) {
  CHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecodeAccelerator>(
          gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds(),
          protected_buffer_manager_),
      std::move(vda_receiver));
}

void GpuServiceImpl::CreateArcVideoDecoderOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoDecoder> vd_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecoder>(protected_buffer_manager_),
      std::move(vd_receiver));
#endif
}

void GpuServiceImpl::CreateArcVideoEncodeAcceleratorOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoEncodeAccelerator>(
          gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds()),
      std::move(vea_receiver));
}

void GpuServiceImpl::CreateArcVideoProtectedBufferAllocatorOnMainThread(
    mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
        pba_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  auto gpu_arc_video_protected_buffer_allocator =
      arc::GpuArcVideoProtectedBufferAllocator::Create(
          protected_buffer_manager_);
  if (!gpu_arc_video_protected_buffer_allocator)
    return;
  mojo::MakeSelfOwnedReceiver(
      std::move(gpu_arc_video_protected_buffer_allocator),
      std::move(pba_receiver));
}

void GpuServiceImpl::CreateArcProtectedBufferManagerOnMainThread(
    mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcProtectedBufferManagerProxy>(
          protected_buffer_manager_),
      std::move(pbm_receiver));
}
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

void GpuServiceImpl::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  chromeos_camera::MojoMjpegDecodeAcceleratorService::Create(
      std::move(jda_receiver),
      base::BindRepeating(
          &GpuServiceImpl::SetMjpegDecodeAcceleratorBeginFrameCB,
          base::Unretained(this)));
}

void GpuServiceImpl::CreateJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  chromeos_camera::MojoJpegEncodeAcceleratorService::Create(
      std::move(jea_receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
void GpuServiceImpl::RegisterDCOMPSurfaceHandle(
    mojo::PlatformHandle surface_handle,
    RegisterDCOMPSurfaceHandleCallback callback) {
  base::UnguessableToken token =
      gl::DCOMPSurfaceRegistry::GetInstance()->RegisterDCOMPSurfaceHandle(
          surface_handle.TakeHandle());
  std::move(callback).Run(token);
}

void GpuServiceImpl::UnregisterDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {
  gl::DCOMPSurfaceRegistry::GetInstance()->UnregisterDCOMPSurfaceHandle(token);
}
#endif  // BUILDFLAG(IS_WIN)

void GpuServiceImpl::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  // Offload VEA providers to a dedicated runner. Things like loading profiles
  // and creating encoder might take quite some time, and they might block
  // processing of other mojo calls if executed on the current runner.
  scoped_refptr<base::SequencedTaskRunner> runner;
#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40850116): Fuchsia does not support FIDL communication from
  // ThreadPool's worker threads.
  if (!vea_thread_) {
    base::Thread::Options thread_options(base::MessagePumpType::IO, /*size=*/0);
    vea_thread_ =
        std::make_unique<base::Thread>("GpuVideoEncodeAcceleratorThread");
    CHECK(vea_thread_->StartWithOptions(std::move(thread_options)));
  }
  runner = vea_thread_->task_runner();
#elif BUILDFLAG(IS_WIN)
  // Windows hardware encoder requires a COM STA thread.
  runner = base::ThreadPool::CreateCOMSTATaskRunner({base::MayBlock()});
#else
  // MayBlock() because MF VEA can take long time running GetSupportedProfiles()
  if (base::FeatureList::IsEnabled(
          media::kUseSequencedTaskRunnerForMojoVEAProvider)) {
    runner = base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
  } else {
    runner = base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});
  }
#endif
  media::MojoVideoEncodeAcceleratorProvider::Create(
      std::move(vea_provider_receiver),
      base::BindRepeating(&media::GpuVideoEncodeAcceleratorFactory::CreateVEA),
      gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds(),
      gpu_info_.active_gpu(), std::move(runner),
      media_gpu_channel_manager_->AsWeakPtr(), main_runner_);
}

void GpuServiceImpl::BindClientGmbInterface(
    mojo::PendingReceiver<gpu::mojom::ClientGmbInterface> pending_receiver,
    int client_id) {
  // Bind the receiver to the IO tread. All IPC in this interface will be
  // then received on the IO thread.
  if (main_runner_->BelongsToCurrentThread()) {
    bind_task_tracker_.PostTask(
        io_runner_.get(), FROM_HERE,
        base::BindOnce(&GpuServiceImpl::BindClientGmbInterface,
                       base::Unretained(this), std::move(pending_receiver),
                       client_id));
    return;
  }
  gmb_clients_[client_id] = std::make_unique<ClientGmbInterfaceImpl>(
      client_id, std::move(pending_receiver), this, io_runner_);
}

void GpuServiceImpl::BindWebNNContextProvider(
    mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> pending_receiver,
    int client_id) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::BindWebNNContextProvider, weak_ptr_,
                       std::move(pending_receiver), client_id));
    return;
  }

  if (!webnn_context_provider_) {
    // TODO(crbug.com/345352987): manage `WebNNContextProviderImpl` instance per
    // `client_id` in order to support memory metrics.
    webnn_context_provider_ = webnn::WebNNContextProviderImpl::Create(
        GetContextState(), gpu_feature_info_, gpu_info_,
        base::BindOnce(&GpuServiceImpl::LoseAllContexts, weak_ptr_));
  }

  webnn_context_provider_->BindWebNNContextProvider(
      std::move(pending_receiver));
}

void GpuServiceImpl::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    CreateGpuMemoryBufferCallback callback) {
  // This needs to happen in the IO thread.
  DCHECK(io_runner_->BelongsToCurrentThread());

  // Create a native buffer handle if supported.
  if (IsNativeBufferSupported(format, usage)) {
    gpu_memory_buffer_factory_->CreateGpuMemoryBufferAsync(
        id, size, format, usage, client_id, surface_handle,
        std::move(callback));
    return;
  }

  // Otherwise, create a shared memory handle if supported.
  if (gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(usage) &&
      gpu::GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(size,
                                                                 format)) {
    gfx::GpuMemoryBufferHandle shm_handle;
    shm_handle = gpu::GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
        id, size, format, usage);
    DCHECK_EQ(gfx::SHARED_MEMORY_BUFFER, shm_handle.type);
    std::move(callback).Run(std::move(shm_handle));
    return;
  }

  // By default, return a null handle.
  std::move(callback).Run(gfx::GpuMemoryBufferHandle());
}

void GpuServiceImpl::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                            int client_id) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DestroyGpuMemoryBuffer,
                                  weak_ptr_, id, client_id));
    return;
  }
  gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id);
}

void GpuServiceImpl::CopyGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory,
    CopyGpuMemoryBufferCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  std::move(callback).Run(
      gpu_memory_buffer_factory_->FillSharedMemoryRegionWithBufferContents(
          std::move(buffer_handle), std::move(shared_memory)));
}

void GpuServiceImpl::GetVideoMemoryUsageStats(
    GetVideoMemoryUsageStatsCallback callback) {
  if (io_runner_->BelongsToCurrentThread()) {
    auto wrap_callback = base::BindPostTask(io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::GetVideoMemoryUsageStats,
                                  weak_ptr_, std::move(wrap_callback)));
    return;
  }
  gpu::VideoMemoryUsageStats video_memory_usage_stats;
  gpu_channel_manager_->GetVideoMemoryUsageStats(&video_memory_usage_stats);
  std::move(callback).Run(video_memory_usage_stats);
}

void GpuServiceImpl::StartPeakMemoryMonitor(uint32_t sequence_num) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::StartPeakMemoryMonitorOnMainThread,
                     weak_ptr_, sequence_num));
}

void GpuServiceImpl::GetPeakMemoryUsage(uint32_t sequence_num,
                                        GetPeakMemoryUsageCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::GetPeakMemoryUsageOnMainThread,
                                weak_ptr_, sequence_num, std::move(callback)));
}

#if BUILDFLAG(IS_WIN)
void GpuServiceImpl::RequestDXGIInfo(RequestDXGIInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::RequestDXGIInfoOnMainThread,
                                weak_ptr_, std::move(callback)));
}

void GpuServiceImpl::RequestDXGIInfoOnMainThread(
    RequestDXGIInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  dxgi_info_ = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), dxgi_info_.Clone()));
}
#endif

void GpuServiceImpl::LoseAllContexts() {
  if (IsExiting())
    return;
  gpu_channel_manager_->LoseAllContexts();
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->LoseContext();
}

void GpuServiceImpl::DidCreateContextSuccessfully() {
  gpu_host_->DidCreateContextSuccessfully();
}

void GpuServiceImpl::DidCreateOffscreenContext(const GURL& active_url) {
  gpu_host_->DidCreateOffscreenContext(active_url);
}

void GpuServiceImpl::DidDestroyChannel(int client_id) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  media_gpu_channel_manager_->RemoveChannel(client_id);
  gpu_host_->DidDestroyChannel(client_id);
}

void GpuServiceImpl::DidDestroyAllChannels() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->DidDestroyAllChannels();
}

void GpuServiceImpl::DidDestroyOffscreenContext(const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->DidDestroyOffscreenContext(active_url);
}

void GpuServiceImpl::DidLoseContext(gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {
  gpu_host_->DidLoseContext(reason, active_url);
}

std::string GpuServiceImpl::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    const gpu::GPUInfo::GPUDevice& active_gpu = gpu_info_.active_gpu();
    std::string product =
        std::string(version_info::GetProductNameAndVersionForUserAgent());

    shader_prefix_key_ =
        product + "-" + gpu_info_.gl_vendor + "-" + gpu_info_.gl_renderer +
        "-" + active_gpu.driver_version + "-" + active_gpu.driver_vendor + "-" +
        base::SysInfo::ProcessCPUArchitecture();

#if BUILDFLAG(IS_ANDROID)
    std::string build_fp =
        base::android::BuildInfo::GetInstance()->android_build_fp();
    shader_prefix_key_ += "-" + build_fp;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    // ChromeOS can update independently of Lacros and the GPU driver
    // information is not enough to ensure blob compatibility. See
    // crbug.com/1444684
    std::string chromeos_version = base::SysInfo::OperatingSystemName() + " " +
                                   base::SysInfo::OperatingSystemVersion();
    shader_prefix_key_ += "-" + chromeos_version;
#endif
  }

  return shader_prefix_key_;
}

void GpuServiceImpl::StoreBlobToDisk(const gpu::GpuDiskCacheHandle& handle,
                                     const std::string& key,
                                     const std::string& shader) {
  std::string prefix_key = key;
  if (GetHandleType(handle) == gpu::GpuDiskCacheType::kGlShaders) {
    std::string prefix = GetShaderPrefixKey();
    prefix_key = prefix + ":" + key;
  }
  gpu_host_->StoreBlobToDisk(handle, prefix_key, shader);
}

void GpuServiceImpl::LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                                const std::string& key,
                                const std::string& data) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::LoadedBlob, weak_ptr_,
                                  handle, key, data));
    return;
  }

  std::string no_prefix_key = key;
  if (GetHandleType(handle) == gpu::GpuDiskCacheType::kGlShaders) {
    std::string prefix = GetShaderPrefixKey();
    bool prefix_ok = !key.compare(0, prefix.length(), prefix);
    UMA_HISTOGRAM_BOOLEAN("GPU.ShaderLoadPrefixOK", prefix_ok);
    if (prefix_ok) {
      // Remove the prefix from the key before load.
      no_prefix_key = key.substr(prefix.length() + 1);
    } else {
      // If the prefix is not ok, its likely that all the other entries in the
      // cache will have prefix that does not matches. Clear the whole disk
      // cache in that case to remove all stale entries and make room for newer
      // entries.
      if (clear_shader_cache_) {
        gpu_host_->ClearGrShaderDiskCache();
      }
      return;
    }
  }
  gpu_channel_manager_->PopulateCache(handle, no_prefix_key, data);
}

void GpuServiceImpl::GetIsolationKey(
    int client_id,
    const blink::WebGPUExecutionContextToken& token,
    GetIsolationKeyCallback cb) {
  gpu_host_->GetIsolationKey(client_id, token, std::move(cb));
}

void GpuServiceImpl::MaybeExitOnContextLost(
    bool synthetic_loss,
    gpu::error::ContextLostReason context_lost_reason) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  if (in_host_process()) {
    // We can't restart the GPU process when running in the host process;
    // instead, just hope for recovery from the context loss.
    return;
  }

  if (IsExiting() || !exit_callback_)
    return;

  LOG(ERROR) << "Exiting GPU process because some drivers can't recover "
                "from errors. GPU process will restart shortly.";
  is_exiting_.Set();
  std::move(exit_callback_).Run(ExitCode::RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST);
}

bool GpuServiceImpl::IsExiting() const {
  return is_exiting_.IsSet();
}

void GpuServiceImpl::EstablishGpuChannel(int32_t client_id,
                                         uint64_t client_tracing_id,
                                         bool is_gpu_host,
                                         EstablishGpuChannelCallback callback) {
  // This should always be called on the IO thread first.
  if (io_runner_->BelongsToCurrentThread()) {
    if (IsExiting()) {
      // We are already exiting so there is no point in responding. Close the
      // receiver so we can safely drop the callback.
      receiver_.reset();
      gmb_clients_.clear();
      return;
    }

    if (gpu::IsReservedClientId(client_id)) {
      // This returns a null handle, which is treated by the client as a failure
      // case.
      std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                              gpu::GpuFeatureInfo(),
                              gpu::SharedImageCapabilities());
      return;
    }

    EstablishGpuChannelCallback wrap_callback =
        base::BindPostTask(io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::EstablishGpuChannel,
                                  weak_ptr_, client_id, client_tracing_id,
                                  is_gpu_host, std::move(wrap_callback)));
    return;
  }

  auto channel_token = base::UnguessableToken::Create();
  gpu::GpuChannel* gpu_channel = gpu_channel_manager_->EstablishChannel(
      channel_token, client_id, client_tracing_id, is_gpu_host, gpu_extra_info_,
      gpu_memory_buffer_factory_.get());

  if (!gpu_channel) {
    // This returns a null handle, which is treated by the client as a failure
    // case.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            gpu::SharedImageCapabilities());
    return;
  }
  mojo::MessagePipe pipe;
  gpu_channel->Init(pipe.handle0.release(), shutdown_event_);

  media_gpu_channel_manager_->AddChannel(client_id, channel_token);

  std::move(callback).Run(
      std::move(pipe.handle1), gpu_info_, gpu_feature_info_,
      gpu_channel->shared_image_stub()->factory()->MakeCapabilities());
}

void GpuServiceImpl::SetChannelClientPid(int32_t client_id,
                                         base::ProcessId client_pid) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&GpuServiceImpl::SetChannelClientPid,
                                          weak_ptr_, client_id, client_pid));
    return;
  }

  // Note that the GpuService client must be trusted by definition, so DCHECKing
  // this condition is reasonable.
  DCHECK_NE(client_pid, base::kNullProcessId);
  gpu_channel_manager_->SetChannelClientPid(client_id, client_pid);
}

void GpuServiceImpl::SetChannelDiskCacheHandle(
    int32_t client_id,
    const gpu::GpuDiskCacheHandle& handle) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::SetChannelDiskCacheHandle,
                                  weak_ptr_, client_id, handle));
    return;
  }
  gpu_channel_manager_->SetChannelDiskCacheHandle(client_id, handle);
}

void GpuServiceImpl::OnDiskCacheHandleDestoyed(
    const gpu::GpuDiskCacheHandle& handle) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::OnDiskCacheHandleDestoyed,
                                  weak_ptr_, handle));
    return;
  }
  gpu_channel_manager_->OnDiskCacheHandleDestoyed(handle);
}

void GpuServiceImpl::CloseChannel(int32_t client_id) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::CloseChannel, weak_ptr_, client_id));
    return;
  }
  gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuServiceImpl::SetWakeUpGpuClosure(base::RepeatingClosure closure) {
  wake_up_closure_ = std::move(closure);
}

void GpuServiceImpl::WakeUpGpu() {
  if (wake_up_closure_)
    wake_up_closure_.Run();
  if (main_runner_->BelongsToCurrentThread()) {
    WakeUpGpuOnMainThread();
    return;
  }
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::WakeUpGpuOnMainThread, weak_ptr_));
}

void GpuServiceImpl::WakeUpGpuOnMainThread() {
  if (gpu_feature_info_.IsWorkaroundEnabled(gpu::WAKE_UP_GPU_BEFORE_DRAWING)) {
#if BUILDFLAG(IS_ANDROID)
    gpu_channel_manager_->WakeUpGpu();
#else
    NOTREACHED_IN_MIGRATION() << "WakeUpGpu() not supported on this platform.";
#endif
  }
}

void GpuServiceImpl::GpuSwitched(gl::GpuPreference active_gpu_heuristic) {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::GpuSwitched, weak_ptr_,
                                  active_gpu_heuristic));
    return;
  }
  DVLOG(1) << "GPU: GPU has switched";

  if (watchdog_thread_)
    watchdog_thread_->ReportProgress();

  if (!in_host_process()) {
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched(
        active_gpu_heuristic);
  }
  GpuServiceImpl::UpdateGPUInfoGL();
}

void GpuServiceImpl::DisplayAdded() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DisplayAdded, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: A monitor is plugged in";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayAdded();
}

void GpuServiceImpl::DisplayRemoved() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DisplayRemoved, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: A monitor is unplugged ";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayRemoved();
}

void GpuServiceImpl::DisplayMetricsChanged() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::DisplayMetricsChanged, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Display Metrics changed";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayMetricsChanged();
}

void GpuServiceImpl::DestroyAllChannels() {
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::DestroyAllChannels, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Removing all contexts";
  gpu_channel_manager_->DestroyAllChannels();
}

void GpuServiceImpl::OnBackgroundCleanup() {
  OnBackgroundCleanupGpuMainThread();
  OnBackgroundCleanupCompositorGpuThread();
}

void GpuServiceImpl::OnBackgroundCleanupGpuMainThread() {
  // Currently only called on Android.
#if BUILDFLAG(IS_ANDROID)
  if (!main_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::OnBackgroundCleanupGpuMainThread,
                       weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Performing background cleanup";
  gpu_channel_manager_->OnBackgroundCleanup();
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void GpuServiceImpl::OnBackgroundCleanupCompositorGpuThread() {
  // Currently only called on Android.
#if BUILDFLAG(IS_ANDROID)
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->OnBackgroundCleanup();
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void GpuServiceImpl::OnBackgrounded() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  if (watchdog_thread_)
    watchdog_thread_->OnBackgrounded();
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->OnBackgrounded();

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::OnBackgroundedOnMainThread, weak_ptr_));
}

void GpuServiceImpl::OnBackgroundedOnMainThread() {
  gpu_channel_manager_->OnApplicationBackgrounded();

  if (visibility_changed_callback_) {
    visibility_changed_callback_.Run(false);
    if (gpu_preferences_.enable_gpu_benchmarking_extension) {
      ++gpu_info_.visibility_callback_call_count;
      UpdateGPUInfoGL();
    }
  }

  base::allocator::PartitionAllocSupport::Get()->OnBackgrounded();
}

void GpuServiceImpl::OnForegrounded() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  if (watchdog_thread_)
    watchdog_thread_->OnForegrounded();
  if (compositor_gpu_thread_)
    compositor_gpu_thread_->OnForegrounded();

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::OnForegroundedOnMainThread, weak_ptr_));
}

void GpuServiceImpl::OnForegroundedOnMainThread() {
  if (visibility_changed_callback_) {
    visibility_changed_callback_.Run(true);
    if (gpu_preferences_.enable_gpu_benchmarking_extension) {
      ++gpu_info_.visibility_callback_call_count;
      UpdateGPUInfoGL();
    }
  }
  gpu_channel_manager_->OnApplicationForegounded();
  base::allocator::PartitionAllocSupport::Get()->OnForegrounded();
}

#if !BUILDFLAG(IS_ANDROID)
void GpuServiceImpl::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  // Forward the notification to the registry of MemoryPressureListeners.
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}
#endif

#if BUILDFLAG(IS_APPLE)
void GpuServiceImpl::BeginCATransaction() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(FROM_HERE, base::BindOnce(&ui::BeginCATransaction));
}

void GpuServiceImpl::CommitCATransaction(CommitCATransactionCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&ui::CommitCATransaction),
      base::BindPostTask(io_runner_, std::move(callback)));
}
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void GpuServiceImpl::WriteClangProfilingProfile(
    WriteClangProfilingProfileCallback callback) {
  base::WriteClangProfilingProfile();
  std::move(callback).Run();
}
#endif

void GpuServiceImpl::Crash() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  gl::Crash();
}

void GpuServiceImpl::Hang() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(FROM_HERE, base::BindOnce(&gl::Hang));
}

void GpuServiceImpl::ThrowJavaException() {
  DCHECK(io_runner_->BelongsToCurrentThread());
#if BUILDFLAG(IS_ANDROID)
  ThrowUncaughtException();
#else
  NOTREACHED_IN_MIGRATION() << "Java exception not supported on this platform.";
#endif
}

void GpuServiceImpl::StartPeakMemoryMonitorOnMainThread(uint32_t sequence_num) {
  gpu_channel_manager_->StartPeakMemoryMonitor(sequence_num);
}

void GpuServiceImpl::GetPeakMemoryUsageOnMainThread(
    uint32_t sequence_num,
    GetPeakMemoryUsageCallback callback) {
  uint64_t peak_memory = 0u;
  auto allocation_per_source =
      gpu_channel_manager_->GetPeakMemoryUsage(sequence_num, &peak_memory);
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), peak_memory,
                                      std::move(allocation_per_source)));
}

gpu::Scheduler* GpuServiceImpl::GetGpuScheduler() {
  return scheduler_;
}

bool GpuServiceImpl::OnBeginFrameDerivedImpl(const BeginFrameArgs& args) {
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&GpuServiceImpl::OnBeginFrameOnIO,
                                      base::Unretained(this), args));
  return true;
}

void GpuServiceImpl::OnBeginFrameSourcePausedChanged(bool paused) {}

void GpuServiceImpl::OnBeginFrameOnIO(const BeginFrameArgs& args) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  if (mjpeg_decode_accelerator_begin_frame_cb_) {
    mjpeg_decode_accelerator_begin_frame_cb_->Run();
  }
}

void GpuServiceImpl::SetRequestBeginFrameForGpuServiceCB(
    RequestBeginFrameForGpuServiceCB cb) {
  request_begin_frame_for_gpu_service_cb_ = std::move(cb);
}

void GpuServiceImpl::SetMjpegDecodeAcceleratorBeginFrameCB(
    std::optional<base::RepeatingClosure> cb) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(FROM_HERE,
                         base::BindOnce(request_begin_frame_for_gpu_service_cb_,
                                        cb ? true : false));
  mjpeg_decode_accelerator_begin_frame_cb_ = std::move(cb);
}

#if BUILDFLAG(IS_WIN)
// Update Overlay and DXGI Info
void GpuServiceImpl::OnOverlayCapsChanged() {
  gpu::OverlayInfo old_overlay_info = gpu_info_.overlay_info;
  gpu::CollectHardwareOverlayInfo(&gpu_info_.overlay_info);

  // Update overlay info in the GPU process and send the updated data back to
  // the GPU host in the Browser process through mojom if the info has changed.
  if (old_overlay_info != gpu_info_.overlay_info)
    gpu_host_->DidUpdateOverlayInfo(gpu_info_.overlay_info);

  // Update DXGI adapter info in the GPU process through the GPU host mojom.
  auto old_dxgi_info = std::move(dxgi_info_);
  dxgi_info_ = gl::GetDirectCompositionHDRMonitorDXGIInfo();
  if (!mojo::Equals(dxgi_info_, old_dxgi_info)) {
    gpu_host_->DidUpdateDXGIInfo(dxgi_info_.Clone());
  }
}
#endif

bool GpuServiceImpl::IsNativeBufferSupported(gfx::BufferFormat format,
                                             gfx::BufferUsage usage) {
  // Note that we are initializing the |supported_gmb_configurations_| here to
  // make sure gpu service have already initialized and required metadata like
  // supported buffer configurations have already been sent from browser
  // process to GPU process for wayland.
  if (!supported_gmb_configurations_inited_) {
    supported_gmb_configurations_inited_ = true;
    if (WillGetGmbConfigFromGpu()) {
      // Note that Chrome can be compiled with multiple OZONE platforms but
      // actual OZONE platform is chosen at run-time. Eg: Chrome can be
      // compiled with X11 and Wayland but Wayland can be chosen at runtime.
      // Hence using WillGetGmbConfigFromGpu() which will determine
      // configurations based on actual platform chosen at runtime.
#if BUILDFLAG(IS_OZONE_X11)
      for (const auto& config : gpu_extra_info_.gpu_memory_buffer_support_x11) {
        supported_gmb_configurations_.emplace(config);
      }
#endif  // BUILDFLAG(IS_OZONE_X11)
    } else {
      supported_gmb_configurations_ =
          gpu::GpuMemoryBufferSupport::GetNativeGpuMemoryBufferConfigurations();
    }
  }
  return supported_gmb_configurations_.find(gfx::BufferUsageAndFormat(
             usage, format)) != supported_gmb_configurations_.end();
}

GpuServiceImpl::ClientGmbInterfaceImpl::PendingBufferInfo::PendingBufferInfo() =
    default;
GpuServiceImpl::ClientGmbInterfaceImpl::PendingBufferInfo::PendingBufferInfo(
    PendingBufferInfo&&) = default;
GpuServiceImpl::ClientGmbInterfaceImpl::PendingBufferInfo::
    ~PendingBufferInfo() = default;

GpuServiceImpl::ClientGmbInterfaceImpl::ClientGmbInterfaceImpl(
    int client_id,
    mojo::PendingReceiver<gpu::mojom::ClientGmbInterface> pending_receiver,
    raw_ptr<GpuServiceImpl> gpu_service,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : client_id_(client_id), gpu_service_(gpu_service) {
  receiver_.Bind(std::move(pending_receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&GpuServiceImpl::ClientGmbInterfaceImpl::OnConnectionError,
                     base::Unretained(this)));
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "GpuServiceImpl::ClientGmbInterfaceImpl", std::move(io_runner));
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuServiceImpl::ClientGmbInterfaceImpl::~ClientGmbInterfaceImpl() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void GpuServiceImpl::ClientGmbInterfaceImpl::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gpu::SurfaceHandle surface_handle,
    CreateGpuMemoryBufferCallback callback) {
  if (!gpu::GpuMemoryBufferSupport::IsSizeValid(size)) {
    receiver_.ReportBadMessage("Invalid GMB size");
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  // Ensure that the buffer corresponding to this id is not already pending or
  // allocated.
  if ((pending_buffers_.find(id) != pending_buffers_.end()) ||
      (allocated_buffers_.find(id) != allocated_buffers_.end())) {
    DLOG(ERROR) << "Allocation request for this buffer is either pending or "
                   "have already completed. Hence not allocating a new buffer.";
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
    return;
  }

  PendingBufferInfo pending_buffer_info;
  pending_buffer_info.size = size;
  pending_buffer_info.format = format;
  pending_buffer_info.callback = std::move(callback);
  pending_buffers_.emplace(id, std::move(pending_buffer_info));
  gpu_service_->CreateGpuMemoryBuffer(
      id, size, format, usage, client_id_, surface_handle,
      base::BindOnce(
          &GpuServiceImpl::ClientGmbInterfaceImpl::OnGpuMemoryBufferAllocated,
          weak_ptr_, id));
}

void GpuServiceImpl::ClientGmbInterfaceImpl::DestroyGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id) {
  // Check if the id is present in the allocated_buffers.
  auto allocated_buffer = allocated_buffers_.find(id);
  if (allocated_buffer == allocated_buffers_.end()) {
    DLOG(ERROR) << "Can not find GpuMemoryBuffer to destroy";
    return;
  }
  DCHECK_NE(gfx::EMPTY_BUFFER, allocated_buffer->second.type());
  if (allocated_buffer->second.type() != gfx::SHARED_MEMORY_BUFFER) {
    gpu_service_->DestroyGpuMemoryBuffer(id, client_id_);
  }
  allocated_buffers_.erase(allocated_buffer);
}

void GpuServiceImpl::ClientGmbInterfaceImpl::CopyGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory,
    CopyGpuMemoryBufferCallback callback) {
  gpu_service_->CopyGpuMemoryBuffer(
      std::move(buffer_handle), std::move(shared_memory), std::move(callback));
}

bool GpuServiceImpl::ClientGmbInterfaceImpl::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  uint64_t client_tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();
  for (const auto& allocated_buffer : allocated_buffers_) {
    auto& buffer_info = allocated_buffer.second;
    if (!buffer_info.OnMemoryDump(pmd, client_id_, client_tracing_process_id)) {
      return false;
    }
  }
  return true;
}

void GpuServiceImpl::ClientGmbInterfaceImpl::OnConnectionError() {
  // Destroy all the GMBs corresponding to this client.
  DestroyAllGpuMemoryBuffers();

  // Note that this method destroys the current ClientGmbInterfaceImpl object.
  // So it is not safe to use this pointer after below line.
  gpu_service_->RemoveGmbClient(client_id_);
}

void GpuServiceImpl::ClientGmbInterfaceImpl::OnGpuMemoryBufferAllocated(
    gfx::GpuMemoryBufferId id,
    gfx::GpuMemoryBufferHandle handle) {
  auto pending_buffer = pending_buffers_.find(id);
  auto pending_buffer_info = std::move(pending_buffer->second);
  pending_buffers_.erase(pending_buffer);
  if (!handle.is_null()) {
    CHECK(handle.id == id);
    gpu::AllocatedBufferInfo buffer_info(handle, pending_buffer_info.size,
                                         pending_buffer_info.format);
    allocated_buffers_.emplace(id, buffer_info);
  }
  std::move(pending_buffer_info.callback).Run(std::move(handle));
}

void GpuServiceImpl::ClientGmbInterfaceImpl::DestroyAllGpuMemoryBuffers() {
  for (const auto& allocated_buffer : allocated_buffers_) {
    DCHECK_NE(gfx::EMPTY_BUFFER, allocated_buffer.second.type());
    if (allocated_buffer.second.type() != gfx::SHARED_MEMORY_BUFFER) {
      gpu_service_->DestroyGpuMemoryBuffer(allocated_buffer.first, client_id_);
    }
  }
  allocated_buffers_.clear();

  // Run all pending_buffers callback with null handle.
  for (auto& pending_buffer : pending_buffers_) {
    std::move(pending_buffer.second.callback).Run(gfx::GpuMemoryBufferHandle());
  }
  pending_buffers_.clear();
}

void GpuServiceImpl::GetDawnInfo(bool collect_metrics,
                                 GetDawnInfoCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::GetDawnInfoOnMain, base::Unretained(this),
                     collect_metrics, std::move(callback)));
}

BASE_FEATURE(kPauseWatchdogDuringDawnInfoCollection,
             "PauseWatchdogDuringDawnInfoCollection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDawnInfoCollectionWatchdogReportOnlyMode,
             "EnableDawnInfoCollectionWatchdogReportOnlyMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

void GpuServiceImpl::GetDawnInfoOnMain(bool collect_metrics,
                                       GetDawnInfoCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  static const bool pause_watchdog =
      base::FeatureList::IsEnabled(kPauseWatchdogDuringDawnInfoCollection);
  static const bool report_only_mode = base::FeatureList::IsEnabled(
      kEnableDawnInfoCollectionWatchdogReportOnlyMode);

  std::vector<std::string> dawn_info_list;
  // Pause the watchdog around Dawn info collection since it is known to be
  // slow loading GPU drivers.
  if (watchdog_thread_ && pause_watchdog) {
    watchdog_thread_->PauseWatchdog();
  }

  if (report_only_mode) {
    SCOPED_UMA_HISTOGRAM_TIMER("GPU.Dawn.InfoCollectionTimeMS");
    gpu::CollectDawnInfo(gpu_preferences_, collect_metrics, &dawn_info_list);
  } else {
    // Don't collect metrics if not in report only mode. Otherwise fast timings
    // will be recorded, and very-slow timings will crash and not record,
    // skewing the results.
    gpu::CollectDawnInfo(gpu_preferences_, collect_metrics, &dawn_info_list);
  }

  if (watchdog_thread_ && pause_watchdog) {
    watchdog_thread_->ResumeWatchdog();
  }

  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), dawn_info_list));
}

void GpuServiceImpl::RemoveGmbClient(int client_id) {
  CHECK(io_runner_->BelongsToCurrentThread());
  auto it = gmb_clients_.find(client_id);
  if (it != gmb_clients_.end()) {
    gmb_clients_.erase(it);
  }
}

#if BUILDFLAG(IS_ANDROID)
void GpuServiceImpl::SetHostProcessId(base::ProcessId pid) {
  host_process_id_ = pid;
}
#endif

GpuServiceImpl::InitParams::InitParams() = default;
GpuServiceImpl::InitParams::InitParams(InitParams&& other) = default;
GpuServiceImpl::InitParams& GpuServiceImpl::InitParams::operator=(
    InitParams&& other) = default;
GpuServiceImpl::InitParams::~InitParams() = default;

}  // namespace viz
