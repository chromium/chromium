// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

// When the screen is this width or narrower, the initial browser launched on
// first run will be maximized.
constexpr int kForceMaximizeWidthLimit = 1366;

bool ShouldForceMaximizeOnFirstRun() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (user) {
    return chromeos::ProfileHelper::Get()
        ->GetProfileByUser(user)
        ->GetPrefs()
        ->GetBoolean(prefs::kForceMaximizeOnFirstRun);
  }
  return false;
}

}  // namespace

bool WindowSizer::GetBrowserBoundsAsh(gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) const {
  if (!browser_)
    return false;

  // This should not be called on a Browser that already has a window.
  DCHECK(!browser_->window());

  bool determined = false;
  if (bounds->IsEmpty()) {
    if (browser_->is_type_normal()) {
      GetTabbedBrowserBoundsAsh(bounds, show_state);
      determined = true;
    } else if (browser_->is_trusted_source()) {
      // For trusted popups (v1 apps and system windows), do not use the last
      // active window bounds, only use saved or default bounds.
      if (!GetSavedWindowBounds(bounds, show_state))
        *bounds = GetDefaultWindowBoundsAsh(GetDisplayForNewWindow());
      determined = true;
    } else if (state_provider_) {
      // In Ash, prioritize the last saved |show_state|. If you have questions
      // or comments about this behavior please contact oshima@chromium.org.
      gfx::Rect ignored_bounds, ignored_work_area;
      state_provider_->GetPersistentState(&ignored_bounds, &ignored_work_area,
                                          show_state);
    }
  }

  if (browser_->is_type_normal() && *show_state == ui::SHOW_STATE_DEFAULT) {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(*bounds);
    gfx::Rect work_area = display.work_area();
    bounds->AdjustToFit(work_area);
    if (*bounds == work_area) {
      // A |browser_| that occupies the whole work area gets maximized.
      // |bounds| returned here become the restore bounds once the window
      // gets maximized after this method returns. Return a sensible default
      // in order to make restored state visibly different from maximized.
      *show_state = ui::SHOW_STATE_MAXIMIZED;
      *bounds = GetDefaultWindowBoundsAsh(display);
      determined = true;
    }
  }
  return determined;
}

void WindowSizer::GetTabbedBrowserBoundsAsh(
    gfx::Rect* bounds_in_screen,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  DCHECK(bounds_in_screen);
  DCHECK(browser_->is_type_normal());
  DCHECK(bounds_in_screen->IsEmpty());

  const ui::WindowShowState passed_show_state = *show_state;

  bool is_saved_bounds = GetSavedWindowBounds(bounds_in_screen, show_state);
  display::Display display = GetDisplayForNewWindow(*bounds_in_screen);
  if (!is_saved_bounds)
    *bounds_in_screen = GetDefaultWindowBoundsAsh(display);

  if (browser_->is_session_restore()) {
    // Respect display for saved bounds during session restore.
    display =
        display::Screen::GetScreen()->GetDisplayMatching(*bounds_in_screen);
  } else if (BrowserList::GetInstance()->empty() && !is_saved_bounds &&
             (ShouldForceMaximizeOnFirstRun() ||
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

gfx::Rect WindowSizer::GetDefaultWindowBoundsAsh(
    const display::Display& display) {
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
