// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_PROXY_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_PROXY_H_

#include "components/arc/mojom/protected_buffer_manager.mojom.h"

namespace arc {

class ProtectedBufferManager;

// Manages mojo IPC translation for arc::ProtectedBufferManager.
class GpuArcProtectedBufferManagerProxy
    : public ::arc::mojom::ProtectedBufferManager {
 public:
  explicit GpuArcProtectedBufferManagerProxy(
      scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager);

  ~GpuArcProtectedBufferManagerProxy() override;

  // arc::mojom::ProtectedBufferManager implementation.
  void GetProtectedSharedMemoryFromHandle(
      mojo::ScopedHandle dummy_handle,
      GetProtectedSharedMemoryFromHandleCallback callback) override;

 private:
  scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(GpuArcProtectedBufferManagerProxy);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_PROXY_H_
