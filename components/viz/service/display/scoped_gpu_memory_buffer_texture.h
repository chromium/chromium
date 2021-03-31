// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SCOPED_GPU_MEMORY_BUFFER_TEXTURE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SCOPED_GPU_MEMORY_BUFFER_TEXTURE_H_

#include "base/macros.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class ContextProvider;

// ScopedGpuMemoryBufferTexture is a GL texture backed by a GL image and a
// GpuMemoryBuffer, so that it can be used as an overlay.
class VIZ_SERVICE_EXPORT ScopedGpuMemoryBufferTexture {
 public:
  explicit ScopedGpuMemoryBufferTexture(ContextProvider* context_provider,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space);

  ScopedGpuMemoryBufferTexture();
  ~ScopedGpuMemoryBufferTexture();

  ScopedGpuMemoryBufferTexture(ScopedGpuMemoryBufferTexture&& other);
  ScopedGpuMemoryBufferTexture& operator=(ScopedGpuMemoryBufferTexture&& other);

  uint32_t id() const { return gl_id_; }
  uint32_t target() const { return target_; }
  const gfx::Size& size() const { return size_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }

 private:
  void Free();

  // The ContextProvider used to free the texture when this object is destroyed,
  // so it must outlive this object.
  ContextProvider* context_provider_ = nullptr;
  uint32_t gl_id_ = 0;
  uint32_t target_ = 0;
  gfx::Size size_;
  gfx::ColorSpace color_space_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SCOPED_GPU_MEMORY_BUFFER_TEXTURE_H_
