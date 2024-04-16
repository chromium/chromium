// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_
#define COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "gpu/vulkan/buildflags.h"

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
#include "mojo/public/cpp/bindings/binder_map.h"
#endif

// START forward declarations for ScopedAllowRacyFeatureListOverrides.
namespace ash {
class AshScopedAllowRacyFeatureListOverrides;
}  // namespace ash

class ChromeShelfControllerTest;
class ShelfContextMenuTest;
// END forward declarations for ScopedAllowRacyFeatureListOverrides.

namespace gpu {
class CommandBufferTaskExecutor;
class SingleTaskSequence;
#if BUILDFLAG(ENABLE_VULKAN)
class VulkanImplementation;
#endif
struct GpuPreferences;
}  // namespace gpu

namespace viz {
class GpuServiceImpl;

// Starts GPU Main and IO threads, and creates a GpuServiceImpl that can be used
// to create a SkiaOutputSurfaceImpl. This isn't a full GPU service
// implementation and should only be used in tests.
class TestGpuServiceHolder : public gpu::GpuInProcessThreadServiceDelegate {
 public:
  class ScopedResetter {
   public:
    ~ScopedResetter() { TestGpuServiceHolder::ResetInstance(); }
  };

  // Don't instantiate FeatureList::ScopedDisallowOverrides when the GPU thread
  // is started. This shouldn't be required but there are existing tests that
  // initialize ScopedFeatureList after TestGpuServiceHolder.
  // TODO(crbug.com/40785850): Fix racy tests and remove this.
  class ScopedAllowRacyFeatureListOverrides {
   public:
    ~ScopedAllowRacyFeatureListOverrides();

   private:
    // Existing allowlisted failures. DO NOT ADD ANYTHING TO THIS LIST! Instead,
    // the test should change so the initialization of ScopedFeatureList happens
    // before TestGpuServiceHolder is created.
    friend class ::ChromeShelfControllerTest;
    friend class ::ShelfContextMenuTest;
    friend class ash::AshScopedAllowRacyFeatureListOverrides;

    ScopedAllowRacyFeatureListOverrides();
  };

  // Exposes a singleton to allow easy sharing of the GpuServiceImpl by
  // different clients (e.g. to share SharedImages via a common
  // SharedImageManager).
  //
  // The instance will parse GpuPreferences from the command line when it is
  // first created (e.g. to allow entire test suite with --use-vulkan).
  //
  // If specific feature flags or GpuPreferences are needed for a specific test,
  // a separate instance of this class can be created.
  //
  // By default the instance created by GetInstance() is destroyed after each
  // gtest completes -- it only applies to gtest because it uses gtest hooks. If
  // this isn't desired call DoNotResetOnTestExit() before first use.
  static TestGpuServiceHolder* GetInstance();

  // Resets the singleton instance, joining the GL thread. This is useful for
  // tests that individually initialize and tear down GL.
  static void ResetInstance();

  // Don't reset global instance on gtest exit. Must be called before
  // GetInstance().
  static void DoNotResetOnTestExit();

  explicit TestGpuServiceHolder(const gpu::GpuPreferences& preferences);

  TestGpuServiceHolder(const TestGpuServiceHolder&) = delete;
  TestGpuServiceHolder& operator=(const TestGpuServiceHolder&) = delete;

  ~TestGpuServiceHolder() override;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_main_thread_task_runner() {
    return gpu_main_thread_.task_runner();
  }

  // Most of |gpu_service_| is not safe to use off of the GPU thread, be careful
  // when accessing this.
  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  gpu::CommandBufferTaskExecutor* task_executor() {
    return main_task_executor_.get();
  }

  void ScheduleGpuMainTask(base::OnceClosure callback);
  void ScheduleCompositorGpuTask(base::OnceClosure callback);

  bool is_vulkan_enabled() {
#if BUILDFLAG(ENABLE_VULKAN)
    return !!vulkan_implementation_;
#else
    return false;
#endif
  }

  scoped_refptr<gpu::SharedContextState>
  GetCompositorGpuThreadSharedContextState();

  // gpu::GpuInProcessThreadServiceDelegate implementation:
  scoped_refptr<gpu::SharedContextState> GetSharedContextState() override;
  scoped_refptr<gl::GLShareGroup> GetShareGroup() override;

 private:
  void InitializeOnGpuThread(const gpu::GpuPreferences& preferences,
                             base::WaitableEvent* completion);
  void DeleteOnGpuThread();

// TODO(crbug.com/40803043): Fuchsia crashes. See details in the crbug.
#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe);
  void BindInterfaceOnGpuThread(const std::string& interface_name,
                                mojo::ScopedMessagePipeHandle interface_pipe);
#endif

  std::optional<base::FeatureList::ScopedDisallowOverrides>
      disallow_feature_overrides_;

  base::Thread gpu_main_thread_;
  base::Thread io_thread_;

  // These should only be created and deleted on the gpu thread.
  std::unique_ptr<GpuServiceImpl> gpu_service_;
  std::unique_ptr<gpu::CommandBufferTaskExecutor> main_task_executor_;
  // This is used to schedule gpu tasks in sequence.
  std::unique_ptr<gpu::SingleTaskSequence> gpu_main_task_sequence_;
  std::unique_ptr<gpu::SingleTaskSequence> compositor_gpu_task_sequence_;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> vulkan_implementation_;
#endif

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
  // Bound interfaces.
  mojo::BinderMap binders_;
#endif
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_GPU_SERVICE_HOLDER_H_
