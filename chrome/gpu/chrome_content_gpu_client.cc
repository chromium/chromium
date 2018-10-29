// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/chrome_content_gpu_client.h"

#include <string>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/child/child_thread.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "media/cdm/cdm_paths.h"
#include "media/cdm/library_cdm/clear_key_cdm/clear_key_cdm_proxy.h"
#include "third_party/widevine/cdm/buildflags.h"
#if BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)
#include "chrome/gpu/widevine_cdm_proxy_factory.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_CHROMEOS)
#include "components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"
#include "components/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "components/arc/video_accelerator/protected_buffer_manager_proxy.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/binder_registry.h"
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

void ChromeContentGpuClient::InitializeRegistry(
    service_manager::BinderRegistry* registry) {
#if defined(OS_CHROMEOS)
  registry->AddInterface(
      base::Bind(&ChromeContentGpuClient::CreateArcVideoDecodeAccelerator,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(
      base::Bind(&ChromeContentGpuClient::CreateArcVideoEncodeAccelerator,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(
      base::Bind(
          &ChromeContentGpuClient::CreateArcVideoProtectedBufferAllocator,
          base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
  registry->AddInterface(
      base::Bind(&ChromeContentGpuClient::CreateProtectedBufferManager,
                 base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get());
#endif
}

void ChromeContentGpuClient::GpuServiceInitialized(
    const gpu::GpuPreferences& gpu_preferences) {
#if defined(OS_CHROMEOS)
  gpu_preferences_ = gpu_preferences;
  ui::OzonePlatform::GetInstance()
      ->GetSurfaceFactoryOzone()
      ->SetGetProtectedNativePixmapDelegate(
          base::Bind(&arc::ProtectedBufferManager::GetProtectedNativePixmapFor,
                     base::Unretained(protected_buffer_manager_.get())));
#endif

  main_thread_profiler_->SetMainThreadTaskRunner(
      base::ThreadTaskRunnerHandle::Get());
  ThreadProfiler::SetServiceManagerConnectorForChildProcess(
      content::ChildThread::Get()->GetConnector());
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

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
std::unique_ptr<media::CdmProxy> ChromeContentGpuClient::CreateCdmProxy(
    const std::string& cdm_guid) {
  if (cdm_guid == media::kClearKeyCdmGuid)
    return std::make_unique<media::ClearKeyCdmProxy>();

#if BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)
  if (cdm_guid == kWidevineCdmGuid)
    return CreateWidevineCdmProxy();
#endif  // BUILDFLAG(ENABLE_WIDEVINE) && defined(OS_WIN)

  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_CHROMEOS)
void ChromeContentGpuClient::CreateArcVideoDecodeAccelerator(
    ::arc::mojom::VideoDecodeAcceleratorRequest request) {
  mojo::MakeStrongBinding(std::make_unique<arc::GpuArcVideoDecodeAccelerator>(
                              gpu_preferences_, protected_buffer_manager_),
                          std::move(request));
}

void ChromeContentGpuClient::CreateArcVideoEncodeAccelerator(
    ::arc::mojom::VideoEncodeAcceleratorRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<arc::GpuArcVideoEncodeAccelerator>(gpu_preferences_),
      std::move(request));
}

void ChromeContentGpuClient::CreateArcVideoProtectedBufferAllocator(
    ::arc::mojom::VideoProtectedBufferAllocatorRequest request) {
  auto gpu_arc_video_protected_buffer_allocator =
      arc::GpuArcVideoProtectedBufferAllocator::Create(
          protected_buffer_manager_);
  if (!gpu_arc_video_protected_buffer_allocator)
    return;
  mojo::MakeStrongBinding(std::move(gpu_arc_video_protected_buffer_allocator),
                          std::move(request));
}

void ChromeContentGpuClient::CreateProtectedBufferManager(
    ::arc::mojom::ProtectedBufferManagerRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<arc::GpuArcProtectedBufferManagerProxy>(
          protected_buffer_manager_),
      std::move(request));
}
#endif
