// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SCOPED_RENDER_PASS_TEXTURE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SCOPED_RENDER_PASS_TEXTURE_H_

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class ContextProvider;

// ScopedRenderPassTexture is resource used inside the same GL context and will
// not being sent into another process. So no need to create fence and mailbox
// for these resources.
class VIZ_SERVICE_EXPORT ScopedRenderPassTexture {
 public:
  ScopedRenderPassTexture();
  ScopedRenderPassTexture(ContextProvider* context_provider,
                          const gfx::Size& size,
                          ResourceFormat format,
                          const gfx::ColorSpace& color_space,
                          bool mipmap);
  ~ScopedRenderPassTexture();

  ScopedRenderPassTexture(ScopedRenderPassTexture&& other);
  ScopedRenderPassTexture& operator=(ScopedRenderPassTexture&& other);
  void BindForSampling();

  GLuint id() const { return gl_id_; }
  const gfx::Size& size() const { return size_; }
  bool mipmap() const { return mipmap_; }
  const gfx::ColorSpace& color_space() const { return color_space_; }
  void set_generate_mipmap() { mipmap_state_ = GENERATE; }

 private:
  void Free();

  ContextProvider* context_provider_ = nullptr;
  // The GL texture id.
  GLuint gl_id_ = 0;
  // Size of the resource in pixels.
  gfx::Size size_;
  // When true, and immutable textures are used, this specifies to
  // generate mipmaps at powers of 2.
  bool mipmap_ = false;
  // TODO(xing.xu): Remove this and set the color space when we draw the
  // CompositorRenderPassDrawQuad.
  gfx::ColorSpace color_space_;
  enum MipmapState { INVALID, GENERATE, VALID };
  MipmapState mipmap_state_ = INVALID;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SCOPED_RENDER_PASS_TEXTURE_H_
