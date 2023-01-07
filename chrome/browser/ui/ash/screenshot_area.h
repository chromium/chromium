// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SCREENSHOT_AREA_H_
#define CHROME_BROWSER_UI_ASH_SCREENSHOT_AREA_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
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
  const absl::optional<const gfx::Rect> rect;

 private:
  ScreenshotArea(ScreenshotType type,
                 const aura::Window* window,
                 absl::optional<const gfx::Rect> rect);
};

#endif  // CHROME_BROWSER_UI_ASH_SCREENSHOT_AREA_H_
