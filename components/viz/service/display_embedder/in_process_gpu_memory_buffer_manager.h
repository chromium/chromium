// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IN_PROCESS_GPU_MEMORY_BUFFER_MANAGER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IN_PROCESS_GPU_MEMORY_BUFFER_MANAGER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"

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
  // Note: Any GpuMemoryBuffers that haven't been destroyed yet will be leaked
  // until the GpuMemoryBufferFactory is destroyed.
  ~InProcessGpuMemoryBufferManager() override;

  // gpu::GpuMemoryBufferManager:
  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::SurfaceHandle surface_handle) override;
  void SetDestructionSyncToken(gfx::GpuMemoryBuffer* buffer,
                               const gpu::SyncToken& sync_token) override;

  // base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  // Provided as callback when a GpuMemoryBuffer should be destroyed.
  void ShouldDestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                                    const gpu::SyncToken& sync_token);
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id);

  gpu::GpuMemoryBufferSupport gpu_memory_buffer_support_;
  const int client_id_;
  int next_gpu_memory_id_ = 1;

  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;
  gpu::SyncPointManager* const sync_point_manager_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::flat_map<gfx::GpuMemoryBufferId, AllocatedBufferInfo>
      allocated_buffers_;

  base::WeakPtr<InProcessGpuMemoryBufferManager> weak_ptr_;
  base::WeakPtrFactory<InProcessGpuMemoryBufferManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InProcessGpuMemoryBufferManager);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_IN_PROCESS_GPU_MEMORY_BUFFER_MANAGER_H_
