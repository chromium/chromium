// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/gpu/browser_exposed_gpu_interfaces.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#include "chromeos/components/cdm_factory_daemon/mojom/cdm_factory_daemon.mojom.h"
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "ui/ozone/public/ozone_platform.h"         // nogncheck
#include "ui/ozone/public/surface_factory_ozone.h"  // nogncheck
#endif

ChromeContentGpuClient::ChromeContentGpuClient()
    : main_thread_profiler_(ThreadProfiler::CreateAndStartOnMainThread()) {
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

  content::ChildThread::Get()->BindHostReceiver(
      chromeos::ChromeOsCdmFactory::GetCdmFactoryDaemonReceiver());
#endif

  // This doesn't work in single-process mode.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kInProcessGPU)) {
    ThreadProfiler::SetMainThreadTaskRunner(
        base::ThreadTaskRunnerHandle::Get());

    mojo::PendingRemote<metrics::mojom::CallStackProfileCollector> collector;
    content::ChildThread::Get()->BindHostReceiver(
        collector.InitWithNewPipeAndPassReceiver());
    ThreadProfiler::SetCollectorForChildProcess(std::move(collector));
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

void ChromeContentGpuClient::PostIOThreadCreated(
    base::SingleThreadTaskRunner* io_task_runner) {
  io_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&ThreadProfiler::StartOnChildThread,
                                metrics::CallStackProfileParams::IO_THREAD));
}

void ChromeContentGpuClient::PostCompositorThreadCreated(
    base::SingleThreadTaskRunner* task_runner) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&ThreadProfiler::StartOnChildThread,
                     metrics::CallStackProfileParams::COMPOSITOR_THREAD));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
scoped_refptr<arc::ProtectedBufferManager>
ChromeContentGpuClient::GetProtectedBufferManager() {
  return protected_buffer_manager_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
