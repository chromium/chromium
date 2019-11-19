// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/buildflags.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkExtensions.h"

namespace viz {

scoped_refptr<VulkanInProcessContextProvider>
VulkanInProcessContextProvider::Create(
    gpu::VulkanImplementation* vulkan_implementation) {
  scoped_refptr<VulkanInProcessContextProvider> context_provider(
      new VulkanInProcessContextProvider(vulkan_implementation));
  if (!context_provider->Initialize())
    return nullptr;
  return context_provider;
}

GrVkGetProc make_unified_getter(const PFN_vkGetInstanceProcAddr& iproc,
                                const PFN_vkGetDeviceProcAddr& dproc) {
  return [&iproc, &dproc](const char* proc_name, VkInstance instance,
                          VkDevice device) {
    if (device != VK_NULL_HANDLE) {
      return dproc(device, proc_name);
    }
    return iproc(instance, proc_name);
  };
}

VulkanInProcessContextProvider::VulkanInProcessContextProvider(
    gpu::VulkanImplementation* vulkan_implementation)
    : vulkan_implementation_(vulkan_implementation) {}

VulkanInProcessContextProvider::~VulkanInProcessContextProvider() {
  Destroy();
}

bool VulkanInProcessContextProvider::Initialize() {
  DCHECK(!device_queue_);

  const auto& instance_extensions = vulkan_implementation_->GetVulkanInstance()
                                        ->vulkan_info()
                                        .enabled_instance_extensions;

  uint32_t flags = gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG;
  constexpr base::StringPiece surface_extension_name(
      VK_KHR_SURFACE_EXTENSION_NAME);
  for (const auto* extension : instance_extensions) {
    if (surface_extension_name == extension) {
      flags |= gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG;
      break;
    }
  }

  device_queue_ = gpu::CreateVulkanDeviceQueue(vulkan_implementation_, flags);
  if (!device_queue_)
    return false;

  GrVkBackendContext backend_context;
  backend_context.fInstance = device_queue_->GetVulkanInstance();
  backend_context.fPhysicalDevice = device_queue_->GetVulkanPhysicalDevice();
  backend_context.fDevice = device_queue_->GetVulkanDevice();
  backend_context.fQueue = device_queue_->GetVulkanQueue();
  backend_context.fGraphicsQueueIndex = device_queue_->GetVulkanQueueIndex();
  backend_context.fMaxAPIVersion = vulkan_implementation_->GetVulkanInstance()
                                       ->vulkan_info()
                                       .used_api_version;

  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  GrVkGetProc get_proc =
      make_unified_getter(vulkan_function_pointers->vkGetInstanceProcAddrFn,
                          vulkan_function_pointers->vkGetDeviceProcAddrFn);

  std::vector<const char*> device_extensions;
  device_extensions.reserve(device_queue_->enabled_extensions().size());
  for (const auto& extension : device_queue_->enabled_extensions())
    device_extensions.push_back(extension.data());
  GrVkExtensions gr_extensions;
  gr_extensions.init(get_proc,
                     vulkan_implementation_->GetVulkanInstance()->vk_instance(),
                     device_queue_->GetVulkanPhysicalDevice(),
                     instance_extensions.size(), instance_extensions.data(),
                     device_extensions.size(), device_extensions.data());
  backend_context.fVkExtensions = &gr_extensions;
  backend_context.fDeviceFeatures2 =
      &device_queue_->enabled_device_features_2();
  backend_context.fGetProc = get_proc;
  backend_context.fProtectedContext =
      vulkan_implementation_->enforce_protected_memory() ? GrProtected::kYes
                                                         : GrProtected::kNo;

  gr_context_ = GrContext::MakeVulkan(backend_context);

  return gr_context_ != nullptr;
}

void VulkanInProcessContextProvider::Destroy() {
  if (device_queue_) {
    // Destroy |fence_helper| will wait idle on the device queue, and then run
    // all enqueued cleanup tasks.
    auto* fence_helper = device_queue_->GetFenceHelper();
    fence_helper->Destroy();
  }

  if (gr_context_) {
    // releaseResourcesAndAbandonContext() will wait on GPU to finish all works,
    // execute pending flush done callbacks and release all resources.
    gr_context_->releaseResourcesAndAbandonContext();
    gr_context_.reset();
  }

  if (device_queue_) {
    device_queue_->Destroy();
    device_queue_.reset();
  }
}

gpu::VulkanImplementation*
VulkanInProcessContextProvider::GetVulkanImplementation() {
  return vulkan_implementation_;
}

gpu::VulkanDeviceQueue* VulkanInProcessContextProvider::GetDeviceQueue() {
  return device_queue_.get();
}

GrContext* VulkanInProcessContextProvider::GetGrContext() {
  return gr_context_.get();
}

GrVkSecondaryCBDrawContext*
VulkanInProcessContextProvider::GetGrSecondaryCBDrawContext() {
  return nullptr;
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBSemaphores(
    std::vector<VkSemaphore> semaphores) {
  NOTREACHED();
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBPostSubmitTask(
    base::OnceClosure closure) {
  NOTREACHED();
}

}  // namespace viz
