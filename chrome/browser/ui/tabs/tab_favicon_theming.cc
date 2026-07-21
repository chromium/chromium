// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_favicon_theming.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace tabs {

gfx::ImageSkia ThemeFaviconForTab(const gfx::ImageSkia& source,
                                  const ui::ColorProvider& color_provider) {
  return favicon::ThemeFavicon(
      source, color_provider.GetColor(kColorToolbarButtonIcon),
      color_provider.GetColor(kColorTabBackgroundActiveFrameActive),
      color_provider.GetColor(kColorTabBackgroundInactiveFrameActive));
}

gfx::ImageSkia ThemeMonochromeFaviconForTab(
    const gfx::ImageSkia& source,
    const ui::ColorProvider& color_provider,
    bool is_active) {
  return favicon::ThemeMonochromeFavicon(
      source, color_provider.GetColor(
                  is_active ? kColorTabBackgroundActiveFrameActive
                            : kColorTabBackgroundInactiveFrameActive));
}

}  // namespace tabs
