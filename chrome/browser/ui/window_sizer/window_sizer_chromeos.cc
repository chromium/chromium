// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_chromeos.h"

#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if !BUILDFLAG(IS_LACROS)
#include "chrome/common/pref_names.h"
#endif

namespace {

// When the screen is this width or narrower, the initial browser launched on
// first run will be maximized.
constexpr int kForceMaximizeWidthLimit = 1366;

bool ShouldForceMaximizeOnFirstRun(Profile* profile) {
#if BUILDFLAG(IS_LACROS)
  // TODO(https://crbug.com/1110548): Support the ForceMaximizeOnFirstRun policy
  // in lacros-chrome.
  return false;
#else
  return profile->GetPrefs()->GetBoolean(prefs::kForceMaximizeOnFirstRun);
#endif
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
    ui::WindowShowState* show_state) {
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
    ui::WindowShowState* show_state) const {
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
      if (!GetSavedWindowBounds(bounds, show_state))
        *bounds = GetDefaultWindowBounds(GetDisplayForNewWindow());
      determined = true;
    } else if (state_provider()) {
      // Finally, prioritize the last saved |show_state|. If you have questions
      // or comments about this behavior please contact oshima@chromium.org.
      gfx::Rect ignored_bounds, ignored_work_area;
      state_provider()->GetPersistentState(&ignored_bounds, &ignored_work_area,
                                           show_state);
      // |determined| is not set here, so we fall back to cross-platform window
      // bounds computation.
    }
  }

  if (browser()->is_type_normal() && *show_state == ui::SHOW_STATE_DEFAULT) {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(*bounds);
    gfx::Rect work_area = display.work_area();
    bounds->AdjustToFit(work_area);
    if (*bounds == work_area) {
      // A browser that occupies the whole work area gets maximized. The
      // |bounds| returned here become the restore bounds once the window
      // gets maximized after this method returns. Return a sensible default
      // in order to make restored state visibly different from maximized.
      *show_state = ui::SHOW_STATE_MAXIMIZED;
      *bounds = GetDefaultWindowBounds(display);
      determined = true;
    }
  }
  return determined;
}

void WindowSizerChromeOS::GetTabbedBrowserBounds(
    gfx::Rect* bounds_in_screen,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  DCHECK(bounds_in_screen);
  DCHECK(browser()->is_type_normal());
  DCHECK(bounds_in_screen->IsEmpty());

  const ui::WindowShowState passed_show_state = *show_state;

  bool is_saved_bounds = GetSavedWindowBounds(bounds_in_screen, show_state);
  display::Display display = GetDisplayForNewWindow(*bounds_in_screen);
  if (!is_saved_bounds)
    *bounds_in_screen = GetDefaultWindowBounds(display);

  if (browser()->is_session_restore()) {
    // Respect display for saved bounds during session restore.
    display =
        display::Screen::GetScreen()->GetDisplayMatching(*bounds_in_screen);
  } else if (BrowserList::GetInstance()->empty() && !is_saved_bounds &&
             (ShouldForceMaximizeOnFirstRun(browser()->profile()) ||
              display.work_area().width() <= kForceMaximizeWidthLimit)) {
    // No browsers, no saved bounds: assume first run. Maximize if set by policy
    // or if the screen is narrower than a predetermined size.
    *show_state = ui::SHOW_STATE_MAXIMIZED;
  } else {
    // Take the show state from the last active window and copy its restored
    // bounds only if we don't have saved bounds.
    gfx::Rect bounds_copy = *bounds_in_screen;
    ui::WindowShowState show_state_copy = passed_show_state;
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
