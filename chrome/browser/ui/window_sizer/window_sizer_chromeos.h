// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_CHROMEOS_H_
#define CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_CHROMEOS_H_

#include <memory>

#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Rect;
}

// Support Chrome OS platform specific window sizing and positioning.
class WindowSizerChromeOS : public WindowSizer {
 public:
  // The number of pixels which are kept free top, left and right when a window
  // gets positioned to its default location. Visible for testing.
  static const int kDesktopBorderSize = 16;

  // Maximum width of a window even if there is more room on the desktop.
  // Visible for testing.
  static const int kMaximumWindowWidth = 1100;

  WindowSizerChromeOS(std::unique_ptr<StateProvider> state_provider,
                      const Browser* browser);
  WindowSizerChromeOS(const WindowSizerChromeOS&) = delete;
  WindowSizerChromeOS& operator=(const WindowSizerChromeOS&) = delete;
  ~WindowSizerChromeOS() override;

  // WindowSizer:
  void DetermineWindowBoundsAndShowState(
      const gfx::Rect& specified_bounds,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) override;
  gfx::Rect GetDefaultWindowBounds(
      const display::Display& display) const override;

 private:
  // Returns true if |bounds| and |show_state| have been fully determined,
  // otherwise returns false (but may still affect |show_state|). If the window
  // is too big to fit in the display work area then the |bounds| are adjusted
  // to default bounds and the |show_state| is adjusted to SHOW_STATE_MAXIMIZED.
  bool GetBrowserBounds(gfx::Rect* bounds,
                        ui::mojom::WindowShowState* show_state) const;

  // Determines the position and size for a tabbed browser window as it gets
  // created. This will be called before other standard placement logic.
  // |show_state| will only be changed if it was set to SHOW_STATE_DEFAULT.
  void GetTabbedBrowserBounds(gfx::Rect* bounds,
                              ui::mojom::WindowShowState* show_state) const;

  // Determines the position and size for an app browser window as it gets
  // created, basing its position on existing browser windows for the same app.
  // Returns false, if no last active app browsers were found.
  bool GetAppBrowserBoundsFromLastActive(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const;
};

#endif  // CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_CHROMEOS_H_
