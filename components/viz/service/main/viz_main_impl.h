// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "ui/gfx/font_render_params.h"

#if defined(OS_WIN)
#include "components/viz/service/gl/info_collection_gpu_service_impl.h"
#include "services/viz/privileged/mojom/gl/info_collection_gpu_service.mojom.h"
#endif

namespace base {
class PowerMonitorSource;
}

namespace gpu {
class GpuInit;
class SyncPointManager;
class SharedImageManager;
}  // namespace gpu

namespace ukm {
class MojoUkmRecorder;
}

namespace viz {
#if defined(OS_WIN)
class InfoCollectionGpuServiceImpl;
#endif

class VizMainImpl : public mojom::VizMain,
                    public gpu::GpuInProcessThreadServiceDelegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnInitializationFailed() = 0;
    virtual void OnGpuServiceConnection(GpuServiceImpl* gpu_service) = 0;
    virtual void PostCompositorThreadCreated(
        base::SingleThreadTaskRunner* task_runner) = 0;
    virtual void QuitMainMessageLoop() = 0;
  };

  struct ExternalDependencies {
   public:
    ExternalDependencies();
    ExternalDependencies(ExternalDependencies&& other);
    ~ExternalDependencies();

    ExternalDependencies& operator=(ExternalDependencies&& other);

    // Note that, due to the design of |base::PowerMonitor|, it is inherently
    // racy to decide to initialize or not based on a call to
    // |base::PowerMonitor::IsInitialized|. This makes it difficult for
    // VizMainImpl to know whether to call initialize or not as the correct
    // choice depends on the context in which VizMainImpl will be used.
    //
    // To work around this, calling code must understand whether VizMainImpl
    // should initialize |base::PowerMonitor| or not and can then use the
    // |power_monitor_source| to signal its intent.
    //
    // If |power_monitor_source| is null, |PowerMonitor::Initialize| will not
    // be called. If |power_monitor_source| is non-null, it will be std::move'd
    // in to a call of |PowerMonitor::Initialize|.
    //
    // We use a |PowerMonitorSource| here instead of a boolean flag so that
    // tests can use mocks and fakes for testing.
    mutable std::unique_ptr<base::PowerMonitorSource> power_monitor_source;
    gpu::SyncPointManager* sync_point_manager = nullptr;
    gpu::SharedImageManager* shared_image_manager = nullptr;
    base::WaitableEvent* shutdown_event = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner;
    std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder;
    VizCompositorThreadRunner* viz_compositor_thread_runner = nullptr;

   private:
    DISALLOW_COPY_AND_ASSIGN(ExternalDependencies);
  };

  VizMainImpl(Delegate* delegate,
              ExternalDependencies dependencies,
              std::unique_ptr<gpu::GpuInit> gpu_init);
  // Destruction must happen on the GPU thread.
  ~VizMainImpl() override;

  void BindAssociated(
      mojo::PendingAssociatedReceiver<mojom::VizMain> pending_receiver);

  // mojom::VizMain implementation:
  void CreateGpuService(
      mojo::PendingReceiver<mojom::GpuService> pending_receiver,
      mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
      mojo::PendingRemote<
          discardable_memory::mojom::DiscardableSharedMemoryManager>
          discardable_memory_manager,
      mojo::ScopedSharedBufferHandle activity_flags,
      gfx::FontRenderParams::SubpixelRendering subpixel_rendering) override;
#if defined(OS_WIN)
  void CreateInfoCollectionGpuService(
      mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver)
      override;
#endif
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params) override;
  void CreateVizDevTools(mojom::VizDevToolsParamsPtr params) override;

  // gpu::GpuInProcessThreadServiceDelegate implementation:
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  scoped_refptr<gl::GLShareGroup> GetShareGroup() override;

  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }
  const GpuServiceImpl* gpu_service() const { return gpu_service_.get(); }

  // Note that this may be null if viz is running in the browser process and
  // using the ServiceDiscardableSharedMemoryManager.
  discardable_memory::ClientDiscardableSharedMemoryManager*
  discardable_shared_memory_manager() {
    return discardable_shared_memory_manager_.get();
  }

  // Cleanly exits the process. If |immediate_exit_code| is base::nullopt, the
  // process exits by shutting down the GPU main thread. Otherwise, the process
  // is terminated immediately with the specified exit code.
  void ExitProcess(base::Optional<ExitCode> immediate_exit_code);

 private:
  void CreateFrameSinkManagerInternal(mojom::FrameSinkManagerParamsPtr params);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_thread_ ? io_thread_->task_runner()
                      : dependencies_.io_thread_task_runner;
  }

  Delegate* const delegate_;

  const ExternalDependencies dependencies_;

  // The thread that handles IO events for Gpu (if one isn't already provided).
  // |io_thread_| must be ordered above GPU service related variables so it's
  // destroyed after they are.
  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;
#if defined(OS_WIN)
  std::unique_ptr<InfoCollectionGpuServiceImpl> info_collection_gpu_service_;
#endif

  // Allows the display compositor to use InProcessCommandBuffer to send GPU
  // commands to the GPU thread from the compositor thread. This must outlive
  // |viz_compositor_thread_runner_|.
  std::unique_ptr<gpu::CommandBufferTaskExecutor> task_executor_;

  // If the gpu service is not yet ready then we stash pending
  // FrameSinkManagerParams.
  mojom::FrameSinkManagerParamsPtr pending_frame_sink_manager_params_;

  // Runs the VizCompositorThread for the display compositor.
  std::unique_ptr<VizCompositorThreadRunnerImpl>
      viz_compositor_thread_runner_impl_;
  // Note under Android WebView where VizCompositorThreadRunner is not created
  // and owned by this, Viz does not interact with other objects in this class,
  // such as GpuServiceImpl or CommandBufferTaskExecutor. Code should take care
  // to avoid introducing such assumptions.
  VizCompositorThreadRunner* viz_compositor_thread_runner_ = nullptr;

  const scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_task_runner_;

  mojo::AssociatedReceiver<mojom::VizMain> receiver_{this};

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  DISALLOW_COPY_AND_ASSIGN(VizMainImpl);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_
