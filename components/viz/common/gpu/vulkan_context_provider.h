// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_VULKAN_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_VULKAN_CONTEXT_PROVIDER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/viz_vulkan_context_provider_export.h"
#include "gpu/vulkan/buildflags.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif

#if !defined(VK_VERSION_1_1)
// Workaround compiling issue when vulkan is disabled.
typedef void* VkSemaphore;
#endif

struct GrContextOptions;
class GrDirectContext;
class GrVkSecondaryCBDrawContext;

namespace gpu {
class VulkanDeviceQueue;
class VulkanImplementation;
}

namespace viz {

// The VulkanContextProvider groups sharing of vulkan objects synchronously.
class VIZ_VULKAN_CONTEXT_PROVIDER_EXPORT VulkanContextProvider
    : public base::RefCountedThreadSafe<VulkanContextProvider> {
 public:
  virtual bool InitializeGrContext(const GrContextOptions& context_options) = 0;
  virtual gpu::VulkanImplementation* GetVulkanImplementation() = 0;
  virtual gpu::VulkanDeviceQueue* GetDeviceQueue() = 0;
  virtual GrDirectContext* GetGrContext() = 0;

  // Get the current SecondaryCBDrawContext for the default render target.
  virtual GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() = 0;

  // Enqueue semaphores which will be submitted with GrSecondaryCB to device
  // queue for signalling.
  virtual void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) = 0;

  // Enqueue task which will be executed after the GrSecondaryCB and post submit
  // semphores are submitted.
  virtual void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) = 0;

  // Returns a valid limit in MB if there is a memory limit where GPU work
  // should be synchronized with the CPU in order to free previously released
  // memory immediately. In other words, the CPU will wait for GPU work to
  // complete before proceeding when the current amount of allocated memory
  // exceeds this limit.
  virtual std::optional<uint32_t> GetSyncCpuMemoryLimit() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<VulkanContextProvider>;
  virtual ~VulkanContextProvider() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_VULKAN_CONTEXT_PROVIDER_H_
