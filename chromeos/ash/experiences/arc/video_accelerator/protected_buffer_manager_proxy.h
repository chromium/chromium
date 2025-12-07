// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_PROXY_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_PROXY_H_

#include "chromeos/ash/experiences/arc/mojom/protected_buffer_manager.mojom.h"

namespace arc {

class ProtectedBufferManager;

// Manages mojo IPC translation for arc::ProtectedBufferManager.
class GpuArcProtectedBufferManagerProxy
    : public ::arc::mojom::ProtectedBufferManager {
 public:
  explicit GpuArcProtectedBufferManagerProxy(
      scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager);

  GpuArcProtectedBufferManagerProxy(const GpuArcProtectedBufferManagerProxy&) =
      delete;
  GpuArcProtectedBufferManagerProxy& operator=(
      const GpuArcProtectedBufferManagerProxy&) = delete;

  ~GpuArcProtectedBufferManagerProxy() override;

  // arc::mojom::ProtectedBufferManager implementation.
  void DeprecatedGetProtectedSharedMemoryFromHandle(
      mojo::ScopedHandle dummy_handle,
      DeprecatedGetProtectedSharedMemoryFromHandleCallback callback) override;
  void GetProtectedSharedMemoryFromHandle(
      mojo::ScopedHandle dummy_handle,
      GetProtectedSharedMemoryFromHandleCallback callback) override;
  void GetProtectedNativePixmapHandleFromHandle(
      mojo::ScopedHandle dummy_handle,
      GetProtectedNativePixmapHandleFromHandleCallback callback) override;
  void IsProtectedNativePixmapHandle(
      mojo::ScopedHandle dummy_handle,
      IsProtectedNativePixmapHandleCallback callback) override;

 private:
  scoped_refptr<arc::ProtectedBufferManager> protected_buffer_manager_;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_VIDEO_ACCELERATOR_PROTECTED_BUFFER_MANAGER_PROXY_H_
