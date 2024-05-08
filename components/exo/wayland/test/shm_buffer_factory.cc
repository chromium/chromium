// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/test/shm_buffer_factory.h"

#include "base/memory/platform_shared_memory_handle.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "surface-augmenter-client-protocol.h"

namespace exo::wayland::test {
namespace {

constexpr int32_t kBytesPerPixel = 4;

}  // namespace

ShmBufferFactory::ShmBufferFactory() = default;

ShmBufferFactory::~ShmBufferFactory() = default;

bool ShmBufferFactory::Init(wl_shm* shm,
                            int32_t pool_size,
                            BufferListener* buffer_listener) {
  base::UnsafeSharedMemoryRegion shm_region =
      base::UnsafeSharedMemoryRegion::Create(pool_size);
  if (!shm_region.IsValid()) {
    return false;
  }

  base::subtle::PlatformSharedMemoryRegion platform_shm =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          std::move(shm_region));
  base::subtle::ScopedFDPair fd_pair = platform_shm.PassPlatformHandle();
  shm_pool_.reset(wl_shm_create_pool(shm, fd_pair.fd.get(), pool_size));

  buffer_listener_ = buffer_listener;
  return true;
}

std::unique_ptr<TestBuffer> ShmBufferFactory::CreateBuffer(int32_t offset,
                                                           int32_t pixel_width,
                                                           int32_t pixel_height,
                                                           int32_t stride,
                                                           uint32_t format) {
  if (stride == -1) {
    stride = pixel_width * kBytesPerPixel;
  }

  auto buffer_resource =
      std::unique_ptr<wl_buffer, decltype(&wl_buffer_destroy)>(
          wl_shm_pool_create_buffer(shm_pool_.get(), offset, pixel_width,
                                    pixel_height, stride, format),
          &wl_buffer_destroy);
  auto buffer = std::make_unique<TestBuffer>(std::move(buffer_resource));
  if (buffer_listener_) {
    buffer->SetListener(buffer_listener_);
  }

  return buffer;
}

}  // namespace exo::wayland::test
