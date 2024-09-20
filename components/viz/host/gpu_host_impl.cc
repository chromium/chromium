// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/gpu_host_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/common/features.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/host/gpu_disk_cache.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "ui/gfx/font_render_params.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/rendering_window_manager.h"
#elif BUILDFLAG(IS_MAC)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {
namespace {

// A wrapper around gfx::FontRenderParams that checks it is set and accessed on
// the same thread.
class FontRenderParams {
 public:
  FontRenderParams(const FontRenderParams&) = delete;
  FontRenderParams& operator=(const FontRenderParams&) = delete;

  void Set(const gfx::FontRenderParams& params) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    params_ = params;
    if (gpu_host_impl_) {
      gpu_host_impl_->MaybeSendFontRenderParams();
    }
  }

  const std::optional<gfx::FontRenderParams>& Get() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return params_;
  }

  void SetGpuHostImpl(GpuHostImpl* gpu_host_impl) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    gpu_host_impl_ = gpu_host_impl;
  }

 private:
  friend class base::NoDestructor<FontRenderParams>;

  FontRenderParams() = default;

  ~FontRenderParams() { NOTREACHED_IN_MIGRATION(); }

  THREAD_CHECKER(thread_checker_);
  std::optional<gfx::FontRenderParams> params_;
  raw_ptr<GpuHostImpl> gpu_host_impl_ = nullptr;
};

FontRenderParams& GetFontRenderParams() {
  static base::NoDestructor<FontRenderParams> instance;
  return *instance;
}

}  // namespace

GpuHostImpl::InitParams::InitParams() = default;

GpuHostImpl::InitParams::InitParams(InitParams&&) = default;

GpuHostImpl::InitParams::~InitParams() = default;

GpuHostImpl::GpuHostImpl(Delegate* delegate,
                         mojo::PendingRemote<mojom::VizMain> viz_main,
                         InitParams params)
    : delegate_(delegate),
      viz_main_(std::move(viz_main)),
      params_(std::move(params)) {
  // Create a special GPU info collection service if the GPU process is used for
  // info collection only.
#if BUILDFLAG(IS_WIN)
  if (params_.info_collection_gpu_process) {
    viz_main_->CreateInfoCollectionGpuService(
        info_collection_gpu_service_remote_.BindNewPipeAndPassReceiver());
    return;
  }
#endif

  DCHECK(delegate_);

  mojo::PendingRemote<discardable_memory::mojom::DiscardableSharedMemoryManager>
      discardable_manager_remote;
  delegate_->BindDiscardableMemoryReceiver(
      discardable_manager_remote.InitWithNewPipeAndPassReceiver());

  scoped_refptr<base::SequencedTaskRunner> task_runner = nullptr;
#if BUILDFLAG(IS_MAC)
  if (params_.main_thread_task_runner->BelongsToCurrentThread())
    task_runner = ui::WindowResizeHelperMac::Get()->task_runner();
#endif

#if BUILDFLAG(IS_ANDROID)
  viz_main_->SetHostProcessId(base::GetCurrentProcId());
#endif

  viz_main_->CreateGpuService(
      gpu_service_remote_.BindNewPipeAndPassReceiver(task_runner),
      gpu_host_receiver_.BindNewPipeAndPassRemote(task_runner),
      std::move(discardable_manager_remote),
      use_shader_cache_shm_count_.CloneRegion());
  MaybeSendFontRenderParams();

#if BUILDFLAG(IS_OZONE)
  InitOzone();
#endif  // BUILDFLAG(IS_OZONE)
}

GpuHostImpl::~GpuHostImpl() {
  GetFontRenderParams().SetGpuHostImpl(nullptr);
  SendOutstandingReplies();
}

// static
void GpuHostImpl::InitFontRenderParams(const gfx::FontRenderParams& params) {
  DCHECK(!GetFontRenderParams().Get());
  GetFontRenderParams().Set(params);
}

void GpuHostImpl::SetProcessId(base::ProcessId pid) {
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
  if (use_shader_cache_shm_count_.GetCount() > 0) {
    auto* gpu_disk_cache_factory = delegate_->GetGpuDiskCacheFactory();
    for (auto& [_, cache] : client_id_to_caches_) {
      // This call will temporarily extend the lifetime of the cache (kept
      // alive in the factory), and may drop loads of cached shader binaries if
      // it takes a while to complete. As we are intentionally dropping all
      // binaries, this behavior is fine.
      gpu_disk_cache_factory->ClearByCache(
          cache, base::Time(), base::Time::Max(), base::DoNothing());
    }
  }
}

void GpuHostImpl::AddConnectionErrorHandler(base::OnceClosure handler) {
  connection_error_handlers_.push_back(std::move(handler));
}

void GpuHostImpl::BlockLiveOffscreenContexts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<GURL> urls(urls_with_live_offscreen_contexts_.begin(),
                      urls_with_live_offscreen_contexts_.end());
  delegate_->BlockDomainsFrom3DAPIs(urls, gpu::DomainGuilt::kUnknown);
}

void GpuHostImpl::ConnectFrameSinkManager(
    mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
    mojo::PendingRemote<mojom::FrameSinkManagerClient> client,
    const DebugRendererSettings& debug_renderer_settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::ConnectFrameSinkManager");

  mojom::FrameSinkManagerParamsPtr params =
      mojom::FrameSinkManagerParams::New();
  params->restart_id = params_.restart_id;
  params->use_activation_deadline =
      params_.deadline_to_synchronize_surfaces.has_value();
  params->activation_deadline_in_frames =
      params_.deadline_to_synchronize_surfaces.value_or(0u);
  params->frame_sink_manager = std::move(receiver);
  params->frame_sink_manager_client = std::move(client);
  params->debug_renderer_settings = debug_renderer_settings;
  viz_main_->CreateFrameSinkManager(std::move(params));
}

void GpuHostImpl::EstablishGpuChannel(int client_id,
                                      uint64_t client_tracing_id,
                                      bool is_gpu_host,
                                      bool sync,
                                      EstablishChannelCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::EstablishGpuChannel");

  shutdown_timeout_.Stop();

  if (gpu::IsReservedClientId(client_id)) {
    // The display-compositor/GrShaderCache in the gpu process uses these
    // special client ids.
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            gpu::SharedImageCapabilities(),
                            EstablishChannelStatus::kGpuAccessDenied);
    return;
  }

  channel_requests_[client_id] = std::move(callback);
  if (sync) {
    mojo::ScopedMessagePipeHandle channel_handle;
    gpu::GPUInfo gpu_info;
    gpu::GpuFeatureInfo gpu_feature_info;
    gpu::SharedImageCapabilities shared_image_capabilities;
    {
      mojo::SyncCallRestrictions::ScopedAllowSyncCall scoped_allow;
      gpu_service_remote_->EstablishGpuChannel(
          client_id, client_tracing_id, is_gpu_host, &channel_handle, &gpu_info,
          &gpu_feature_info, &shared_image_capabilities);
    }
    OnChannelEstablished(client_id, true, std::move(channel_handle), gpu_info,
                         gpu_feature_info, shared_image_capabilities);
  } else {
    gpu_service_remote_->EstablishGpuChannel(
        client_id, client_tracing_id, is_gpu_host,
        base::BindOnce(&GpuHostImpl::OnChannelEstablished,
                       weak_ptr_factory_.GetWeakPtr(), client_id, false));
  }

  // The gpu host channel uses the same cache as the compositor client.
  if (is_gpu_host) {
    SetChannelDiskCacheHandle(client_id,
                              gpu::kDisplayCompositorGpuDiskCacheHandle);
  }
}

void GpuHostImpl::SetChannelClientPid(int client_id,
                                      base::ProcessId client_pid) {
  gpu_service_remote_->SetChannelClientPid(client_id, client_pid);
}

void GpuHostImpl::SetChannelDiskCacheHandle(
    int client_id,
    const gpu::GpuDiskCacheHandle& handle) {
  if (params_.disable_gpu_shader_disk_cache) {
    return;
  }

  scoped_refptr<gpu::GpuDiskCache> cache =
      delegate_->GetGpuDiskCacheFactory()->Get(handle);
  if (!cache) {
    // Create the cache if necessary and save a reference.
    cache = delegate_->GetGpuDiskCacheFactory()->Create(
        handle,
        base::BindRepeating(&GpuHostImpl::LoadedBlob,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&GpuHostImpl::OnDiskCacheHandleDestoyed,
                       weak_ptr_factory_.GetWeakPtr()));
    if (!cache) {
      return;
    }
  }

  client_id_to_caches_.emplace(client_id, cache);
  gpu_service_remote_->SetChannelDiskCacheHandle(client_id, handle);
}

void GpuHostImpl::RemoveChannelDiskCacheHandles(int client_id) {
  // Release the handle, then release the cache.
  auto [start, end] = client_id_to_caches_.equal_range(client_id);
  for (auto it = start; it != end; ++it) {
    delegate_->GetGpuDiskCacheFactory()->ReleaseCacheHandle(it->second.get());
  }
  client_id_to_caches_.erase(client_id);
}

void GpuHostImpl::CloseChannel(int client_id) {
  gpu_service_remote_->CloseChannel(client_id);

  channel_requests_.erase(client_id);
}

#if BUILDFLAG(USE_VIZ_DEBUGGER)
void GpuHostImpl::FilterVisualDebugStream(base::Value::Dict json) {
  viz_main_->FilterDebugStream(std::move(json));
}

void GpuHostImpl::StartVisualDebugStream(
    base::RepeatingCallback<void(base::Value)> callback) {
  viz_debug_output_callback_ = std::move(callback);
  viz_main_->StartDebugStream(viz_debug_output_.BindNewPipeAndPassRemote());
}

void GpuHostImpl::StopVisualDebugStream() {
  viz_main_->StopDebugStream();
  viz_debug_output_.reset();
}
#endif

void GpuHostImpl::SendOutstandingReplies() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& handler : connection_error_handlers_)
    std::move(handler).Run();
  connection_error_handlers_.clear();

  // Send empty channel handles for all EstablishChannel requests.
  for (auto& entry : channel_requests_) {
    std::move(entry.second)
        .Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
             gpu::GpuFeatureInfo(), gpu::SharedImageCapabilities(),
             EstablishChannelStatus::kGpuHostInvalid);
  }
  channel_requests_.clear();
}

void GpuHostImpl::BindInterface(const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe) {
  delegate_->BindInterface(interface_name, std::move(interface_pipe));
}

mojom::GpuService* GpuHostImpl::gpu_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(gpu_service_remote_.is_bound());
  return gpu_service_remote_.get();
}

#if BUILDFLAG(IS_WIN)
mojom::InfoCollectionGpuService* GpuHostImpl::info_collection_gpu_service() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(info_collection_gpu_service_remote_.is_bound());
  return info_collection_gpu_service_remote_.get();
}
#endif

#if BUILDFLAG(IS_OZONE)

void GpuHostImpl::InitOzone() {
  // Ozone needs to send the primary DRM device to GPU service as early as
  // possible to ensure the latter always has a valid device.
  // https://crbug.com/608839
  //
  // The Ozone/Wayland requires mojo communication to be established to be
  // functional with a separate gpu process. Thus, using the PlatformProperties,
  // check if there is such a requirement.
  auto interface_binder = base::BindRepeating(&GpuHostImpl::BindInterface,
                                              weak_ptr_factory_.GetWeakPtr());
  auto terminate_callback = base::BindOnce(&GpuHostImpl::TerminateGpuProcess,
                                           weak_ptr_factory_.GetWeakPtr());

  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnGpuServiceLaunched(params_.restart_id, interface_binder,
                             std::move(terminate_callback));
}

void GpuHostImpl::TerminateGpuProcess(const std::string& message) {
  delegate_->TerminateGpuProcess(message);
}

#endif  // BUILDFLAG(IS_OZONE)

std::string GpuHostImpl::GetShaderPrefixKey() {
  if (shader_prefix_key_.empty()) {
    const gpu::GPUInfo& info = delegate_->GetGPUInfo();
    const gpu::GPUInfo::GPUDevice& active_gpu = info.active_gpu();

    shader_prefix_key_ = params_.product + "-" + info.gl_vendor + "-" +
                         info.gl_renderer + "-" + active_gpu.driver_version +
                         "-" + active_gpu.driver_vendor + "-" +
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

void GpuHostImpl::LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                             const std::string& key,
                             const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TRACE_EVENT1("gpu", "GpuHostImpl::LoadedBlob", "handle_type",
               GetHandleType(handle));
  gpu_service_remote_->LoadedBlob(handle, key, data);
}

void GpuHostImpl::OnDiskCacheHandleDestoyed(
    const gpu::GpuDiskCacheHandle& handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  gpu_service_remote_->OnDiskCacheHandleDestoyed(handle);
}

void GpuHostImpl::OnChannelEstablished(
    int client_id,
    bool sync,
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const gpu::SharedImageCapabilities& shared_image_capabilities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("gpu", "GpuHostImpl::OnChannelEstablished");

  auto it = channel_requests_.find(client_id);
  if (it == channel_requests_.end())
    return;

  auto callback = std::move(it->second);
  channel_requests_.erase(it);

  // If the GPU process sent an empty handle back, it could be a transient error
  // in which case the client should try again so return kGpuHostInvalid.
  if (!channel_handle.is_valid()) {
    std::move(callback).Run(mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
                            gpu::GpuFeatureInfo(),
                            gpu::SharedImageCapabilities(),
                            EstablishChannelStatus::kGpuHostInvalid);
    return;
  }

  // TODO(jam): always use GPUInfo & GpuFeatureInfo from the service once we
  // know there's no issue with the ProcessHostOnUI which is the only mode
  // that currently uses it. This is because in that mode the sync mojo call
  // in the caller means we won't get the async DidInitialize() call before
  // this point, so the delegate_ methods won't have the GPU info structs yet.
  if (sync) {
    std::move(callback).Run(std::move(channel_handle), gpu_info,
                            gpu_feature_info, shared_image_capabilities,
                            EstablishChannelStatus::kSuccess);
  } else {
    std::move(callback).Run(std::move(channel_handle), delegate_->GetGPUInfo(),
                            delegate_->GetGpuFeatureInfo(),
                            shared_image_capabilities,
                            EstablishChannelStatus::kSuccess);
  }
}

void GpuHostImpl::DidInitialize(
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
    const std::optional<gpu::GpuFeatureInfo>& gpu_feature_info_for_hardware_gpu,
    const gfx::GpuExtraInfo& gpu_extra_info) {
  delegate_->DidInitialize(gpu_info, gpu_feature_info,
                           gpu_info_for_hardware_gpu,
                           gpu_feature_info_for_hardware_gpu, gpu_extra_info);

  if (!params_.disable_gpu_shader_disk_cache) {
    SetChannelDiskCacheHandle(gpu::kDisplayCompositorClientId,
                              gpu::kDisplayCompositorGpuDiskCacheHandle);
    SetChannelDiskCacheHandle(gpu::kGrShaderCacheClientId,
                              gpu::kGrShaderGpuDiskCacheHandle);
    SetChannelDiskCacheHandle(gpu::kGraphiteDawnClientId,
                              gpu::kGraphiteDawnGpuDiskCacheHandle);
  }
}

void GpuHostImpl::DidFailInitialize() {
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
  client_id_to_caches_.erase(client_id);
}

void GpuHostImpl::DidDestroyAllChannels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!channel_requests_.empty())
    return;
  constexpr base::TimeDelta kShutDownTimeout = base::Seconds(10);
  shutdown_timeout_.Start(FROM_HERE, kShutDownTimeout,
                          base::BindOnce(&GpuHostImpl::MaybeShutdownGpuProcess,
                                         base::Unretained(this)));
}

void GpuHostImpl::MaybeShutdownGpuProcess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(channel_requests_.empty());
  delegate_->MaybeShutdownGpuProcess();
}

void GpuHostImpl::DidLoseContext(gpu::error::ContextLostReason reason,
                                 const GURL& active_url) {
  TRACE_EVENT2("gpu", "GpuHostImpl::DidLoseContext", "reason", reason, "url",
               active_url.possibly_invalid_spec());

  if (active_url.is_empty()) {
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

  std::set<GURL> urls{active_url};
  delegate_->BlockDomainsFrom3DAPIs(urls, guilt);
}

void GpuHostImpl::DisableGpuCompositing() {
  delegate_->DisableGpuCompositing();
}

void GpuHostImpl::GetIsolationKey(
    int32_t client_id,
    const blink::WebGPUExecutionContextToken& token,
    GetIsolationKeyCallback cb) {
  std::string isolation_key = delegate_->GetIsolationKey(client_id, token);
  std::move(cb).Run(isolation_key);
}

void GpuHostImpl::DidUpdateGPUInfo(const gpu::GPUInfo& gpu_info) {
  delegate_->DidUpdateGPUInfo(gpu_info);
}

#if BUILDFLAG(IS_WIN)
void GpuHostImpl::DidUpdateOverlayInfo(const gpu::OverlayInfo& overlay_info) {
  delegate_->DidUpdateOverlayInfo(overlay_info);
}

void GpuHostImpl::DidUpdateDXGIInfo(gfx::mojom::DXGIInfoPtr dxgi_info) {
  delegate_->DidUpdateDXGIInfo(std::move(dxgi_info));
}

void GpuHostImpl::AddChildWindow(gpu::SurfaceHandle parent_window,
                                 gpu::SurfaceHandle child_window) {
  if (pid_ != base::kNullProcessId) {
    gfx::RenderingWindowManager::GetInstance()->RegisterChild(
        parent_window, child_window, /*expected_child_process_id=*/pid_);
  }
}
#endif  // BUILDFLAG(IS_WIN)

void GpuHostImpl::MaybeSendFontRenderParams() {
  if (const auto& params = GetFontRenderParams().Get()) {
    viz_main_->SetRenderParams(params->subpixel_rendering,
                               params->text_contrast, params->text_gamma);
  } else {
    GetFontRenderParams().SetGpuHostImpl(this);
  }
}

void GpuHostImpl::StoreBlobToDisk(const gpu::GpuDiskCacheHandle& handle,
                                  const std::string& key,
                                  const std::string& blob) {
  scoped_refptr<gpu::GpuDiskCache> cache =
      delegate_->GetGpuDiskCacheFactory()->Get(handle);
  if (!cache) {
    return;
  }

  TRACE_EVENT1("gpu", "GpuHostImpl::StoreBlobToDisk", "handle_type",
               GetHandleType(handle));
  cache->Cache(key, blob);
}

void GpuHostImpl::RecordLogMessage(int32_t severity,
                                   const std::string& header,
                                   const std::string& message) {
  delegate_->RecordLogMessage(severity, header, message);
}

void GpuHostImpl::ClearGrShaderDiskCache() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* gpu_disk_cache_factory = delegate_->GetGpuDiskCacheFactory();
  for (auto& [client_id, cache] : client_id_to_caches_) {
    // This call will temporarily extend the lifetime of the cache (kept
    // alive in the factory), and may drop loads of cached shader binaries if
    // it takes a while to complete. As we are intentionally dropping all
    // binaries, this behavior is fine.
    if (client_id == gpu::kGrShaderCacheClientId) {
      gpu_disk_cache_factory->ClearByCache(
          cache, base::Time(), base::Time::Max(), base::DoNothing());
    }
  }
}

#if BUILDFLAG(USE_VIZ_DEBUGGER)
void GpuHostImpl::LogFrame(base::Value frame_data) {
  if (!viz_debug_output_callback_.is_null())
    viz_debug_output_callback_.Run(std::move(frame_data));
}
#endif

}  // namespace viz
