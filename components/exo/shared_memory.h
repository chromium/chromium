// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHARED_MEMORY_H_
#define COMPONENTS_EXO_SHARED_MEMORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

namespace exo {
class Buffer;

// Shared memory abstraction. Provides a wrapper around
// base::UnsafeSharedMemoryRegion with functionality to create buffers from the
// memory to use for display.
class SharedMemory {
 public:
  explicit SharedMemory(base::UnsafeSharedMemoryRegion shared_memory_region);
  ~SharedMemory();

  // Creates a buffer from the shared memory. The buffer is created offset bytes
  // into the memory and has width and height as specified. The stride
  // arguments specifies the number of bytes from beginning of one row to the
  // beginning of the next. The format is the pixel format of the buffer and
  // must be one of RGBX_8888, RGBA_8888, BGRX_8888, BGRA_8888.
  std::unique_ptr<Buffer> CreateBuffer(const gfx::Size& size,
                                       gfx::BufferFormat format,
                                       unsigned offset,
                                       int stride);

  size_t GetSize() const;
  bool Resize(const size_t new_size);

 private:
  base::UnsafeSharedMemoryRegion shared_memory_region_;

  DISALLOW_COPY_AND_ASSIGN(SharedMemory);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHARED_MEMORY_H_
