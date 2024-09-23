// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "components/viz/common/color_space_utils.h"

#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"

namespace viz {

// static
gfx::ColorSpace ColorSpaceUtils::OutputColorSpace(
    const gfx::DisplayColorSpaces& display_color_spaces,
    gfx::ContentColorUsage content_color_usage,
    bool has_transparent_background) {
  return display_color_spaces
      .GetOutputColorSpace(content_color_usage, has_transparent_background)
      .GetWithSdrWhiteLevel(display_color_spaces.GetSDRMaxLuminanceNits());
}

// static
gfx::ColorSpace ColorSpaceUtils::CompositingColorSpace(
    const gfx::DisplayColorSpaces& display_color_spaces,
    gfx::ContentColorUsage content_color_usage,
    bool has_transparent_background) {
  return display_color_spaces
      .GetCompositingColorSpace(has_transparent_background, content_color_usage)
      .GetWithSdrWhiteLevel(display_color_spaces.GetSDRMaxLuminanceNits());
}

}  // namespace viz
