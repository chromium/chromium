// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IN_PROCESS_GPU_MEMORY_BUFFER_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IN_PROCESS_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_pool.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class GpuMemoryBufferFactory;
class SyncPointManager;
}

namespace viz {

// GpuMemoryBufferManager implementation usable from any thread in the GPU
// process. Must be created and destroyed on the same thread.
class VIZ_SERVICE_EXPORT InProcessGpuMemoryBufferManager
    : public gpu::GpuMemoryBufferManager,
      public base::trace_event::MemoryDumpProvider {
 public:
  // |gpu_memory_buffer_factory| and |sync_point_manager| must outlive |this|.
  InProcessGpuMemoryBufferManager(
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      gpu::SyncPointManager* sync_point_manager);

  InProcessGpuMemoryBufferManager(const InProcessGpuMemoryBufferManager&) =
      delete;
  InProcessGpuMemoryBufferManager& operator=(
      const InProcessGpuMemoryBufferManager&) = delete;

  // Note: Any GpuMemoryBuffers that haven't been destroyed yet will be leaked
  // until the GpuMemoryBufferFactory is destroyed.
  ~InProcessGpuMemoryBufferManager() override;

  // gpu::GpuMemoryBufferManager:
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

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // Provided as callback when a GpuMemoryBuffer should be destroyed.
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id);

  gpu::GpuMemoryBufferSupport gpu_memory_buffer_support_;
  const int client_id_;
  int next_gpu_memory_id_ = 1;

  scoped_refptr<base::UnsafeSharedMemoryPool> pool_;

  const raw_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;
  const raw_ptr<gpu::SyncPointManager> sync_point_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::flat_map<gfx::GpuMemoryBufferId, gpu::AllocatedBufferInfo>
      allocated_buffers_;

  base::WeakPtr<InProcessGpuMemoryBufferManager> weak_ptr_;
  base::WeakPtrFactory<InProcessGpuMemoryBufferManager> weak_ptr_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IN_PROCESS_GPU_MEMORY_BUFFER_MANAGER_H_
