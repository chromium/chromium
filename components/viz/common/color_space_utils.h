// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_COLOR_SPACE_UTILS_H_
#define COMPONENTS_VIZ_COMMON_COLOR_SPACE_UTILS_H_

#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"

namespace viz {

// A helper class responsible for some color space computations.
class VIZ_COMMON_EXPORT ColorSpaceUtils {
 public:
  static gfx::ColorSpace OutputColorSpace(
      const gfx::DisplayColorSpaces& display_color_spaces,
      gfx::ContentColorUsage content_color_usage,
      bool has_transparent_background);

  static gfx::ColorSpace CompositingColorSpace(
      const gfx::DisplayColorSpaces& display_color_spaces,
      gfx::ContentColorUsage content_color_usage,
      bool has_transparent_background);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_COLOR_SPACE_UTILS_H_
