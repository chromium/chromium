// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/shared_image_format.h"

#include "base/notreached.h"

namespace viz {

bool SharedImageFormat::IsBitmapFormatSupported() const {
  return is_single_plane() && resource_format() == RGBA_8888;
}

// TODO(hitawala): Add support for multiplanar formats.
const char* SharedImageFormat::ToString() const {
  DCHECK(is_single_plane());
  switch (resource_format()) {
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
    case ResourceFormat::P010:
      return "P010";
  }
  NOTREACHED();
}

}  // namespace viz
