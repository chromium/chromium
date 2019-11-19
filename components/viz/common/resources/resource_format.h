// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_H_

namespace viz {

// If these values are modified, then it is likely that resource_format_utils.cc
// has to be updated as well.
enum ResourceFormat {
  RGBA_8888,
  RGBA_4444,
  BGRA_8888,
  ALPHA_8,
  LUMINANCE_8,
  RGB_565,
  BGR_565,
  ETC1,
  RED_8,
  RG_88,
  LUMINANCE_F16,
  RGBA_F16,
  R16_EXT,
  RGBX_8888,
  BGRX_8888,
  RGBX_1010102,
  BGRX_1010102,
  YVU_420,
  YUV_420_BIPLANAR,
  P010,
  RESOURCE_FORMAT_MAX = P010,
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_RESOURCE_FORMAT_H_
