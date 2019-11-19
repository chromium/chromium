// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_PLATFORM_COLOR_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_PLATFORM_COLOR_H_

#include "base/logging.h"
#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace viz {

class PlatformColor {
 public:
  enum SourceDataFormat { SOURCE_FORMAT_RGBA8, SOURCE_FORMAT_BGRA8 };

  static SourceDataFormat Format() {
    return SK_B32_SHIFT ? SOURCE_FORMAT_RGBA8 : SOURCE_FORMAT_BGRA8;
  }

  // Returns the most efficient supported format for textures that will be
  // software-generated and uploaded via TexImage2D et al.
  static ResourceFormat BestSupportedTextureFormat(
      const gpu::Capabilities& caps) {
    switch (Format()) {
      case SOURCE_FORMAT_BGRA8:
        return (caps.texture_format_bgra8888) ? BGRA_8888 : RGBA_8888;
      case SOURCE_FORMAT_RGBA8:
        return RGBA_8888;
    }
    NOTREACHED();
    return RGBA_8888;
  }

  // Returns the most efficient supported format for textures that will be
  // rastered in the gpu (bound as a framebuffer and drawn to).
  static ResourceFormat BestSupportedRenderBufferFormat(
      const gpu::Capabilities& caps) {
    switch (Format()) {
      case SOURCE_FORMAT_BGRA8:
        return (caps.render_buffer_format_bgra8888) ? BGRA_8888 : RGBA_8888;
      case SOURCE_FORMAT_RGBA8:
        return RGBA_8888;
    }
    NOTREACHED();
    return RGBA_8888;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformColor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_PLATFORM_COLOR_H_
