// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_

#include <stdint.h>

#include <compare>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/component_export.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "services/viz/public/mojom/compositing/internal/singleplanar_format.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

class SinglePlaneFormat;

namespace mojom {
class SharedImageFormatDataView;
class MultiplanarFormatDataView;
}  // namespace mojom

// This class represents the image format used by SharedImages for single plane
// images (eg. RGBA) or multiplanar images (eg. NV12). This format can be
// either SingleplanarFormat or MultiplanarFormat (PlaneConfig + Subsampling +
// ChannelFormat).
class COMPONENT_EXPORT(VIZ_SHARED_IMAGE_FORMAT) SharedImageFormat final {
 public:
  // Specifies how YUV (and optionally A) are divided among planes. Planes are
  // separated by underscores in the enum value names. Within each plane the
  // pixmap/texture channels are mapped to the YUVA channels in the order
  // specified, e.g. for kY_UV Y is in channel 0 of plane 0, U is in channel 0
  // of plane 1, and V is in channel 1 of plane 1.
  enum class PlaneConfig : uint8_t {
    kY_U_V,    // Plane 0: Y, Plane 1: U,  Plane 2: V
    kY_V_U,    // Plane 0: Y, Plane 1: V,  Plane 2: U
    kY_UV,     // Plane 0: Y, Plane 1: UV
    kY_UV_A,   // Plane 0: Y, Plane 1: UV, Plane 2: A
    kY_U_V_A,  // Plane 0: Y, Plane 1: U,  Plane 2: V, Plane 3: A
  };

  // UV subsampling is also specified in the enum value names using J:a:b
  // notation (e.g. 4:2:0 is 1/2 horizontal and 1/2 vertical resolution for U
  // and V). If alpha is present it is not subsampled.
  enum class Subsampling : uint8_t {
    k420,  // 1 set of UV values for each 2x2 block of Y values.
    k422,  // 1 set of UV values for each 2x1 block of Y values.
    k444,  // No subsampling. UV values for each Y.
  };

  // Specifies the channel format for Y plane in the YUV (and optionally A)
  // plane config. The channel format for remaining planes are identified based
  // on the planes in the PlaneConfig. For individual planes like Y_V_U, U and V
  // are both 8 bit channel formats whereas for Y_UV, the UV plane contains 2
  // channels with each being an 8 bit channel format.
  enum class ChannelFormat : uint8_t {
    k8,   // 8 bit unorm
    k10,  // 10 bit unorm
    k16,  // 16 bit unorm
    k16F  // 16 bit float
  };

  SharedImageFormat() = default;
  static constexpr SharedImageFormat MultiPlane(PlaneConfig plane_config,
                                                Subsampling subsampling,
                                                ChannelFormat channel_format) {
    return SharedImageFormat(plane_config, subsampling, channel_format);
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

  // Returns whether this format needs to be externally sampled. Note that
  // external sampling is supported only on Ozone.
  bool PrefersExternalSampler() const {
#if BUILDFLAG(IS_OZONE)
    return is_multi_plane()
               ? format_.multiplanar_format.prefers_external_sampler
               : false;
#else
    return false;
#endif
  }

#if BUILDFLAG(IS_OZONE)
  // Sets this format (which must be multiplanar) as needing external sampling.
  void SetPrefersExternalSampler() {
    CHECK(is_multi_plane());
    format_.multiplanar_format.prefers_external_sampler = true;
  }
#endif

  // Clears this format as needing external sampling. Note that with MappableSI,
  // the type of underlying buffer (native or shared memory) is not known until
  // the shared image is created. This is problematic for clients which needs to
  // call SharedImageFormat::SetPrefersExternalSampler() before creating a
  // shared image. In those cases clients will unconditionally call
  // SharedImageFormat::SetPrefersExternalSampler() before creating a
  // mappableSI. SI will internally take care of clearing it back to false by
  // using this method in case it is determined that the it's backed by shared
  // memory. https://issues.chromium.org/339546249.
  void ClearPrefersExternalSampler() {
#if BUILDFLAG(IS_OZONE)
    CHECK(is_multi_plane() &&
          format_.multiplanar_format.prefers_external_sampler);
    format_.multiplanar_format.prefers_external_sampler = false;
#endif
  }

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
  std::optional<size_t> MaybeEstimatedSizeInBytes(const gfx::Size& size) const;

  // Returns estimated size in bytes for a plane of an image in this format of
  // `size` or nullopt if size in bytes overflows.
  std::optional<size_t> MaybeEstimatedPlaneSizeInBytes(
      int plane_index,
      const gfx::Size& size) const;

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

  // NOTE: Supported only for true single-plane formats.
  int BitsPerPixel() const;

  bool operator==(const SharedImageFormat& o) const;
  std::weak_ordering operator<=>(const SharedImageFormat& o) const;

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
#if BUILDFLAG(IS_OZONE)
      // NOTE: This field is intentionally not used as part of defining equality
      // between two MultiplanarFormat instances as clients should not generally
      // need to care. Clients who need to distinguish for a particular
      // SharedImageFormat `format` should call
      // format.PrefersExternalSampler().
      bool prefers_external_sampler = false;
#endif

      bool operator==(const MultiplanarFormat& o) const;
      std::weak_ordering operator<=>(const MultiplanarFormat& o) const;
    };

    SharedImageFormatUnion() {}
    explicit constexpr SharedImageFormatUnion(
        mojom::SingleplanarFormat singleplanar_format)
        : singleplanar_format(singleplanar_format) {}
    constexpr SharedImageFormatUnion(PlaneConfig plane_config,
                                     Subsampling subsampling,
                                     ChannelFormat channel_format)
        : multiplanar_format({plane_config, subsampling, channel_format}) {}

    mojom::SingleplanarFormat singleplanar_format;
    MultiplanarFormat multiplanar_format;
  };

  friend class SinglePlaneFormat;
  friend struct mojo::UnionTraits<mojom::SharedImageFormatDataView,
                                  SharedImageFormat>;
  friend struct mojo::StructTraits<mojom::MultiplanarFormatDataView,
                                   SharedImageFormatUnion::MultiplanarFormat>;

  explicit constexpr SharedImageFormat(
      mojom::SingleplanarFormat singleplanar_format)
      : plane_type_(PlaneType::kSinglePlane), format_(singleplanar_format) {}
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

  mojom::SingleplanarFormat singleplanar_format() const {
    DCHECK(is_single_plane());
    return format_.singleplanar_format;
  }

  PlaneType plane_type_ = PlaneType::kUnknown;
  // `format_` can only be SingleplanarFormat (for single plane, eg. RGBA) or
  // MultiplanarFormat at any given time.
  SharedImageFormatUnion format_;
};

// Constants for common single-planar formats.
// NOTE: This is a class rather than a namespace so that SharedImageFormat can
// friend it to give it access to the private constructor needed for creating
// these constants.
class SinglePlaneFormat {
 public:
  static constexpr SharedImageFormat kRGBA_8888 =
      SharedImageFormat(mojom::SingleplanarFormat::RGBA_8888);
  static constexpr SharedImageFormat kRGBA_4444 =
      SharedImageFormat(mojom::SingleplanarFormat::RGBA_4444);
  static constexpr SharedImageFormat kBGRA_8888 =
      SharedImageFormat(mojom::SingleplanarFormat::BGRA_8888);
  static constexpr SharedImageFormat kALPHA_8 =
      SharedImageFormat(mojom::SingleplanarFormat::ALPHA_8);
  static constexpr SharedImageFormat kLUMINANCE_8 =
      SharedImageFormat(mojom::SingleplanarFormat::LUMINANCE_8);
  static constexpr SharedImageFormat kRGB_565 =
      SharedImageFormat(mojom::SingleplanarFormat::RGB_565);
  static constexpr SharedImageFormat kBGR_565 =
      SharedImageFormat(mojom::SingleplanarFormat::BGR_565);
  static constexpr SharedImageFormat kETC1 =
      SharedImageFormat(mojom::SingleplanarFormat::ETC1);
  static constexpr SharedImageFormat kR_8 =
      SharedImageFormat(mojom::SingleplanarFormat::R_8);
  static constexpr SharedImageFormat kRG_88 =
      SharedImageFormat(mojom::SingleplanarFormat::RG_88);
  static constexpr SharedImageFormat kLUMINANCE_F16 =
      SharedImageFormat(mojom::SingleplanarFormat::LUMINANCE_F16);
  static constexpr SharedImageFormat kRGBA_F16 =
      SharedImageFormat(mojom::SingleplanarFormat::RGBA_F16);
  static constexpr SharedImageFormat kR_16 =
      SharedImageFormat(mojom::SingleplanarFormat::R_16);
  static constexpr SharedImageFormat kRG_1616 =
      SharedImageFormat(mojom::SingleplanarFormat::RG_1616);
  static constexpr SharedImageFormat kRGBX_8888 =
      SharedImageFormat(mojom::SingleplanarFormat::RGBX_8888);
  static constexpr SharedImageFormat kBGRX_8888 =
      SharedImageFormat(mojom::SingleplanarFormat::BGRX_8888);
  static constexpr SharedImageFormat kRGBA_1010102 =
      SharedImageFormat(mojom::SingleplanarFormat::RGBA_1010102);
  static constexpr SharedImageFormat kBGRA_1010102 =
      SharedImageFormat(mojom::SingleplanarFormat::BGRA_1010102);
  static constexpr SharedImageFormat kR_F16 =
      SharedImageFormat(mojom::SingleplanarFormat::R_F16);

  // All known singleplanar formats.
  static constexpr SharedImageFormat kAll[19] = {
      kRGBA_8888,     kRGBA_4444,    kBGRA_8888,    kALPHA_8, kLUMINANCE_8,
      kRGB_565,       kBGR_565,      kETC1,         kR_8,     kRG_88,
      kLUMINANCE_F16, kRGBA_F16,     kR_16,         kRG_1616, kRGBX_8888,
      kBGRX_8888,     kRGBA_1010102, kBGRA_1010102, kR_F16};
};

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
// NOTE: These formats do not have an equivalent BufferFormat as they are not
// used with GpuMemoryBuffers.
inline constexpr SharedImageFormat kNV16 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                  SharedImageFormat::Subsampling::k422,
                                  SharedImageFormat::ChannelFormat::k8);
inline constexpr SharedImageFormat kNV24 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                  SharedImageFormat::Subsampling::k444,
                                  SharedImageFormat::ChannelFormat::k8);
inline constexpr SharedImageFormat kP210 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                  SharedImageFormat::Subsampling::k422,
                                  SharedImageFormat::ChannelFormat::k10);
inline constexpr SharedImageFormat kP410 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_UV,
                                  SharedImageFormat::Subsampling::k444,
                                  SharedImageFormat::ChannelFormat::k10);
inline constexpr SharedImageFormat kI420 =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V,
                                  SharedImageFormat::Subsampling::k420,
                                  SharedImageFormat::ChannelFormat::k8);
inline constexpr SharedImageFormat kI420A =
    SharedImageFormat::MultiPlane(SharedImageFormat::PlaneConfig::kY_U_V_A,
                                  SharedImageFormat::Subsampling::k420,
                                  SharedImageFormat::ChannelFormat::k8);
}  // namespace MultiPlaneFormat

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_
