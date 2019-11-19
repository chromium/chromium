// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_VULKAN_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_VULKAN_CONTEXT_PROVIDER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/viz/common/viz_vulkan_context_provider_export.h"
#include "third_party/vulkan/include/vulkan/vulkan.h"

class GrContext;
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
  virtual gpu::VulkanImplementation* GetVulkanImplementation() = 0;
  virtual gpu::VulkanDeviceQueue* GetDeviceQueue() = 0;
  virtual GrContext* GetGrContext() = 0;

  // Get the current SecondaryCBDrawContext for the default render target.
  virtual GrVkSecondaryCBDrawContext* GetGrSecondaryCBDrawContext() = 0;

  // Enqueue semaphores which will be submitted with GrSecondaryCB to device
  // queue for signalling.
  virtual void EnqueueSecondaryCBSemaphores(
      std::vector<VkSemaphore> semaphores) = 0;

  // Enqueue task which will be executed after the GrSecondaryCB and post submit
  // semphores are submitted.
  virtual void EnqueueSecondaryCBPostSubmitTask(base::OnceClosure closure) = 0;

 protected:
  friend class base::RefCountedThreadSafe<VulkanContextProvider>;
  virtual ~VulkanContextProvider() {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_VULKAN_CONTEXT_PROVIDER_H_
