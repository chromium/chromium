// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format.h"

#include <compare>
#include <type_traits>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace viz {
namespace {

static size_t ConvertBitsToBytes(size_t bits) {
  size_t bytes = bits / 8;
  // Don't add anything to `bits` to avoid potential overflow.
  if ((bits & 7) != 0) {
    ++bytes;
  }
  return bytes;
}

const char* SinglePlaneFormatToString(SharedImageFormat format) {
  CHECK(format.is_single_plane());
  if (format == SinglePlaneFormat::kRGBA_8888) {
    return "RGBA_8888";
  } else if (format == SinglePlaneFormat::kRGBA_4444) {
    return "RGBA_4444";
  } else if (format == SinglePlaneFormat::kBGRA_8888) {
    return "BGRA_8888";
  } else if (format == SinglePlaneFormat::kALPHA_8) {
    return "ALPHA_8";
  } else if (format == SinglePlaneFormat::kLUMINANCE_8) {
    return "LUMINANCE_8";
  } else if (format == SinglePlaneFormat::kRGB_565) {
    return "RGB_565";
  } else if (format == SinglePlaneFormat::kBGR_565) {
    return "BGR_565";
  } else if (format == SinglePlaneFormat::kETC1) {
    return "ETC1";
  } else if (format == SinglePlaneFormat::kR_8) {
    return "R_8";
  } else if (format == SinglePlaneFormat::kRG_88) {
    return "RG_88";
  } else if (format == SinglePlaneFormat::kLUMINANCE_F16) {
    return "LUMINANCE_F16";
  } else if (format == SinglePlaneFormat::kRGBA_F16) {
    return "RGBA_F16";
  } else if (format == SinglePlaneFormat::kR_16) {
    return "R_16";
  } else if (format == SinglePlaneFormat::kRG_1616) {
    return "RG_1616";
  } else if (format == SinglePlaneFormat::kRGBX_8888) {
    return "RGBX_8888";
  } else if (format == SinglePlaneFormat::kBGRX_8888) {
    return "BGRX_8888";
  } else if (format == SinglePlaneFormat::kRGBA_1010102) {
    return "RGBA_1010102";
  } else if (format == SinglePlaneFormat::kBGRA_1010102) {
    return "BGRA_1010102";
  } else if (format == LegacyMultiPlaneFormat::kYV12) {
    return "YV12_LEGACY";
  } else if (format == LegacyMultiPlaneFormat::kNV12) {
    return "NV12_LEGACY";
  } else if (format == LegacyMultiPlaneFormat::kNV12A) {
    return "NV12A_LEGACY";
  } else if (format == LegacyMultiPlaneFormat::kP010) {
    return "P010_LEGACY";
  }
  NOTREACHED_NORETURN();
}

int BitsPerPixelForTrueSinglePlaneFormat(SharedImageFormat format) {
  CHECK(format.is_single_plane() && !format.IsLegacyMultiplanar());
  if (format == SinglePlaneFormat::kRGBA_F16) {
    return 64;
  } else if (format == SinglePlaneFormat::kBGRA_8888 ||
             format == SinglePlaneFormat::kRGBA_8888 ||
             format == SinglePlaneFormat::kRGBX_8888 ||
             format == SinglePlaneFormat::kBGRX_8888 ||
             format == SinglePlaneFormat::kRGBA_1010102 ||
             format == SinglePlaneFormat::kBGRA_1010102 ||
             format == SinglePlaneFormat::kRG_1616) {
    return 32;
  } else if (format == SinglePlaneFormat::kRGBA_4444 ||
             format == SinglePlaneFormat::kRGB_565 ||
             format == SinglePlaneFormat::kLUMINANCE_F16 ||
             format == SinglePlaneFormat::kR_16 ||
             format == SinglePlaneFormat::kBGR_565 ||
             format == SinglePlaneFormat::kRG_88) {
    return 16;
  } else if (format == SinglePlaneFormat::kALPHA_8 ||
             format == SinglePlaneFormat::kLUMINANCE_8 ||
             format == SinglePlaneFormat::kR_8) {
    return 8;
  } else if (format == SinglePlaneFormat::kETC1) {
    return 4;
  }
  NOTREACHED_NORETURN();
}

uint64_t StorageBytesPerElement(SharedImageFormat::ChannelFormat channel) {
  switch (channel) {
    case SharedImageFormat::ChannelFormat::k8:
      return 1;
    // 10 bit formats like P010 still use 2 bytes per element.
    case SharedImageFormat::ChannelFormat::k10:
    case SharedImageFormat::ChannelFormat::k16:
    case SharedImageFormat::ChannelFormat::k16F:
      return 2;
  }
}

const char* PlaneConfigToString(SharedImageFormat::PlaneConfig plane) {
  switch (plane) {
    case SharedImageFormat::PlaneConfig::kY_U_V:
      return "Y_U_V";
    case SharedImageFormat::PlaneConfig::kY_V_U:
      return "Y_V_U";
    case SharedImageFormat::PlaneConfig::kY_UV:
      return "Y_UV";
    case SharedImageFormat::PlaneConfig::kY_UV_A:
      return "Y_UV_A";
  }
}

const char* SubsamplingToString(SharedImageFormat::Subsampling subsampling) {
  switch (subsampling) {
    case SharedImageFormat::Subsampling::k420:
      return "420";
    case SharedImageFormat::Subsampling::k422:
      return "422";
    case SharedImageFormat::Subsampling::k444:
      return "444";
  }
}

const char* ChannelFormatToString(SharedImageFormat::ChannelFormat channel) {
  switch (channel) {
    case SharedImageFormat::ChannelFormat::k8:
      return "8unorm";
    case SharedImageFormat::ChannelFormat::k10:
      return "10unorm";
    case SharedImageFormat::ChannelFormat::k16:
      return "16unorm";
    case SharedImageFormat::ChannelFormat::k16F:
      return "16float";
  }
}

const char* PrefersExternalSamplerToString(SharedImageFormat format) {
  return format.PrefersExternalSampler() ? "ExtSamplerOn" : "ExtSamplerOff";
}

SharedImageFormat GetEquivalentMultiplanarFormat(SharedImageFormat format) {
  if (format == LegacyMultiPlaneFormat::kYV12) {
    return MultiPlaneFormat::kYV12;
  } else if (format == LegacyMultiPlaneFormat::kNV12) {
    return MultiPlaneFormat::kNV12;
  } else if (format == LegacyMultiPlaneFormat::kNV12A) {
    return MultiPlaneFormat::kNV12A;
  } else if (format == LegacyMultiPlaneFormat::kP010) {
    return MultiPlaneFormat::kP010;
  }

  NOTREACHED();
  return SinglePlaneFormat::kRGBA_8888;
}

}  // namespace

// Ensure that SharedImageFormat is suitable for passing around by value.
static_assert(sizeof(SharedImageFormat) <= 8);
static_assert(std::is_trivially_destructible_v<SharedImageFormat>);
static_assert(std::is_trivially_copyable_v<SharedImageFormat>);

// TODO(kylechar): Ideally SharedImageFormat would be "trivially comparable" so
// that operator==() is just memcmp(). That would probably require something
// like manually packing bits into a single uint64_t for storage.

bool SharedImageFormat::IsBitmapFormatSupported() const {
  return is_single_plane() &&
         singleplanar_format() == mojom::SingleplanarFormat::RGBA_8888;
}

int SharedImageFormat::NumberOfPlanes() const {
  if (is_single_plane()) {
    return 1;
  }
  switch (plane_config()) {
    case PlaneConfig::kY_U_V:
    case PlaneConfig::kY_V_U:
      return 3;
    case PlaneConfig::kY_UV:
      return 2;
    case PlaneConfig::kY_UV_A:
      return 3;
  }
}

bool SharedImageFormat::IsValidPlaneIndex(int plane_index) const {
  return plane_index >= 0 && plane_index < NumberOfPlanes();
}

absl::optional<size_t> SharedImageFormat::MaybeEstimatedPlaneSizeInBytes(
    int plane_index,
    const gfx::Size& size) const {
  DCHECK(!size.IsEmpty());

  if (is_single_plane()) {
    if (IsLegacyMultiplanar()) {
      return GetEquivalentMultiplanarFormat(*this)
          .MaybeEstimatedPlaneSizeInBytes(plane_index, size);
    }

    DCHECK_EQ(plane_index, 0);

    base::CheckedNumeric<size_t> bits_per_row =
        BitsPerPixelForTrueSinglePlaneFormat(*this);
    bits_per_row *= size.width();
    if (!bits_per_row.IsValid()) {
      return absl::nullopt;
    }

    base::CheckedNumeric<size_t> estimated_bytes =
        ConvertBitsToBytes(bits_per_row.ValueOrDie());
    estimated_bytes *= size.height();
    if (!estimated_bytes.IsValid()) {
      return absl::nullopt;
    }

    return estimated_bytes.ValueOrDie();
  }

  size_t bytes_per_element = StorageBytesPerElement(channel_format());

  gfx::Size plane_size = GetPlaneSize(plane_index, size);

  base::CheckedNumeric<size_t> plane_estimated_bytes =
      bytes_per_element * NumChannelsInPlane(plane_index);
  DCHECK(plane_estimated_bytes.IsValid());
  plane_estimated_bytes *= plane_size.width();
  plane_estimated_bytes *= plane_size.height();
  if (!plane_estimated_bytes.IsValid()) {
    return absl::nullopt;
  }

  return plane_estimated_bytes.ValueOrDie();
}

absl::optional<size_t> SharedImageFormat::MaybeEstimatedSizeInBytes(
    const gfx::Size& size) const {
  DCHECK(!size.IsEmpty());

  if (is_single_plane()) {
    if (IsLegacyMultiplanar()) {
      return GetEquivalentMultiplanarFormat(*this).MaybeEstimatedSizeInBytes(
          size);
    }
    return MaybeEstimatedPlaneSizeInBytes(0, size);
  }

  base::CheckedNumeric<size_t> total_estimated_bytes = 0;
  for (int plane_index = 0; plane_index < NumberOfPlanes(); ++plane_index) {
    absl::optional<size_t> plane_estimated_bytes =
        MaybeEstimatedPlaneSizeInBytes(plane_index, size);
    if (!plane_estimated_bytes.has_value()) {
      return absl::nullopt;
    }

    total_estimated_bytes += plane_estimated_bytes.value();
    if (!total_estimated_bytes.IsValid()) {
      return absl::nullopt;
    }
  }

  return total_estimated_bytes.ValueOrDie();
}

size_t SharedImageFormat::EstimatedSizeInBytes(const gfx::Size& size) const {
  return MaybeEstimatedSizeInBytes(size).value_or(0);
}

bool SharedImageFormat::VerifySizeInBytes(const gfx::Size& size) const {
  return MaybeEstimatedSizeInBytes(size).has_value();
}

gfx::Size SharedImageFormat::GetPlaneSize(int plane_index,
                                          const gfx::Size& size) const {
  DCHECK(IsValidPlaneIndex(plane_index));
  if (is_single_plane()) {
    return size;
  }

  // Y plane is always size
  if (plane_index == 0) {
    return size;
  }
  // A plane is always size
  if (plane_config() == PlaneConfig::kY_UV_A && plane_index == 2) {
    return size;
  }

  // UV scales
  float width_scale = 1.0;
  float height_scale = 1.0;
  switch (subsampling()) {
    case Subsampling::k420:
      width_scale = 0.5;
      height_scale = 0.5;
      break;
    case Subsampling::k422:
      width_scale = 0.5;
      break;
    case Subsampling::k444:
      break;
  }
  return gfx::ScaleToCeiledSize(size, width_scale, height_scale);
}

// For multiplanar formats.
int SharedImageFormat::NumChannelsInPlane(int plane_index) const {
  DCHECK(IsValidPlaneIndex(plane_index));
  switch (plane_config()) {
    case PlaneConfig::kY_U_V:
    case PlaneConfig::kY_V_U:
      return 1;
    case PlaneConfig::kY_UV:
      return plane_index == 1 ? 2 : 1;
    case PlaneConfig::kY_UV_A:
      return plane_index == 1 ? 2 : 1;
  }
  NOTREACHED();
  return 0;
}

// For multiplanar formats.
int SharedImageFormat::MultiplanarBitDepth() const {
  switch (channel_format()) {
    case ChannelFormat::k8:
      return 8;
    case ChannelFormat::k10:
      return 10;
    case ChannelFormat::k16:
    case ChannelFormat::k16F:
      return 16;
  }
  NOTREACHED();
  return 0;
}

std::string SharedImageFormat::ToString() const {
  switch (plane_type_) {
    case PlaneType::kUnknown:
      return "Unknown";
    case PlaneType::kSinglePlane:
      return SinglePlaneFormatToString(*this);
    case PlaneType::kMultiPlane:
      return base::StringPrintf("(%s, %s, %s, %s)",
                                PlaneConfigToString(plane_config()),
                                SubsamplingToString(subsampling()),
                                ChannelFormatToString(channel_format()),
                                PrefersExternalSamplerToString(*this));
  }
}

std::string SharedImageFormat::ToTestParamString() const {
  switch (plane_type_) {
    case PlaneType::kUnknown:
      return "Unknown";
    case PlaneType::kSinglePlane:
      return SinglePlaneFormatToString(*this);
    case PlaneType::kMultiPlane:
      return base::StringPrintf("%s_%s_%s_%s",
                                PlaneConfigToString(plane_config()),
                                SubsamplingToString(subsampling()),
                                ChannelFormatToString(channel_format()),
                                PrefersExternalSamplerToString(*this));
  }
}

bool SharedImageFormat::HasAlpha() const {
  if (is_single_plane()) {
    switch (singleplanar_format()) {
      case mojom::SingleplanarFormat::RGBA_8888:
      case mojom::SingleplanarFormat::RGBA_4444:
      case mojom::SingleplanarFormat::RGBX_1010102:
      case mojom::SingleplanarFormat::BGRA_8888:
      case mojom::SingleplanarFormat::BGRX_1010102:
      case mojom::SingleplanarFormat::ALPHA_8:
      case mojom::SingleplanarFormat::RGBA_F16:
      case mojom::SingleplanarFormat::NV12A_LEGACY:
        return true;
      default:
        return false;
    }
  }
  switch (plane_config()) {
    case PlaneConfig::kY_U_V:
    case PlaneConfig::kY_V_U:
    case PlaneConfig::kY_UV:
      return false;
    case PlaneConfig::kY_UV_A:
      return true;
  }
}

bool SharedImageFormat::IsCompressed() const {
  return is_single_plane() &&
         singleplanar_format() == mojom::SingleplanarFormat::ETC1;
}

bool SharedImageFormat::IsLegacyMultiplanar() const {
  if (!is_single_plane()) {
    return false;
  }

  switch (singleplanar_format()) {
    case mojom::SingleplanarFormat::YV12_LEGACY:
    case mojom::SingleplanarFormat::NV12_LEGACY:
    case mojom::SingleplanarFormat::NV12A_LEGACY:
    case mojom::SingleplanarFormat::P010_LEGACY:
      return true;
    default:
      return false;
  }
}

int SharedImageFormat::BitsPerPixel() const {
  CHECK(is_single_plane());
  switch (singleplanar_format()) {
    case mojom::SingleplanarFormat::RGBA_F16:
      return 64;
    case mojom::SingleplanarFormat::BGRA_8888:
    case mojom::SingleplanarFormat::RGBA_8888:
    case mojom::SingleplanarFormat::RGBX_8888:
    case mojom::SingleplanarFormat::BGRX_8888:
    case mojom::SingleplanarFormat::RGBX_1010102:
    case mojom::SingleplanarFormat::BGRX_1010102:
    case mojom::SingleplanarFormat::RG16_EXT:
      return 32;
    case mojom::SingleplanarFormat::RGBA_4444:
    case mojom::SingleplanarFormat::RGB_565:
    case mojom::SingleplanarFormat::LUMINANCE_F16:
    case mojom::SingleplanarFormat::R16_EXT:
    case mojom::SingleplanarFormat::BGR_565:
    case mojom::SingleplanarFormat::RG_88:
      return 16;
    case mojom::SingleplanarFormat::ALPHA_8:
    case mojom::SingleplanarFormat::LUMINANCE_8:
    case mojom::SingleplanarFormat::RED_8:
      return 8;
    case mojom::SingleplanarFormat::ETC1:
      return 4;
    case mojom::SingleplanarFormat::P010_LEGACY:
    case mojom::SingleplanarFormat::NV12A_LEGACY:
    case mojom::SingleplanarFormat::YV12_LEGACY:
    case mojom::SingleplanarFormat::NV12_LEGACY:
      // Legacy multiplanar formats are not supported.
      CHECK(0);
  }
  NOTREACHED_NORETURN();
}

bool SharedImageFormat::operator==(const SharedImageFormat& o) const {
  if (plane_type_ != o.plane_type()) {
    return false;
  }

  switch (plane_type_) {
    case PlaneType::kUnknown:
      return true;
    case PlaneType::kSinglePlane:
      return singleplanar_format() == o.singleplanar_format();
    case PlaneType::kMultiPlane:
      return multiplanar_format() == o.multiplanar_format();
  }
}

std::weak_ordering SharedImageFormat::operator<=>(
    const SharedImageFormat& o) const {
  if (plane_type_ != o.plane_type()) {
    return plane_type_ <=> o.plane_type();
  }

  switch (plane_type_) {
    case PlaneType::kUnknown:
      return std::weak_ordering::equivalent;
    case PlaneType::kSinglePlane:
      return singleplanar_format() <=> o.singleplanar_format();
    case PlaneType::kMultiPlane:
      return multiplanar_format() <=> o.multiplanar_format();
  }
}

bool SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat::operator==(
    const MultiplanarFormat& o) const {
  return plane_config == o.plane_config && subsampling == o.subsampling &&
         channel_format == o.channel_format;
}

std::weak_ordering
SharedImageFormat::SharedImageFormatUnion::MultiplanarFormat::operator<=>(
    const MultiplanarFormat& o) const {
  return std::tie(plane_config, subsampling, channel_format) <=>
         std::tie(o.plane_config, o.subsampling, o.channel_format);
}

}  // namespace viz
