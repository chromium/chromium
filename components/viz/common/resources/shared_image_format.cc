// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format.h"

#include <type_traits>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace viz {
namespace {

const char* ResourceFormatToString(ResourceFormat format) {
  switch (format) {
    case ResourceFormat::RGBA_8888:
      return "RGBA_8888";
    case ResourceFormat::RGBA_4444:
      return "RGBA_4444";
    case ResourceFormat::BGRA_8888:
      return "BGRA_8888";
    case ResourceFormat::ALPHA_8:
      return "ALPHA_8";
    case ResourceFormat::LUMINANCE_8:
      return "LUMINANCE_8";
    case ResourceFormat::RGB_565:
      return "RGB_565";
    case ResourceFormat::BGR_565:
      return "BGR_565";
    case ResourceFormat::ETC1:
      return "ETC1";
    case ResourceFormat::RED_8:
      return "RED_8";
    case ResourceFormat::RG_88:
      return "RG_88";
    case ResourceFormat::LUMINANCE_F16:
      return "LUMINANCE_F16";
    case ResourceFormat::RGBA_F16:
      return "RGBA_F16";
    case ResourceFormat::R16_EXT:
      return "R16_EXT";
    case ResourceFormat::RG16_EXT:
      return "RG16_EXT";
    case ResourceFormat::RGBX_8888:
      return "RGBX_8888";
    case ResourceFormat::BGRX_8888:
      return "BGRX_8888";
    case ResourceFormat::RGBA_1010102:
      return "RGBA_1010102";
    case ResourceFormat::BGRA_1010102:
      return "BGRA_1010102";
    case ResourceFormat::YVU_420:
      return "YVU_420";
    case ResourceFormat::YUV_420_BIPLANAR:
      return "YUV_420_BIPLANAR";
    case ResourceFormat::YUVA_420_TRIPLANAR:
      return "YUVA_420_TRIPLANAR";
    case ResourceFormat::P010:
      return "P010";
  }
}

const char* PlaneConfigToString(SharedImageFormat::PlaneConfig plane) {
  switch (plane) {
    case SharedImageFormat::PlaneConfig::kY_V_U:
      return "Y+V+U";
    case SharedImageFormat::PlaneConfig::kY_UV:
      return "Y+UV";
    case SharedImageFormat::PlaneConfig::kY_UV_A:
      return "Y+UV+A";
  }
}
const char* SubsamplingToString(SharedImageFormat::Subsampling subsampling) {
  switch (subsampling) {
    case SharedImageFormat::Subsampling::k420:
      return "4:2:0";
  }
}

const char* ChannelFormatToString(SharedImageFormat::ChannelFormat channel) {
  switch (channel) {
    case SharedImageFormat::ChannelFormat::k8:
      return "8 unorm";
    case SharedImageFormat::ChannelFormat::k10:
      return "10 unorm";
    case SharedImageFormat::ChannelFormat::k16:
      return "16 unorm";
    case SharedImageFormat::ChannelFormat::k16F:
      return "16 float";
  }
}

}  // namespace

// Ensure that SharedImageFormat is suitable for passing around by value.
static_assert(sizeof(SharedImageFormat) <= 8);
static_assert(std::is_trivially_destructible<SharedImageFormat>::value);

bool SharedImageFormat::IsBitmapFormatSupported() const {
  return is_single_plane() && resource_format() == RGBA_8888;
}

int SharedImageFormat::NumberOfPlanes() const {
  if (is_single_plane())
    return 1;
  if (is_multi_plane()) {
    switch (plane_config()) {
      case PlaneConfig::kY_V_U:
        return 3;
      case PlaneConfig::kY_UV:
        return 2;
      case PlaneConfig::kY_UV_A:
        return 3;
    }
  }
  NOTREACHED();
  return 0;
}

std::string SharedImageFormat::ToString() const {
  switch (plane_type_) {
    case PlaneType::kUnknown:
      return "Unknown";
    case PlaneType::kSinglePlane:
      return ResourceFormatToString(resource_format());
    case PlaneType::kMultiPlane:
      return base::StringPrintf("(%s, %s, %s)",
                                PlaneConfigToString(plane_config()),
                                SubsamplingToString(subsampling()),
                                ChannelFormatToString(channel_format()));
  }
}

bool SharedImageFormat::operator==(const SharedImageFormat& o) const {
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

bool SharedImageFormat::operator!=(const SharedImageFormat& o) const {
  return !operator==(o);
}

}  // namespace viz
