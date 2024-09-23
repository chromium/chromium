// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_HOST_GPU_MEMORY_BUFFER_MANAGER_H_
#define COMPONENTS_VIZ_HOST_HOST_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/host/viz_host_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

namespace gpu {
class GpuMemoryBufferSupport;
}

namespace viz {

namespace mojom {
class GpuService;
}

// This GpuMemoryBufferManager implementation is for [de]allocating GPU memory
// from the GPU process over the mojom.GpuService api. Parts of this class,
// namely methods in gpu::GpuMemoryBufferManager, are usable from any thread but
// this class must be created and destroyed on the UI thread.
//
// Note: `Shutdown()` must be called before the class is destroyed. Shutdown()
// should be called while other threads are still running to cancel any pending
// requests and unblock waiting threads. This class should only be destroyed
// after other threads are stopped to guarantee nothing is using it.
class VIZ_HOST_EXPORT HostGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  // Callback used to get the current instance of GpuService. The callback
  // should retry launching GPU service if it is not already running, or return
  // nullptr if it is impossible. |connection_error_handler| will be called when
  // the GpuService is shut down. The return value will be cached until the GPU
  // service is shut down.
  using GpuServiceProvider = base::RepeatingCallback<mojom::GpuService*(
      base::OnceClosure connection_error_handler)>;

  // All function of HostGpuMemoryBufferManager must be called the thread
  // associated with |task_runner|, other than the constructor and the
  // gpu::GpuMemoryBufferManager implementation (which can be called from any
  // thread).
  HostGpuMemoryBufferManager(
      GpuServiceProvider gpu_service_provider,
      int gpu_service_client_id,
      std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  HostGpuMemoryBufferManager(const HostGpuMemoryBufferManager&) = delete;
  HostGpuMemoryBufferManager& operator=(const HostGpuMemoryBufferManager&) =
      delete;

  ~HostGpuMemoryBufferManager() override;

  // Shutdown GpuMemoryBufferManager before it's destroyed. This will cancel any
  // pending requests to CreateGpuMemoryBuffer() and unblock any threads waiting
  // on requests. Must be called from UI thread.
  void Shutdown();

  // Overridden from gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::WaitableEvent* shutdown_event) override;
  void CopyGpuMemoryBufferAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region,
      base::OnceCallback<void(bool)> callback) override;
  bool CopyGpuMemoryBufferSync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion memory_region) override;

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class HostGpuMemoryBufferManagerTest;
  FRIEND_TEST_ALL_PREFIXES(HostGpuMemoryBufferManagerTest,
                           AllocationRequestsForDestroyedClient);
  FRIEND_TEST_ALL_PREFIXES(HostGpuMemoryBufferManagerTest,
                           AllocationRequestFromDeadGpuService);

  void AllocateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle,
      base::OnceCallback<void(gfx::GpuMemoryBufferHandle)> callback,
      bool call_sync = false);

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id);

  struct PendingBufferInfo {
    PendingBufferInfo();
    PendingBufferInfo(PendingBufferInfo&&);
    ~PendingBufferInfo();

    gfx::Size size;
    gfx::BufferFormat format;
    gfx::BufferUsage usage;
    gpu::SurfaceHandle surface_handle;
    base::OnceCallback<void(gfx::GpuMemoryBufferHandle)> callback;
  };
  using PendingBuffers = std::unordered_map<gfx::GpuMemoryBufferId,
                                            PendingBufferInfo,
                                            std::hash<gfx::GpuMemoryBufferId>>;

  using AllocatedBuffers =
      std::unordered_map<gfx::GpuMemoryBufferId,
                         gpu::AllocatedBufferInfo,
                         std::hash<gfx::GpuMemoryBufferId>>;

  mojom::GpuService* GetGpuService();

  // This is called whenever GPU service is shut down (e.g. GPU process
  // crashes). It will invalidate any allocated memory buffer and retry
  // allocation requests for pending memory buffers.
  void OnConnectionError();

  void OnGpuMemoryBufferAllocated(int gpu_service_version,
                                  gfx::GpuMemoryBufferId id,
                                  gfx::GpuMemoryBufferHandle handle);

  GpuServiceProvider gpu_service_provider_;
  raw_ptr<mojom::GpuService> gpu_service_ = nullptr;

  // This is incremented every time GPU service is shut down in order check
  // whether a buffer is allocated by the most current GPU service or not.
  int gpu_service_version_ = 0;

  const int gpu_service_client_id_;
  int next_gpu_memory_id_ = 1;

  // Used to cancel pending requests on shutdown.
  base::WaitableEvent shutdown_event_;

  PendingBuffers pending_buffers_;
  AllocatedBuffers allocated_buffers_;

  std::unique_ptr<gpu::GpuMemoryBufferSupport> gpu_memory_buffer_support_;

  scoped_refptr<base::UnsafeSharedMemoryPool> pool_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<HostGpuMemoryBufferManager> weak_ptr_;
  base::WeakPtrFactory<HostGpuMemoryBufferManager> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_HOST_GPU_MEMORY_BUFFER_MANAGER_H_
