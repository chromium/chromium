// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_FAVICON_THEMING_H_
#define CHROME_BROWSER_UI_TABS_TAB_FAVICON_THEMING_H_

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ui {
class ColorProvider;
}  // namespace ui

namespace tabs {

// Recolors a default or "themify" tab favicon (e.g. the default favicon or a
// chrome:// page favicon) so it stays visible against the tab background.
// Wraps favicon::ThemeFavicon() with the tab strip color ids.
gfx::ImageSkia ThemeFaviconForTab(const gfx::ImageSkia& source,
                                  const ui::ColorProvider& color_provider);

// Recolors a monochrome tab favicon (e.g. a web app icon) to contrast with the
// tab background for the tab's active state. Wraps
// favicon::ThemeMonochromeFavicon().
gfx::ImageSkia ThemeMonochromeFaviconForTab(
    const gfx::ImageSkia& source,
    const ui::ColorProvider& color_provider,
    bool is_active);

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_TAB_FAVICON_THEMING_H_
