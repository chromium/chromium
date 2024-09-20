// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gpu_service_holder.h"

#include <tuple>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_util.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/dawn_thread_dispatch_proc.h>
#include <dawn/native/DawnNative.h>
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

namespace viz {

namespace {

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
namespace {
constexpr int kGpuProcessHostId = 1;
}  // namespace
#endif

base::Lock& GetLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

// We expect GetLock() to be acquired before accessing these variables.
TestGpuServiceHolder* g_holder = nullptr;
bool g_disallow_feature_list_overrides = true;
bool g_should_register_listener = true;
bool g_registered_listener = false;

class InstanceResetter
    : public testing::EmptyTestEventListener,
      public base::test::TaskEnvironment::DestructionObserver {
 public:
  InstanceResetter() {
    base::test::TaskEnvironment::AddDestructionObserver(this);
  }

  InstanceResetter(const InstanceResetter&) = delete;
  InstanceResetter& operator=(const InstanceResetter&) = delete;

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

TestGpuServiceHolder::ScopedAllowRacyFeatureListOverrides::
    ScopedAllowRacyFeatureListOverrides() {
  base::AutoLock locked(GetLock());

  // This must be called before GetInstance() is ever called.
  DCHECK(!g_holder);
  DCHECK(g_disallow_feature_list_overrides);
  g_disallow_feature_list_overrides = false;
}

TestGpuServiceHolder::ScopedAllowRacyFeatureListOverrides::
    ~ScopedAllowRacyFeatureListOverrides() {
  base::AutoLock locked(GetLock());

  DCHECK(!g_disallow_feature_list_overrides);
  g_disallow_feature_list_overrides = true;
}

TestGpuServiceHolder::TestGpuServiceHolder(
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_main_thread_("GPUMainThread"), io_thread_("GPUIOThread") {
  if (g_disallow_feature_list_overrides) {
    disallow_feature_overrides_.emplace(
        "FeatureList overrides must happen before the GPU service thread has "
        "been started.");
  }

#if BUILDFLAG(USE_DAWN) || BUILDFLAG(SKIA_USE_DAWN)
  // The test will run both service and client in the same process, so we need
  // to set dawn procs for both.
  dawnProcSetProcs(&dawnThreadDispatchProcTable);

  // Use the native procs as default procs for all threads. It will be used
  // for GPU service side threads.
  dawnProcSetDefaultThreadProcs(&dawn::native::GetProcs());
#endif

  base::Thread::Options gpu_thread_options;
#if BUILDFLAG(IS_OZONE)
  gpu_thread_options.message_pump_type = ui::OzonePlatform::GetInstance()
                                             ->GetPlatformProperties()
                                             .message_pump_type_for_gpu;
#endif

    CHECK(gpu_main_thread_.StartWithOptions(std::move(gpu_thread_options)));
    CHECK(io_thread_.Start());

    base::WaitableEvent completion;
    gpu_main_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&TestGpuServiceHolder::InitializeOnGpuThread,
                       base::Unretained(this), gpu_preferences, &completion));
    completion.Wait();

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
    if (auto* gpu_platform_support_host =
            ui::OzonePlatform::GetInstance()->GetGpuPlatformSupportHost()) {
      auto interface_binder = base::BindRepeating(
          &TestGpuServiceHolder::BindInterface, base::Unretained(this));
      gpu_platform_support_host->OnGpuServiceLaunched(
          kGpuProcessHostId, interface_binder, base::DoNothing());
    }
#endif
}

TestGpuServiceHolder::~TestGpuServiceHolder() {
#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
  if (auto* gpu_platform_support_host =
          ui::OzonePlatform::GetInstance()->GetGpuPlatformSupportHost()) {
    gpu_platform_support_host->OnChannelDestroyed(kGpuProcessHostId);
  }
#endif

  // Ensure members created on GPU thread are destroyed there too.
  gpu_main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::DeleteOnGpuThread,
                                base::Unretained(this)));
  gpu_main_thread_.Stop();
  io_thread_.Stop();
}

scoped_refptr<gpu::SharedContextState>
TestGpuServiceHolder::GetCompositorGpuThreadSharedContextState() {
  if (gpu_service_->compositor_gpu_thread()) {
    return gpu_service_->compositor_gpu_thread()->GetSharedContextState();
  }

  return GetSharedContextState();
}

scoped_refptr<gpu::SharedContextState>
TestGpuServiceHolder::GetSharedContextState() {
  return gpu_service_->GetContextState();
}

scoped_refptr<gl::GLShareGroup> TestGpuServiceHolder::GetShareGroup() {
  return gpu_service_->share_group();
}

void TestGpuServiceHolder::ScheduleGpuMainTask(base::OnceClosure callback) {
  DCHECK(gpu_main_task_sequence_);
  gpu_main_task_sequence_->ScheduleTask(std::move(callback), {});
}

void TestGpuServiceHolder::ScheduleCompositorGpuTask(
    base::OnceClosure callback) {
  if (compositor_gpu_task_sequence_)
    compositor_gpu_task_sequence_->ScheduleTask(std::move(callback), {});
  else
    ScheduleGpuMainTask(std::move(callback));
}

void TestGpuServiceHolder::InitializeOnGpuThread(
    const gpu::GpuPreferences& gpu_preferences,
    base::WaitableEvent* completion) {
  DCHECK(gpu_main_thread_.task_runner()->BelongsToCurrentThread());

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
  ui::OzonePlatform::GetInstance()->AddInterfaces(&binders_);
#endif

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
    NOTREACHED_IN_MIGRATION();
#endif
  }

  // Always enable gpu and oop raster, regardless of platform and blocklist.
  // The latter instructs GpuChannelManager::GetSharedContextState to create a
  // GrContext, which is required by SkiaRenderer as well as OOP-R.
  gpu::GPUInfo gpu_info;
  gpu::GpuFeatureInfo gpu_feature_info = gpu::ComputeGpuFeatureInfo(
      gpu_info, gpu_preferences, base::CommandLine::ForCurrentProcess(),
      /*needs_more_info=*/nullptr);
  gpu_feature_info.status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;

  GpuServiceImpl::InitParams init_params;
  init_params.io_runner = io_thread_.task_runner();
#if BUILDFLAG(ENABLE_VULKAN)
  init_params.vulkan_implementation = vulkan_implementation_.get();
#endif
  init_params.exit_callback = base::DoNothing();

  if (gpu_preferences.gr_context_type == gpu::GrContextType::kGraphiteDawn) {
#if BUILDFLAG(SKIA_USE_DAWN)
    init_params.dawn_context_provider = gpu::DawnContextProvider::Create(
        gpu_preferences, gpu::DawnContextProvider::DefaultValidateAdapterFn,
        gpu::GpuDriverBugWorkarounds(
            gpu_feature_info.enabled_gpu_driver_bug_workarounds));
    CHECK(init_params.dawn_context_provider);
#else
    NOTREACHED();
#endif
  }

  // TODO(rivr): Investigate why creating a GPUInfo and GpuFeatureInfo from
  // the command line causes the test SkiaOutputSurfaceImplTest.SubmitPaint to
  // fail on Android.
  gpu_service_ = std::make_unique<GpuServiceImpl>(
      gpu_preferences, gpu_info, gpu_feature_info,
      /*gpu_info_for_hardware_gpu=*/gpu::GPUInfo(),
      /*gpu_feature_info_for_hardware_gpu=*/gpu::GpuFeatureInfo(),
      /*gpu_extra_info=*/gfx::GpuExtraInfo(), std::move(init_params));

  // Use a disconnected mojo remote for GpuHost, we don't need to receive any
  // messages.
  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service_->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*scheduler=*/nullptr, /*shutdown_event=*/nullptr);

  main_task_executor_ = std::make_unique<gpu::GpuInProcessThreadService>(
      this, gpu_main_thread_.task_runner(), gpu_service_->GetGpuScheduler(),
      gpu_service_->sync_point_manager(),
      gpu_service_->gpu_channel_manager()
          ->default_offscreen_surface()
          ->GetFormat(),
      gpu_service_->gpu_feature_info(),
      gpu_service_->gpu_channel_manager()->gpu_preferences(),
      gpu_service_->shared_image_manager(),
      gpu_service_->gpu_channel_manager()->program_cache());

  // TODO(weiliangc): Since SkiaOutputSurface should not depend on command
  // buffer, the |gpu_main_task_sequence_| should be coming from
  // SkiaOutputSurfaceDependency. SkiaOutputSurfaceDependency cannot be
  // initialized here because the it will not have correct client thread set up
  // when unit tests are running in parallel.
  gpu_main_task_sequence_ = main_task_executor_->CreateSequence();

  if (gpu_service_->compositor_gpu_thread()) {
    compositor_gpu_task_sequence_ = std::make_unique<gpu::SchedulerSequence>(
        gpu_service_->GetGpuScheduler(),
        gpu_service_->compositor_gpu_task_runner());
  }

  completion->Signal();
}

void TestGpuServiceHolder::DeleteOnGpuThread() {
  main_task_executor_.reset();
  gpu_main_task_sequence_.reset();
  compositor_gpu_task_sequence_.reset();
  gpu_service_.reset();
}

#if BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)
void TestGpuServiceHolder::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  // The interfaces must be bound on the gpu to ensure the mojo calls happen
  // on the correct sequence (same happens when the browser runs with a real
  // gpu service).
  gpu_main_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestGpuServiceHolder::BindInterfaceOnGpuThread,
                                base::Unretained(this), interface_name,
                                std::move(interface_pipe)));
}

void TestGpuServiceHolder::BindInterfaceOnGpuThread(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  mojo::GenericPendingReceiver receiver =
      mojo::GenericPendingReceiver(interface_name, std::move(interface_pipe));
  CHECK(binders_.TryBind(&receiver))
      << "Unable to find mojo interface " << interface_name;
}
#endif  // BUILDFLAG(IS_OZONE) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace viz
