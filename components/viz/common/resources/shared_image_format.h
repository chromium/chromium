// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_

#include <stdint.h>

#include "base/check.h"
#include "base/check_op.h"
#include "components/viz/common/resources/resource_format.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace viz {

namespace mojom {
class SharedImageFormatDataView;
class MultiplanarFormatDataView;
}  // namespace mojom

// This class represents the image format used by SharedImages for single plane
// images (eg. RGBA) or multiplanar images (eg. NV12). This format can be
// either ResourceFormat or MultiplanarFormat (PlaneConfig + Subsampling +
// ChannelFormat).
class SharedImageFormat {
 public:
  /**
   * Specifies how YUV (and optionally A) are divided among planes. Planes are
   * separated by underscores in the enum value names. Within each plane the
   * pixmap/texture channels are mapped to the YUVA channels in the order
   * specified, e.g. for kY_UV Y is in channel 0 of plane 0, U is in channel 0
   * of plane 1, and V is in channel 1 of plane 1. Channel ordering within a
   * pixmap/texture given the channels it contains:
   * A:                       0:A
   * Luminance/Gray:          0:Gray
   * Luminance/Gray + Alpha:  0:Gray, 1:A
   * RG                       0:R,    1:G
   * RGB                      0:R,    1:G, 2:B
   * RGBA                     0:R,    1:G, 2:B, 3:A
   */
  enum class PlaneConfig : uint8_t {
    kY_V_U,  ///< Plane 0: Y, Plane 1: V,  Plane 2: U
    kY_UV,   ///< Plane 0: Y, Plane 1: UV
  };

  /**
   * UV subsampling is also specified in the enum value names using J:a:b
   * notation (e.g. 4:2:0 is 1/2 horizontal and 1/2 vertical resolution for U
   * and V). If alpha is present it is not sub- sampled. Note that Subsampling
   * values other than k444 are only valid with PlaneConfig values that have U
   * and V in different planes than Y (and A, if present).
   */
  enum class Subsampling : uint8_t {
    k420,  ///< 1 set of UV values for each 2x2 block of Y values.
  };

  /**
   * 8 bit, 10 bit, 16 bit unorn, 16 bit float channel formats.
   * Specifies the channel format for Y plane in the YUV (and optionally A)
   * plane config. The channel format for remaining planes are identified based
   * on the planes in the PlaneConfig. For individual planes like Y_V_U, U and V
   * are both 8 bit channel formats whereas for Y_UV, the UV plane contains 2
   * channels with each being an 8 bit channel format.
   * TODO(hitawala): Add a helper function that gets the channel format for UV
   * plane.
   */
  enum class ChannelFormat : uint8_t { k8, k10, k16, k16F };

  static const SharedImageFormat kRGBA_8888;

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

  // Returns whether the resource format can be used as a software bitmap for
  // export to the display compositor.
  bool IsBitmapFormatSupported() const;

  const char* ToString() const;

  bool operator==(const SharedImageFormat& o) const {
    if (plane_type_ != o.plane_type())
      return false;

    switch (plane_type_) {
      case PlaneType::kUnknown:
        return true;
      case PlaneType::kSinglePlane:
        return resource_format() == o.resource_format();
      case PlaneType::kMultiPlane:
        return multiplanar_format() == o.multiplanar_format();
    }
  }
  bool operator!=(const SharedImageFormat& o) const { return !operator==(o); }

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

      bool operator==(const MultiplanarFormat& o) const {
        return plane_config == o.plane_config && subsampling == o.subsampling &&
               channel_format == o.channel_format;
      }
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

static_assert(sizeof(SharedImageFormat) <= 8);

constexpr SharedImageFormat SharedImageFormat::kRGBA_8888 =
    SharedImageFormat::SinglePlane(ResourceFormat::RGBA_8888);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_IMAGE_FORMAT_H_
