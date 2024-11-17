// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/profiler/chrome_thread_profiler_client.h"
#include "chrome/common/profiler/thread_profiler_configuration.h"
#include "chrome/common/profiler/unwind_util.h"
#include "chrome/gpu/browser_exposed_gpu_interfaces.h"
#include "components/heap_profiling/in_process/heap_profiler_controller.h"
#include "components/metrics/call_stacks/call_stack_profile_builder.h"
#include "components/sampling_profiler/process_type.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "media/media_buildflags.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "ui/ozone/public/ozone_platform.h"         // nogncheck
#include "ui/ozone/public/surface_factory_ozone.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#include "chromeos/components/cdm_factory_daemon/mojom/browser_cdm_factory.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

ChromeContentGpuClient::ChromeContentGpuClient() {
  sampling_profiler::ThreadProfiler::SetClient(
      std::make_unique<ChromeThreadProfilerClient>());

  // The profiler can't start before the sandbox is initialized on
  // ChromeOS due to ChromeOS's sandbox initialization code's use of
  // AssertSingleThreaded().
#if !BUILDFLAG(IS_CHROMEOS)
  main_thread_profiler_ =
      sampling_profiler::ThreadProfiler::CreateAndStartOnMainThread();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  protected_buffer_manager_ = new arc::ProtectedBufferManager();
#endif
}

ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::GpuServiceInitialized() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->SetGetProtectedNativePixmapDelegate(base::BindRepeating(
          &arc::ProtectedBufferManager::GetProtectedNativePixmapFor,
          base::Unretained(protected_buffer_manager_.get())));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
  content::ChildThread::Get()->BindHostReceiver(
      chromeos::ChromeOsCdmFactory::GetBrowserCdmFactoryReceiver());
#endif  // BUILDFLAG(IS_CHROMEOS)

  // This doesn't work in single-process mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    const auto* heap_profiler_controller =
        heap_profiling::HeapProfilerController::GetInstance();
    // The HeapProfilerController should have been created in
    // ChromeMainDelegate::PostEarlyInitialization.
    CHECK(heap_profiler_controller);
    if (ThreadProfilerConfiguration::Get()
            ->IsProfilerEnabledForCurrentProcess() ||
        heap_profiler_controller->IsEnabled()) {
      sampling_profiler::ThreadProfiler::SetMainThreadTaskRunner(
          base::SingleThreadTaskRunner::GetCurrentDefault());

      mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector;
      content::ChildThread::Get()->BindHostReceiver(
          collector.InitWithNewPipeAndPassReceiver());
      metrics::CallStackProfileBuilder::
          SetParentProfileCollectorForChildProcess(std::move(collector));
    }
  }
}

void ChromeContentGpuClient::ExposeInterfacesToBrowser(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
  // NOTE: Do not add binders directly within this method. Instead, modify the
  // definition of |ExposeChromeGpuInterfacesToBrowser()|, as this ensures
  // security review coverage.
  ExposeChromeGpuInterfacesToBrowser(this, gpu_preferences, gpu_workarounds,
                                     binders);
}

void ChromeContentGpuClient::PostSandboxInitialized() {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(!main_thread_profiler_);
  main_thread_profiler_ =
      sampling_profiler::ThreadProfiler::CreateAndStartOnMainThread();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void ChromeContentGpuClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_task_runner) {
  io_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&sampling_profiler::ThreadProfiler::StartOnChildThread,
                     sampling_profiler::ProfilerThreadType::kIo));
}

void ChromeContentGpuClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&sampling_profiler::ThreadProfiler::StartOnChildThread,
                     sampling_profiler::ProfilerThreadType::kCompositor));
  // Enable stack sampling for tracing.
  // We pass in CreateCoreUnwindersFactory here since it lives in the chrome/
  // layer while TracingSamplerProfiler is outside of chrome/.
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&tracing::TracingSamplerProfiler::
                                    CreateOnChildThreadWithCustomUnwinders,
                                base::BindRepeating(
                                    &CreateCoreUnwindersFactory)));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
scoped_refptr<arc::ProtectedBufferManager>
ChromeContentGpuClient::GetProtectedBufferManager() {
  return protected_buffer_manager_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
