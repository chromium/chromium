// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_

#include <memory>

#include "base/threading/thread.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"

namespace gpu {
class GpuChannelManager;
}  // namespace gpu

namespace viz {

class VIZ_SERVICE_EXPORT CompositorGpuThread
    : public base::Thread,
      public gpu::MemoryTracker::Observer {
 public:
  static std::unique_ptr<CompositorGpuThread> Create(
      gpu::GpuChannelManager* gpu_channel_manager);

  // Disallow copy and assign.
  CompositorGpuThread(const CompositorGpuThread&) = delete;
  CompositorGpuThread& operator=(const CompositorGpuThread&) = delete;

  ~CompositorGpuThread() override;

  scoped_refptr<gpu::SharedContextState> shared_context_state() const {
    return shared_context_state_;
  }

  // gpu::MemoryTracker::Observer implementation.
  void OnMemoryAllocatedChange(
      gpu::CommandBufferId id,
      uint64_t old_size,
      uint64_t new_size,
      gpu::GpuPeakMemoryAllocationSource source) override;

 private:
  explicit CompositorGpuThread(gpu::GpuChannelManager* gpu_channel_manager);

  bool Initialize();

  // Runs on compositor gpu thread.
  void InitializeOnThread(base::WaitableEvent* event, bool* success);
  void DestroyOnThread(base::WaitableEvent* event);

  gpu::GpuChannelManager* gpu_channel_manager_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;

  base::WeakPtrFactory<CompositorGpuThread> weak_ptr_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_
