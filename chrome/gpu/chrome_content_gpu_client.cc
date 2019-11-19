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
#include "chrome/gpu/browser_exposed_gpu_interfaces.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_CDM_PROXY)
#include "media/cdm/cdm_paths.h"
#include "media/cdm/library_cdm/clear_key_cdm/clear_key_cdm_proxy.h"
#include "third_party/widevine/cdm/buildflags.h"
#if BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)
#include "chrome/gpu/widevine_cdm_proxy_factory.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

#if defined(OS_CHROMEOS)
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

ChromeContentGpuClient::ChromeContentGpuClient()
    : main_thread_profiler_(ThreadProfiler::CreateAndStartOnMainThread()) {
#if defined(OS_CHROMEOS)
  protected_buffer_manager_ = new arc::ProtectedBufferManager();
#endif
}

ChromeContentGpuClient::~ChromeContentGpuClient() {}

void ChromeContentGpuClient::GpuServiceInitialized() {
#if defined(OS_CHROMEOS)
  ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->SetGetProtectedNativePixmapDelegate(base::BindRepeating(
          &arc::ProtectedBufferManager::GetProtectedNativePixmapFor,
          base::Unretained(protected_buffer_manager_.get())));
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
    mojo::BinderMap* binders) {
  // NOTE: Do not add binders directly within this method. Instead, modify the
  // definition of |ExposeChromeGpuInterfacesToBrowser()|, as this ensures
  // security review coverage.
  ExposeChromeGpuInterfacesToBrowser(this, gpu_preferences, binders);
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

#if BUILDFLAG(ENABLE_CDM_PROXY)
std::unique_ptr<media::CdmProxy> ChromeContentGpuClient::CreateCdmProxy(
    const base::Token& cdm_guid) {
  if (cdm_guid == media::kClearKeyCdmGuid)
    return std::make_unique<media::ClearKeyCdmProxy>();

#if BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)
  if (cdm_guid == kWidevineCdmGuid)
    return CreateWidevineCdmProxy();
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)

  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)

#if defined(OS_CHROMEOS)
scoped_refptr<arc::ProtectedBufferManager>
ChromeContentGpuClient::GetProtectedBufferManager() {
  return protected_buffer_manager_;
}
#endif  // defined(OS_CHROMEOS)
