// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format.h"

#include <compare>
#include <optional>
#include <type_traits>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"

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
  } else if (format == SinglePlaneFormat::kR_F16) {
    return "R_F16";
  }
  NOTREACHED();
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
    case SharedImageFormat::PlaneConfig::kY_U_V_A:
      return "Y_U_V_A";
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
    case PlaneConfig::kY_U_V_A:
      return 4;
  }
}

bool SharedImageFormat::IsValidPlaneIndex(int plane_index) const {
  return plane_index >= 0 && plane_index < NumberOfPlanes();
}

std::optional<size_t> SharedImageFormat::MaybeEstimatedPlaneSizeInBytes(
    int plane_index,
    const gfx::Size& size) const {
  DCHECK(!size.IsEmpty());

  if (is_single_plane()) {
    DCHECK_EQ(plane_index, 0);

    base::CheckedNumeric<size_t> bits_per_row = BitsPerPixel();
    bits_per_row *= size.width();
    if (!bits_per_row.IsValid()) {
      return std::nullopt;
    }

    base::CheckedNumeric<size_t> estimated_bytes =
        ConvertBitsToBytes(bits_per_row.ValueOrDie());
    estimated_bytes *= size.height();
    if (!estimated_bytes.IsValid()) {
      return std::nullopt;
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
    return std::nullopt;
  }

  return plane_estimated_bytes.ValueOrDie();
}

std::optional<size_t> SharedImageFormat::MaybeEstimatedSizeInBytes(
    const gfx::Size& size) const {
  DCHECK(!size.IsEmpty());

  if (is_single_plane()) {
    return MaybeEstimatedPlaneSizeInBytes(0, size);
  }

  base::CheckedNumeric<size_t> total_estimated_bytes = 0;
  for (int plane_index = 0; plane_index < NumberOfPlanes(); ++plane_index) {
    std::optional<size_t> plane_estimated_bytes =
        MaybeEstimatedPlaneSizeInBytes(plane_index, size);
    if (!plane_estimated_bytes.has_value()) {
      return std::nullopt;
    }

    total_estimated_bytes += plane_estimated_bytes.value();
    if (!total_estimated_bytes.IsValid()) {
      return std::nullopt;
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

  // First plane is always Y plane and it is always size (not subsampled).
  if (plane_index == 0) {
    return size;
  }
  // A plane is always size
  if (plane_config() == PlaneConfig::kY_UV_A && plane_index == 2) {
    return size;
  }
  if (plane_config() == PlaneConfig::kY_U_V_A && plane_index == 3) {
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
    case PlaneConfig::kY_U_V_A:
      return 1;
    case PlaneConfig::kY_UV:
      return plane_index == 1 ? 2 : 1;
    case PlaneConfig::kY_UV_A:
      return plane_index == 1 ? 2 : 1;
  }
  NOTREACHED_IN_MIGRATION();
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
  NOTREACHED_IN_MIGRATION();
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
      case mojom::SingleplanarFormat::RGBA_1010102:
      case mojom::SingleplanarFormat::BGRA_8888:
      case mojom::SingleplanarFormat::BGRA_1010102:
      case mojom::SingleplanarFormat::ALPHA_8:
      case mojom::SingleplanarFormat::RGBA_F16:
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
    case PlaneConfig::kY_U_V_A:
      return true;
  }
}

bool SharedImageFormat::IsCompressed() const {
  return is_single_plane() &&
         singleplanar_format() == mojom::SingleplanarFormat::ETC1;
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
    case mojom::SingleplanarFormat::RGBA_1010102:
    case mojom::SingleplanarFormat::BGRA_1010102:
    case mojom::SingleplanarFormat::RG_1616:
      return 32;
    case mojom::SingleplanarFormat::RGBA_4444:
    case mojom::SingleplanarFormat::RGB_565:
    case mojom::SingleplanarFormat::LUMINANCE_F16:
    case mojom::SingleplanarFormat::R_F16:
    case mojom::SingleplanarFormat::R_16:
    case mojom::SingleplanarFormat::BGR_565:
    case mojom::SingleplanarFormat::RG_88:
      return 16;
    case mojom::SingleplanarFormat::ALPHA_8:
    case mojom::SingleplanarFormat::LUMINANCE_8:
    case mojom::SingleplanarFormat::R_8:
      return 8;
    case mojom::SingleplanarFormat::ETC1:
      return 4;
  }
  NOTREACHED();
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
