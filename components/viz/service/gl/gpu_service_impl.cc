// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/metal_context_provider.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/dx_diag_node.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_decode_accelerator_worker.h"
#include "gpu/vulkan/buildflags.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/gpu_video_accelerator_util.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/ipc/service/gpu_video_decode_accelerator.h"
#include "media/gpu/ipc/service/media_gpu_channel_manager.h"
#include "media/mojo/services/mojo_video_encode_accelerator_provider.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLAssembleInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
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

#if defined(OS_ANDROID)
#include "components/viz/service/gl/throw_uncaught_exception.h"
#include "media/base/android/media_codec_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"
#include "components/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "components/arc/video_accelerator/protected_buffer_manager_proxy.h"
#include "components/chromeos_camera/gpu_mjpeg_decode_accelerator_factory.h"
#include "components/chromeos_camera/mojo_jpeg_encode_accelerator_service.h"
#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_WIN)
#include "ui/gl/direct_composition_surface_win.h"
#endif

#if defined(OS_APPLE)
#include "ui/base/cocoa/quartz_util.h"
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "components/viz/common/gpu/dawn_context_provider.h"
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
#include "base/test/clang_profiling.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#endif

namespace viz {

namespace {

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
  void ShutdownLogging() { logging::SetLogMessageHandler(nullptr); }

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

void GetVideoCapabilities(const gpu::GpuPreferences& gpu_preferences,
                          const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
                          gpu::GPUInfo* gpu_info) {
  // Due to https://crbug.com/709631, we don't want to query Android video
  // decode/encode capabilities during startup. The renderer needs this info
  // though, so assume some baseline capabilities.
#if defined(OS_ANDROID)
  // Note: Video encoding on Android relies on MediaCodec, so all cases
  // where it's disabled for decoding it is also disabled for encoding.
  if (gpu_preferences.disable_accelerated_video_decode ||
      gpu_preferences.disable_accelerated_video_encode) {
    return;
  }

  auto& encoding_profiles =
      gpu_info->video_encode_accelerator_supported_profiles;

  gpu::VideoEncodeAcceleratorSupportedProfile vea_profile;
  vea_profile.max_resolution = gfx::Size(1280, 720);
  vea_profile.max_framerate_numerator = 30;
  vea_profile.max_framerate_denominator = 1;

  if (media::MediaCodecUtil::IsVp8EncoderAvailable()) {
    vea_profile.profile = gpu::VP8PROFILE_ANY;
    encoding_profiles.push_back(vea_profile);
  }

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (media::MediaCodecUtil::IsH264EncoderAvailable(/*use_codec_list*/ false)) {
    vea_profile.profile = gpu::H264PROFILE_BASELINE;
    encoding_profiles.push_back(vea_profile);
  }
#endif

  // Note: Since Android doesn't have to support PPAPI/Flash, we have not
  // returned the decoder profiles here since https://crrev.com/665999.
#else
  gpu_info->video_decode_accelerator_capabilities =
      media::GpuVideoDecodeAccelerator::GetCapabilities(gpu_preferences,
                                                        gpu_workarounds);
  gpu_info->video_encode_accelerator_supported_profiles =
      media::GpuVideoAcceleratorUtil::ConvertMediaToGpuEncodeProfiles(
          media::GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
              gpu_preferences, gpu_workarounds));
#endif
}

// Returns a callback which does a PostTask to run |callback| on the |runner|
// task runner.
template <typename... Params>
base::OnceCallback<void(Params&&...)> WrapCallback(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    base::OnceCallback<void(Params...)> callback) {
  return base::BindOnce(
      [](base::SingleThreadTaskRunner* runner,
         base::OnceCallback<void(Params && ...)> callback, Params&&... params) {
        runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback),
                                        std::forward<Params>(params)...));
      },
      base::RetainedRef(std::move(runner)), std::move(callback));
}

}  // namespace

GpuServiceImpl::GpuServiceImpl(
    const gpu::GPUInfo& gpu_info,
    std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::GpuPreferences& gpu_preferences,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info,
    gpu::VulkanImplementation* vulkan_implementation,
    base::OnceCallback<void(base::Optional<ExitCode>)> exit_callback)
    : main_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_runner_(std::move(io_runner)),
      watchdog_thread_(std::move(watchdog_thread)),
      gpu_preferences_(gpu_preferences),
      gpu_info_(gpu_info),
      gpu_feature_info_(gpu_feature_info),
      gpu_info_for_hardware_gpu_(gpu_info_for_hardware_gpu),
      gpu_feature_info_for_hardware_gpu_(gpu_feature_info_for_hardware_gpu),
      gpu_extra_info_(gpu_extra_info),
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_(vulkan_implementation),
#endif
      exit_callback_(std::move(exit_callback)) {
  DCHECK(!io_runner_->BelongsToCurrentThread());
  DCHECK(exit_callback_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  protected_buffer_manager_ = new arc::ProtectedBufferManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  GrContextOptions context_options =
      GetDefaultGrContextOptions(gpu_preferences_.gr_context_type);
  if (gpu_preferences_.force_max_texture_size) {
    context_options.fMaxTextureSizeOverride =
        gpu_preferences_.force_max_texture_size;
  }

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
    // If GL is using a real GPU, the gpu_info will be passed in and vulkan will
    // use the same GPU.
    vulkan_context_provider_ = VulkanInProcessContextProvider::Create(
        vulkan_implementation_, gpu_preferences_.vulkan_heap_memory_limit,
        gpu_preferences_.vulkan_sync_cpu_memory_limit,
        (is_native_vulkan && is_native_gl) ? &gpu_info : nullptr);
    if (vulkan_context_provider_) {
      // If Vulkan is supported, then OOP-R is supported.
      gpu_info_.oop_rasterization_supported = true;
      gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
          gpu::kGpuFeatureStatusEnabled;
    } else {
      DLOG(ERROR) << "Failed to create Vulkan context provider.";
    }
  }
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
  if (gpu_preferences_.gr_context_type == gpu::GrContextType::kDawn) {
    dawn_context_provider_ = DawnContextProvider::Create();
    if (dawn_context_provider_) {
      gpu_info_.oop_rasterization_supported = true;
      gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
          gpu::kGpuFeatureStatusEnabled;
    } else {
      DLOG(ERROR) << "Failed to create Dawn context provider.";
    }
  }
#endif

#if BUILDFLAG(USE_VAAPI_IMAGE_CODECS)
  image_decode_accelerator_worker_ =
      media::VaapiImageDecodeAcceleratorWorker::Create();
#endif  // BUILDFLAG(USE_VAAPI_IMAGE_CODECS)

#if defined(OS_APPLE)
  if (gpu_feature_info_.status_values[gpu::GPU_FEATURE_TYPE_METAL] ==
      gpu::kGpuFeatureStatusEnabled) {
    metal_context_provider_ = MetalContextProvider::Create(context_options);
  }
#endif

#if defined(OS_WIN)
  auto info_callback = base::BindRepeating(
      &GpuServiceImpl::UpdateOverlayAndHDRInfo, weak_ptr_factory_.GetWeakPtr());
  gl::DirectCompositionSurfaceWin::SetOverlayHDRGpuInfoUpdateCallback(
      info_callback);
#endif

  gpu_memory_buffer_factory_ =
      gpu::GpuMemoryBufferFactory::CreateNativeType(vulkan_context_provider());

  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

GpuServiceImpl::~GpuServiceImpl() {
  DCHECK(main_runner_->BelongsToCurrentThread());

  // Ensure we don't try to exit when already in the process of exiting.
  is_exiting_.Set();

  bind_task_tracker_.TryCancelAll();
  GetLogMessageManager()->ShutdownLogging();

  // Destroy the receiver on the IO thread.
  {
    base::WaitableEvent wait;
    auto destroy_receiver_task = base::BindOnce(
        [](mojo::Receiver<mojom::GpuService>* receiver,
           base::WaitableEvent* wait) {
          receiver->reset();
          wait->Signal();
        },
        &receiver_, base::Unretained(&wait));
    if (io_runner_->PostTask(FROM_HERE, std::move(destroy_receiver_task)))
      wait.Wait();
  }

  if (watchdog_thread_)
    watchdog_thread_->OnGpuProcessTearDown();

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
  scheduler_.reset();
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
  gpu::GpuDriverBugWorkarounds gpu_workarounds(
      gpu_feature_info_.enabled_gpu_driver_bug_workarounds);

  GetVideoCapabilities(gpu_preferences_, gpu_workarounds, &gpu_info_);

  gpu_info_.jpeg_decode_accelerator_supported =
      IsAcceleratedJpegDecodeSupported();

  if (image_decode_accelerator_worker_) {
    gpu_info_.image_decode_accelerator_supported_profiles =
        image_decode_accelerator_worker_->GetSupportedProfiles();
  }

  // Record initialization only after collecting the GPU info because that can
  // take a significant amount of time.
  gpu_info_.initialization_time = base::Time::Now() - start_time_;
}

void GpuServiceImpl::UpdateGPUInfoGL() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu::CollectGraphicsInfoGL(&gpu_info_);
  gpu_host_->DidUpdateGPUInfo(gpu_info_);
}

void GpuServiceImpl::InitializeWithHost(
    mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
    gpu::GpuProcessActivityFlags activity_flags,
    scoped_refptr<gl::GLSurface> default_offscreen_surface,
    gpu::SyncPointManager* sync_point_manager,
    gpu::SharedImageManager* shared_image_manager,
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
    bool thread_safe_manager = features::ShouldUseRealBuffersForPageFlipTest();
    owned_shared_image_manager_ = std::make_unique<gpu::SharedImageManager>(
        thread_safe_manager, false /* display_context_on_another_thread */);
    shared_image_manager = owned_shared_image_manager_.get();
  } else {
    // With this feature enabled, we don't expect to receive an external
    // SharedImageManager.
    DCHECK(!features::ShouldUseRealBuffersForPageFlipTest());
  }

  shutdown_event_ = shutdown_event;
  if (!shutdown_event_) {
    owned_shutdown_event_ = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    shutdown_event_ = owned_shutdown_event_.get();
  }

  scheduler_ = std::make_unique<gpu::Scheduler>(
      main_runner_, sync_point_manager, gpu_preferences_);

  // Defer creation of the render thread. This is to prevent it from handling
  // IPC messages before the sandbox has been enabled and all other necessary
  // initialization has succeeded.
  gpu_channel_manager_ = std::make_unique<gpu::GpuChannelManager>(
      gpu_preferences_, this, watchdog_thread_.get(), main_runner_, io_runner_,
      scheduler_.get(), sync_point_manager, shared_image_manager,
      gpu_memory_buffer_factory_.get(), gpu_feature_info_,
      std::move(activity_flags), std::move(default_offscreen_surface),
      image_decode_accelerator_worker_.get(), vulkan_context_provider(),
      metal_context_provider_.get(), dawn_context_provider());

  media_gpu_channel_manager_.reset(
      new media::MediaGpuChannelManager(gpu_channel_manager_.get()));
  if (watchdog_thread())
    watchdog_thread()->AddPowerObserver();
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

gpu::ImageFactory* GpuServiceImpl::gpu_image_factory() {
  return gpu_memory_buffer_factory_
             ? gpu_memory_buffer_factory_->AsImageFactory()
             : nullptr;
}

// static
void GpuServiceImpl::InstallPreInitializeLogHandler() {
  GetLogMessageManager()->InstallPreInitializeLogHandler();
}

// static
void GpuServiceImpl::FlushPreInitializeLogMessages(mojom::GpuHost* gpu_host) {
  GetLogMessageManager()->FlushMessages(gpu_host);
}

void GpuServiceImpl::RecordLogMessage(int severity,
                                      const std::string& header,
                                      const std::string& message) {
  // This can be run from any thread.
  gpu_host_->RecordLogMessage(severity, std::move(header), std::move(message));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void GpuServiceImpl::CreateArcVideoDecodeAccelerator(
    mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GpuServiceImpl::CreateArcVideoDecodeAcceleratorOnMainThread,
          weak_ptr_, std::move(vda_receiver)));
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
  DCHECK(main_runner_->BelongsToCurrentThread());
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecodeAccelerator>(
          gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds(),
          protected_buffer_manager_),
      std::move(vda_receiver));
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

void GpuServiceImpl::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  chromeos_camera::MojoMjpegDecodeAcceleratorService::Create(
      std::move(jda_receiver));
}

void GpuServiceImpl::CreateJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  chromeos_camera::MojoJpegEncodeAcceleratorService::Create(
      std::move(jea_receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void GpuServiceImpl::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        vea_provider_receiver) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  media::MojoVideoEncodeAcceleratorProvider::Create(
      std::move(vea_provider_receiver),
      base::BindRepeating(&media::GpuVideoEncodeAcceleratorFactory::CreateVEA),
      gpu_preferences_, gpu_channel_manager_->gpu_driver_bug_workarounds());
}

void GpuServiceImpl::CreateGpuMemoryBuffer(
    gfx::GpuMemoryBufferId id,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    int client_id,
    gpu::SurfaceHandle surface_handle,
    CreateGpuMemoryBufferCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  // This needs to happen in the IO thread.
  gpu_memory_buffer_factory_->CreateGpuMemoryBufferAsync(
      id, size, format, usage, client_id, surface_handle, std::move(callback));
}

void GpuServiceImpl::DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                            int client_id,
                                            const gpu::SyncToken& sync_token) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DestroyGpuMemoryBuffer,
                                  weak_ptr_, id, client_id, sync_token));
    return;
  }
  gpu_channel_manager_->DestroyGpuMemoryBuffer(id, client_id, sync_token);
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
    auto wrap_callback = WrapCallback(io_runner_, std::move(callback));
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

void GpuServiceImpl::RequestHDRStatus(RequestHDRStatusCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::RequestHDRStatusOnMainThread,
                                weak_ptr_, std::move(callback)));
}

void GpuServiceImpl::RequestHDRStatusOnMainThread(
    RequestHDRStatusCallback callback) {
  DCHECK(main_runner_->BelongsToCurrentThread());

#if defined(OS_WIN)
  hdr_enabled_ = gl::DirectCompositionSurfaceWin::IsHDRSupported();
#endif
  io_runner_->PostTask(FROM_HERE,
                       base::BindOnce(std::move(callback), hdr_enabled_));
}

void GpuServiceImpl::RegisterDisplayContext(
    gpu::DisplayContext* display_context) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  display_contexts_.AddObserver(display_context);
}

void GpuServiceImpl::UnregisterDisplayContext(
    gpu::DisplayContext* display_context) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  display_contexts_.RemoveObserver(display_context);
}

void GpuServiceImpl::LoseAllContexts() {
  DCHECK(main_runner_->BelongsToCurrentThread());

  if (IsExiting())
    return;

  for (auto& display_context : display_contexts_)
    display_context.MarkContextLost();
  gpu_channel_manager_->LoseAllContexts();
}

void GpuServiceImpl::DidCreateContextSuccessfully() {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->DidCreateContextSuccessfully();
}

void GpuServiceImpl::DidCreateOffscreenContext(const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
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

void GpuServiceImpl::DidLoseContext(bool offscreen,
                                    gpu::error::ContextLostReason reason,
                                    const GURL& active_url) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->DidLoseContext(offscreen, reason, active_url);
}

#if defined(OS_WIN)
void GpuServiceImpl::DidUpdateOverlayInfo(
    const gpu::OverlayInfo& overlay_info) {
  gpu_host_->DidUpdateOverlayInfo(gpu_info_.overlay_info);
}

void GpuServiceImpl::DidUpdateHDRStatus(bool hdr_enabled) {
  gpu_host_->DidUpdateHDRStatus(hdr_enabled);
}
#endif

void GpuServiceImpl::StoreShaderToDisk(int client_id,
                                       const std::string& key,
                                       const std::string& shader) {
  DCHECK(main_runner_->BelongsToCurrentThread());
  gpu_host_->StoreShaderToDisk(client_id, key, shader);
}

void GpuServiceImpl::MaybeExitOnContextLost() {
  MaybeExit(true);
}

bool GpuServiceImpl::IsExiting() const {
  return is_exiting_.IsSet();
}

#if defined(OS_WIN)
void GpuServiceImpl::SendCreatedChildWindow(gpu::SurfaceHandle parent_window,
                                            gpu::SurfaceHandle child_window) {
  // This can be called from main or display compositor thread.
  gpu_host_->SetChildSurface(parent_window, child_window);
}
#endif

void GpuServiceImpl::EstablishGpuChannel(int32_t client_id,
                                         uint64_t client_tracing_id,
                                         bool is_gpu_host,
                                         bool cache_shaders_on_disk,
                                         EstablishGpuChannelCallback callback) {
  // This should always be called on the IO thread first.
  if (io_runner_->BelongsToCurrentThread()) {
    if (IsExiting()) {
      // We are already exiting so there is no point in responding. Close the
      // receiver so we can safely drop the callback.
      receiver_.reset();
      return;
    }

    if (gpu::IsReservedClientId(client_id)) {
      // This returns a null handle, which is treated by the client as a failure
      // case.
      std::move(callback).Run(mojo::ScopedMessagePipeHandle());
      return;
    }

    EstablishGpuChannelCallback wrap_callback = base::BindOnce(
        [](scoped_refptr<base::SingleThreadTaskRunner> runner,
           EstablishGpuChannelCallback cb,
           mojo::ScopedMessagePipeHandle handle) {
          runner->PostTask(FROM_HERE,
                           base::BindOnce(std::move(cb), std::move(handle)));
        },
        io_runner_, std::move(callback));
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::EstablishGpuChannel, weak_ptr_,
                       client_id, client_tracing_id, is_gpu_host,
                       cache_shaders_on_disk, std::move(wrap_callback)));
    return;
  }

  gpu::GpuChannel* gpu_channel = gpu_channel_manager_->EstablishChannel(
      client_id, client_tracing_id, is_gpu_host, cache_shaders_on_disk);

  if (!gpu_channel) {
    // This returns a null handle, which is treated by the client as a failure
    // case.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle());
    return;
  }
  mojo::MessagePipe pipe;
  gpu_channel->Init(pipe.handle0.release(), shutdown_event_);

  media_gpu_channel_manager_->AddChannel(client_id);

  std::move(callback).Run(std::move(pipe.handle1));
}

void GpuServiceImpl::CloseChannel(int32_t client_id) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::CloseChannel, weak_ptr_, client_id));
    return;
  }
  gpu_channel_manager_->RemoveChannel(client_id);
}

void GpuServiceImpl::LoadedShader(int32_t client_id,
                                  const std::string& key,
                                  const std::string& data) {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::LoadedShader, weak_ptr_,
                                  client_id, key, data));
    return;
  }
  gpu_channel_manager_->PopulateShaderCache(client_id, key, data);
}

void GpuServiceImpl::WakeUpGpu() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::WakeUpGpu, weak_ptr_));
    return;
  }
#if defined(OS_ANDROID)
  gpu_channel_manager_->WakeUpGpu();
#else
  NOTREACHED() << "WakeUpGpu() not supported on this platform.";
#endif
}

void GpuServiceImpl::GpuSwitched(gl::GpuPreference active_gpu_heuristic) {
  DVLOG(1) << "GPU: GPU has switched";
  if (!in_host_process()) {
    ui::GpuSwitchingManager::GetInstance()->NotifyGpuSwitched(
        active_gpu_heuristic);
  }
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::UpdateGPUInfoGL, weak_ptr_));
    return;
  }
  GpuServiceImpl::UpdateGPUInfoGL();
}

void GpuServiceImpl::DisplayAdded() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DisplayAdded, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: A monitor is plugged in";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayAdded();
}

void GpuServiceImpl::DisplayRemoved() {
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE, base::BindOnce(&GpuServiceImpl::DisplayRemoved, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: A monitor is unplugged ";

  if (!in_host_process())
    ui::GpuSwitchingManager::GetInstance()->NotifyDisplayRemoved();
}

void GpuServiceImpl::DisplayMetricsChanged() {
  if (io_runner_->BelongsToCurrentThread()) {
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
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::DestroyAllChannels, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Removing all contexts";
  gpu_channel_manager_->DestroyAllChannels();
}

void GpuServiceImpl::OnBackgroundCleanup() {
// Currently only called on Android.
#if defined(OS_ANDROID)
  if (io_runner_->BelongsToCurrentThread()) {
    main_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&GpuServiceImpl::OnBackgroundCleanup, weak_ptr_));
    return;
  }
  DVLOG(1) << "GPU: Performing background cleanup";
  gpu_channel_manager_->OnBackgroundCleanup();
#else
  NOTREACHED();
#endif
}

void GpuServiceImpl::OnBackgrounded() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  if (watchdog_thread_)
    watchdog_thread_->OnBackgrounded();

  main_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&GpuServiceImpl::OnBackgroundedOnMainThread, weak_ptr_));
}

void GpuServiceImpl::OnBackgroundedOnMainThread() {
  gpu_channel_manager_->OnApplicationBackgrounded();
}

void GpuServiceImpl::OnForegrounded() {
  if (watchdog_thread_)
    watchdog_thread_->OnForegrounded();
}

#if !defined(OS_ANDROID)
void GpuServiceImpl::OnMemoryPressure(
    ::base::MemoryPressureListener::MemoryPressureLevel level) {
  // Forward the notification to the registry of MemoryPressureListeners.
  base::MemoryPressureListener::NotifyMemoryPressure(level);
}
#endif

#if defined(OS_APPLE)
void GpuServiceImpl::BeginCATransaction() {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTask(FROM_HERE, base::BindOnce(&ui::BeginCATransaction));
}

void GpuServiceImpl::CommitCATransaction(CommitCATransactionCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTaskAndReply(FROM_HERE,
                                 base::BindOnce(&ui::CommitCATransaction),
                                 WrapCallback(io_runner_, std::move(callback)));
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
#if defined(OS_ANDROID)
  ThrowUncaughtException();
#else
  NOTREACHED() << "Java exception not supported on this platform.";
#endif
}

void GpuServiceImpl::Stop(StopCallback callback) {
  DCHECK(io_runner_->BelongsToCurrentThread());
  main_runner_->PostTaskAndReply(
      FROM_HERE, base::BindOnce(&GpuServiceImpl::MaybeExit, weak_ptr_, false),
      std::move(callback));
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

void GpuServiceImpl::MaybeExit(bool for_context_loss) {
  DCHECK(main_runner_->BelongsToCurrentThread());

  // We can't restart the GPU process when running in the host process.
  if (in_host_process())
    return;

  if (IsExiting() || !exit_callback_)
    return;

  if (for_context_loss) {
    LOG(ERROR) << "Exiting GPU process because some drivers can't recover "
                  "from errors. GPU process will restart shortly.";
  }
  is_exiting_.Set();
  // For the unsandboxed GPU info collection process used for info collection,
  // if we exit immediately, then the reply message could be lost. That's why
  // the |exit_callback_| takes the boolean argument.
  if (for_context_loss)
    std::move(exit_callback_)
        .Run(ExitCode::RESULT_CODE_GPU_EXIT_ON_CONTEXT_LOST);
  else
    std::move(exit_callback_).Run(base::nullopt);
}

gpu::Scheduler* GpuServiceImpl::GetGpuScheduler() {
  return scheduler_.get();
}

#if defined(OS_WIN)
void GpuServiceImpl::UpdateOverlayAndHDRInfo() {
  gpu::OverlayInfo old_overlay_info = gpu_info_.overlay_info;
  gpu::CollectHardwareOverlayInfo(&gpu_info_.overlay_info);

  // Update overlay info in the GPU process and send the updated data back to
  // the GPU host in the Browser process through mojom if the info has changed.
  if (old_overlay_info != gpu_info_.overlay_info)
    DidUpdateOverlayInfo(gpu_info_.overlay_info);

  // Update HDR status in the GPU process through the GPU host mojom.
  bool old_hdr_enabled_status = hdr_enabled_;
  hdr_enabled_ = gl::DirectCompositionSurfaceWin::IsHDRSupported();
  if (old_hdr_enabled_status != hdr_enabled_)
    DidUpdateHDRStatus(hdr_enabled_);
}
#endif

}  // namespace viz
