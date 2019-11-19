// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/resource_format_utils.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/buffer_types.h"

namespace viz {

SkColorType ResourceFormatToClosestSkColorType(bool gpu_compositing,
                                               ResourceFormat format) {
  if (!gpu_compositing) {
    // TODO(crbug.com/986405): Remove this assumption and have clients tag
    // resources with the correct format.
    // In software compositing we lazily use RGBA_8888 throughout the system,
    // but actual pixel encodings are the native skia bit ordering, which can be
    // RGBA or BGRA.
    return kN32_SkColorType;
  }

  switch (format) {
    case RGBA_4444:
      return kARGB_4444_SkColorType;
    case RGBA_8888:
      return kRGBA_8888_SkColorType;
    case BGRA_8888:
      return kBGRA_8888_SkColorType;
    case ALPHA_8:
      return kAlpha_8_SkColorType;
    case RGB_565:
      return kRGB_565_SkColorType;
    case LUMINANCE_8:
      return kGray_8_SkColorType;
    case RGBX_8888:
    case ETC1:
      return kRGB_888x_SkColorType;
    case RGBX_1010102:
    case BGRX_1010102:
      return kRGBA_1010102_SkColorType;

    // YUV images are sampled as RGB.
    case YVU_420:
    case YUV_420_BIPLANAR:
      return kRGB_888x_SkColorType;

    // Use kN32_SkColorType if there is no corresponding SkColorType.
    case RED_8:
      return kGray_8_SkColorType;
    case LUMINANCE_F16:
    case R16_EXT:
    case BGR_565:
    case RG_88:
    case BGRX_8888:
    case P010:
      return kN32_SkColorType;

    case RGBA_F16:
      return kRGBA_F16_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

int BitsPerPixel(ResourceFormat format) {
  switch (format) {
    case RGBA_F16:
      return 64;
    case BGRA_8888:
    case RGBA_8888:
    case RGBX_8888:
    case BGRX_8888:
    case RGBX_1010102:
    case BGRX_1010102:
    case P010:
      return 32;
    case RGBA_4444:
    case RGB_565:
    case LUMINANCE_F16:
    case R16_EXT:
    case BGR_565:
    case RG_88:
      return 16;
    case YVU_420:
    case YUV_420_BIPLANAR:
      return 12;
    case ALPHA_8:
    case LUMINANCE_8:
    case RED_8:
      return 8;
    case ETC1:
      return 4;
  }
  NOTREACHED();
  return 0;
}

bool HasAlpha(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
    case RGBA_4444:
    case BGRA_8888:
    case ALPHA_8:
    case RGBA_F16:
      return true;
    case LUMINANCE_8:
    case RGB_565:
    case BGR_565:
    case ETC1:
    case RED_8:
    case RG_88:
    case LUMINANCE_F16:
    case R16_EXT:
    case RGBX_8888:
    case BGRX_8888:
    case RGBX_1010102:
    case BGRX_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case P010:
      return false;
  }
  NOTREACHED();
  return false;
}

unsigned int GLDataType(ResourceFormat format) {
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_type[] = {
      GL_UNSIGNED_BYTE,                    // RGBA_8888
      GL_UNSIGNED_SHORT_4_4_4_4,           // RGBA_4444
      GL_UNSIGNED_BYTE,                    // BGRA_8888
      GL_UNSIGNED_BYTE,                    // ALPHA_8
      GL_UNSIGNED_BYTE,                    // LUMINANCE_8
      GL_UNSIGNED_SHORT_5_6_5,             // RGB_565,
      GL_ZERO,                             // BGR_565
      GL_UNSIGNED_BYTE,                    // ETC1
      GL_UNSIGNED_BYTE,                    // RED_8
      GL_UNSIGNED_BYTE,                    // RG_88
      GL_HALF_FLOAT_OES,                   // LUMINANCE_F16
      GL_HALF_FLOAT_OES,                   // RGBA_F16
      GL_UNSIGNED_SHORT,                   // R16_EXT
      GL_UNSIGNED_BYTE,                    // RGBX_8888
      GL_ZERO,                             // BGRX_8888
      GL_UNSIGNED_INT_2_10_10_10_REV_EXT,  // RGBX_1010102
      GL_ZERO,                             // BGRX_1010102
      GL_ZERO,                             // YVU_420
      GL_ZERO,                             // YUV_420_BIPLANAR
      GL_ZERO,                             // P010
  };
  static_assert(base::size(format_gl_data_type) == (RESOURCE_FORMAT_MAX + 1),
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
      GL_ZERO,       // BGR_565
      GL_RGB,        // ETC1
      GL_RED_EXT,    // RED_8
      GL_RG_EXT,     // RG_88
      GL_LUMINANCE,  // LUMINANCE_F16
      GL_RGBA,       // RGBA_F16
      GL_RED_EXT,    // R16_EXT
      GL_RGB,        // RGBX_8888
      GL_ZERO,       // BGRX_8888
      GL_RGBA,       // RGBX_1010102
      GL_ZERO,       // BGRX_1010102
      GL_ZERO,       // YVU_420
      GL_ZERO,       // YUV_420_BIPLANAR
      GL_ZERO,       // P010
  };
  static_assert(base::size(format_gl_data_format) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_format does not handle all cases.");

  return format_gl_data_format[format];
}

unsigned int GLInternalFormat(ResourceFormat format) {
  // In GLES2, the internal format must match the texture format. (It no longer
  // is true in GLES3, however it still holds for the BGRA extension.)
  // GL_EXT_texture_norm16 follows GLES3 semantics and only exposes a sized
  // internal format (GL_R16_EXT).
  if (format == R16_EXT)
    return GL_R16_EXT;
  else if (format == RG_88)
    return GL_RG8_EXT;
  else if (format == ETC1)
    return GL_ETC1_RGB8_OES;

  return GLDataFormat(format);
}

unsigned int GLCopyTextureInternalFormat(ResourceFormat format) {
  // In GLES2, valid formats for glCopyTexImage2D are: GL_ALPHA, GL_LUMINANCE,
  // GL_LUMINANCE_ALPHA, GL_RGB, or GL_RGBA.
  // Extensions typically used for glTexImage2D do not also work for
  // glCopyTexImage2D. For instance GL_BGRA_EXT may not be used for
  // anything but gl(Sub)TexImage2D:
  // https://www.khronos.org/registry/gles/extensions/EXT/EXT_texture_format_BGRA8888.txt
  DCHECK_LE(format, RESOURCE_FORMAT_MAX);
  static const GLenum format_gl_data_format[] = {
      GL_RGBA,       // RGBA_8888
      GL_RGBA,       // RGBA_4444
      GL_RGBA,       // BGRA_8888
      GL_ALPHA,      // ALPHA_8
      GL_LUMINANCE,  // LUMINANCE_8
      GL_RGB,        // RGB_565
      GL_ZERO,       // BGR_565
      GL_RGB,        // ETC1
      GL_LUMINANCE,  // RED_8
      GL_RGBA,       // RG_88
      GL_LUMINANCE,  // LUMINANCE_F16
      GL_RGBA,       // RGBA_F16
      GL_LUMINANCE,  // R16_EXT
      GL_RGB,        // RGBX_8888
      GL_RGB,        // BGRX_8888
      GL_ZERO,       // RGBX_1010102
      GL_ZERO,       // BGRX_1010102
      GL_ZERO,       // YVU_420
      GL_ZERO,       // YUV_420_BIPLANAR
      GL_ZERO,       // P010
  };

  static_assert(base::size(format_gl_data_format) == (RESOURCE_FORMAT_MAX + 1),
                "format_gl_data_format does not handle all cases.");

  return format_gl_data_format[format];
}

gfx::BufferFormat BufferFormat(ResourceFormat format) {
  switch (format) {
    case BGRA_8888:
      return gfx::BufferFormat::BGRA_8888;
    case RED_8:
      return gfx::BufferFormat::R_8;
    case R16_EXT:
      return gfx::BufferFormat::R_16;
    case RGBA_4444:
      return gfx::BufferFormat::RGBA_4444;
    case RGBA_8888:
      return gfx::BufferFormat::RGBA_8888;
    case RGBA_F16:
      return gfx::BufferFormat::RGBA_F16;
    case BGR_565:
      return gfx::BufferFormat::BGR_565;
    case RG_88:
      return gfx::BufferFormat::RG_88;
    case RGBX_8888:
      return gfx::BufferFormat::RGBX_8888;
    case BGRX_8888:
      return gfx::BufferFormat::BGRX_8888;
    case RGBX_1010102:
      return gfx::BufferFormat::RGBX_1010102;
    case BGRX_1010102:
      return gfx::BufferFormat::BGRX_1010102;
    case YVU_420:
      return gfx::BufferFormat::YVU_420;
    case YUV_420_BIPLANAR:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case P010:
      return gfx::BufferFormat::P010;
    case ETC1:
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case LUMINANCE_F16:
      // These types not allowed by IsGpuMemoryBufferFormatSupported(), so
      // give a default value that will not be used.
      break;
  }
  return gfx::BufferFormat::RGBA_8888;
}

bool IsResourceFormatCompressed(ResourceFormat format) {
  return format == ETC1;
}

unsigned int TextureStorageFormat(ResourceFormat format) {
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
    case RGBX_8888:
    case ETC1:
      return GL_RGB8_OES;
    case RGBX_1010102:
    case BGRX_1010102:
      return GL_RGB10_A2_EXT;
    case BGR_565:
    case BGRX_8888:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case P010:
      break;
  }
  NOTREACHED();
  return GL_RGBA8_OES;
}

bool IsGpuMemoryBufferFormatSupported(ResourceFormat format) {
  switch (format) {
    case BGRA_8888:
    case RED_8:
    case R16_EXT:
    case RGBA_4444:
    case RGBA_8888:
    case RGBA_F16:
      return true;
    // These formats have no BufferFormat equivalent or are only used
    // for external textures, or have no GL equivalent formats.
    case ETC1:
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case LUMINANCE_F16:
    case BGR_565:
    case RG_88:
    case RGBX_8888:
    case BGRX_8888:
    case RGBX_1010102:
    case BGRX_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case P010:
      return false;
  }
  NOTREACHED();
  return false;
}

bool IsBitmapFormatSupported(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
      return true;
    case RGBA_4444:
    case BGRA_8888:
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case ETC1:
    case RED_8:
    case LUMINANCE_F16:
    case RGBA_F16:
    case R16_EXT:
    case BGR_565:
    case RG_88:
    case RGBX_8888:
    case BGRX_8888:
    case RGBX_1010102:
    case BGRX_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case P010:
      return false;
  }
  NOTREACHED();
  return false;
}

ResourceFormat GetResourceFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRA_8888:
      return BGRA_8888;
    case gfx::BufferFormat::R_8:
      return RED_8;
    case gfx::BufferFormat::R_16:
      return R16_EXT;
    case gfx::BufferFormat::RGBA_4444:
      return RGBA_4444;
    case gfx::BufferFormat::RGBA_8888:
      return RGBA_8888;
    case gfx::BufferFormat::RGBA_F16:
      return RGBA_F16;
    case gfx::BufferFormat::BGR_565:
      return BGR_565;
    case gfx::BufferFormat::RG_88:
      return RG_88;
    case gfx::BufferFormat::RGBX_8888:
      return RGBX_8888;
    case gfx::BufferFormat::BGRX_8888:
      return BGRX_8888;
    case gfx::BufferFormat::RGBX_1010102:
      return RGBX_1010102;
    case gfx::BufferFormat::BGRX_1010102:
      return BGRX_1010102;
    case gfx::BufferFormat::YVU_420:
      return YVU_420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return YUV_420_BIPLANAR;
    case gfx::BufferFormat::P010:
      return P010;
  }
  NOTREACHED();
  return RGBA_8888;
}

bool GLSupportsFormat(ResourceFormat format) {
  switch (format) {
    case BGR_565:
    case BGRX_8888:
    case BGRX_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case P010:
      return false;
    default:
      return true;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
VkFormat ToVkFormat(ResourceFormat format) {
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
    case RGBX_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case BGRX_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case RGBX_1010102:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case BGRX_1010102:
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
    case P010:
      break;
  }
  NOTREACHED() << "Unsupported format " << format;
  return VK_FORMAT_UNDEFINED;
}
#endif

#if BUILDFLAG(SKIA_USE_DAWN)
dawn::TextureFormat ToDawnFormat(ResourceFormat format) {
  switch (format) {
    case RGBA_8888:
    case RGBX_8888:
      return dawn::TextureFormat::RGBA8Unorm;
    case BGRA_8888:
    case BGRX_8888:
      return dawn::TextureFormat::BGRA8Unorm;
    case RED_8:
    case ALPHA_8:
    case LUMINANCE_8:
      return dawn::TextureFormat::R8Unorm;
    case RG_88:
      return dawn::TextureFormat::RG8Unorm;
    case RGBA_F16:
      return dawn::TextureFormat::RGBA16Float;
    case RGBX_1010102:
      return dawn::TextureFormat::RGB10A2Unorm;
    case RGBA_4444:
    case RGB_565:
    case BGR_565:
    case R16_EXT:
    case BGRX_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case ETC1:
    case LUMINANCE_F16:
    case P010:
      break;
  }
  NOTREACHED() << "Unsupported format " << format;
  return dawn::TextureFormat::Undefined;
}
#endif

}  // namespace viz
