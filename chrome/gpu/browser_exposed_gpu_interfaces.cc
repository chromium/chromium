// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/browser_exposed_gpu_interfaces.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/gpu/chrome_content_gpu_client.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "ash/components/arc/mojom/protected_buffer_manager.mojom.h"
#include "ash/components/arc/mojom/video_decode_accelerator.mojom.h"
#include "ash/components/arc/mojom/video_decoder.mojom.h"
#include "ash/components/arc/mojom/video_encode_accelerator.mojom.h"
#include "ash/components/arc/mojom/video_protected_buffer_allocator.mojom.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_decoder.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"
#include "ash/components/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"
#include "ash/components/arc/video_accelerator/protected_buffer_manager_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) &&
        // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
void CreateArcVideoDecodeAccelerator(
    ChromeContentGpuClient* client,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::PendingReceiver<::arc::mojom::VideoDecodeAccelerator> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecodeAccelerator>(
          gpu_preferences, gpu_workarounds,
          client->GetProtectedBufferManager()),
      std::move(receiver));
}

void CreateArcVideoDecoder(
    ChromeContentGpuClient* client,
    mojo::PendingReceiver<::arc::mojom::VideoDecoder> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<arc::GpuArcVideoDecoder>(
                                  client->GetProtectedBufferManager()),
                              std::move(receiver));
}

void CreateArcVideoEncodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::PendingReceiver<::arc::mojom::VideoEncodeAccelerator> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoEncodeAccelerator>(gpu_preferences,
                                                          gpu_workarounds),
      std::move(receiver));
}

void CreateArcVideoProtectedBufferAllocator(
    ChromeContentGpuClient* client,
    mojo::PendingReceiver<::arc::mojom::VideoProtectedBufferAllocator>
        receiver) {
  auto gpu_arc_video_protected_buffer_allocator =
      arc::GpuArcVideoProtectedBufferAllocator::Create(
          client->GetProtectedBufferManager());
  if (!gpu_arc_video_protected_buffer_allocator)
    return;
  mojo::MakeSelfOwnedReceiver(
      std::move(gpu_arc_video_protected_buffer_allocator), std::move(receiver));
}

void CreateProtectedBufferManager(
    ChromeContentGpuClient* client,
    mojo::PendingReceiver<::arc::mojom::ProtectedBufferManager> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcProtectedBufferManagerProxy>(
          client->GetProtectedBufferManager()),
      std::move(receiver));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) &&
        // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

}  // namespace

void ExposeChromeGpuInterfacesToBrowser(
    ChromeContentGpuClient* client,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  binders->Add<::arc::mojom::VideoDecodeAccelerator>(
      base::BindRepeating(&CreateArcVideoDecodeAccelerator, client,
                          gpu_preferences, gpu_workarounds),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<::arc::mojom::VideoDecoder>(
      base::BindRepeating(&CreateArcVideoDecoder, client),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<::arc::mojom::VideoEncodeAccelerator>(
      base::BindRepeating(&CreateArcVideoEncodeAccelerator, gpu_preferences,
                          gpu_workarounds),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<::arc::mojom::VideoProtectedBufferAllocator>(
      base::BindRepeating(&CreateArcVideoProtectedBufferAllocator, client),
      base::SingleThreadTaskRunner::GetCurrentDefault());
  binders->Add<::arc::mojom::ProtectedBufferManager>(
      base::BindRepeating(&CreateProtectedBufferManager, client),
      base::SingleThreadTaskRunner::GetCurrentDefault());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) &&
        // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
}
