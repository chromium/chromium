// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource_format_utils.h"

#include <Metal/MTLPixelFormat.h>

#include "base/logging.h"

namespace viz {

unsigned int ToMTLPixelFormat(ResourceFormat format) {
  MTLPixelFormat mtl_pixel_format = MTLPixelFormatInvalid;
  switch (format) {
    case RED_8:
    case ALPHA_8:
    case LUMINANCE_8:
      mtl_pixel_format = MTLPixelFormatR8Unorm;
      break;
    case RG_88:
      mtl_pixel_format = MTLPixelFormatRG8Unorm;
      break;
    case RGBA_8888:
      mtl_pixel_format = MTLPixelFormatRGBA8Unorm;
      break;
    case BGRA_8888:
      mtl_pixel_format = MTLPixelFormatBGRA8Unorm;
      break;
    default:
      DLOG(ERROR) << "Invalid Metal pixel format.";
      break;
  }
  return static_cast<unsigned int>(mtl_pixel_format);
}

}  // namespace viz
