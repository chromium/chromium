// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_

#include <stdint.h>

#include <string>

#include "base/check.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/viz_resource_format_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

namespace mojom {
class SharedImageFormatDataView;
class MultiplanarFormatDataView;
}  // namespace mojom

// This class represents the image format used by SharedImages for single plane
// images (eg. RGBA) or multiplanar images (eg. NV12). This format can be
// either ResourceFormat or MultiplanarFormat (PlaneConfig + Subsampling +
// ChannelFormat).
class VIZ_RESOURCE_FORMAT_EXPORT SharedImageFormat {
 public:
  // Specifies how YUV (and optionally A) are divided among planes. Planes are
  // separated by underscores in the enum value names. Within each plane the
  // pixmap/texture channels are mapped to the YUVA channels in the order
  // specified, e.g. for kY_UV Y is in channel 0 of plane 0, U is in channel 0
  // of plane 1, and V is in channel 1 of plane 1.
  enum class PlaneConfig : uint8_t {
    kY_V_U,   // Plane 0: Y, Plane 1: V,  Plane 2: U
    kY_UV,    // Plane 0: Y, Plane 1: UV
    kY_UV_A,  // Plane 0: Y, Plane 1: UV, Plane 2: A
  };

  // UV subsampling is also specified in the enum value names using J:a:b
  // notation (e.g. 4:2:0 is 1/2 horizontal and 1/2 vertical resolution for U
  // and V). If alpha is present it is not subsampled.
  enum class Subsampling : uint8_t {
    k420,  // 1 set of UV values for each 2x2 block of Y values.
  };

  // Specifies the channel format for Y plane in the YUV (and optionally A)
  // plane config. The channel format for remaining planes are identified based
  // on the planes in the PlaneConfig. For individual planes like Y_V_U, U and V
  // are both 8 bit channel formats whereas for Y_UV, the UV plane contains 2
  // channels with each being an 8 bit channel format.
  // TODO(hitawala): Add a helper function that gets the channel format for UV
  // plane.
  enum class ChannelFormat : uint8_t {
    k8,   // 8 bit unorm
    k10,  // 10 bit unorm
    k16,  // 16 bit unorm
    k16F  // 16 bit float
  };

  SharedImageFormat() = default;
  static constexpr SharedImageFormat SinglePlane(
      ResourceFormat resource_format) {
    return SharedImageFormat(resource_format);
  }
  static constexpr SharedImageFormat MultiPlane(PlaneConfig plane_config,
                                                Subsampling subsampling,
                                                ChannelFormat channel_format) {
    return SharedImageFormat(plane_config, subsampling, channel_format);
  }

  ResourceFormat resource_format() const {
    DCHECK(is_single_plane());
    return format_.resource_format;
  }
  PlaneConfig plane_config() const {
    DCHECK(is_multi_plane());
    return format_.multiplanar_format.plane_config;
  }
  Subsampling subsampling() const {
    DCHECK(is_multi_plane());
    return format_.multiplanar_format.subsampling;
  }
  ChannelFormat channel_format() const {
    DCHECK(is_multi_plane());
    return format_.multiplanar_format.channel_format;
  }

  bool is_single_plane() const {
    return plane_type_ == PlaneType::kSinglePlane;
  }
  bool is_multi_plane() const { return plane_type_ == PlaneType::kMultiPlane; }

  // Stub function that always returns false for preferring external sampler.
  // TODO(hitawala): Check if external sampler support is needed for clients and
  // if needed return accordingly.
  bool PrefersExternalSampler() const { return false; }

  // Returns whether the resource format can be used as a software bitmap for
  // export to the display compositor.
  bool IsBitmapFormatSupported() const;

  // Return the number of planes associated with the format.
  int NumberOfPlanes() const;

  // Returns true is `plane_index` is valid.
  bool IsValidPlaneIndex(int plane_index) const;

  // Returns the size for a plane given `plane_index`.
  gfx::Size GetPlaneSize(int plane_index, const gfx::Size& size) const;

  // Returns estimated size in bytes of an image in this format of `size` or
  // nullopt if size in bytes overflows. Includes all planes for multiplanar
  // formats.
  absl::optional<size_t> MaybeEstimatedSizeInBytes(const gfx::Size& size) const;

  // Returns estimated size in bytes for an image in this format of `size` or 0
  // if size in bytes overflows. Includes all planes for multiplanar formats.
  size_t EstimatedSizeInBytes(const gfx::Size& size) const;

  // Returns true if the size in bytes doesn't overflow size_t.
  bool VerifySizeInBytes(const gfx::Size& size) const;

  // Returns number of channels for a plane for multiplanar formats.
  int NumChannelsInPlane(int plane_index) const;

  // Returns the bit depth for multiplanar format based on the channel format.
  int MultiplanarBitDepth() const;

  // Returns a std::string for the format.
  std::string ToString() const;

  // Returns a std::string for the format that is compatible with gtest.
  std::string ToTestParamString() const;

  // Returns true if the format contains alpha.
  bool HasAlpha() const;

  // Returns true if the format is ETC1 compressed.
  bool IsCompressed() const;

  // Returns true if format is legacy multiplanar ResourceFormat i.e.
  // YUV_420_BIPLANAR, YVU_420, YUVA_420_TRIPLANAR, P010.
  bool IsLegacyMultiplanar() const;

  // NOTE: Supported only for true single-plane formats (i.e., formats for
  // which is_single_plane() is true and IsLegacyMultiplanar() is false).
  int BitsPerPixel() const;

  bool operator==(const SharedImageFormat& o) const;
  bool operator!=(const SharedImageFormat& o) const;
  bool operator<(const SharedImageFormat& o) const;

 private:
  enum class PlaneType : uint8_t {
    kUnknown,
    kSinglePlane,
    kMultiPlane,
  };

  union SharedImageFormatUnion {
    // A struct for multiplanar format that is defined by the PlaneConfig,
    // Subsampling and ChannelFormat it holds.
    struct MultiplanarFormat {
      PlaneConfig plane_config;
      Subsampling subsampling;
      ChannelFormat channel_format;

      bool operator==(const MultiplanarFormat& o) const;
      bool operator!=(const MultiplanarFormat& o) const;
      bool operator<(const MultiplanarFormat& o) const;
    };

    SharedImageFormatUnion() = default;
    explicit constexpr SharedImageFormatUnion(ResourceFormat resource_format)
        : resource_format(resource_format) {}
    constexpr SharedImageFormatUnion(PlaneConfig plane_config,
                                     Subsampling subsampling,
                                     ChannelFormat channel_format)
        : multiplanar_format({plane_config, subsampling, channel_format}) {}

    ResourceFormat resource_format;
    MultiplanarFormat multiplanar_format;
  };

  friend struct mojo::UnionTraits<mojom::SharedImageFormatDataView,
                                  SharedImageFormat>;
  friend struct mojo::StructTraits<mojom::MultiplanarFormatDataView,
                                   SharedImageFormatUnion::MultiplanarFormat>;

  explicit constexpr SharedImageFormat(ResourceFormat resource_format)
      : plane_type_(PlaneType::kSinglePlane), format_(resource_format) {}
  constexpr SharedImageFormat(PlaneConfig plane_config,
                              Subsampling subsampling,
                              ChannelFormat channel_format)
      : plane_type_(PlaneType::kMultiPlane),
        format_(plane_config, subsampling, channel_format) {}

  PlaneType plane_type() const { return plane_type_; }
  SharedImageFormatUnion::MultiplanarFormat multiplanar_format() const {
    DCHECK(is_multi_plane());
    return format_.multiplanar_format;
  }

  PlaneType plane_type_ = PlaneType::kUnknown;
  // `format_` can only be ResourceFormat (for single plane, eg. RGBA) or
  // MultiplanarFormat at any given time.
  SharedImageFormatUnion format_;
};

// Constants for common single-planar formats.
namespace SinglePlaneFormat {
inline constexpr SharedImageFormat kRGBA_8888 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGBA_8888);
inline constexpr SharedImageFormat kRGBA_4444 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGBA_4444);
inline constexpr SharedImageFormat kBGRA_8888 =
    SharedImageFormat::SinglePlane(ResourceFormat::BGRA_8888);
inline constexpr SharedImageFormat kALPHA_8 =
    SharedImageFormat::SinglePlane(ResourceFormat::ALPHA_8);
inline constexpr SharedImageFormat kLUMINANCE_8 =
    SharedImageFormat::SinglePlane(ResourceFormat::LUMINANCE_8);
inline constexpr SharedImageFormat kRGB_565 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGB_565);
inline constexpr SharedImageFormat kBGR_565 =
    SharedImageFormat::SinglePlane(ResourceFormat::BGR_565);
inline constexpr SharedImageFormat kETC1 =
    SharedImageFormat::SinglePlane(ResourceFormat::ETC1);
inline constexpr SharedImageFormat kR_8 =
    SharedImageFormat::SinglePlane(ResourceFormat::RED_8);
inline constexpr SharedImageFormat kRG_88 =
    SharedImageFormat::SinglePlane(ResourceFormat::RG_88);
inline constexpr SharedImageFormat kLUMINANCE_F16 =
    SharedImageFormat::SinglePlane(ResourceFormat::LUMINANCE_F16);
inline constexpr SharedImageFormat kRGBA_F16 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGBA_F16);
inline constexpr SharedImageFormat kR_16 =
    SharedImageFormat::SinglePlane(ResourceFormat::R16_EXT);
inline constexpr SharedImageFormat kRG_1616 =
    SharedImageFormat::SinglePlane(ResourceFormat::RG16_EXT);
inline constexpr SharedImageFormat kRGBX_8888 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGBX_8888);
inline constexpr SharedImageFormat kBGRX_8888 =
    SharedImageFormat::SinglePlane(ResourceFormat::BGRX_8888);
inline constexpr SharedImageFormat kRGBA_1010102 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGBA_1010102);
inline constexpr SharedImageFormat kBGRA_1010102 =
    SharedImageFormat::SinglePlane(ResourceFormat::BGRA_1010102);

// All known singleplanar formats.
constexpr SharedImageFormat kAll[18] = {
    kRGBA_8888,     kRGBA_4444,    kBGRA_8888,   kALPHA_8, kLUMINANCE_8,
    kRGB_565,       kBGR_565,      kETC1,        kR_8,     kRG_88,
    kLUMINANCE_F16, kRGBA_F16,     kR_16,        kRG_1616, kRGBX_8888,
    kBGRX_8888,     kRGBA_1010102, kBGRA_1010102};

}  // namespace SinglePlaneFormat

// Constants for legacy single-plane representations of multiplanar formats.
// TODO(crbug.com/1366495): Eliminate these once the codebase is completely
// converted to using MultiplanarSharedImage.
namespace LegacyMultiPlaneFormat {
inline constexpr SharedImageFormat kYV12 =
    SharedImageFormat::SinglePlane(ResourceFormat::YVU_420);
inline constexpr SharedImageFormat kNV12 =
    SharedImageFormat::SinglePlane(ResourceFormat::YUV_420_BIPLANAR);
inline constexpr SharedImageFormat kNV12A =
    SharedImageFormat::SinglePlane(ResourceFormat::YUVA_420_TRIPLANAR);
inline constexpr SharedImageFormat kP010 =
    SharedImageFormat::SinglePlane(ResourceFormat::P010);

// All known legacy multiplanar formats.
constexpr SharedImageFormat kAll[4] = {kYV12, kNV12, kNV12A, kP010};

}  // namespace LegacyMultiPlaneFormat

// The number of singleplanar and legacy multiplanar formats should correspond
// exactly to the number of ResourceFormat types. Note that RESOURCE_FORMAT_MAX
// uses zero-based indexing.
constexpr auto kNumResourceFormatTypes = RESOURCE_FORMAT_MAX + 1;
static_assert(std::size(SinglePlaneFormat::kAll) +
                  std::size(LegacyMultiPlaneFormat::kAll) ==
              kNumResourceFormatTypes);

// Constants for common multi-planar formats.
namespace MultiPlaneFormat {
inline constexpr SharedImageFormat kYV12 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_V_U,
                                  SharedImageFormat::Subsampling::k420,
                                  SharedImageFormat::ChannelFormat::k8);
inline constexpr SharedImageFormat kNV12 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                  SharedImageFormat::Subsampling::k420,
                                  SharedImageFormat::ChannelFormat::k8);
inline constexpr SharedImageFormat kNV12A =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV_A,
                                  SharedImageFormat::Subsampling::k420,
                                  SharedImageFormat::ChannelFormat::k8);
inline constexpr SharedImageFormat kP010 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                  SharedImageFormat::Subsampling::k420,
                                  SharedImageFormat::ChannelFormat::k10);
}  // namespace MultiPlaneFormat

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_
