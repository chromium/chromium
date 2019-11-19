// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <memory>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/viz_vulkan_context_provider_export.h"
#include "gpu/vulkan/buildflags.h"
#if BUILDFLAG(ENABLE_VULKAN)
#include "third_party/skia/include/gpu/vk/GrVkBackendContext.h"
#endif

namespace gpu {
class VulkanImplementation;
class VulkanDeviceQueue;
}

namespace viz {

class VIZ_VULKAN_CONTEXT_PROVIDER_EXPORT VulkanInProcessContextProvider
    : public VulkanContextProvider {
 public:
  static scoped_refptr<VulkanInProcessContextProvider> Create(
      gpu::VulkanImplementation* vulkan_implementation);

  void Destroy();

  // VulkanContextProvider implementation
  gpu::VulkanImplementation* GetVulkanImplementation() override;
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;
  GrContext* GetGrContext() override;
  GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() override;
  void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) override;
  void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) override;

 private:
  explicit VulkanInProcessContextProvider(
      gpu::VulkanImplementation* vulkan_implementation);
  ~VulkanInProcessContextProvider() override;

  bool Initialize();

#if BUILDFLAG(ENABLE_VULKAN)
  sk_sp<GrContext> gr_context_;
  gpu::VulkanImplementation* vulkan_implementation_;
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VulkanInProcessContextProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
