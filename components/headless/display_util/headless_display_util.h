// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HEADLESS_DISPLAY_UTIL_HEADLESS_DISPLAY_UTIL_H_
#define COMPONENTS_HEADLESS_DISPLAY_UTIL_HEADLESS_DISPLAY_UTIL_H_

#include <optional>
#include <vector>

#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace headless {

// Return display that the screen rectangle belongs or is closest to.
std::optional<display::Display> GetDisplayFromScreenRect(
    const std::vector<display::Display>& displays,
    const gfx::Rect& rect);

// Return display that the screen point is on.
std::optional<display::Display> GetDisplayFromScreenPoint(
    const std::vector<display::Display>& displays,
    const gfx::Point& point);

}  // namespace headless

#endif  // COMPONENTS_HEADLESS_DISPLAY_UTIL_HEADLESS_DISPLAY_UTIL_H_
