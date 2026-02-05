// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <array>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "third_party/skia/include/core/SkColorType.h"

namespace viz {
namespace {

constexpr auto kMappableSharedImageFormats = std::to_array<SharedImageFormat>(
    {SinglePlaneFormat::kR_8, SinglePlaneFormat::kR_16,
     SinglePlaneFormat::kRG_88, SinglePlaneFormat::kRG_1616,
     SinglePlaneFormat::kBGR_565, SinglePlaneFormat::kRGBA_4444,
     SinglePlaneFormat::kRGBX_8888, SinglePlaneFormat::kRGBA_8888,
     SinglePlaneFormat::kBGRX_8888, SinglePlaneFormat::kBGRA_1010102,
     SinglePlaneFormat::kRGBA_1010102, SinglePlaneFormat::kBGRA_8888,
     SinglePlaneFormat::kRGBA_F16, MultiPlaneFormat::kNV12,
     MultiPlaneFormat::kYV12, MultiPlaneFormat::kNV12A,
     MultiPlaneFormat::kP010});

}  // namespace

SharedImageFormatUMA GetSharedImageFormatUMA(SharedImageFormat format) {
  if (format.is_single_plane()) {
    if (format == SinglePlaneFormat::kRGBA_8888) {
      return SharedImageFormatUMA::kRGBA_8888;
    } else if (format == SinglePlaneFormat::kRGBA_4444) {
      return SharedImageFormatUMA::kRGBA_4444;
    } else if (format == SinglePlaneFormat::kBGRA_8888) {
      return SharedImageFormatUMA::kBGRA_8888;
    } else if (format == SinglePlaneFormat::kALPHA_8) {
      return SharedImageFormatUMA::kALPHA_8;
    } else if (format == SinglePlaneFormat::kBGR_565) {
      return SharedImageFormatUMA::kBGR_565;
    } else if (format == SinglePlaneFormat::kETC1) {
      return SharedImageFormatUMA::kETC1;
    } else if (format == SinglePlaneFormat::kR_8) {
      return SharedImageFormatUMA::kR_8;
    } else if (format == SinglePlaneFormat::kRG_88) {
      return SharedImageFormatUMA::kRG_88;
    } else if (format == SinglePlaneFormat::kLUMINANCE_F16) {
      return SharedImageFormatUMA::kLUMINANCE_F16;
    } else if (format == SinglePlaneFormat::kRGBA_F16) {
      return SharedImageFormatUMA::kRGBA_F16;
    } else if (format == SinglePlaneFormat::kR_16) {
      return SharedImageFormatUMA::kR_16;
    } else if (format == SinglePlaneFormat::kRG_1616) {
      return SharedImageFormatUMA::kRG_1616;
    } else if (format == SinglePlaneFormat::kRGBX_8888) {
      return SharedImageFormatUMA::kRGBX_8888;
    } else if (format == SinglePlaneFormat::kBGRX_8888) {
      return SharedImageFormatUMA::kBGRX_8888;
    } else if (format == SinglePlaneFormat::kRGBA_1010102) {
      return SharedImageFormatUMA::kRGBA_1010102;
    } else if (format == SinglePlaneFormat::kBGRA_1010102) {
      return SharedImageFormatUMA::kBGRA_1010102;
    } else {
      DCHECK_EQ(format, SinglePlaneFormat::kR_F16);
      return SharedImageFormatUMA::kR_F16;
    }
  }

  using PlaneConfig = SharedImageFormat::PlaneConfig;
  using Subsampling = SharedImageFormat::Subsampling;
  using ChannelFormat = SharedImageFormat::ChannelFormat;

  if (format == MultiPlaneFormat::kYV12) {
    return SharedImageFormatUMA::kYV12;
  } else if (format == MultiPlaneFormat::kNV12) {
    return SharedImageFormatUMA::kNV12;
  } else if (format == MultiPlaneFormat::kNV12A) {
    return SharedImageFormatUMA::kNV12A;
  } else if (format == MultiPlaneFormat::kP010) {
    return SharedImageFormatUMA::kP010;
  } else if (format == MultiPlaneFormat::kNV16) {
    return SharedImageFormatUMA::kNV16;
  } else if (format == MultiPlaneFormat::kNV24) {
    return SharedImageFormatUMA::kNV24;
  } else if (format == MultiPlaneFormat::kP210) {
    return SharedImageFormatUMA::kP210;
  } else if (format == MultiPlaneFormat::kP410) {
    return SharedImageFormatUMA::kP410;
  } else if (format == MultiPlaneFormat::kI420A) {
    return SharedImageFormatUMA::kI420A;
  } else if (format.is_multi_plane() &&
             format.plane_config() == PlaneConfig::kY_U_V) {
    // Y_U_V planar formats are usually used by software video frames.
    switch (format.channel_format()) {
      case ChannelFormat::k8:
        switch (format.subsampling()) {
          case Subsampling::k420:
            return SharedImageFormatUMA::kI420;
          case Subsampling::k422:
            return SharedImageFormatUMA::kI422;
          case Subsampling::k444:
            return SharedImageFormatUMA::kI444;
        }
      case ChannelFormat::k10:
        switch (format.subsampling()) {
          case Subsampling::k420:
            return SharedImageFormatUMA::kYUV420P10;
          case Subsampling::k422:
            return SharedImageFormatUMA::kYUV422P10;
          case Subsampling::k444:
            return SharedImageFormatUMA::kYUV444P10;
        }
      case ChannelFormat::k16:
      case ChannelFormat::k16F:
        switch (format.subsampling()) {
          case Subsampling::k420:
            return SharedImageFormatUMA::kYUV420P16;
          case Subsampling::k422:
            return SharedImageFormatUMA::kYUV422P16;
          case Subsampling::k444:
            return SharedImageFormatUMA::kYUV444P16;
        }
    }
  } else {
    return SharedImageFormatUMA::kOther;
  }
}

SkColorType ToClosestSkColorType(SharedImageFormat format) {
  CHECK(format.is_single_plane());

  if (format == SinglePlaneFormat::kRGBA_4444) {
    return kARGB_4444_SkColorType;
  } else if (format == SinglePlaneFormat::kRGBA_8888) {
    return kRGBA_8888_SkColorType;
  } else if (format == SinglePlaneFormat::kBGRA_8888) {
    return kBGRA_8888_SkColorType;
  } else if (format == SinglePlaneFormat::kALPHA_8) {
    return kAlpha_8_SkColorType;
  } else if (format == SinglePlaneFormat::kBGR_565) {
    return kRGB_565_SkColorType;
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
    return kR8_unorm_SkColorType;
  } else if (format == SinglePlaneFormat::kR_16) {
    return kR16_unorm_SkColorType;
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

SkColorType ToClosestSkColorType(SharedImageFormat format, int plane_index) {
  CHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane()) {
    return ToClosestSkColorType(format);
  }

  // No external sampling, format is per plane.
  CHECK(!format.PrefersExternalSampler());
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_GT(num_channels, 0);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case SharedImageFormat::ChannelFormat::k8:
      return num_channels == 1 ? kR8_unorm_SkColorType
                               : kR8G8_unorm_SkColorType;
    case SharedImageFormat::ChannelFormat::k10:
    case SharedImageFormat::ChannelFormat::k16:
      return num_channels == 1 ? kR16_unorm_SkColorType
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
    case kRGB_888x_SkColorType:
      return SinglePlaneFormat::kRGBX_8888;
    case kRGBA_1010102_SkColorType:
      return SinglePlaneFormat::kRGBA_1010102;
    case kBGRA_1010102_SkColorType:
      return SinglePlaneFormat::kBGRA_1010102;
    case kR8_unorm_SkColorType:
      return SinglePlaneFormat::kR_8;
    case kR8G8_unorm_SkColorType:
      return SinglePlaneFormat::kRG_88;
    case kA16_float_SkColorType:
      return SinglePlaneFormat::kR_F16;
    case kA16_unorm_SkColorType:
    case kR16_unorm_SkColorType:
      return SinglePlaneFormat::kR_16;
    case kR16G16_unorm_SkColorType:
      return SinglePlaneFormat::kRG_1616;
    // These colortypes are just for reading from - not to render to.
    case kR16G16_float_SkColorType:
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

bool CanCreateNativeBufferForFormat(SharedImageFormat format) {
  return (format == SinglePlaneFormat::kBGRA_8888 ||
          format == SinglePlaneFormat::kR_8 ||
          format == SinglePlaneFormat::kRG_88 ||
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

size_t SharedMemoryOffsetForSharedImageFormat(SharedImageFormat format,
                                              int plane_index,
                                              const gfx::Size& size) {
  if (format.is_single_plane()) {
    return 0;
  }

  size_t offset = 0;
  for (int plane = 0; plane < plane_index; ++plane) {
    std::optional<size_t> plane_size =
        format.MaybeEstimatedPlaneSizeInBytes(plane, size);
    if (!plane_size) {
      DLOG(ERROR) << "Could not calculate plane size for plane " << plane;
      return 0;
    }
    offset += *plane_size;
  }
  return offset;
}

std::optional<size_t> SharedMemoryRowSizeForSharedImageFormat(
    SharedImageFormat format,
    int plane_index,
    int width) {
  if (!format.IsValidPlaneIndex(plane_index)) {
    return std::nullopt;
  }

  if (format.is_single_plane()) {
    CHECK_NE(format, SinglePlaneFormat::kETC1);
    DCHECK_EQ(plane_index, 0);

    base::CheckedNumeric<size_t> bytes_per_row = format.BytesPerPixel();
    bytes_per_row *= width;

    // Row size must be aligned to 4 bytes.
    bytes_per_row += 3;
    bytes_per_row -= bytes_per_row % 4;
    if (!bytes_per_row.IsValid()) {
      return std::nullopt;
    }

    return bytes_per_row.ValueOrDie();
  }

  int plane_width =
      format.GetPlaneSize(plane_index, gfx::Size(width, 0)).width();
  int num_channels = format.NumChannelsInPlane(plane_index);

  base::CheckedNumeric<size_t> bytes_per_row =
      format.MultiplanarStorageBytesPerChannel();
  bytes_per_row *= num_channels;
  bytes_per_row *= plane_width;

  if (!bytes_per_row.IsValid()) {
    return std::nullopt;
  }

  return bytes_per_row.ValueOrDie();
}

std::optional<size_t> SharedMemoryPlaneSizeForSharedImageFormat(
    SharedImageFormat format,
    int plane_index,
    const gfx::Size& size) {
  std::optional<size_t> row_size = SharedMemoryRowSizeForSharedImageFormat(
      format, plane_index, base::checked_cast<size_t>(size.width()));
  if (!row_size) {
    return std::nullopt;
  }
  base::CheckedNumeric<size_t> plane_size = row_size.value();
  plane_size *= format.GetPlaneSize(plane_index, size).height();
  if (!plane_size.IsValid()) {
    return std::nullopt;
  }

  return plane_size.ValueOrDie();
}

std::optional<size_t> SharedMemorySizeForSharedImageFormat(
    SharedImageFormat format,
    const gfx::Size& size) {
  base::CheckedNumeric<size_t> buffer_size = 0;
  for (int plane = 0; plane < format.NumberOfPlanes(); plane++) {
    auto plane_size =
        SharedMemoryPlaneSizeForSharedImageFormat(format, plane, size);
    if (!plane_size) {
      return std::nullopt;
    }
    buffer_size += plane_size.value();
    if (!buffer_size.IsValid()) {
      return std::nullopt;
    }
  }

  return buffer_size.ValueOrDie();
}

bool IsOddSizeMultiPlanarBuffersAllowed() {
#if BUILDFLAG(IS_APPLE)
  return true;
#else
  return false;
#endif
}

base::span<const SharedImageFormat> GetMappableSharedImageFormatForTesting() {
  return kMappableSharedImageFormats;
}

}  // namespace viz
