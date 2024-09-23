// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_chromeos.h"

#include <utility>

#include "base/command_line.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

// When the screen is this width or narrower, the initial browser launched on
// first run will be maximized.
constexpr int kForceMaximizeWidthLimit = 1366;

bool ShouldForceMaximizeOnFirstRun(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kForceMaximizeOnFirstRun);
}

}  // namespace

WindowSizerChromeOS::WindowSizerChromeOS(
    std::unique_ptr<StateProvider> state_provider,
    const Browser* browser)
    : WindowSizer(std::move(state_provider), browser) {}

WindowSizerChromeOS::~WindowSizerChromeOS() = default;

void WindowSizerChromeOS::DetermineWindowBoundsAndShowState(
    const gfx::Rect& specified_bounds,
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) {
  // If we got *both* the bounds and show state, we're done.
  if (GetBrowserBounds(bounds, show_state))
    return;

  // Fall back to cross-platform behavior. Note that |show_state| may have been
  // changed by the function above.
  WindowSizer::DetermineWindowBoundsAndShowState(specified_bounds, bounds,
                                                 show_state);
}

gfx::Rect WindowSizerChromeOS::GetDefaultWindowBounds(
    const display::Display& display) const {
  // Let apps set their own default.
  if (browser() && browser()->app_controller()) {
    gfx::Rect bounds = browser()->app_controller()->GetDefaultBounds();
    if (!bounds.IsEmpty())
      return bounds;
  }

  const gfx::Rect work_area = display.work_area();
  // There should be a 'desktop' border around the window at the left and right
  // side.
  int default_width = work_area.width() - 2 * kDesktopBorderSize;
  // There should also be a 'desktop' border around the window at the top.
  // Since the workspace excludes the tray area we only need one border size.
  int default_height = work_area.height() - kDesktopBorderSize;
  int offset_x = kDesktopBorderSize;
  if (default_width > kMaximumWindowWidth) {
    // The window should get centered on the screen and not follow the grid.
    offset_x = (work_area.width() - kMaximumWindowWidth) / 2;
    default_width = kMaximumWindowWidth;
  }
  return gfx::Rect(work_area.x() + offset_x, work_area.y() + kDesktopBorderSize,
                   default_width, default_height);
}

bool WindowSizerChromeOS::GetBrowserBounds(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  if (!browser())
    return false;

  // This should not be called on a Browser that already has a window.
  DCHECK(!browser()->window());

  bool determined = false;
  if (bounds->IsEmpty()) {
    if (browser()->is_type_normal()) {
      GetTabbedBrowserBounds(bounds, show_state);
      determined = true;
    } else if (browser()->is_trusted_source()) {
      // For trusted popups (v1 apps and system windows), do not use the last
      // active window bounds, only use saved or default bounds.
      // For PWA app windows (which are also a trusted source) we do want to use
      // the last active window bounds.
      if (!browser()->is_type_app() || !browser()->app_controller() ||
          !GetAppBrowserBoundsFromLastActive(bounds, show_state)) {
        if (!browser()->create_params().can_resize ||
            !GetSavedWindowBounds(bounds, show_state)) {
          *bounds = GetDefaultWindowBounds(GetDisplayForNewWindow());
        }
      }
      determined = true;
    } else if (state_provider()) {
      // Finally, prioritize the last saved |show_state|. If you have questions
      // or comments about this behavior please contact oshima@chromium.org.
      gfx::Rect ignored_bounds, ignored_work_area;
      // TODO(ellyjones): This code shouldn't ignore the return value of
      // GetPersistentState()... we could end up using an undefined
      // |show_state|?
      state_provider()->GetPersistentState(&ignored_bounds, &ignored_work_area,
                                           show_state);
      // |determined| is not set here, so we fall back to cross-platform window
      // bounds computation.
    }
  }

  if (browser()->is_type_normal() &&
      *show_state == ui::mojom::WindowShowState::kDefault) {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(*bounds);
    gfx::Rect work_area = display.work_area();
    bounds->AdjustToFit(work_area);
    if (*bounds == work_area) {
      // A browser that occupies the whole work area gets maximized. The
      // |bounds| returned here become the restore bounds once the window
      // gets maximized after this method returns. Return a sensible default
      // in order to make restored state visibly different from maximized.
      *show_state = ui::mojom::WindowShowState::kMaximized;
      *bounds = GetDefaultWindowBounds(display);
      determined = true;
    }
  }
  return determined;
}

void WindowSizerChromeOS::GetTabbedBrowserBounds(
    gfx::Rect* bounds_in_screen,
    ui::mojom::WindowShowState* show_state) const {
  DCHECK(show_state);
  DCHECK(bounds_in_screen);
  DCHECK(browser()->is_type_normal());
  DCHECK(bounds_in_screen->IsEmpty());

  const ui::mojom::WindowShowState passed_show_state = *show_state;

  bool is_saved_bounds = GetSavedWindowBounds(bounds_in_screen, show_state);
  display::Display display = GetDisplayForNewWindow(*bounds_in_screen);
  if (!is_saved_bounds)
    *bounds_in_screen = GetDefaultWindowBounds(display);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (browser()->is_session_restore()) {
    // Respect display for saved bounds during session restore.
    display =
        display::Screen::GetScreen()->GetDisplayMatching(*bounds_in_screen);
  } else if (BrowserList::GetInstance()->empty() && !is_saved_bounds &&
             (ShouldForceMaximizeOnFirstRun(browser()->profile()) ||
              (display.work_area().width() <= kForceMaximizeWidthLimit &&
               !command_line->HasSwitch(
                   switches::kDisableAutoMaximizeForTests)))) {
    // No browsers, no saved bounds: assume first run. Maximize if set by policy
    // or if the screen is narrower than a predetermined size.
    *show_state = ui::mojom::WindowShowState::kMaximized;
  } else {
    // Take the show state from the last active window and copy its restored
    // bounds only if we don't have saved bounds.
    gfx::Rect bounds_copy = *bounds_in_screen;
    ui::mojom::WindowShowState show_state_copy = passed_show_state;
    if (state_provider() && state_provider()->GetLastActiveWindowState(
                                &bounds_copy, &show_state_copy)) {
      *show_state = show_state_copy;
      if (!is_saved_bounds) {
        *bounds_in_screen = bounds_copy;
        bounds_in_screen->Offset(kWindowTilePixels, kWindowTilePixels);
      }
    }
  }

  bounds_in_screen->AdjustToFit(display.work_area());
}

bool WindowSizerChromeOS::GetAppBrowserBoundsFromLastActive(
    gfx::Rect* bounds_in_screen,
    ui::mojom::WindowShowState* show_state) const {
  DCHECK(show_state);
  DCHECK(bounds_in_screen);
  DCHECK(browser()->app_controller());

  if (state_provider() && state_provider()->GetLastActiveWindowState(
                              bounds_in_screen, show_state)) {
    bounds_in_screen->Offset(kWindowTilePixels, kWindowTilePixels);
    // Adjusting bounds_in_screen to fit on the display as returned by
    // GetDisplayForNewWindow here matches behavior for tabbed browsers above.
    // This would mean that we might take into account the size of the last
    // active matching window but ignore the position, if it is on a different
    // display. However the current implementation for GetLastActiveWindowState
    // only looks for windows on the same display, so in practice there should
    // never be a mismatch.
    bounds_in_screen->AdjustToFit(GetDisplayForNewWindow().work_area());
    return true;
  }
  return false;
}
