// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/numerics/ranges.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

// Minimum height of the visible part of a window.
const int kMinVisibleHeight = 30;
// Minimum width of the visible part of a window.
const int kMinVisibleWidth = 30;

///////////////////////////////////////////////////////////////////////////////
// An implementation of WindowSizer::StateProvider that gets the last active
// and persistent state from the browser window and the user's profile.
class DefaultStateProvider : public WindowSizer::StateProvider {
 public:
  DefaultStateProvider(const std::string& app_name, const Browser* browser)
      : app_name_(app_name), browser_(browser) {
  }

  // Overridden from WindowSizer::StateProvider:
  bool GetPersistentState(gfx::Rect* bounds,
                          gfx::Rect* work_area,
                          ui::WindowShowState* show_state) const override {
    DCHECK(bounds);
    DCHECK(show_state);

    if (!browser_ || !browser_->profile()->GetPrefs())
      return false;

    const base::DictionaryValue* wp_pref =
        chrome::GetWindowPlacementDictionaryReadOnly(
            chrome::GetWindowName(browser_), browser_->profile()->GetPrefs());
    int top = 0, left = 0, bottom = 0, right = 0;
    bool maximized = false;
    bool has_prefs = wp_pref &&
                     wp_pref->GetInteger("top", &top) &&
                     wp_pref->GetInteger("left", &left) &&
                     wp_pref->GetInteger("bottom", &bottom) &&
                     wp_pref->GetInteger("right", &right) &&
                     wp_pref->GetBoolean("maximized", &maximized);
    bounds->SetRect(left, top, std::max(0, right - left),
                    std::max(0, bottom - top));

    int work_area_top = 0;
    int work_area_left = 0;
    int work_area_bottom = 0;
    int work_area_right = 0;
    if (wp_pref) {
      wp_pref->GetInteger("work_area_top", &work_area_top);
      wp_pref->GetInteger("work_area_left", &work_area_left);
      wp_pref->GetInteger("work_area_bottom", &work_area_bottom);
      wp_pref->GetInteger("work_area_right", &work_area_right);
      if (*show_state == ui::SHOW_STATE_DEFAULT && maximized)
        *show_state = ui::SHOW_STATE_MAXIMIZED;
    }
    work_area->SetRect(work_area_left, work_area_top,
                      std::max(0, work_area_right - work_area_left),
                      std::max(0, work_area_bottom - work_area_top));

    return has_prefs;
  }

  bool GetLastActiveWindowState(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const override {
    DCHECK(show_state);
    // Applications are always restored with the same position.
    if (!app_name_.empty())
      return false;

    // If a reference browser is set, use its window. Otherwise find last
    // active. Panels are never used as reference browsers as panels are
    // specially positioned.
    BrowserWindow* window = NULL;
    // Window may be null if browser is just starting up.
    if (browser_ && browser_->window()) {
      window = browser_->window();
    } else {
      const BrowserList* browser_list = BrowserList::GetInstance();
      for (auto it = browser_list->begin_last_active();
           it != browser_list->end_last_active(); ++it) {
        Browser* last_active = *it;
        if (last_active && last_active->is_type_normal()) {
          window = last_active->window();
          DCHECK(window);
          break;
        }
      }
    }

    if (window) {
#if defined(OS_CHROMEOS)
      if (window->IsVisible())
        *bounds = window->GetRestoredBounds();
#else
      *bounds = window->GetRestoredBounds();
#endif
      if (*show_state == ui::SHOW_STATE_DEFAULT && window->IsMaximized())
        *show_state = ui::SHOW_STATE_MAXIMIZED;
      return true;
    }

    return false;
  }

 private:
  std::string app_name_;

  // If set, is used as the reference browser for GetLastActiveWindowState.
  const Browser* browser_;
  DISALLOW_COPY_AND_ASSIGN(DefaultStateProvider);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, public:

WindowSizer::WindowSizer(std::unique_ptr<StateProvider> state_provider,
                         const Browser* browser)
    : state_provider_(std::move(state_provider)), browser_(browser) {}

WindowSizer::~WindowSizer() = default;

// static
void WindowSizer::GetBrowserWindowBoundsAndShowState(
    const std::string& app_name,
    const gfx::Rect& specified_bounds,
    const Browser* browser,
    gfx::Rect* window_bounds,
    ui::WindowShowState* show_state) {
  std::unique_ptr<StateProvider> state_provider(
      new DefaultStateProvider(app_name, browser));
  const WindowSizer sizer(std::move(state_provider), browser);
  sizer.DetermineWindowBoundsAndShowState(specified_bounds,
                                          window_bounds,
                                          show_state);
}

///////////////////////////////////////////////////////////////////////////////
// WindowSizer, private:

void WindowSizer::DetermineWindowBoundsAndShowState(
    const gfx::Rect& specified_bounds,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  DCHECK(bounds);
  DCHECK(show_state);
  // Pre-populate the window state with our default.
  *show_state = GetWindowDefaultShowState();
  *bounds = specified_bounds;

#if defined(OS_CHROMEOS)
  // See if ash should decide the window placement.
  if (GetBrowserBoundsAsh(bounds, show_state))
    return;
#endif

  if (bounds->IsEmpty()) {
    // See if there's last active window's placement information.
    if (GetLastActiveWindowBounds(bounds, show_state))
      return;
    // See if there's saved placement information.
    if (GetSavedWindowBounds(bounds, show_state))
      return;

    // No saved placement, figure out some sensible default size based on
    // the user's screen size.
    GetDefaultWindowBounds(GetDisplayForNewWindow(), bounds);
    return;
  }

  // In case that there was a bound given we need to make sure that it is
  // visible and fits on the screen.
  // Find the size of the work area of the monitor that intersects the bounds
  // of the anchor window. Note: AdjustBoundsToBeVisibleOnMonitorContaining
  // does not exactly what we want: It makes only sure that "a minimal part"
  // is visible on the screen.
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayMatching(*bounds).work_area();
  // Resize so that it fits.
  bounds->AdjustToFit(work_area);
}

bool WindowSizer::GetLastActiveWindowBounds(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  DCHECK(bounds);
  DCHECK(show_state);
  if (!state_provider_.get() ||
      !state_provider_->GetLastActiveWindowState(bounds, show_state))
    return false;
  bounds->Offset(kWindowTilePixels, kWindowTilePixels);
  AdjustBoundsToBeVisibleOnDisplay(
      display::Screen::GetScreen()->GetDisplayMatching(*bounds), gfx::Rect(),
      bounds);
  return true;
}

bool WindowSizer::GetSavedWindowBounds(gfx::Rect* bounds,
                                       ui::WindowShowState* show_state) const {
  DCHECK(bounds);
  DCHECK(show_state);
  gfx::Rect saved_work_area;
  if (!state_provider_.get() ||
      !state_provider_->GetPersistentState(bounds,
                                           &saved_work_area,
                                           show_state))
    return false;
  AdjustBoundsToBeVisibleOnDisplay(GetDisplayForNewWindow(*bounds),
                                   saved_work_area, bounds);
  return true;
}

void WindowSizer::GetDefaultWindowBounds(const display::Display& display,
                                         gfx::Rect* default_bounds) const {
  DCHECK(default_bounds);
#if defined(OS_CHROMEOS)
  *default_bounds = GetDefaultWindowBoundsAsh(display);
  return;
#endif
  gfx::Rect work_area = display.work_area();

  // The default size is either some reasonably wide width, or if the work
  // area is narrower, then the work area width less some aesthetic padding.
  int default_width = std::min(work_area.width() - 2 * kWindowTilePixels,
                               kWindowMaxDefaultWidth);
  int default_height = work_area.height() - 2 * kWindowTilePixels;

#if !defined(OS_MACOSX)
  // For wider aspect ratio displays at higher resolutions, we might size the
  // window narrower to allow two windows to easily be placed side-by-side.
  gfx::Rect screen_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  double width_to_height =
    static_cast<double>(screen_size.width()) / screen_size.height();

  // The least wide a screen can be to qualify for the halving described above.
  static const int kMinScreenWidthForWindowHalving = 1600;
  // We assume 16:9/10 is a fairly standard indicator of a wide aspect ratio
  // computer display.
  if (((width_to_height * 10) >= 16) &&
      work_area.width() > kMinScreenWidthForWindowHalving) {
    // Halve the work area, subtracting aesthetic padding on either side.
    // The padding is set so that two windows, side by side have
    // kWindowTilePixels between screen edge and each other.
    default_width = static_cast<int>(work_area.width() / 2. -
        1.5 * kWindowTilePixels);
  }
#endif  // !defined(OS_MACOSX)
  default_bounds->SetRect(kWindowTilePixels + work_area.x(),
                          kWindowTilePixels + work_area.y(),
                          default_width, default_height);
}

void WindowSizer::AdjustBoundsToBeVisibleOnDisplay(
    const display::Display& display,
    const gfx::Rect& saved_work_area,
    gfx::Rect* bounds) const {
  DCHECK(bounds);

  // If |bounds| is empty, reset to the default size.
  if (bounds->IsEmpty()) {
    gfx::Rect default_bounds;
    GetDefaultWindowBounds(display, &default_bounds);
    if (bounds->height() <= 0)
      bounds->set_height(default_bounds.height());
    if (bounds->width() <= 0)
      bounds->set_width(default_bounds.width());
  }

  // Ensure the minimum height and width.
  bounds->set_height(std::max(kMinVisibleHeight, bounds->height()));
  bounds->set_width(std::max(kMinVisibleWidth, bounds->width()));

  gfx::Rect work_area = display.work_area();
  // Ensure that the title bar is not above the work area.
  if (bounds->y() < work_area.y())
    bounds->set_y(work_area.y());

  // Reposition and resize the bounds if the saved_work_area is different from
  // the current work area and the current work area doesn't completely contain
  // the bounds.
  if (!saved_work_area.IsEmpty() &&
      saved_work_area != work_area &&
      !work_area.Contains(*bounds)) {
    bounds->set_width(std::min(bounds->width(), work_area.width()));
    bounds->set_height(std::min(bounds->height(), work_area.height()));
    bounds->set_x(base::ClampToRange(bounds->x(), work_area.x(),
                                     work_area.right() - bounds->width()));
    bounds->set_y(base::ClampToRange(bounds->y(), work_area.y(),
                                     work_area.bottom() - bounds->height()));
  }

#if defined(OS_MACOSX)
  // Limit the maximum height.  On the Mac the sizer is on the
  // bottom-right of the window, and a window cannot be moved "up"
  // past the menubar.  If the window is too tall you'll never be able
  // to shrink it again.  Windows does not have this limitation
  // (e.g. can be resized from the top).
  bounds->set_height(std::min(work_area.height(), bounds->height()));

  // On mac, we want to be aggressive about repositioning windows that are
  // partially offscreen.  If the window is partially offscreen horizontally,
  // move it to be flush with the left edge of the work area.
  if (bounds->x() < work_area.x() || bounds->right() > work_area.right())
    bounds->set_x(work_area.x());

  // If the window is partially offscreen vertically, move it to be flush with
  // the top of the work area.
  if (bounds->y() < work_area.y() || bounds->bottom() > work_area.bottom())
    bounds->set_y(work_area.y());
#else
  // On non-Mac platforms, we are less aggressive about repositioning.  Simply
  // ensure that at least kMinVisibleWidth * kMinVisibleHeight is visible.
  const int min_y = work_area.y() + kMinVisibleHeight - bounds->height();
  const int min_x = work_area.x() + kMinVisibleWidth - bounds->width();
  const int max_y = work_area.bottom() - kMinVisibleHeight;
  const int max_x = work_area.right() - kMinVisibleWidth;
  bounds->set_y(base::ClampToRange(bounds->y(), min_y, max_y));
  bounds->set_x(base::ClampToRange(bounds->x(), min_x, max_x));
#endif  // defined(OS_MACOSX)
}

ui::WindowShowState WindowSizer::GetWindowDefaultShowState() const {
  if (!browser_)
    return ui::SHOW_STATE_DEFAULT;

  // Only tabbed browsers and dev tools use the command line.
  bool use_command_line =
      browser_->is_type_normal() || browser_->is_type_devtools();

#if defined(USE_AURA)
  // We use the apps save state as well on aura.
  use_command_line = use_command_line || browser_->deprecated_is_app();
#endif

  if (use_command_line && base::CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kStartMaximized)) {
    return ui::SHOW_STATE_MAXIMIZED;
  }

  return browser_->initial_show_state();
}

// static
display::Display WindowSizer::GetDisplayForNewWindow(const gfx::Rect& bounds) {
#if defined(OS_CHROMEOS)
  // Prefer the display where the user last activated a window.
  return display::Screen::GetScreen()->GetDisplayForNewWindows();
#else
  return display::Screen::GetScreen()->GetDisplayMatching(bounds);
#endif  // defined(OS_CHROMEOS)
}
