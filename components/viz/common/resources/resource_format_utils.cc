// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource_format_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include <ostream>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "ui/gfx/buffer_types.h"

namespace viz {

unsigned int TextureStorageFormat(ResourceFormat format,
                                  bool use_angle_rgbx_format) {
  switch (format) {
    case RGBA_8888:
      return GL_RGBA8_OES;
    case BGRA_8888:
      return GL_BGRA8_EXT;
    case RGBA_F16:
      return GL_RGBA16F_EXT;
    case RGBA_4444:
      return GL_RGBA4;
    case ALPHA_8:
      return GL_ALPHA8_EXT;
    case LUMINANCE_8:
      return GL_LUMINANCE8_EXT;
    case BGR_565:
    case RGB_565:
      return GL_RGB565;
    case RED_8:
      return GL_R8_EXT;
    case RG_88:
      return GL_RG8_EXT;
    case LUMINANCE_F16:
      return GL_LUMINANCE16F_EXT;
    case R16_EXT:
      return GL_R16_EXT;
    case RG16_EXT:
      return GL_RG16_EXT;
    case RGBX_8888:
    case BGRX_8888:
      return use_angle_rgbx_format ? GL_RGBX8_ANGLE : GL_RGB8_OES;
    case ETC1:
      return GL_ETC1_RGB8_OES;
    case P010:
#if BUILDFLAG(IS_APPLE)
      DLOG(ERROR) << "Sampling of P010 resources must be done per-plane.";
#endif
      return GL_RGB10_A2_EXT;
    case RGBA_1010102:
    case BGRA_1010102:
      return GL_RGB10_A2_EXT;
    case YVU_420:
    case YUV_420_BIPLANAR:
#if BUILDFLAG(IS_APPLE)
      DLOG(ERROR) << "Sampling of YUV_420 resources must be done per-plane.";
#endif
      return GL_RGB8_OES;
    case YUVA_420_TRIPLANAR:
#if BUILDFLAG(IS_APPLE)
      DLOG(ERROR) << "Sampling of YUVA_420 resources must be done per-plane.";
#endif
      return GL_RGBA8_OES;
    default:
      break;
  }
  NOTREACHED();
  return GL_RGBA8_OES;
}

}  // namespace viz
