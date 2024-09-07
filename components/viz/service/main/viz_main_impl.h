// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/viz/common/buildflags.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "components/viz/service/main/viz_compositor_thread_runner_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/privileged/mojom/viz_main.mojom.h"
#include "ui/gfx/font_render_params.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/gl/info_collection_gpu_service_impl.h"
#include "services/viz/privileged/mojom/gl/info_collection_gpu_service.mojom.h"
#endif

namespace base {
class PowerMonitorSource;
class WaitableEvent;
}

namespace gpu {
class GpuInit;
class Scheduler;
class SharedImageManager;
class SyncPointManager;
}  // namespace gpu

namespace ukm {
class MojoUkmRecorder;
}

namespace viz {
#if BUILDFLAG(IS_WIN)
class InfoCollectionGpuServiceImpl;
#endif

class VizMainImpl : public mojom::VizMain {
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

    ExternalDependencies(const ExternalDependencies&) = delete;
    ExternalDependencies& operator=(const ExternalDependencies&) = delete;

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
    raw_ptr<gpu::SyncPointManager> sync_point_manager = nullptr;
    raw_ptr<gpu::SharedImageManager> shared_image_manager = nullptr;
    raw_ptr<gpu::Scheduler> scheduler = nullptr;
    raw_ptr<base::WaitableEvent> shutdown_event = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner;
    std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder;
    raw_ptr<VizCompositorThreadRunner> viz_compositor_thread_runner = nullptr;
  };

  VizMainImpl(Delegate* delegate,
              ExternalDependencies dependencies,
              std::unique_ptr<gpu::GpuInit> gpu_init);

  VizMainImpl(const VizMainImpl&) = delete;
  VizMainImpl& operator=(const VizMainImpl&) = delete;

  // Destruction must happen on the GPU thread.
  ~VizMainImpl() override;

  void Bind(mojo::PendingReceiver<mojom::VizMain> receiver);

  // mojom::VizMain implementation:
  void CreateGpuService(
      mojo::PendingReceiver<mojom::GpuService> pending_receiver,
      mojo::PendingRemote<mojom::GpuHost> pending_gpu_host,
      mojo::PendingRemote<
          discardable_memory::mojom::DiscardableSharedMemoryManager>
          discardable_memory_manager,
      base::UnsafeSharedMemoryRegion use_shader_cache_shm_region) override;
  void SetRenderParams(
      gfx::FontRenderParams::SubpixelRendering subpixel_rendering,
      float text_contrast,
      float text_gamma) override;
#if BUILDFLAG(IS_WIN)
  void CreateInfoCollectionGpuService(
      mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver)
      override;
#endif
#if BUILDFLAG(IS_ANDROID)
  void SetHostProcessId(int32_t pid) override;
#endif
  void CreateFrameSinkManager(mojom::FrameSinkManagerParamsPtr params) override;
#if BUILDFLAG(USE_VIZ_DEBUGGER)
  void FilterDebugStream(base::Value::Dict filter_data) override;
  void StartDebugStream(
      mojo::PendingRemote<mojom::VizDebugOutput> debug_output) override;
  void StopDebugStream() override;
#endif

  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }
  const GpuServiceImpl* gpu_service() const { return gpu_service_.get(); }

  // Note that this may be null if viz is running in the browser process and
  // using the ServiceDiscardableSharedMemoryManager.
  discardable_memory::ClientDiscardableSharedMemoryManager*
  discardable_shared_memory_manager() {
    return discardable_shared_memory_manager_.get();
  }

  // If it's in browser process, shut down the GPU main thread. Otherwise, the
  // GPU process is terminated immediately with the specified exit code.
  void ExitProcess(ExitCode immediate_exit_code);

 private:
  void CreateFrameSinkManagerInternal(mojom::FrameSinkManagerParamsPtr params);
  void RequestBeginFrameForGpuService(bool toggle);

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() const {
    return io_thread_ ? io_thread_->task_runner()
                      : dependencies_.io_thread_task_runner;
  }

  const raw_ptr<Delegate> delegate_;

  const ExternalDependencies dependencies_;

  // The thread that handles IO events for Gpu (if one isn't already provided).
  // |io_thread_| must be ordered above GPU service related variables so it's
  // destroyed after they are.
  std::unique_ptr<base::Thread> io_thread_;

  std::unique_ptr<gpu::GpuInit> gpu_init_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;
#if BUILDFLAG(IS_WIN)
  std::unique_ptr<InfoCollectionGpuServiceImpl> info_collection_gpu_service_;
#endif

  // If the gpu service is not yet ready then we stash pending
  // FrameSinkManagerParams.
  mojom::FrameSinkManagerParamsPtr pending_frame_sink_manager_params_;

  bool has_created_frame_sink_manager_ = false;

  // Runs the VizCompositorThread for the display compositor.
  std::unique_ptr<VizCompositorThreadRunnerImpl>
      viz_compositor_thread_runner_impl_;
  // Note under Android WebView where VizCompositorThreadRunner is not created
  // and owned by this, Viz does not interact with other objects in this class,
  // such as GpuServiceImpl. Code should take care to avoid introducing such
  // assumptions.
  raw_ptr<VizCompositorThreadRunner> viz_compositor_thread_runner_ = nullptr;

  const scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_task_runner_;

  mojo::Receiver<mojom::VizMain> receiver_{this};

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_MAIN_VIZ_MAIN_IMPL_H_
