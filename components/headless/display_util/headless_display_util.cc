// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/display_util/headless_display_util.h"

#include "base/notimplemented.h"
#include "build/build_config.h"
#include "ui/display/display_finder.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using display::Display;

namespace headless {

std::optional<Display> GetDisplayFromWindow(
    const std::vector<Display>& displays,
    gfx::NativeWindow window) {
#if defined(USE_AURA)
  if (window) {
    const gfx::Rect bounds = window->GetBoundsInScreen();
    return GetDisplayFromScreenRect(displays, bounds);
  }
#else
  NOTIMPLEMENTED_LOG_ONCE();
#endif  // #if defined(USE_AURA)

  return std::nullopt;
}

std::optional<Display> GetDisplayFromScreenRect(
    const std::vector<Display>& displays,
    const gfx::Rect& rect) {
  if (const Display* display =
          display::FindDisplayWithBiggestIntersection(displays, rect)) {
    return *display;
  }

  if (const Display* display =
          display::FindDisplayNearestPoint(displays, rect.CenterPoint())) {
    return *display;
  }

  return std::nullopt;
}

std::optional<Display> GetDisplayFromScreenPoint(
    const std::vector<Display>& displays,
    const gfx::Point& point) {
  if (const Display* display =
          display::FindDisplayNearestPoint(displays, point)) {
    return *display;
  }

  return std::nullopt;
}

}  // namespace headless
