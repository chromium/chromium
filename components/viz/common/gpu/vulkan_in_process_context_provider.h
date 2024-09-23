// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/viz_vulkan_context_provider_export.h"
#include "gpu/vulkan/buildflags.h"
#include "third_party/skia/include/gpu/ganesh/GrContextOptions.h"

namespace gpu {
class VulkanImplementation;
class VulkanDeviceQueue;
struct GPUInfo;
}

namespace viz {

class VIZ_VULKAN_CONTEXT_PROVIDER_EXPORT VulkanInProcessContextProvider
    : public VulkanContextProvider {
 public:
  // if |sync_cpu_memory_limit| is set and greater than zero,
  // |cooldown_duration_at_memory_pressure_critical| is the duration of applying
  // zero sync cpu memory limit after CRITICAL memory pressure signal is
  // received. 15s is default to sync with memory monitor cycles.
  static scoped_refptr<VulkanInProcessContextProvider> Create(
      gpu::VulkanImplementation* vulkan_implementation,
      uint32_t heap_memory_limit = 0,
      uint32_t sync_cpu_memory_limit = 0,
      const bool is_thread_safe = false,
      const gpu::GPUInfo* gpu_info = nullptr,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical =
          base::Seconds(15));

  // Creates a VulkanContextProvider for the CompositorGpuThread.
  static scoped_refptr<VulkanInProcessContextProvider>
  CreateForCompositorGpuThread(
      gpu::VulkanImplementation* vulkan_implementation,
      std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue,
      uint32_t sync_cpu_memory_limit = 0,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical =
          base::Seconds(15));

  VulkanInProcessContextProvider(const VulkanInProcessContextProvider&) =
      delete;
  VulkanInProcessContextProvider& operator=(
      const VulkanInProcessContextProvider&) = delete;

  void Destroy();

  // VulkanContextProvider implementation
  bool InitializeGrContext(const GrContextOptions& context_options) override;
  gpu::VulkanImplementation* GetVulkanImplementation() override;
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;
  GrDirectContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;
  void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) override;
  void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) override;
  std::optional<uint32_t> GetSyncCpuMemoryLimit() const override;

 private:
  friend class VulkanInProcessContextProviderTest;

  VulkanInProcessContextProvider(
      gpu::VulkanImplementation* vulkan_implementation,
      uint32_t heap_memory_limit,
      uint32_t sync_cpu_memory_limit,
      base::TimeDelta cooldown_duration_at_memory_pressure_critical);
  ~VulkanInProcessContextProvider() override;

  bool Initialize(const gpu::GPUInfo* gpu_info,
                  const bool is_thread_safe = false);

  void InitializeForCompositorGpuThread(
      std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue);

  // Memory pressure handler, called by |memory_pressure_listener_|.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

#if BUILDFLAG(ENABLE_VULKAN)
  sk_sp<GrDirectContext> gr_context_;
  raw_ptr<gpu::VulkanImplementation> vulkan_implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
  const uint32_t heap_memory_limit_;
  const uint32_t sync_cpu_memory_limit_;
  const base::TimeDelta cooldown_duration_at_memory_pressure_critical_;
  base::TimeTicks critical_memory_pressure_expiration_time_;
#endif

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
