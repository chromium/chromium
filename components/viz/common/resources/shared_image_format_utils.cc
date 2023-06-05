// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format_utils.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"

namespace viz {

SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format) {
  CHECK(format.is_single_plane());

  if (!gpu_compositing) {
    // TODO(crbug.com/986405): Remove this assumption and have clients tag
    // resources with the correct format.
    // In software compositing we lazily use RGBA_8888 throughout the system,
    // but actual pixel encodings are the native skia bit ordering, which can be
    // RGBA or BGRA.
    return kN32_SkColorType;
  }

  switch (format.resource_format()) {
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
  NOTREACHED_NORETURN();
}

SkColorType ToClosestSkColorType(bool gpu_compositing,
                                 SharedImageFormat format,
                                 int plane_index) {
  CHECK(format.IsValidPlaneIndex(plane_index));
  if (!gpu_compositing) {
    // TODO(crbug.com/986405): Remove this assumption and have clients tag
    // resources with the correct format.
    // In software compositing we lazily use RGBA_8888 throughout the system,
    // but actual pixel encodings are the native skia bit ordering, which can be
    // RGBA or BGRA.
    return kN32_SkColorType;
  }
  if (format.is_single_plane()) {
    return ToClosestSkColorType(gpu_compositing, format);
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
      return SinglePlaneFormat::kRGB_565;
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
  NOTREACHED_NORETURN();
}

bool CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat(
    SharedImageFormat format) {
  CHECK(format.is_single_plane());
  switch (format.resource_format()) {
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
  NOTREACHED_NORETURN();
}

bool HasEquivalentBufferFormat(SharedImageFormat format) {
  if (format.is_single_plane()) {
    switch (format.resource_format()) {
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

  return format == MultiPlaneFormat::kYV12 ||
         format == MultiPlaneFormat::kNV12 ||
         format == MultiPlaneFormat::kNV12A ||
         format == MultiPlaneFormat::kP010;
}

gfx::BufferFormat SinglePlaneSharedImageFormatToBufferFormat(
    SharedImageFormat format) {
  CHECK(format.is_single_plane());
  switch (format.resource_format()) {
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
      // CanCreateGpuMemoryBufferForSinglePlaneSharedImageFormat() returns
      // false for these types, so give a default value that will not be used.
      break;
  }
  return gfx::BufferFormat::RGBA_8888;
}

SharedImageFormat GetSharedImageFormat(gfx::BufferFormat format) {
  switch (format) {
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
      return LegacyMultiPlaneFormat::kYV12;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return LegacyMultiPlaneFormat::kNV12;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return LegacyMultiPlaneFormat::kNV12A;
    case gfx::BufferFormat::P010:
      return LegacyMultiPlaneFormat::kP010;
  }
  NOTREACHED_NORETURN();
}

}  // namespace viz
