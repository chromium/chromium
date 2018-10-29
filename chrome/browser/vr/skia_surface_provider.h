// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_H_
#define CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_H_

#include "chrome/browser/vr/gl_bindings.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/gpu/GrContext.h"

class SkSurface;

namespace gfx {
class Size;
}  // namespace gfx

namespace vr {

// Provides a Skia surface to draw textures of UI elements into.
class SkiaSurfaceProvider {
 public:
  virtual ~SkiaSurfaceProvider() = default;

  // Creates a surface with the specified size.
  virtual sk_sp<SkSurface> MakeSurface(const gfx::Size& size) = 0;
  // Applies all drawing commands and returns the ID of the texture containing
  // the rendered image. If possible, it uses the texture passed
  // by |reuse_texture_id| to draw into.
  virtual GLuint FlushSurface(SkSurface* surface, GLuint reuse_texture_id) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_H_
