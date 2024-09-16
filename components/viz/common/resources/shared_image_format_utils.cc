// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "ui/gfx/buffer_types.h"

namespace viz {

SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format) {
  CHECK(format.is_single_plane());

  if (!gpu_compositing) {
    // TODO(crbug.com/41472025): Remove this assumption and have clients tag
    // resources with the correct format.
    // In software compositing we lazily use RGBA_8888 throughout the system,
    // but actual pixel encodings are the native skia bit ordering, which can be
    // RGBA or BGRA.
    return kN32_SkColorType;
  }

  if (format == SinglePlaneFormat::kRGBA_4444) {
    return kARGB_4444_SkColorType;
  } else if (format == SinglePlaneFormat::kRGBA_8888) {
    return kRGBA_8888_SkColorType;
  } else if (format == SinglePlaneFormat::kBGRA_8888) {
    return kBGRA_8888_SkColorType;
  } else if (format == SinglePlaneFormat::kALPHA_8) {
    return kAlpha_8_SkColorType;
  } else if (format == SinglePlaneFormat::kBGR_565 ||
             format == SinglePlaneFormat::kRGB_565) {
    return kRGB_565_SkColorType;
  } else if (format == SinglePlaneFormat::kLUMINANCE_8) {
    return kGray_8_SkColorType;
  } else if (format == SinglePlaneFormat::kRGBX_8888 ||
             format == SinglePlaneFormat::kBGRX_8888 ||
             format == SinglePlaneFormat::kETC1) {
    return kRGB_888x_SkColorType;
  } else if (format == SinglePlaneFormat::kRGBA_1010102 ||
             // This intentionally returns kRGBA_1010102_SkColorType for
             // BGRA_1010102 even though kBGRA_1010102_SkColorType exists. It
             // should only be used on macOS (outside of tests).
             format == SinglePlaneFormat::kBGRA_1010102) {
    return kRGBA_1010102_SkColorType;

  } else if (format == SinglePlaneFormat::kR_8) {
    return kAlpha_8_SkColorType;
  } else if (format == SinglePlaneFormat::kR_16) {
    return kA16_unorm_SkColorType;
  } else if (format == SinglePlaneFormat::kRG_1616) {
    return kR16G16_unorm_SkColorType;
  } else if (format == SinglePlaneFormat::kLUMINANCE_F16 ||
             format == SinglePlaneFormat::kR_F16) {
    return kA16_float_SkColorType;
  } else if (format == SinglePlaneFormat::kRG_88) {
    return kR8G8_unorm_SkColorType;
  } else if (format == SinglePlaneFormat::kRGBA_F16) {
    return kRGBA_F16_SkColorType;
  }
  NOTREACHED();
}

SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format,
                                 int plane_index) {
  CHECK(format.IsValidPlaneIndex(plane_index));
  if (!gpu_compositing) {
    // TODO(crbug.com/41472025): Remove this assumption and have clients tag
    // resources with the correct format.
    // In software compositing we lazily use RGBA_8888 throughout the system,
    // but actual pixel encodings are the native skia bit ordering, which can be
    // RGBA or BGRA.
    return kN32_SkColorType;
  }
  if (format.is_single_plane()) {
    return ToClosestSkColorType(gpu_compositing, format);
  }

  // No external sampling, format is per plane.
  CHECK(!format.PrefersExternalSampler());
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_GT(num_channels, 0);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case SharedImageFormat::ChannelFormat::k8:
      return num_channels == 1 ? kAlpha_8_SkColorType : kR8G8_unorm_SkColorType;
    case SharedImageFormat::ChannelFormat::k10:
    case SharedImageFormat::ChannelFormat::k16:
      return num_channels == 1 ? kA16_unorm_SkColorType
                               : kR16G16_unorm_SkColorType;
    case SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 1 ? kA16_float_SkColorType
                               : kR16G16_float_SkColorType;
  }
}

SharedImageFormat SkColorTypeToSinglePlaneSharedImageFormat(
    SkColorType color_type) {
  switch (color_type) {
    case kARGB_4444_SkColorType:
      return SinglePlaneFormat::kRGBA_4444;
    case kBGRA_8888_SkColorType:
      return SinglePlaneFormat::kBGRA_8888;
    case kRGBA_8888_SkColorType:
      return SinglePlaneFormat::kRGBA_8888;
    case kRGBA_F16_SkColorType:
      return SinglePlaneFormat::kRGBA_F16;
    case kAlpha_8_SkColorType:
      return SinglePlaneFormat::kALPHA_8;
    case kRGB_565_SkColorType:
      return SinglePlaneFormat::kBGR_565;
    case kGray_8_SkColorType:
      return SinglePlaneFormat::kLUMINANCE_8;
    case kRGB_888x_SkColorType:
      return SinglePlaneFormat::kRGBX_8888;
    case kRGBA_1010102_SkColorType:
      return SinglePlaneFormat::kRGBA_1010102;
    case kBGRA_1010102_SkColorType:
      return SinglePlaneFormat::kBGRA_1010102;
    // These colortypes are just for reading from - not to render to.
    case kR8G8_unorm_SkColorType:
    case kA16_float_SkColorType:
    case kR16G16_float_SkColorType:
    case kA16_unorm_SkColorType:
    case kR16G16_unorm_SkColorType:
    case kR16G16B16A16_unorm_SkColorType:
    case kUnknown_SkColorType:
    // These colortypes are don't have an equivalent in SharedImageFormat.
    case kRGB_101010x_SkColorType:
    case kBGR_101010x_SkColorType:
    case kRGBA_F16Norm_SkColorType:
    case kRGBA_F32_SkColorType:
    case kSRGBA_8888_SkColorType:
    // Default case is for new color types added to Skia.
    default:
      break;
  }
  NOTREACHED();
}

bool CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(
    SharedImageFormat format) {
  CHECK(format.is_single_plane());
  return (format == SinglePlaneFormat::kBGRA_8888 ||
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
          // TODO(crbug.com/40828687): On ARM devices LaCrOS can't create RED_8
          // GpuMemoryBuffer Objects with GBM device. This capability should be
          // plumbed and known by clients requesting shared images as overlay
          // candidate.
          format == SinglePlaneFormat::kR_8 ||
          format == SinglePlaneFormat::kRG_88 ||
#endif
#if BUILDFLAG(IS_APPLE)
          format == SinglePlaneFormat::kBGRX_8888 ||
          format == SinglePlaneFormat::kRGBX_8888 ||
          format == SinglePlaneFormat::kR_16 ||
          format == SinglePlaneFormat::kRG_1616 ||
#endif
          format == SinglePlaneFormat::kRGBA_4444 ||
          format == SinglePlaneFormat::kRGBA_8888 ||
          format == SinglePlaneFormat::kRGBA_1010102 ||
          format == SinglePlaneFormat::kBGRA_1010102 ||
          format == SinglePlaneFormat::kRGBA_F16);
}

bool HasEquivalentBufferFormat(SharedImageFormat format) {
  return format == SinglePlaneFormat::kBGRA_8888 ||
         format == SinglePlaneFormat::kR_8 ||
         format == SinglePlaneFormat::kR_16 ||
         format == SinglePlaneFormat::kRG_1616 ||
         format == SinglePlaneFormat::kRGBA_4444 ||
         format == SinglePlaneFormat::kRGBA_8888 ||
         format == SinglePlaneFormat::kRGBA_F16 ||
         format == SinglePlaneFormat::kBGR_565 ||
         format == SinglePlaneFormat::kRG_88 ||
         format == SinglePlaneFormat::kRGBX_8888 ||
         format == SinglePlaneFormat::kBGRX_8888 ||
         format == SinglePlaneFormat::kRGBA_1010102 ||
         format == SinglePlaneFormat::kBGRA_1010102 ||
         format == MultiPlaneFormat::kYV12 ||
         format == MultiPlaneFormat::kNV12 ||
         format == MultiPlaneFormat::kNV12A ||
         format == MultiPlaneFormat::kP010;
}

gfx::BufferFormat SinglePlaneSharedImageFormatToBufferFormat(
    SharedImageFormat format) {
  CHECK(format.is_single_plane());
  if (format == SinglePlaneFormat::kBGRA_8888) {
    return gfx::BufferFormat::BGRA_8888;
  } else if (format == SinglePlaneFormat::kR_8) {
    return gfx::BufferFormat::R_8;
  } else if (format == SinglePlaneFormat::kR_16) {
    return gfx::BufferFormat::R_16;
  } else if (format == SinglePlaneFormat::kRG_1616) {
    return gfx::BufferFormat::RG_1616;
  } else if (format == SinglePlaneFormat::kRGBA_4444) {
    return gfx::BufferFormat::RGBA_4444;
  } else if (format == SinglePlaneFormat::kRGBA_8888) {
    return gfx::BufferFormat::RGBA_8888;
  } else if (format == SinglePlaneFormat::kRGBA_F16) {
    return gfx::BufferFormat::RGBA_F16;
  } else if (format == SinglePlaneFormat::kBGR_565) {
    return gfx::BufferFormat::BGR_565;
  } else if (format == SinglePlaneFormat::kRG_88) {
    return gfx::BufferFormat::RG_88;
  } else if (format == SinglePlaneFormat::kRGBX_8888) {
    return gfx::BufferFormat::RGBX_8888;
  } else if (format == SinglePlaneFormat::kBGRX_8888) {
    return gfx::BufferFormat::BGRX_8888;
  } else if (format == SinglePlaneFormat::kRGBA_1010102) {
    return gfx::BufferFormat::RGBA_1010102;
  } else if (format == SinglePlaneFormat::kBGRA_1010102) {
    return gfx::BufferFormat::BGRA_1010102;
  } else {
    // CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat() returns
    // false for all other types, so give a default value that will not be used.
    return gfx::BufferFormat::RGBA_8888;
  }
}

SharedImageFormat GetSharedImageFormat(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::BGRA_8888:
      return SinglePlaneFormat::kBGRA_8888;
    case gfx::BufferFormat::R_8:
      return SinglePlaneFormat::kR_8;
    case gfx::BufferFormat::R_16:
      return SinglePlaneFormat::kR_16;
    case gfx::BufferFormat::RG_1616:
      return SinglePlaneFormat::kRG_1616;
    case gfx::BufferFormat::RGBA_4444:
      return SinglePlaneFormat::kRGBA_4444;
    case gfx::BufferFormat::RGBA_8888:
      return SinglePlaneFormat::kRGBA_8888;
    case gfx::BufferFormat::RGBA_F16:
      return SinglePlaneFormat::kRGBA_F16;
    case gfx::BufferFormat::BGR_565:
      return SinglePlaneFormat::kBGR_565;
    case gfx::BufferFormat::RG_88:
      return SinglePlaneFormat::kRG_88;
    case gfx::BufferFormat::RGBX_8888:
      return SinglePlaneFormat::kRGBX_8888;
    case gfx::BufferFormat::BGRX_8888:
      return SinglePlaneFormat::kBGRX_8888;
    case gfx::BufferFormat::RGBA_1010102:
      return SinglePlaneFormat::kRGBA_1010102;
    case gfx::BufferFormat::BGRA_1010102:
      return SinglePlaneFormat::kBGRA_1010102;
    case gfx::BufferFormat::YVU_420:
      return MultiPlaneFormat::kYV12;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return MultiPlaneFormat::kNV12;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return MultiPlaneFormat::kNV12A;
    case gfx::BufferFormat::P010:
      return MultiPlaneFormat::kP010;
  }
  NOTREACHED();
}

// static
unsigned int
SharedImageFormatRestrictedSinglePlaneUtils::ToGLTextureStorageFormat(
    SharedImageFormat format,
    bool use_angle_rgbx_format) {
  CHECK(format.is_single_plane());
  if (format == SinglePlaneFormat::kRGBA_8888) {
    return GL_RGBA8_OES;
  } else if (format == SinglePlaneFormat::kBGRA_8888) {
    return GL_BGRA8_EXT;
  } else if (format == SinglePlaneFormat::kRGBA_F16) {
    return GL_RGBA16F_EXT;
  } else if (format == SinglePlaneFormat::kRGBA_4444) {
    return GL_RGBA4;
  } else if (format == SinglePlaneFormat::kALPHA_8) {
    return GL_ALPHA8_EXT;
  } else if (format == SinglePlaneFormat::kLUMINANCE_8) {
    return GL_LUMINANCE8_EXT;
  } else if (format == SinglePlaneFormat::kBGR_565 ||
             format == SinglePlaneFormat::kRGB_565) {
    return GL_RGB565;
  } else if (format == SinglePlaneFormat::kR_8) {
    return GL_R8_EXT;
  } else if (format == SinglePlaneFormat::kRG_88) {
    return GL_RG8_EXT;
  } else if (format == SinglePlaneFormat::kLUMINANCE_F16) {
    return GL_LUMINANCE16F_EXT;
  } else if (format == SinglePlaneFormat::kR_F16) {
    return GL_R16F_EXT;
  } else if (format == SinglePlaneFormat::kR_16) {
    return GL_R16_EXT;
  } else if (format == SinglePlaneFormat::kRG_1616) {
    return GL_RG16_EXT;
  } else if (format == SinglePlaneFormat::kRGBX_8888 ||
             format == SinglePlaneFormat::kBGRX_8888) {
    return use_angle_rgbx_format ? GL_RGBX8_ANGLE : GL_RGB8_OES;
  } else if (format == SinglePlaneFormat::kETC1) {
    return GL_ETC1_RGB8_OES;
  } else if (format == SinglePlaneFormat::kRGBA_1010102 ||
             format == SinglePlaneFormat::kBGRA_1010102) {
    return GL_RGB10_A2_EXT;
  }
  NOTREACHED_IN_MIGRATION();
  return GL_RGBA8_OES;
}

// static
gfx::BufferFormat
SharedImageFormatToBufferFormatRestrictedUtils::ToBufferFormat(
    SharedImageFormat format) {
  if (format.is_single_plane()) {
    return SinglePlaneSharedImageFormatToBufferFormat(format);
  }

  if (format == MultiPlaneFormat::kYV12) {
    return gfx::BufferFormat::YVU_420;
  } else if (format == MultiPlaneFormat::kNV12) {
    return gfx::BufferFormat::YUV_420_BIPLANAR;
  } else if (format == MultiPlaneFormat::kNV12A) {
    return gfx::BufferFormat::YUVA_420_TRIPLANAR;
  } else if (format == MultiPlaneFormat::kP010) {
    return gfx::BufferFormat::P010;
  }
  NOTREACHED_IN_MIGRATION() << "format=" << format.ToString();
  return gfx::BufferFormat::RGBA_8888;
}

}  // namespace viz
