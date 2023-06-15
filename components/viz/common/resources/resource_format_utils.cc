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

unsigned int GLDataType(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_type[] = {
      GL_UNSIGNED_BYTE,                    // RGBA_8888
      GL_UNSIGNED_SHORT_4_4_4_4,           // RGBA_4444
      GL_UNSIGNED_BYTE,                    // BGRA_8888
      GL_UNSIGNED_BYTE,                    // ALPHA_8
      GL_UNSIGNED_BYTE,                    // LUMINANCE_8
      GL_UNSIGNED_SHORT_5_6_5,             // RGB_565,
      GL_UNSIGNED_SHORT_5_6_5,             // BGR_565
      GL_UNSIGNED_BYTE,                    // ETC1
      GL_UNSIGNED_BYTE,                    // RED_8
      GL_UNSIGNED_BYTE,                    // RG_88
      GL_HALF_FLOAT_OES,                   // LUMINANCE_F16
      GL_HALF_FLOAT_OES,                   // RGBA_F16
      GL_UNSIGNED_SHORT,                   // R16_EXT
      GL_UNSIGNED_SHORT,                   // RG16_EXT
      GL_UNSIGNED_BYTE,                    // RGBX_8888
      GL_UNSIGNED_BYTE,                    // BGRX_8888
      GL_UNSIGNED_INT_2_10_10_10_REV_EXT,  // RGBA_1010102
      GL_UNSIGNED_INT_2_10_10_10_REV_EXT,  // BGRA_1010102
      GL_ZERO,                             // YVU_420
      GL_ZERO,                             // YUV_420_BIPLANAR
      GL_ZERO,                             // YUVA_420_TRIPLANAR
      GL_ZERO,                             // P010
  };
  static_assert(std::size(format_gl_data_type) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_type does not handle all cases.");

  return format_gl_data_type[format];
}

unsigned int GLDataFormat(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_format[] = {
      GL_RGBA,       // RGBA_8888
      GL_RGBA,       // RGBA_4444
      GL_BGRA_EXT,   // BGRA_8888
      GL_ALPHA,      // ALPHA_8
      GL_LUMINANCE,  // LUMINANCE_8
      GL_RGB,        // RGB_565
      GL_RGB,        // BGR_565
      GL_RGB,        // ETC1
      GL_RED_EXT,    // RED_8
      GL_RG_EXT,     // RG_88
      GL_LUMINANCE,  // LUMINANCE_F16
      GL_RGBA,       // RGBA_F16
      GL_RED_EXT,    // R16_EXT
      GL_RG_EXT,     // RG16_EXT
      GL_RGB,        // RGBX_8888
      GL_RGB,        // BGRX_8888
      GL_RGBA,       // RGBA_1010102
      GL_RGBA,       // BGRA_1010102
      GL_ZERO,       // YVU_420
      GL_ZERO,       // YUV_420_BIPLANAR
      GL_ZERO,       // YUVA_420_TRIPLANAR
      GL_ZERO,       // P010
  };
  static_assert(std::size(format_gl_data_format) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_format does not handle all cases.");

  return format_gl_data_format[format];
}

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

#if BUILDFLAG(ENABLE_VULKAN)
namespace {
VkFormat ToVkFormatInternal(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;  // or VK_FORMAT_R8G8B8A8_SRGB
    case RGBA_4444:
      return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case BGRA_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case RED_8:
      return VK_FORMAT_R8_UNORM;
    case RGB_565:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case BGR_565:
      return VK_FORMAT_B5G6R5_UNORM_PACK16;
    case RG_88:
      return VK_FORMAT_R8G8_UNORM;
    case RGBA_F16:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case R16_EXT:
      return VK_FORMAT_R16_UNORM;
    case RG16_EXT:
      return VK_FORMAT_R16G16_UNORM;
    case RGBX_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case BGRX_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case RGBA_1010102:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case BGRA_1010102:
      return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case ALPHA_8:
      return VK_FORMAT_R8_UNORM;
    case LUMINANCE_8:
      return VK_FORMAT_R8_UNORM;
    case YVU_420:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
    case YUV_420_BIPLANAR:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case ETC1:
      return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case LUMINANCE_F16:
      return VK_FORMAT_R16_SFLOAT;
    case P010:
      return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
    case YUVA_420_TRIPLANAR:
      break;
  }
  return VK_FORMAT_UNDEFINED;
}
}  // namespace

bool HasVkFormat(ResourceFormat format) {
  return ToVkFormatInternal(format) != VK_FORMAT_UNDEFINED;
}

VkFormat ToVkFormat(ResourceFormat format) {
  auto result = ToVkFormatInternal(format);
  DCHECK_NE(result, VK_FORMAT_UNDEFINED) << "Unsupported format " << format;
  return result;
}
#endif

}  // namespace viz
