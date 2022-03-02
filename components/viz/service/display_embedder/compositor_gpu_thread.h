// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"

namespace gpu {
class GpuChannelManager;
class VulkanImplementation;
class VulkanDeviceQueue;
class VulkanContextProvider;
}  // namespace gpu

namespace viz {

class VIZ_SERVICE_EXPORT CompositorGpuThread
    : public base::Thread,
      public gpu::MemoryTracker::Observer {
 public:
  static std::unique_ptr<CompositorGpuThread> Create(
      gpu::GpuChannelManager* gpu_channel_manager,
      gpu::VulkanImplementation* vulkan_implementation,
      gpu::VulkanDeviceQueue* device_queue,
      bool enable_watchdog);

  // Disallow copy and assign.
  CompositorGpuThread(const CompositorGpuThread&) = delete;
  CompositorGpuThread& operator=(const CompositorGpuThread&) = delete;

  ~CompositorGpuThread() override;

  scoped_refptr<gpu::SharedContextState> GetSharedContextState();

  VulkanContextProvider* vulkan_context_provider() const {
    return vulkan_context_provider_.get();
  }

  // base::Thread implementation.
  void Init() override;
  void CleanUp() override;

  // gpu::MemoryTracker::Observer implementation.
  void OnMemoryAllocatedChange(
      gpu::CommandBufferId id,
      uint64_t old_size,
      uint64_t new_size,
      gpu::GpuPeakMemoryAllocationSource source) override;

  void OnBackgrounded();
  void OnForegrounded();

 private:
  CompositorGpuThread(
      gpu::GpuChannelManager* gpu_channel_manager,
      scoped_refptr<VulkanContextProvider> vulkan_context_provider,
      bool enable_watchdog);

  bool Initialize();

  raw_ptr<gpu::GpuChannelManager> gpu_channel_manager_;
  const bool enable_watchdog_;
  bool init_succeded_ = false;

  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;

  // WatchdogThread to monitor CompositorGpuThread. Ensure that the members
  // which needs to be monitored by |watchdog_thread_| should be destroyed
  // before it by either adding them below it or explicitly destroying them
  // before it.
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;

  base::WeakPtrFactory<CompositorGpuThread> weak_ptr_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_
