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
  return SharedImageFormat::SinglePlane(
      SkColorTypeToResourceFormat(color_type));
}

}  // namespace viz
