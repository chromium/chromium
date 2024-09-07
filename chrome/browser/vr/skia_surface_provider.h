// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_H_
#define CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_H_

#include "base/functional/function_ref.h"
#include "device/vr/gl_bindings.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

class SkSurface;
class SkCanvas;

namespace gfx {
class Size;
}  // namespace gfx

namespace vr {

// Provides a Skia surface to draw textures of UI elements into.
class SkiaSurfaceProvider {
 public:
  class Texture {
   public:
    explicit Texture(sk_sp<SkSurface> surface);
    ~Texture();
    GLuint texture_id() { return texture_id_; }

   private:
    const GLuint texture_id_;
    sk_sp<SkSurface> surface_;
  };

  virtual ~SkiaSurfaceProvider() = default;

  // Creates Texture and initializes it by provided `paint` function using Skia.
  // SkiaSurfaceProvider must outlive returned Texture.
  virtual std::unique_ptr<Texture> CreateTextureWithSkia(
      const gfx::Size& size,
      base::FunctionRef<void(SkCanvas*)> paint) = 0;

 protected:
  std::unique_ptr<Texture> CreateTextureWithSkiaImpl(
      GrDirectContext* gr_context,
      const gfx::Size& size,
      base::FunctionRef<void(SkCanvas*)> paint);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_H_
