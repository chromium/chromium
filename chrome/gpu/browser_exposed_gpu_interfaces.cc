// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/browser_exposed_gpu_interfaces.h"

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/gpu/chrome_content_gpu_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if defined(OS_CHROMEOS)
#include "components/arc/mojom/protected_buffer_manager.mojom.h"
#include "components/arc/mojom/video_decode_accelerator.mojom.h"
#include "components/arc/mojom/video_encode_accelerator.mojom.h"
#include "components/arc/mojom/video_protected_buffer_allocator.mojom.h"
#include "components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"
#include "components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"
#include "components/arc/video_accelerator/gpu_arc_video_protected_buffer_allocator.h"
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "components/arc/video_accelerator/protected_buffer_manager_proxy.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)
void CreateArcVideoDecodeAccelerator(
    ChromeContentGpuClient* client,
    const gpu::GpuPreferences& gpu_preferences,
    mojo::PendingReceiver<::arc::mojom::VideoDecodeAccelerator> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoDecodeAccelerator>(
          gpu_preferences, client->GetProtectedBufferManager()),
      std::move(receiver));
}

void CreateArcVideoEncodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences,
    mojo::PendingReceiver<::arc::mojom::VideoEncodeAccelerator> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<arc::GpuArcVideoEncodeAccelerator>(gpu_preferences),
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
#endif

}  // namespace

void ExposeChromeGpuInterfacesToBrowser(
    ChromeContentGpuClient* client,
    const gpu::GpuPreferences& gpu_preferences,
    mojo::BinderMap* binders) {
#if defined(OS_CHROMEOS)
  binders->Add(base::BindRepeating(&CreateArcVideoDecodeAccelerator, client,
                                   gpu_preferences),
               base::ThreadTaskRunnerHandle::Get());
  binders->Add(
      base::BindRepeating(&CreateArcVideoEncodeAccelerator, gpu_preferences),
      base::ThreadTaskRunnerHandle::Get());
  binders->Add(
      base::BindRepeating(&CreateArcVideoProtectedBufferAllocator, client),
      base::ThreadTaskRunnerHandle::Get());
  binders->Add(base::BindRepeating(&CreateProtectedBufferManager, client),
               base::ThreadTaskRunnerHandle::Get());
#endif
}
