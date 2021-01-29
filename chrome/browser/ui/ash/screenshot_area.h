// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SCREENSHOT_AREA_H_
#define CHROME_BROWSER_UI_ASH_SCREENSHOT_AREA_H_

#include "base/optional.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

// Type of the screenshot mode.
enum class ScreenshotType {
  kAllRootWindows,
  kPartialWindow,
  kWindow,
};

// Structure representing the area of screenshot.
// For kWindow screenshots |window| should be set.
// For kPartialWindow screenshots |rect| and |window| should be set.
struct ScreenshotArea {
  static ScreenshotArea CreateForAllRootWindows();
  static ScreenshotArea CreateForWindow(const aura::Window* window);
  static ScreenshotArea CreateForPartialWindow(const aura::Window* window,
                                               const gfx::Rect rect);

  ScreenshotArea(const ScreenshotArea& area);

  const ScreenshotType type;
  const aura::Window* window = nullptr;
  const base::Optional<const gfx::Rect> rect;

 private:
  ScreenshotArea(ScreenshotType type,
                 const aura::Window* window,
                 base::Optional<const gfx::Rect> rect);
};

#endif  // CHROME_BROWSER_UI_ASH_SCREENSHOT_AREA_H_
