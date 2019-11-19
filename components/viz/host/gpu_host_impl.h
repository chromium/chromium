// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_GPU_HOST_IMPL_H_
#define COMPONENTS_VIZ_HOST_GPU_HOST_IMPL_H_

#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/discardable_memory/public/mojom/discardable_shared_memory_manager.mojom.h"
#include "components/ui_devtools/buildflags.h"
#include "components/viz/host/viz_host_export.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/config/gpu_domain_guilt.h"
#include "gpu/config/gpu_extra_info.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/mojom/service.mojom.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_host.mojom.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "url/gurl.h"

namespace gfx {
struct FontRenderParams;
}

namespace gpu {
class ShaderCacheFactory;
class ShaderDiskCache;
}  // namespace gpu

namespace viz {

class VIZ_HOST_EXPORT GpuHostImpl : public mojom::GpuHost {
 public:
  class VIZ_HOST_EXPORT Delegate {
   public:
    virtual gpu::GPUInfo GetGPUInfo() const = 0;
    virtual gpu::GpuFeatureInfo GetGpuFeatureInfo() const = 0;
    virtual void DidInitialize(
        const gpu::GPUInfo& gpu_info,
        const gpu::GpuFeatureInfo& gpu_feature_info,
        const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
        const base::Optional<gpu::GpuFeatureInfo>&
            gpu_feature_info_for_hardware_gpu,
        const gpu::GpuExtraInfo& gpu_extra_info) = 0;
    virtual void DidFailInitialize() = 0;
    virtual void DidCreateContextSuccessfully() = 0;
    virtual void BlockDomainFrom3DAPIs(const GURL& url,
                                       gpu::DomainGuilt guilt) = 0;
    virtual void DisableGpuCompositing() = 0;
    virtual bool GpuAccessAllowed() const = 0;
    virtual gpu::ShaderCacheFactory* GetShaderCacheFactory() = 0;
    virtual void RecordLogMessage(int32_t severity,
                                  const std::string& header,
                                  const std::string& message) = 0;
    virtual void BindDiscardableMemoryReceiver(
        mojo::PendingReceiver<
            discardable_memory::mojom::DiscardableSharedMemoryManager>
            receiver) = 0;
    virtual void BindInterface(
        const std::string& interface_name,
        mojo::ScopedMessagePipeHandle interface_pipe) = 0;
    virtual void RunService(
        const std::string& service_name,
        mojo::PendingReceiver<service_manager::mojom::Service> receiver) = 0;
#if defined(USE_OZONE)
    virtual void TerminateGpuProcess(const std::string& message) = 0;

    // TODO(https://crbug.com/806092): Remove this when legacy IPC-based Ozone
    // is removed.
    virtual void SendGpuProcessMessage(IPC::Message* message) = 0;
#endif

   protected:
    virtual ~Delegate() {}
  };

  struct VIZ_HOST_EXPORT InitParams {
    InitParams();
    InitParams(InitParams&&);
    ~InitParams();

    // An ID that changes for each GPU restart.
    int restart_id = -1;

    // Whether caching GPU shader on disk is disabled or not.
    bool disable_gpu_shader_disk_cache = false;

    // A string representing the product name and version; used to build a
    // prefix for shader keys.
    std::string product;

    // Number of frames to CompositorFrame activation deadline.
    base::Optional<uint32_t> deadline_to_synchronize_surfaces;

    // Task runner corresponding to the main thread.
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner;
  };

  enum class EstablishChannelStatus {
    kGpuAccessDenied,  // GPU access was not allowed.
    kGpuHostInvalid,   // Request failed because the GPU host became invalid
                       // while processing the request (e.g. the GPU process
                       // may have been killed). The caller should normally
                       // make another request to establish a new channel.
    kSuccess,
  };
  using EstablishChannelCallback =
      base::OnceCallback<void(mojo::ScopedMessagePipeHandle,
                              const gpu::GPUInfo&,
                              const gpu::GpuFeatureInfo&,
                              EstablishChannelStatus)>;

  GpuHostImpl(Delegate* delegate,
              mojo::PendingAssociatedRemote<mojom::VizMain> viz_main,
              InitParams params);
  ~GpuHostImpl() override;

  static void InitFontRenderParams(const gfx::FontRenderParams& params);
  static void ResetFontRenderParams();

  void SetProcessId(base::ProcessId pid);
  void OnProcessCrashed();

  // Adds a connection error handler for the GpuService.
  void AddConnectionErrorHandler(base::OnceClosure handler);

  void BlockLiveOffscreenContexts();

  // Connects to FrameSinkManager running in the Viz service.
  void ConnectFrameSinkManager(
      mojo::PendingReceiver<mojom::FrameSinkManager> receiver,
      mojo::PendingRemote<mojom::FrameSinkManagerClient> client);

#if BUILDFLAG(USE_VIZ_DEVTOOLS)
  // Connects to Viz DevTools running in the Viz service.
  void ConnectVizDevTools(mojom::VizDevToolsParamsPtr params);
#endif

  // Tells the GPU service to create a new channel for communication with a
  // client. Once the GPU service responds asynchronously with the channel
  // handle and GPUInfo, we call the callback.
  void EstablishGpuChannel(int client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           EstablishChannelCallback callback);

  void SendOutstandingReplies();

  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe);
  void RunService(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);

  mojom::GpuService* gpu_service();

  bool wake_up_gpu_before_drawing() const {
    return wake_up_gpu_before_drawing_;
  }

 private:
  friend class GpuHostImplTestApi;

#if defined(USE_OZONE)
  void InitOzone();
  void TerminateGpuProcess(const std::string& message);
#endif  // defined(USE_OZONE)

  std::string GetShaderPrefixKey();

  void LoadedShader(int32_t client_id,
                    const std::string& key,
                    const std::string& data);

  void CreateChannelCache(int32_t client_id);

  void OnChannelEstablished(int client_id,
                            mojo::ScopedMessagePipeHandle channel_handle);

  // mojom::GpuHost:
  void DidInitialize(
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const base::Optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const base::Optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gpu::GpuExtraInfo& gpu_extra_info) override;
  void DidFailInitialize() override;
  void DidCreateContextSuccessfully() override;
  void DidCreateOffscreenContext(const GURL& url) override;
  void DidDestroyOffscreenContext(const GURL& url) override;
  void DidDestroyChannel(int32_t client_id) override;
  void DidLoseContext(bool offscreen,
                      gpu::error::ContextLostReason reason,
                      const GURL& active_url) override;
  void DisableGpuCompositing() override;
#if defined(OS_WIN)
  void SetChildSurface(gpu::SurfaceHandle parent,
                       gpu::SurfaceHandle child) override;
#endif
  void StoreShaderToDisk(int32_t client_id,
                         const std::string& key,
                         const std::string& shader) override;
  void RecordLogMessage(int32_t severity,
                        const std::string& header,
                        const std::string& message) override;

  Delegate* const delegate_;
  mojo::AssociatedRemote<mojom::VizMain> viz_main_;
  const InitParams params_;

  // Task runner corresponding to the thread |this| is created on.
  scoped_refptr<base::SingleThreadTaskRunner> host_thread_task_runner_;

  mojo::Remote<mojom::GpuService> gpu_service_remote_;
  mojo::Receiver<mojom::GpuHost> gpu_host_receiver_{this};
  gpu::GpuProcessHostActivityFlags activity_flags_;

  base::ProcessId pid_ = base::kNullProcessId;

  // List of connection error handlers for the GpuService.
  std::vector<base::OnceClosure> connection_error_handlers_;

  // The following are a list of driver bug workarounds that will only be
  // set to true in DidInitialize(), where GPU service has started and GPU
  // driver bug workarounds have been computed and sent back.
  bool wake_up_gpu_before_drawing_ = false;
  bool dont_disable_webgl_when_compositor_context_lost_ = false;

  // Track the URLs of the pages which have live offscreen contexts, assumed to
  // be associated with untrusted content such as WebGL. For best robustness,
  // when any context lost notification is received, assume all of these URLs
  // are guilty, and block automatic execution of 3D content from those domains.
  std::multiset<GURL> urls_with_live_offscreen_contexts_;

  std::map<int32_t, scoped_refptr<gpu::ShaderDiskCache>>
      client_id_to_shader_cache_;
  std::string shader_prefix_key_;

  // These are the channel requests that we have already sent to the GPU
  // service, but haven't heard back about yet.
  base::queue<EstablishChannelCallback> channel_requests_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GpuHostImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GpuHostImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_GPU_HOST_IMPL_H_
