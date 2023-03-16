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
    case BGR_565:
    case RGB_565:
      return kRGB_565_SkColorType;
    case LUMINANCE_8:
      return kGray_8_SkColorType;
    case RGBX_8888:
    case BGRX_8888:
    case ETC1:
      return kRGB_888x_SkColorType;
    case P010:
#if BUILDFLAG(IS_APPLE)
      DLOG(ERROR) << "Sampling of P010 resources must be done per-plane.";
#endif
      return kRGBA_1010102_SkColorType;
    case RGBA_1010102:
    // This intentionally returns kRGBA_1010102_SkColorType for BGRA_1010102
    // even though kBGRA_1010102_SkColorType exists. It should only be used on
    // macOS (outside of tests).
    case BGRA_1010102:
      return kRGBA_1010102_SkColorType;

    // YUV images are sampled as RGB.
    case YVU_420:
    case YUV_420_BIPLANAR:
#if BUILDFLAG(IS_APPLE)
      DLOG(ERROR) << "Sampling of YUV_420 resources must be done per-plane.";
#endif
      return kRGB_888x_SkColorType;
    case YUVA_420_TRIPLANAR:
#if BUILDFLAG(IS_APPLE)
      DLOG(ERROR) << "Sampling of YUVA_420 resources must be done per-plane.";
#endif
      return kRGBA_8888_SkColorType;
    case RED_8:
      return kAlpha_8_SkColorType;
    case R16_EXT:
      return kA16_unorm_SkColorType;
    case RG16_EXT:
      return kR16G16_unorm_SkColorType;
    // Use kN32_SkColorType if there is no corresponding SkColorType.
    case LUMINANCE_F16:
      return kN32_SkColorType;
    case RG_88:
      return kR8G8_unorm_SkColorType;
    case RGBA_F16:
      return kRGBA_F16_SkColorType;
  }
  NOTREACHED();
  return kN32_SkColorType;
}

ResourceFormat SkColorTypeToResourceFormat(SkColorType color_type) {
  switch (color_type) {
    case kARGB_4444_SkColorType:
      return RGBA_4444;
    case kBGRA_8888_SkColorType:
      return BGRA_8888;
    case kRGBA_8888_SkColorType:
      return RGBA_8888;
    case kRGBA_F16_SkColorType:
      return RGBA_F16;
    case kAlpha_8_SkColorType:
      return ALPHA_8;
    case kRGB_565_SkColorType:
      return RGB_565;
    case kGray_8_SkColorType:
      return LUMINANCE_8;
    case kRGB_888x_SkColorType:
      return RGBX_8888;
    case kRGBA_1010102_SkColorType:
      return RGBA_1010102;
    case kBGRA_1010102_SkColorType:
      return BGRA_1010102;
    // These colortypes are just for reading from - not to render to
    case kR8G8_unorm_SkColorType:
    case kA16_float_SkColorType:
    case kR16G16_float_SkColorType:
    case kA16_unorm_SkColorType:
    case kR16G16_unorm_SkColorType:
    case kR16G16B16A16_unorm_SkColorType:
    case kUnknown_SkColorType:
    // These colortypes are don't have an equivalent in ResourceFormat
    case kRGB_101010x_SkColorType:
    case kBGR_101010x_SkColorType:
    case kRGBA_F16Norm_SkColorType:
    case kRGBA_F32_SkColorType:
    case kSRGBA_8888_SkColorType:
    // Default case is for new color types added to Skia
    default:
      break;
  }
  NOTREACHED();
  return RGBA_8888;
}

int BitsPerPixel(ResourceFormat format) {
  switch (format) {
    case RGBA_F16:
      return 64;
    case BGRA_8888:
    case RGBA_8888:
    case RGBX_8888:
    case BGRX_8888:
    case RGBA_1010102:
    case BGRA_1010102:
    case RG16_EXT:
      return 32;
    case P010:
      return 24;
    case YUVA_420_TRIPLANAR:
      return 20;
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

int AlphaBits(ResourceFormat format) {
  switch (format) {
    case RGBA_F16:
      return 16;
    case BGRA_8888:
    case RGBA_8888:
    case YUVA_420_TRIPLANAR:
    case ALPHA_8:
      return 8;
    case RGBA_4444:
      return 4;
    case RGBA_1010102:
    case BGRA_1010102:
      return 2;
    case RGBX_8888:
    case BGRX_8888:
    case P010:
    case RG16_EXT:
    case RGB_565:
    case LUMINANCE_F16:
    case R16_EXT:
    case BGR_565:
    case RG_88:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case LUMINANCE_8:
    case RED_8:
    case ETC1:
      return 0;
  }
  NOTREACHED();
  return 0;
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

unsigned int GLInternalFormat(ResourceFormat format) {
  // In GLES2, the internal format must match the texture format. (It no longer
  // is true in GLES3, however it still holds for the BGRA extension.)
  // GL_EXT_texture_norm16 follows GLES3 semantics and only exposes a sized
  // internal format (GL_R16_EXT).
  switch (format) {
    case R16_EXT:
      return GL_R16_EXT;
    case RG16_EXT:
      return GL_RG16_EXT;
    case ETC1:
      return GL_ETC1_RGB8_OES;
    case RGBA_1010102:
    case BGRA_1010102:
      return GL_RGB10_A2_EXT;
    default:
      return GLDataFormat(format);
  }
}

bool HasEquivalentBufferFormat(SharedImageFormat format) {
  if (format.is_single_plane()) {
    return HasEquivalentBufferFormat(format.resource_format());
  }

  return format == MultiPlaneFormat::kYVU_420 ||
         format == MultiPlaneFormat::kYUV_420_BIPLANAR ||
         format == MultiPlaneFormat::kYUVA_420_TRIPLANAR ||
         format == MultiPlaneFormat::kP010;
}

bool HasEquivalentBufferFormat(ResourceFormat format) {
  switch (format) {
    case BGRA_8888:
    case RED_8:
    case R16_EXT:
    case RG16_EXT:
    case RGBA_4444:
    case RGBA_8888:
    case RGBA_F16:
    case BGR_565:
    case RG_88:
    case RGBX_8888:
    case BGRX_8888:
    case RGBA_1010102:
    case BGRA_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case YUVA_420_TRIPLANAR:
    case P010:
      return true;
    case ETC1:
    case ALPHA_8:
    case LUMINANCE_8:
    case RGB_565:
    case LUMINANCE_F16:
      return false;
  }
}

gfx::BufferFormat BufferFormat(ResourceFormat format) {
  switch (format) {
    case BGRA_8888:
      return gfx::BufferFormat::BGRA_8888;
    case RED_8:
      return gfx::BufferFormat::R_8;
    case R16_EXT:
      return gfx::BufferFormat::R_16;
    case RG16_EXT:
      return gfx::BufferFormat::RG_1616;
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
    case RGBA_1010102:
      return gfx::BufferFormat::RGBA_1010102;
    case BGRA_1010102:
      return gfx::BufferFormat::BGRA_1010102;
    case YVU_420:
      return gfx::BufferFormat::YVU_420;
    case YUV_420_BIPLANAR:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case YUVA_420_TRIPLANAR:
      return gfx::BufferFormat::YUVA_420_TRIPLANAR;
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

bool IsGpuMemoryBufferFormatSupported(ResourceFormat format) {
  switch (format) {
    case BGRA_8888:
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    // TODO(crbug.com/1307837): On ARM devices LaCrOS can't create RED_8
    // GpuMemoryBuffer Objects with GBM device. This capability should be
    // plumbed and known by clients requesting shared images as overlay
    // candidate.
    case RED_8:
#endif
#if BUILDFLAG(IS_APPLE)
    case BGRX_8888:
    case RGBX_8888:
#endif
    case R16_EXT:
    case RGBA_4444:
    case RGBA_8888:
    case RGBA_1010102:
    case BGRA_1010102:
    case RGBA_F16:
      return true;
    // These formats have no BufferFormat equivalent or are only used
    // for external textures, or have no GL equivalent formats.
    case ETC1:
    case ALPHA_8:
    case LUMINANCE_8:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    case RED_8:
#endif
#if !BUILDFLAG(IS_APPLE)
    case BGRX_8888:
    case RGBX_8888:
#endif
    case RGB_565:
    case LUMINANCE_F16:
    case BGR_565:
    case RG_88:
    case RG16_EXT:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case YUVA_420_TRIPLANAR:
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
    case RG16_EXT:
    case BGR_565:
    case RG_88:
    case RGBX_8888:
    case BGRX_8888:
    case RGBA_1010102:
    case BGRA_1010102:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case YUVA_420_TRIPLANAR:
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
    case gfx::BufferFormat::RG_1616:
      return RG16_EXT;
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
    case gfx::BufferFormat::RGBA_1010102:
      return RGBA_1010102;
    case gfx::BufferFormat::BGRA_1010102:
      return BGRA_1010102;
    case gfx::BufferFormat::YVU_420:
      return YVU_420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return YUV_420_BIPLANAR;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return YUVA_420_TRIPLANAR;
    case gfx::BufferFormat::P010:
      return P010;
  }
  NOTREACHED();
  return RGBA_8888;
}

bool GLSupportsFormat(ResourceFormat format) {
  switch (format) {
    case BGR_565:
    case YVU_420:
    case YUV_420_BIPLANAR:
    case YUVA_420_TRIPLANAR:
    case P010:
      return false;
    default:
      return true;
  }
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

SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format,
                                 int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (!gpu_compositing) {
    // TODO(crbug.com/986405): Remove this assumption and have clients tag
    // resources with the correct format.
    // In software compositing we lazily use RGBA_8888 throughout the system,
    // but actual pixel encodings are the native skia bit ordering, which can be
    // RGBA or BGRA.
    return kN32_SkColorType;
  }
  if (format.is_single_plane()) {
    return ResourceFormatToClosestSkColorType(gpu_compositing,
                                              format.resource_format());
  }

  auto plane_config = format.plane_config();
  auto channel_format = format.channel_format();
  if (format.PrefersExternalSampler()) {
    switch (channel_format) {
      case SharedImageFormat::ChannelFormat::k8:
        return plane_config == SharedImageFormat::PlaneConfig::kY_UV_A
                   ? kRGBA_8888_SkColorType
                   : kRGB_888x_SkColorType;
      case SharedImageFormat::ChannelFormat::k10:
        return kRGBA_1010102_SkColorType;
      case SharedImageFormat::ChannelFormat::k16:
        return kR16G16B16A16_unorm_SkColorType;
      case SharedImageFormat::ChannelFormat::k16F:
        return kRGBA_F16_SkColorType;
    }
  } else {
    // No external sampling, format is per plane.
    int num_channels = format.NumChannelsInPlane(plane_index);
    DCHECK_GT(num_channels, 0);
    DCHECK_LE(num_channels, 2);
    switch (channel_format) {
      case SharedImageFormat::ChannelFormat::k8:
        return num_channels == 1 ? kAlpha_8_SkColorType
                                 : kR8G8_unorm_SkColorType;
      case SharedImageFormat::ChannelFormat::k10:
      case SharedImageFormat::ChannelFormat::k16:
        return num_channels == 1 ? kA16_unorm_SkColorType
                                 : kR16G16_unorm_SkColorType;
      case SharedImageFormat::ChannelFormat::k16F:
        return num_channels == 1 ? kA16_float_SkColorType
                                 : kR16G16_float_SkColorType;
    }
  }
}

}  // namespace viz
