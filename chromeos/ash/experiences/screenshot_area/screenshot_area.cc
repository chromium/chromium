// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/screenshot_area/screenshot_area.h"

// static
ScreenshotArea ScreenshotArea::CreateForAllRootWindows() {
  return ScreenshotArea(ScreenshotType::kAllRootWindows, nullptr, std::nullopt);
}

// static
ScreenshotArea ScreenshotArea::CreateForWindow(const aura::Window* window) {
  return ScreenshotArea(ScreenshotType::kWindow, window, std::nullopt);
}

// static
ScreenshotArea ScreenshotArea::CreateForPartialWindow(
    const aura::Window* window,
    const gfx::Rect rect) {
  return ScreenshotArea(ScreenshotType::kPartialWindow, window, rect);
}

ScreenshotArea::ScreenshotArea(const ScreenshotArea& area) = default;

ScreenshotArea::ScreenshotArea(ScreenshotType type,
                               const aura::Window* window,
                               std::optional<const gfx::Rect> rect)
    : type(type), window(window), rect(rect) {}
