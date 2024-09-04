// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"

#include <string_view>
#include <utility>

#include "gpu/vulkan/buildflags.h"
#include "gpu/vulkan/init/skia_vk_memory_allocator_impl.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkDirectContext.h"
#include "third_party/skia/include/gpu/vk/VulkanBackendContext.h"
#include "third_party/skia/include/gpu/vk/VulkanExtensions.h"
#include "third_party/skia/include/gpu/vk/VulkanTypes.h"

namespace {

// Setting this limit to 0 practically forces sync at every submit.
constexpr uint32_t kSyncCpuMemoryLimitAtMemoryPressureCritical = 0;

}  // namespace

namespace viz {

// static
scoped_refptr<VulkanInProcessContextProvider>
VulkanInProcessContextProvider::Create(
    gpu::VulkanImplementation* vulkan_implementation,
    uint32_t heap_memory_limit,
    uint32_t sync_cpu_memory_limit,
    const bool is_thread_safe,
    const gpu::GPUInfo* gpu_info,
    base::TimeDelta cooldown_duration_at_memory_pressure_critical) {
  scoped_refptr<VulkanInProcessContextProvider> context_provider(
      new VulkanInProcessContextProvider(
          vulkan_implementation, heap_memory_limit, sync_cpu_memory_limit,
          cooldown_duration_at_memory_pressure_critical));
  if (!context_provider->Initialize(gpu_info, is_thread_safe))
    return nullptr;
  return context_provider;
}

scoped_refptr<VulkanInProcessContextProvider>
VulkanInProcessContextProvider::CreateForCompositorGpuThread(
    gpu::VulkanImplementation* vulkan_implementation,
    std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue,
    uint32_t sync_cpu_memory_limit,
    base::TimeDelta cooldown_duration_at_memory_pressure_critical) {
  if (!vulkan_implementation)
    return nullptr;

  scoped_refptr<VulkanInProcessContextProvider> context_provider(
      new VulkanInProcessContextProvider(
          vulkan_implementation, /*heap_memory_limit=*/0, sync_cpu_memory_limit,
          cooldown_duration_at_memory_pressure_critical));
  context_provider->InitializeForCompositorGpuThread(
      std::move(vulkan_device_queue));
  return context_provider;
}

VulkanInProcessContextProvider::VulkanInProcessContextProvider(
    gpu::VulkanImplementation* vulkan_implementation,
    uint32_t heap_memory_limit,
    uint32_t sync_cpu_memory_limit,
    base::TimeDelta cooldown_duration_at_memory_pressure_critical)
    : vulkan_implementation_(vulkan_implementation),
      heap_memory_limit_(heap_memory_limit),
      sync_cpu_memory_limit_(sync_cpu_memory_limit),
      cooldown_duration_at_memory_pressure_critical_(
          cooldown_duration_at_memory_pressure_critical) {
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&VulkanInProcessContextProvider::OnMemoryPressure,
                          base::Unretained(this)));
}

VulkanInProcessContextProvider::~VulkanInProcessContextProvider() {
  Destroy();
}

bool VulkanInProcessContextProvider::Initialize(const gpu::GPUInfo* gpu_info,
                                                const bool is_thread_safe) {
  DCHECK(!device_queue_);

  const auto& instance_extensions = vulkan_implementation_->GetVulkanInstance()
                                        ->vulkan_info()
                                        .enabled_instance_extensions;

  uint32_t flags = gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG;
  constexpr std::string_view surface_extension_name(
      VK_KHR_SURFACE_EXTENSION_NAME);
  for (const auto* extension : instance_extensions) {
    if (surface_extension_name == extension) {
      flags |= gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG;
      break;
    }
  }

  device_queue_ =
      gpu::CreateVulkanDeviceQueue(vulkan_implementation_, flags, gpu_info,
                                   heap_memory_limit_, is_thread_safe);
  if (!device_queue_)
    return false;

  return true;
}

void VulkanInProcessContextProvider::InitializeForCompositorGpuThread(
    std::unique_ptr<gpu::VulkanDeviceQueue> vulkan_device_queue) {
  DCHECK(!device_queue_);
  DCHECK(vulkan_device_queue);

  device_queue_ = std::move(vulkan_device_queue);
}

bool VulkanInProcessContextProvider::InitializeGrContext(
    const GrContextOptions& context_options) {
  skgpu::VulkanBackendContext backend_context;
  backend_context.fInstance = device_queue_->GetVulkanInstance();
  backend_context.fPhysicalDevice = device_queue_->GetVulkanPhysicalDevice();
  backend_context.fDevice = device_queue_->GetVulkanDevice();
  backend_context.fQueue = device_queue_->GetVulkanQueue();
  backend_context.fGraphicsQueueIndex = device_queue_->GetVulkanQueueIndex();
  backend_context.fMaxAPIVersion = vulkan_implementation_->GetVulkanInstance()
                                       ->vulkan_info()
                                       .used_api_version;
  backend_context.fMemoryAllocator =
      gpu::CreateSkiaVulkanMemoryAllocator(device_queue_.get());

  skgpu::VulkanGetProc get_proc = [](const char* proc_name, VkInstance instance,
                                     VkDevice device) {
    if (device) {
      // Using vkQueue*Hook for all vkQueue* methods here to make both chrome
      // side access and skia side access to the same queue thread safe.
      // vkQueue*Hook routes all skia side access to the same
      // VulkanFunctionPointers vkQueue* api which chrome uses and is under the
      // lock.
      if (std::strcmp("vkCreateGraphicsPipelines", proc_name) == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::CreateGraphicsPipelinesHook);
      } else if (std::strcmp("vkQueueSubmit", proc_name) == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::VulkanQueueSubmitHook);
      } else if (std::strcmp("vkQueueWaitIdle", proc_name) == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::VulkanQueueWaitIdleHook);
      } else if (std::strcmp("vkQueuePresentKHR", proc_name) == 0) {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::VulkanQueuePresentKHRHook);
      }
      return vkGetDeviceProcAddr(device, proc_name);
    }
    return vkGetInstanceProcAddr(instance, proc_name);
  };

  const auto& instance_extensions = vulkan_implementation_->GetVulkanInstance()
                                        ->vulkan_info()
                                        .enabled_instance_extensions;

  std::vector<const char*> device_extensions;
  device_extensions.reserve(device_queue_->enabled_extensions().size());
  for (const auto& extension : device_queue_->enabled_extensions())
    device_extensions.push_back(extension.data());
  skgpu::VulkanExtensions vk_extensions;
  vk_extensions.init(get_proc,
                     vulkan_implementation_->GetVulkanInstance()->vk_instance(),
                     device_queue_->GetVulkanPhysicalDevice(),
                     instance_extensions.size(), instance_extensions.data(),
                     device_extensions.size(), device_extensions.data());
  backend_context.fVkExtensions = &vk_extensions;
  backend_context.fDeviceFeatures2 =
      &device_queue_->enabled_device_features_2();
  backend_context.fGetProc = get_proc;
  backend_context.fProtectedContext = GrProtected::kNo;

  gr_context_ = GrDirectContexts::MakeVulkan(backend_context, context_options);

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

GrDirectContext* VulkanInProcessContextProvider::GetGrContext() {
  return gr_context_.get();
}

GrVkSecondaryCBDrawContext*
VulkanInProcessContextProvider::GetGrSecondaryCBDrawContext() {
  return nullptr;
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBSemaphores(
    std::vector<VkSemaphore> semaphores) {
  NOTREACHED_IN_MIGRATION();
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBPostSubmitTask(
    base::OnceClosure closure) {
  NOTREACHED_IN_MIGRATION();
}

std::optional<uint32_t> VulkanInProcessContextProvider::GetSyncCpuMemoryLimit()
    const {
  // Return false to indicate that there's no limit.
  if (!sync_cpu_memory_limit_) {
    return std::nullopt;
  }
  return base::TimeTicks::Now() < critical_memory_pressure_expiration_time_
             ? std::optional<uint32_t>(
                   kSyncCpuMemoryLimitAtMemoryPressureCritical)
             : std::optional<uint32_t>(sync_cpu_memory_limit_);
}

void VulkanInProcessContextProvider::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level != base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL)
    return;

  critical_memory_pressure_expiration_time_ =
      base::TimeTicks::Now() + cooldown_duration_at_memory_pressure_critical_;
}

}  // namespace viz
