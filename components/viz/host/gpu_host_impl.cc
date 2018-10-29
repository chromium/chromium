// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/gpu_host_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/font_render_params.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#endif

namespace viz {
namespace {

// A wrapper around gfx::FontRenderParams that checks it is set and accessed on
// the same thread.
class FontRenderParams {
 public:
  void Set(const gfx::FontRenderParams& params);
  void Reset();
  const base::Optional<gfx::FontRenderParams>& Get();

 private:
  friend class base::NoDestructor<FontRenderParams>;

  FontRenderParams();
  ~FontRenderParams();

  THREAD_CHECKER(thread_checker_);
  base::Optional<gfx::FontRenderParams> params_;

  DISALLOW_COPY_AND_ASSIGN(FontRenderParams);
};

void FontRenderParams::Set(const gfx::FontRenderParams& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params_ = params;
}

void FontRenderParams::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params_ = base::nullopt;
}

const base::Optional<gfx::FontRenderParams>& FontRenderParams::Get() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return params_;
}

FontRenderParams::FontRenderParams() = default;

FontRenderParams::~FontRenderParams() {
  NOTREACHED();
}

FontRenderParams& GetFontRenderParams() {
  static base::NoDestructor<FontRenderParams> instance;
  return *instance;
}

#if defined(USE_OZONE)
// Helper to register Mus/conventional thread bouncers for ozone startup.
void OzoneRegisterStartupCallbackHelper(
    scoped_refptr<base::SingleThreadTaskRunner> host_thread_task_runner,
    base::OnceCallback<void(ui::OzonePlatform*)> callback) {
  // The callback registered in ozone can be called in any thread. So use an
  // intermediary callback that bounces to the GpuHost thread if needed, before
  // running the callback.
  auto bounce_callback = base::BindOnce(
      [](base::SingleThreadTaskRunner* host_thread_task_runner,
         base::OnceCallback<void(ui::OzonePlatform*)> callback,
         ui::OzonePlatform* platform) {
        if (host_thread_task_runner->BelongsToCurrentThread()) {
          std::move(callback).Run(platform);
        } else {
          host_thread_task_runner->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), platform));
        }
      },
      base::RetainedRef(host_thread_task_runner), std::move(callback));
  ui::OzonePlatform::RegisterStartupCallback(std::move(bounce_callback));
}
#endif  // defined(USE_OZONE)

}  // namespace

VizMainWrapper::VizMainWrapper(mojom::VizMainPtr viz_main_ptr)
    : viz_main_ptr_(std::move(viz_main_ptr)) {}

VizMainWrapper::VizMainWrapper(
    mojom::VizMainAssociatedPtr viz_main_associated_ptr)
    : viz_main_associated_ptr_(std::move(viz_main_associated_ptr)) {}

VizMainWrapper::~VizMainWrapper() = default;

void VizMainWrapper::CreateGpuService(
    mojom::GpuServiceRequest request,
    mojom::GpuHostPtr gpu_host,
    discardable_memory::mojom::DiscardableSharedMemoryManagerPtr
        discardable_memory_manager,
    mojo::ScopedSharedBufferHandle activity_flags,
    gfx::FontRenderParams::SubpixelRendering subpixel_rendering) {
  if (viz_main_ptr_) {
    viz_main_ptr_->CreateGpuService(std::move(request), std::move(gpu_host),
                                    std::move(discardable_memory_manager),
                                    std::move(activity_flags),
                                    subpixel_rendering);
  } else {
    viz_main_associated_ptr_->CreateGpuService(
        std::move(request), std::move(gpu_host),
        std::move(discardable_memory_manager), std::move(activity_flags),
        subpixel_rendering);
  }
}

void VizMainWrapper::CreateFrameSinkManager(
    mojom::FrameSinkManagerParamsPtr params) {
  if (viz_main_ptr_)
    viz_main_ptr_->CreateFrameSinkManager(std::move(params));
  else
    viz_main_associated_ptr_->CreateFrameSinkManager(std::move(params));
}

GpuHostImpl::InitParams::InitParams() = default;

GpuHostImpl::InitParams::InitParams(InitParams&&) = default;

GpuHostImpl::InitParams::~InitParams() = default;

GpuHostImpl::GpuHostImpl(Delegate* delegate,
                         std::unique_ptr<VizMainWrapper> viz_main_ptr,
                         InitParams params)
    : delegate_(delegate),
      viz_main_ptr_(std::move(viz_main_ptr)),
      params_(std::move(params)),
      host_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      gpu_host_binding_(this),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  mojom::GpuHostPtr host_proxy;
  gpu_host_binding_.Bind(mojo::MakeRequest(&host_proxy));

  discardable_memory::mojom::DiscardableSharedMemoryManagerPtr
      discardable_manager_ptr;
  auto discardable_request = mojo::MakeRequest(&discardable_manager_ptr);
  delegate_->BindDiscardableMemoryRequest(std::move(discardable_request));

  DCHECK(GetFontRenderParams().Get());
  viz_main_ptr_->CreateGpuService(
      mojo::MakeRequest(&gpu_service_ptr_), std::move(host_proxy),
      std::move(discardable_manager_ptr), activity_flags_.CloneHandle(),
      GetFontRenderParams().Get()->subpixel_rendering);

#if defined(USE_OZONE)
  InitOzone();
#endif  // defined(USE_OZONE)
}

GpuHostImpl::~GpuHostImpl() {
  SendOutstandingReplies();
}

// static
void GpuHostImpl::InitFontRenderParams(const gfx::FontRenderParams& params) {
  DCHECK(!GetFontRenderParams().Get());
  GetFontRenderParams().Set(params);
}

// static
void GpuHostImpl::ResetFontRenderParams() {
  DCHECK(GetFontRenderParams().Get());
  GetFontRenderParams().Reset();
}

void GpuHostImpl::OnProcessLaunched(base::ProcessId pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(base::kNullProcessId, pid_);
  DCHECK_NE(base::kNullProcessId, pid);
  pid_ = pid;
}

void GpuHostImpl::OnProcessCrashed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the GPU process crashed while compiling a shader, we may have invalid
  // cached binaries. Completely clear the shader cache to force shader binaries
  // to be re-created.
  if (activity_flags_.IsFlagSet(
          gpu::ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY)) {
    auto* shader_cache_factory = delegate_->GetShaderCacheFactory();
    for (auto cache_key : client_id_to_shader_cache_) {
      // This call will temporarily extend the lifetime of the cache (kept
      // alive in the factory), and may drop loads of cached shader binaries if
      // it takes a while to complete. As we are intentionally dropping all
      // binaries, this behavior is fine.
      shader_cache_factory->ClearByClientId(
          cache_key.first, base::Time(), base::Time::Max(), base::DoNothing());
    }
  }
}

void GpuHostImpl::AddConnectionErrorHandler(base::OnceClosure handler) {
  connection_error_handlers_.push_back(std::move(handler));
}

void GpuHostImpl::BlockLiveOffscreenContexts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto iter = urls_with_live_offscreen_contexts_.begin();
       iter != urls_with_live_offscreen_contexts_.end(); ++iter) {
    delegate_->BlockDomainFrom3DAPIs(*iter, gpu::DomainGuilt::kUnknown);
  }
}

void GpuHostImpl::ConnectFrameSinkManager(
    mojom::FrameSinkManagerRequest request,
    mojom::FrameSinkManagerClientPtrInfo client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::ConnectFrameSinkManager");

  mojom::FrameSinkManagerParamsPtr params =
      mojom::FrameSinkManagerParams::New();
  params->restart_id = params_.restart_id;
  params->use_activation_deadline =
      params_.deadline_to_synchronize_surfaces.has_value();
  params->activation_deadline_in_frames =
      params_.deadline_to_synchronize_surfaces.value_or(0u);
  params->frame_sink_manager = std::move(request);
  params->frame_sink_manager_client = std::move(client);
  viz_main_ptr_->CreateFrameSinkManager(std::move(params));
}

void GpuHostImpl::EstablishGpuChannel(int client_id,
                                      uint64_t client_tracing_id,
                                      bool is_gpu_host,
                                      EstablishChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::EstablishGpuChannel");

  // If GPU features are already blacklisted, no need to establish the channel.
  if (!delegate_->GpuAccessAllowed()) {
    DVLOG(1) << "GPU blacklisted, refusing to open a GPU channel.";
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  if (gpu::IsReservedClientId(client_id)) {
    // The display-compositor/GrShaderCache in the gpu process uses these
    // special client ids.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  bool cache_shaders_on_disk =
      delegate_->GetShaderCacheFactory()->Get(client_id) != nullptr;

  channel_requests_.push(std::move(callback));
  gpu_service_ptr_->EstablishGpuChannel(
      client_id, client_tracing_id, is_gpu_host, cache_shaders_on_disk,
      base::BindOnce(&GpuHostImpl::OnChannelEstablished,
                     weak_ptr_factory_.GetWeakPtr(), client_id));

  if (!params_.disable_gpu_shader_disk_cache) {
    CreateChannelCache(client_id);

    bool oopd_enabled =
        base::FeatureList::IsEnabled(features::kVizDisplayCompositor);
    if (oopd_enabled)
      CreateChannelCache(gpu::kInProcessCommandBufferClientId);

    bool oopr_enabled =
        base::FeatureList::IsEnabled(features::kDefaultEnableOopRasterization);
    if (oopr_enabled)
      CreateChannelCache(gpu::kGrShaderCacheClientId);
  }
}

void GpuHostImpl::SendOutstandingReplies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& handler : connection_error_handlers_)
    std::move(handler).Run();
  connection_error_handlers_.clear();

  // Send empty channel handles for all EstablishChannel requests.
  while (!channel_requests_.empty()) {
    auto callback = std::move(channel_requests_.front());
    channel_requests_.pop();
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuHostInvalid);
  }
}

void GpuHostImpl::BindInterface(const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe) {
  delegate_->BindInterface(interface_name, std::move(interface_pipe));
}

mojom::GpuService* GpuHostImpl::gpu_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(gpu_service_ptr_.is_bound());
  return gpu_service_ptr_.get();
}

#if defined(USE_OZONE)
void GpuHostImpl::InitOzone() {
  // Ozone needs to send the primary DRM device to GPU service as early as
  // possible to ensure the latter always has a valid device.
  // https://crbug.com/608839
  // If the OzonePlatform is not created yet, defer the callback until
  // OzonePlatform instance is created.
  //
  // The Ozone/Wayland requires mojo communication to be established to be
  // functional with a separate gpu process. Thus, using the PlatformProperties,
  // check if there is such a requirement.
  if (features::IsOzoneDrmMojo() || ui::OzonePlatform::EnsureInstance()
                                        ->GetPlatformProperties()
                                        .requires_mojo) {
    // TODO(rjkroege): Remove the legacy IPC code paths when no longer
    // necessary. https://crbug.com/806092
    auto interface_binder = base::BindRepeating(&GpuHostImpl::BindInterface,
                                                weak_ptr_factory_.GetWeakPtr());
    auto terminate_callback = base::BindOnce(&GpuHostImpl::TerminateGpuProcess,
                                             weak_ptr_factory_.GetWeakPtr());

    auto startup_callback = base::BindOnce(
        [](const base::RepeatingCallback<void(const std::string&,
                                              mojo::ScopedMessagePipeHandle)>&
               interface_binder,
           base::OnceCallback<void(const std::string&)> terminate_callback,
           scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
           scoped_refptr<base::SingleThreadTaskRunner> host_thread_task_runner,
           ui::OzonePlatform* platform) {
          DCHECK(host_thread_task_runner->BelongsToCurrentThread());
          platform->GetGpuPlatformSupportHost()->OnGpuServiceLaunched(
              main_thread_task_runner, host_thread_task_runner,
              interface_binder, std::move(terminate_callback));
        },
        interface_binder, std::move(terminate_callback),
        params_.main_thread_task_runner, host_thread_task_runner_);
    OzoneRegisterStartupCallbackHelper(host_thread_task_runner_,
                                       std::move(startup_callback));
  } else {
    auto send_callback = base::BindRepeating(
        [](base::WeakPtr<GpuHostImpl> host, IPC::Message* message) {
          if (host)
            host->delegate_->SendGpuProcessMessage(message);
          else
            delete message;
        },
        weak_ptr_factory_.GetWeakPtr());
    // Create the callback that should run on the current thread.
    auto startup_callback = base::BindOnce(
        [](int host_id,
           const base::RepeatingCallback<void(IPC::Message*)>& send_callback,
           scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
           scoped_refptr<base::SingleThreadTaskRunner> host_thread_task_runner,
           ui::OzonePlatform* platform) {
          DCHECK(host_thread_task_runner->BelongsToCurrentThread());
          platform->GetGpuPlatformSupportHost()->OnGpuProcessLaunched(
              host_id, main_thread_task_runner, host_thread_task_runner,
              send_callback);
        },
        params_.restart_id, send_callback, params_.main_thread_task_runner,
        host_thread_task_runner_);
    OzoneRegisterStartupCallbackHelper(host_thread_task_runner_,
                                       std::move(startup_callback));
  }
}

void GpuHostImpl::TerminateGpuProcess(const std::string& message) {
  delegate_->TerminateGpuProcess(message);
}

#endif  // defined(USE_OZONE)

std::string GpuHostImpl::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    const gpu::GPUInfo& info = delegate_->GetGPUInfo();
    const gpu::GPUInfo::GPUDevice& active_gpu = info.active_gpu();

    shader_prefix_key_ = params_.product + "-" + info.gl_vendor + "-" +
                         info.gl_renderer + "-" + active_gpu.driver_version +
                         "-" + active_gpu.driver_vendor;

#if defined(OS_ANDROID)
    std::string build_fp =
        base::android::BuildInfo::GetInstance()->android_build_fp();
    shader_prefix_key_ += "-" + build_fp;
#endif
  }

  return shader_prefix_key_;
}

void GpuHostImpl::LoadedShader(int32_t client_id,
                               const std::string& key,
                               const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string prefix = GetShaderPrefixKey();
  bool prefix_ok = !key.compare(0, prefix.length(), prefix);
  UMA_HISTOGRAM_BOOLEAN("GPU.ShaderLoadPrefixOK", prefix_ok);
  if (prefix_ok) {
    // Remove the prefix from the key before load.
    std::string key_no_prefix = key.substr(prefix.length() + 1);
    gpu_service_ptr_->LoadedShader(client_id, key_no_prefix, data);
  }
}

void GpuHostImpl::CreateChannelCache(int32_t client_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::CreateChannelCache");

  scoped_refptr<gpu::ShaderDiskCache> cache =
      delegate_->GetShaderCacheFactory()->Get(client_id);
  if (!cache)
    return;

  cache->set_shader_loaded_callback(base::BindRepeating(
      &GpuHostImpl::LoadedShader, weak_ptr_factory_.GetWeakPtr(), client_id));

  client_id_to_shader_cache_[client_id] = cache;
}

void GpuHostImpl::OnChannelEstablished(
    int client_id,
    mojo::ScopedMessagePipeHandle channel_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::OnChannelEstablished");

  DCHECK(!channel_requests_.empty());
  auto callback = std::move(channel_requests_.front());
  channel_requests_.pop();

  // Currently if any of the GPU features are blacklisted, we don't establish a
  // GPU channel.
  if (channel_handle.is_valid() && !delegate_->GpuAccessAllowed()) {
    gpu_service_ptr_->CloseChannel(client_id);
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            EstablishChannelStatus::kGpuAccessDenied);
    RecordLogMessage(logging::LOG_WARNING, "WARNING",
                     "Hardware acceleration is unavailable.");
    return;
  }

  std::move(callback).Run(std::move(channel_handle), delegate_->GetGPUInfo(),
                          delegate_->GetGpuFeatureInfo(),
                          EstablishChannelStatus::kSuccess);
}

void GpuHostImpl::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const base::Optional<gpu::GpuFeatureInfo>&
        gpu_feature_info_for_hardware_gpu) {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", true);
  initialized_ = true;

  // Set GPU driver bug workaround flags that are checked on the browser side.
  wake_up_gpu_before_drawing_ =
      gpu_feature_info.IsWorkaroundEnabled(gpu::WAKE_UP_GPU_BEFORE_DRAWING);
  dont_disable_webgl_when_compositor_context_lost_ =
      gpu_feature_info.IsWorkaroundEnabled(
          gpu::DONT_DISABLE_WEBGL_WHEN_COMPOSITOR_CONTEXT_LOST);

  delegate_->DidInitialize(gpu_info, gpu_feature_info,
                           gpu_info_for_hardware_gpu,
                           gpu_feature_info_for_hardware_gpu);
}

void GpuHostImpl::DidFailInitialize() {
  UMA_HISTOGRAM_BOOLEAN("GPU.GPUProcessInitialized", false);
  delegate_->DidFailInitialize();
}

void GpuHostImpl::DidCreateContextSuccessfully() {
  delegate_->DidCreateContextSuccessfully();
}

void GpuHostImpl::DidCreateOffscreenContext(const GURL& url) {
  urls_with_live_offscreen_contexts_.insert(url);
}

void GpuHostImpl::DidDestroyOffscreenContext(const GURL& url) {
  // We only want to remove *one* of the entries in the multiset for this
  // particular URL, so can't use the erase method taking a key.
  auto candidate = urls_with_live_offscreen_contexts_.find(url);
  if (candidate != urls_with_live_offscreen_contexts_.end())
    urls_with_live_offscreen_contexts_.erase(candidate);
}

void GpuHostImpl::DidDestroyChannel(int32_t client_id) {
  TRACE_EVENT0("gpu", "GpuHostImpl::DidDestroyChannel");
  client_id_to_shader_cache_.erase(client_id);
}

void GpuHostImpl::DidLoseContext(bool offscreen,
                                 gpu::error::ContextLostReason reason,
                                 const GURL& active_url) {
  // TODO(kbr): would be nice to see the "offscreen" flag too.
  TRACE_EVENT2("gpu", "GpuHostImpl::DidLoseContext", "reason", reason, "url",
               active_url.possibly_invalid_spec());

  if (!offscreen || active_url.is_empty()) {
    // Assume that the loss of the compositor's or accelerated canvas'
    // context is a serious event and blame the loss on all live
    // offscreen contexts. This more robustly handles situations where
    // the GPU process may not actually detect the context loss in the
    // offscreen context. However, situations have been seen where the
    // compositor's context can be lost due to driver bugs (as of this
    // writing, on Android), so allow that possibility.
    if (!dont_disable_webgl_when_compositor_context_lost_)
      BlockLiveOffscreenContexts();
    return;
  }

  gpu::DomainGuilt guilt = gpu::DomainGuilt::kUnknown;
  switch (reason) {
    case gpu::error::kGuilty:
      guilt = gpu::DomainGuilt::kKnown;
      break;
    // Treat most other error codes as though they had unknown provenance.
    // In practice this doesn't affect the user experience. A lost context
    // of either known or unknown guilt still causes user-level 3D APIs
    // (e.g. WebGL) to be blocked on that domain until the user manually
    // reenables them.
    case gpu::error::kUnknown:
    case gpu::error::kOutOfMemory:
    case gpu::error::kMakeCurrentFailed:
    case gpu::error::kGpuChannelLost:
    case gpu::error::kInvalidGpuMessage:
      break;
    case gpu::error::kInnocent:
      return;
  }

  delegate_->BlockDomainFrom3DAPIs(active_url, guilt);
}

void GpuHostImpl::DisableGpuCompositing() {
  delegate_->DisableGpuCompositing();
}

#if defined(OS_WIN)
void GpuHostImpl::SetChildSurface(gpu::SurfaceHandle parent,
                                  gpu::SurfaceHandle child) {
  if (pid_ != base::kNullProcessId) {
    gfx::RenderingWindowManager::GetInstance()->RegisterChild(
        parent, child, /*expected_child_process_id=*/pid_);
  }
}
#endif  // defined(OS_WIN)

void GpuHostImpl::StoreShaderToDisk(int32_t client_id,
                                    const std::string& key,
                                    const std::string& shader) {
  TRACE_EVENT0("gpu", "GpuHostImpl::StoreShaderToDisk");
  auto iter = client_id_to_shader_cache_.find(client_id);
  // If the cache doesn't exist then this is an off the record profile.
  if (iter == client_id_to_shader_cache_.end())
    return;
  std::string prefix = GetShaderPrefixKey();
  iter->second->Cache(prefix + ":" + key, shader);
}

void GpuHostImpl::RecordLogMessage(int32_t severity,
                                   const std::string& header,
                                   const std::string& message) {
  delegate_->RecordLogMessage(severity, header, message);
}

}  // namespace viz
