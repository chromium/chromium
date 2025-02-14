// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_SCREEN_INFO_HEADLESS_SCREEN_INFO_H_
#define COMPONENTS_HEADLESS_SCREEN_INFO_HEADLESS_SCREEN_INFO_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/types/expected.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {

struct HeadlessScreenInfo {
  gfx::Rect bounds = gfx::Rect(800, 600);
  gfx::Insets work_area_insets;
  int color_depth = 24;
  float device_pixel_ratio = 1.0f;
  bool is_internal = false;
  std::string label;
  int rotation = 0;

  bool operator==(const HeadlessScreenInfo& other) const;

  // Parse one or more screen specifications returning a list of headless
  // screen infos or an error string.
  //
  // Example of the screen specifications: { 0,0 800x600 colorDepth=24 }
  //
  // Screen origin and size are the only positional parameters. Both can be
  // omitted.
  //
  // Available named parameters:
  //  colorDepth=24
  //  devicePixelRatio=1
  //  isInternal=0|1|false|true
  //  label='primary monitor'
  //  workAreaLeft=0
  //  workAreaRight=0
  //  workAreaTop=0
  //  workAreaBottom=0
  //  rotation=0|90|180|270
  //
  // The first screen specified is assumed to be the primary screen. If origin
  // is omitted for a secondary screen it will be automatically calculated to
  // position the screen at the right of the previous screen, for example:
  //
  // {800x600}{800x600} is equivalent to specifying:
  //
  // {0,0 800x600} {800,0 800x600}

  static base::expected<std::vector<HeadlessScreenInfo>, std::string>
  FromString(std::string_view screen_info);

  std::string ToString() const;
};

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_SCREEN_INFO_HEADLESS_SCREEN_INFO_H_
