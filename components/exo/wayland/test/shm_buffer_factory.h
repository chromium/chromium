// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_TEST_SHM_BUFFER_FACTORY_H_
#define COMPONENTS_EXO_WAYLAND_TEST_SHM_BUFFER_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/exo/wayland/clients/client_helper.h"
#include "components/exo/wayland/test/test_buffer.h"

namespace exo::wayland::test {

// ShmBufferFactory uses wl_shm_pool to create buffers.
class ShmBufferFactory {
 public:
  ShmBufferFactory();

  ShmBufferFactory(const ShmBufferFactory&) = delete;
  ShmBufferFactory& operator=(const ShmBufferFactory&) = delete;

  ~ShmBufferFactory();

  // `buffer_listener`, if set, will be set on every buffer created from this
  // factory.
  bool Init(wl_shm* shm,
            int32_t pool_size,
            BufferListener* buffer_listener = nullptr);

  // If `stride` is set to -1, the byte size of a row will be used.
  std::unique_ptr<TestBuffer> CreateBuffer(
      int32_t offset,
      int32_t pixel_width,
      int32_t pixel_height,
      int32_t stride = -1,
      uint32_t format = WL_SHM_FORMAT_ARGB8888);

 private:
  std::unique_ptr<wl_shm_pool> shm_pool_;
  raw_ptr<BufferListener> buffer_listener_ = nullptr;
};

}  // namespace exo::wayland::test

#endif  // COMPONENTS_EXO_WAYLAND_TEST_SHM_BUFFER_FACTORY_H_
