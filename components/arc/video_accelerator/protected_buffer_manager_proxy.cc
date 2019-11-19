// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/protected_buffer_manager_proxy.h"

#include "components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "components/arc/video_accelerator/protected_buffer_manager.h"
#include "media/gpu/macros.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace arc {

GpuArcProtectedBufferManagerProxy::GpuArcProtectedBufferManagerProxy(
    scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager)
    : protected_buffer_manager_(std::move(protected_buffer_manager)) {
  DCHECK(protected_buffer_manager_);
}

GpuArcProtectedBufferManagerProxy::~GpuArcProtectedBufferManagerProxy() {}

void GpuArcProtectedBufferManagerProxy::GetProtectedSharedMemoryFromHandle(
    mojo::ScopedHandle dummy_handle,
    GetProtectedSharedMemoryFromHandleCallback callback) {
  base::ScopedFD unwrapped_fd = UnwrapFdFromMojoHandle(std::move(dummy_handle));

  auto region = protected_buffer_manager_->GetProtectedSharedMemoryRegionFor(
      std::move(unwrapped_fd));
  // This ScopedFDPair dance is chromeos-specific.
  base::subtle::ScopedFDPair fd_pair = region.PassPlatformHandle();
  std::move(callback).Run(mojo::WrapPlatformFile(fd_pair.fd.release()));
}

}  // namespace arc
