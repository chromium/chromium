// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_IMAGE_FACTORY_H_
#define COMPONENTS_VIZ_TEST_TEST_IMAGE_FACTORY_H_

#include "gpu/command_buffer/service/image_factory.h"

namespace viz {

class TestImageFactory : public gpu::ImageFactory {
 public:
  TestImageFactory();
  TestImageFactory(const TestImageFactory&) = delete;
  ~TestImageFactory() override;

  TestImageFactory& operator=(const TestImageFactory&) = delete;

  // Overridden from gpu::ImageFactory:
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id,
      gpu::SurfaceHandle surface_handle) override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_IMAGE_FACTORY_H_
