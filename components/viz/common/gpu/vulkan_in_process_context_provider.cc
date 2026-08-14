// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/traits.h"
#include "base/memory_coordinator/utils.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/vulkan/buildflags.h"
#include "gpu/vulkan/vma_wrapper.h"
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

constexpr base::MemoryConsumerTraits kMemoryConsumerTraits(
    // Deferred Vulkan resources can accumulate to a moderate amount of memory.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    // Callback only updates atomics without traversing structures.
    base::MemoryConsumerTraits::ReleaseMemoryCost::kFreesPagesWithoutTraversal,
    // Syncing GPU work does not lose user state or cached data.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    // Pre-determined by AsyncMemoryConsumerRegistration on the GPU thread.
    base::MemoryConsumerTraits::ExecutionType::kAsynchronous);

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
      sync_cpu_memory_limit_(sync_cpu_memory_limit > 0
                                 ? std::make_optional(sync_cpu_memory_limit)
                                 : std::nullopt),
      cooldown_duration_at_memory_pressure_critical_(
          cooldown_duration_at_memory_pressure_critical),
      critical_memory_pressure_expiration_time_(base::TimeTicks()),
      active_sync_cpu_memory_limit_(sync_cpu_memory_limit) {
  memory_consumer_registration_ =
      std::make_unique<base::AsyncMemoryConsumerRegistration>(
          "VulkanInProcessContextProvider", kMemoryConsumerTraits, this);
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
  backend_context.fMemoryAllocator = device_queue_->GetSkiaVkMemoryAllocator();

  skgpu::VulkanGetProc get_proc = [](const char* proc_name, VkInstance instance,
                                     VkDevice device) {
    if (device) {
      // Using vkQueue*Hook for all vkQueue* methods here to make both chrome
      // side access and skia side access to the same queue thread safe.
      // vkQueue*Hook routes all skia side access to the same
      // VulkanFunctionPointers vkQueue* api which chrome uses and is under the
      // lock.
      std::string_view proc_name_view(proc_name);
      if (proc_name_view == "vkCreateGraphicsPipelines") {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::CreateGraphicsPipelinesHook);
      } else if (proc_name_view == "vkQueueSubmit") {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::VulkanQueueSubmitHook);
      } else if (proc_name_view == "vkQueueWaitIdle") {
        return reinterpret_cast<PFN_vkVoidFunction>(
            &gpu::VulkanQueueWaitIdleHook);
      } else if (proc_name_view == "vkQueuePresentKHR") {
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
  NOTREACHED();
}

void VulkanInProcessContextProvider::EnqueueSecondaryCBPostSubmitTask(
    base::OnceClosure closure) {
  NOTREACHED();
}

std::optional<uint32_t> VulkanInProcessContextProvider::GetSyncCpuMemoryLimit()
    const {
  // Return nullopt to indicate that there's no limit.
  if (!sync_cpu_memory_limit_) {
    return std::nullopt;
  }

  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    return active_sync_cpu_memory_limit_.load(std::memory_order_relaxed);
  }

  return base::TimeTicks::Now() <
                 critical_memory_pressure_expiration_time_.load(
                     std::memory_order_relaxed)
             ? std::optional<uint32_t>(
                   kSyncCpuMemoryLimitAtMemoryPressureCritical)
             : sync_cpu_memory_limit_;
}

uint32_t VulkanInProcessContextProvider::GetCurrentGpuMemoryUsage() const {
  if (device_queue_) {
    return base::saturated_cast<uint32_t>(
        gpu::vma::GetTotalAllocatedAndUsedMemory(device_queue_->vma_allocator())
            .first);
  }
  return 0;
}

void VulkanInProcessContextProvider::OnUpdateMemoryLimit() {
  if (!sync_cpu_memory_limit_) {
    return;
  }

  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    // We cap the ratio to 1.0 to ensure that we never exceed the
    // user-provided sync_cpu_memory_limit, even if the memory coordinator
    // allows for more memory usage (ratio > 1.0).
    int capped_memory_limit = std::min(100, memory_limit());
    uint32_t target_limit =
        base::ScaleByMemoryLimit(*sync_cpu_memory_limit_, capped_memory_limit);

    uint32_t new_limit = target_limit;
    uint32_t current_active =
        active_sync_cpu_memory_limit_.load(std::memory_order_relaxed);

    if (target_limit <= current_active) {
      // Limit decreased: cap at current usage to prevent growth without
      // triggering immediate eviction.
      new_limit = std::max(GetCurrentGpuMemoryUsage(), target_limit);
    }

    active_sync_cpu_memory_limit_.store(new_limit, std::memory_order_relaxed);
  }
}

void VulkanInProcessContextProvider::OnReleaseMemory() {
  if (!sync_cpu_memory_limit_) {
    return;
  }

  if (base::FeatureList::IsEnabled(base::kStatefulMemoryPressure)) {
    // We cap the ratio to 1.0 to ensure that we never exceed the
    // user-provided sync_cpu_memory_limit, even if the memory coordinator
    // allows for more memory usage (ratio > 1.0).
    int capped_memory_limit = std::min(100, memory_limit());
    uint32_t new_sync_cpu_memory_limit =
        base::ScaleByMemoryLimit(*sync_cpu_memory_limit_, capped_memory_limit);
    active_sync_cpu_memory_limit_.store(new_sync_cpu_memory_limit,
                                        std::memory_order_relaxed);
    return;
  }

  if (memory_limit() > base::kCriticalMemoryPressureThreshold) {
    return;
  }

  critical_memory_pressure_expiration_time_.store(
      base::TimeTicks::Now() + cooldown_duration_at_memory_pressure_critical_,
      std::memory_order_relaxed);
}

}  // namespace viz
