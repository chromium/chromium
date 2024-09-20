// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_SCREENSHOT_AREA_SCREENSHOT_AREA_H_
#define CHROMEOS_ASH_EXPERIENCES_SCREENSHOT_AREA_SCREENSHOT_AREA_H_

#include <optional>

#include "base/memory/raw_ptr_exclusion.h"
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
  // RAW_PTR_EXCLUSION: Makes `VideoCaptureInfo` a complex struct.
  RAW_PTR_EXCLUSION const aura::Window* window = nullptr;
  const std::optional<const gfx::Rect> rect;

 private:
  ScreenshotArea(ScreenshotType type,
                 const aura::Window* window,
                 std::optional<const gfx::Rect> rect);
};

#endif  // CHROMEOS_ASH_EXPERIENCES_SCREENSHOT_AREA_SCREENSHOT_AREA_H_
