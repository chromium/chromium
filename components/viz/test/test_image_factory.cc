// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_image_factory.h"

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "ui/gl/gl_image_shared_memory.h"

namespace viz {

TestImageFactory::TestImageFactory() = default;

TestImageFactory::~TestImageFactory() = default;

scoped_refptr<gl::GLImage> TestImageFactory::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);

  auto image = base::MakeRefCounted<gl::GLImageSharedMemory>(size);
  if (!image->Initialize(handle.region, handle.id, format, handle.offset,
                         base::checked_cast<size_t>(handle.stride)))
    return nullptr;

  return image;
}

}  // namespace viz
