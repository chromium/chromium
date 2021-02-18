// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gpu_service_holder.h"

#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {

namespace {

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// We expect GetLock() to be acquired before accessing these variables.
TestGpuServiceHolder* g_holder = nullptr;
bool g_should_register_listener = true;
bool g_registered_listener = false;

class InstanceResetter
    : public testing::EmptyTestEventListener,
      public base::test::TaskEnvironment::DestructionObserver {
 public:
  InstanceResetter() {
    base::test::TaskEnvironment::AddDestructionObserver(this);
  }

  ~InstanceResetter() override {
    base::test::TaskEnvironment::RemoveDestructionObserver(this);
  }

  // testing::EmptyTestEventListener:
  void OnTestEnd(const testing::TestInfo& test_info) override {
    {
      base::AutoLock locked(GetLock());
      // Make sure the TestGpuServiceHolder instance is not re-created after
      // WillDestroyCurrentTaskEnvironment().
      // Otherwise we'll end up with GPU tasks weirdly running in a different
      // context after the test.
      DCHECK(!(reset_by_task_env && g_holder))
          << "TestGpuServiceHolder was re-created after "
             "base::test::TaskEnvironment was destroyed.";
    }
    reset_by_task_env = false;
    TestGpuServiceHolder::ResetInstance();
  }

  // base::test::TaskEnvironment::DestructionObserver:
  void WillDestroyCurrentTaskEnvironment() override {
    reset_by_task_env = true;
    TestGpuServiceHolder::ResetInstance();
  }

 private:
  bool reset_by_task_env = false;

  DISALLOW_COPY_AND_ASSIGN(InstanceResetter);
};

}  // namespace

// static
TestGpuServiceHolder* TestGpuServiceHolder::GetInstance() {
  base::AutoLock locked(GetLock());

  // Make sure the global TestGpuServiceHolder is delete after each test. The
  // listener will always be registered with gtest even if gtest isn't
  // otherwised used. This should do nothing in the non-gtest case.
  if (!g_registered_listener && g_should_register_listener) {
    g_registered_listener = true;
    testing::TestEventListeners& listeners =
        testing::UnitTest::GetInstance()->listeners();
    // |listeners| assumes ownership of InstanceResetter.
    listeners.Append(new InstanceResetter);
  }

  // Make sure the global TestGpuServiceHolder is deleted at process exit.
  static bool registered_cleanup = false;
  if (!registered_cleanup) {
    registered_cleanup = true;
    base::AtExitManager::RegisterTask(
        base::BindOnce(&TestGpuServiceHolder::ResetInstance));
  }

  if (!g_holder) {
    g_holder = new TestGpuServiceHolder(gpu::gles2::ParseGpuPreferences(
        base::CommandLine::ForCurrentProcess()));
  }
  return g_holder;
}

// static
void TestGpuServiceHolder::ResetInstance() {
  base::AutoLock locked(GetLock());
  if (g_holder) {
    delete g_holder;
    g_holder = nullptr;
  }
}

// static
void TestGpuServiceHolder::DoNotResetOnTestExit() {
  base::AutoLock locked(GetLock());

  // This must be called before GetInstance() is ever called.
  DCHECK(!g_registered_listener);
  g_should_register_listener = false;
}

TestGpuServiceHolder::TestGpuServiceHolder(
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_thread_("GPUMainThread"), io_thread_("GPUIOThread") {
  base::Thread::Options gpu_thread_options;
#if defined(USE_OZONE)
  if (features::IsUsingOzonePlatform()) {
    gpu_thread_options.message_pump_type = ui::OzonePlatform::GetInstance()
                                               ->GetPlatformProperties()
                                               .message_pump_type_for_gpu;
  }
#endif

  CHECK(gpu_thread_.StartWithOptions(gpu_thread_options));
  CHECK(io_thread_.Start());

  base::WaitableEvent completion;
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TestGpuServiceHolder::InitializeOnGpuThread,
                     base::Unretained(this), gpu_preferences, &completion));
  completion.Wait();
}

TestGpuServiceHolder::~TestGpuServiceHolder() {
  // Ensure members created on GPU thread are destroyed there too.
  gpu_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::DeleteOnGpuThread,
                                base::Unretained(this)));
  gpu_thread_.Stop();
  io_thread_.Stop();
}

scoped_refptr<gpu::SharedContextState>
TestGpuServiceHolder::GetSharedContextState() {
  return gpu_service_->GetContextState();
}

scoped_refptr<gl::GLShareGroup> TestGpuServiceHolder::GetShareGroup() {
  return gpu_service_->share_group();
}

void TestGpuServiceHolder::ScheduleGpuTask(base::OnceClosure callback) {
  DCHECK(gpu_task_sequence_);
  gpu_task_sequence_->ScheduleTask(std::move(callback), {});
}

void TestGpuServiceHolder::InitializeOnGpuThread(
    const gpu::GpuPreferences& gpu_preferences,
    base::WaitableEvent* completion) {
  DCHECK(gpu_thread_.task_runner()->BelongsToCurrentThread());

  if (gpu_preferences.use_vulkan != gpu::VulkanImplementationName::kNone) {
#if BUILDFLAG(ENABLE_VULKAN)
    bool use_swiftshader = gpu_preferences.use_vulkan ==
                           gpu::VulkanImplementationName::kSwiftshader;
    vulkan_implementation_ = gpu::CreateVulkanImplementation(use_swiftshader);
    if (!vulkan_implementation_ ||
        !vulkan_implementation_->InitializeVulkanInstance(
            !gpu_preferences.disable_vulkan_surface)) {
      LOG(FATAL) << "Failed to create and initialize Vulkan implementation.";
    }
#else
    NOTREACHED();
#endif
  }

  // Always enable gpu and oop raster, regardless of platform and blocklist.
  // The latter instructs GpuChannelManager::GetSharedContextState to create a
  // GrContext, which is required by SkiaRenderer as well as OOP-R.
  gpu::GPUInfo gpu_info;
  gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfo(
      gpu_info, gpu_preferences, base::CommandLine::ForCurrentProcess(),
      /*needs_more_info=*/nullptr);
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;

  // TODO(sgilhuly): Investigate why creating a GPUInfo and GpuFeatureInfo from
  // the command line causes the test SkiaOutputSurfaceImplTest.SubmitPaint to
  // fail on Android.
  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu::GPUInfo(), /*watchdog_thread=*/nullptr, io_thread_.task_runner(),
      gpu_feature_info, gpu_preferences,
      /*gpu_info_for_hardware_gpu=*/gpu::GPUInfo(),
      /*gpu_feature_info_for_hardware_gpu=*/gpu::GpuFeatureInfo(),
      /*gpu_extra_info=*/gfx::GpuExtraInfo(),
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_implementation_.get(),
#else
      /*vulkan_implementation=*/nullptr,
#endif
      /*exit_callback=*/base::DoNothing());

  // Use a disconnected mojo remote for GpuHost, we don't need to receive any
  // messages.
  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  ignore_result(gpu_host_proxy.InitWithNewPipeAndPassReceiver());
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*shutdown_event=*/nullptr);

  task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      this, gpu_thread_.task_runner(), gpu_service_->GetGpuScheduler(),
      gpu_service_->sync_point_manager(), gpu_service_->mailbox_manager(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(),
      gpu_service_->gpu_channel_manager()->program_cache());

  // TODO(weiliangc): Since SkiaOutputSurface should not depend on command
  // buffer, the |gpu_task_sequence_| should be coming from
  // SkiaOutputSurfaceDependency. SkiaOutputSurfaceDependency cannot be
  // initialized here because the it will not have correct client thread set up
  // when unit tests are running in parallel.
  gpu_task_sequence_ = task_executor_->CreateSequence();

  completion->Signal();
}

void TestGpuServiceHolder::DeleteOnGpuThread() {
  task_executor_.reset();
  gpu_task_sequence_.reset();
  gpu_service_.reset();
}

}  // namespace viz
