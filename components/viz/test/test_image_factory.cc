// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_image_factory.h"

namespace viz {

TestImageFactory::TestImageFactory() = default;

TestImageFactory::~TestImageFactory() = default;

#if BUILDFLAG(IS_MAC)
scoped_refptr<gl::GLImage> TestImageFactory::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    const gfx::ColorSpace& color_space,
    gfx::BufferPlane plane,
    int client_id,
    gpu::SurfaceHandle surface_handle) {
  return nullptr;
}
#endif

}  // namespace viz
