// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_PLATFORM_COLOR_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_PLATFORM_COLOR_H_

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "third_party/skia/include/core/SkTypes.h"

namespace viz {

class PlatformColor {
 private:
  // Returns the most efficient supported format.
  static SharedImageFormat BestSupportedFormat(bool supports_bgra) {
    if (supports_bgra && !SK_B32_SHIFT) {
      return SinglePlaneFormat::kBGRA_8888;
    }

    return SinglePlaneFormat::kRGBA_8888;
  }

 public:
  PlatformColor() = delete;
  PlatformColor(const PlatformColor&) = delete;
  PlatformColor& operator=(const PlatformColor&) = delete;

  // Returns the most efficient supported format for textures that will be
  // software-generated and uploaded via TexImage2D et al.
  static SharedImageFormat BestSupportedTextureFormat(
      const gpu::Capabilities& caps) {
    return BestSupportedFormat(caps.texture_format_bgra8888);
  }

  // Returns the most efficient supported format for textures that will be
  // rastered in the gpu (bound as a framebuffer and drawn to).
  static SharedImageFormat BestSupportedRenderBufferFormat(
      const gpu::Capabilities& caps) {
    return BestSupportedFormat(caps.render_buffer_format_bgra8888);
  }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_PLATFORM_COLOR_H_
