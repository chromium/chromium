// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_

#include <cstddef>
#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"

namespace gl {
class GLDisplay;
}  // namespace gl

namespace gpu {
class DawnContextProvider;
class GpuChannelManager;
class VulkanImplementation;
class VulkanDeviceQueue;
}  // namespace gpu

namespace viz {

class VulkanContextProvider;

class VIZ_SERVICE_EXPORT CompositorGpuThread
    : public base::Thread,
      public base::MemoryPressureListener {
 public:
  using GetVideoMemoryUsageStatsCallback =
      base::OnceCallback<void(const ::gpu::VideoMemoryUsageStats&)>;

  struct CreateParams {
    raw_ptr<gpu::GpuChannelManager> gpu_channel_manager = nullptr;
    raw_ptr<gl::GLDisplay> display = nullptr;
    bool enable_watchdog = true;
#if BUILDFLAG(ENABLE_VULKAN)
    raw_ptr<gpu::VulkanImplementation> vulkan_implementation = nullptr;
    raw_ptr<gpu::VulkanDeviceQueue> device_queue = nullptr;
#endif
#if BUILDFLAG(SKIA_USE_DAWN)
    raw_ptr<gpu::DawnContextProvider> dawn_context_provider = nullptr;
#endif
  };

  static std::unique_ptr<CompositorGpuThread> Create(
      const CreateParams& params);

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

  // These methods are called when chrome application is backgrounded and
  // foregrounded.
  void OnBackgrounded();
  void OnForegrounded();

  // This method is usually called only for low end devices on android.
  void OnBackgroundCleanup();

  void LoseContext();

  void AddVideoMemoryUsageStatsOnCompositorGpu(
      GetVideoMemoryUsageStatsCallback callback,
      gpu::VideoMemoryUsageStats video_memory_usage_stats);

 private:
  CompositorGpuThread(
      gpu::GpuChannelManager* gpu_channel_manager,
      gl::GLDisplay* display,
      bool enable_watchdog);

  bool Initialize();

  void OnMemoryPressure(
      base::MemoryPressureLevel memory_pressure_level) override;
  void OnBackgroundedOnCompositorGpuThread();

  raw_ptr<gpu::GpuChannelManager> gpu_channel_manager_;
  const bool enable_watchdog_;
  bool init_succeeded_ = false;

  scoped_refptr<VulkanContextProvider> vulkan_context_provider_;

#if BUILDFLAG(SKIA_USE_DAWN)
  std::unique_ptr<gpu::DawnContextProvider> dawn_context_provider_;
#endif

  // The GLDisplay lives in GLDisplayManager, which never deletes displays once
  // they are lazily created.
  raw_ptr<gl::GLDisplay> display_ = nullptr;

  // WatchdogThread to monitor CompositorGpuThread. Ensure that the members
  // which needs to be monitored by |watchdog_thread_| should be destroyed
  // before it by either adding them below it or explicitly destroying them
  // before it.
  std::unique_ptr<gpu::GpuWatchdogThread> watchdog_thread_;
  scoped_refptr<gpu::SharedContextState> shared_context_state_;

  // Listens to the memory pressure signals from the platform.
  std::unique_ptr<base::AsyncMemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;

  base::WeakPtrFactory<CompositorGpuThread> weak_ptr_factory_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_COMPOSITOR_GPU_THREAD_H_
