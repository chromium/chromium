// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/window_util.h"

#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos::wm {

namespace {

gfx::Size GetPreferredFloatedWindowTabletSize(const gfx::Rect& work_area,
                                              bool landscape) {
  // We use the landscape bounds to determine the preferred width and height,
  // even in portrait mode.
  const int landscape_width =
      landscape ? work_area.width() : work_area.height();
  const int landscape_height =
      landscape ? work_area.height() : work_area.width();
  const int preferred_width =
      static_cast<int>(landscape_width * kFloatedWindowTabletWidthRatio);
  const int preferred_height =
      landscape_height * kFloatedWindowTabletHeightRatio;
  return gfx::Size(preferred_width, preferred_height);
}

bool CanFloatWindowInClamshell(aura::Window* window) {
  CHECK(window);

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  if (minimum_size.width() > work_area.width() - 2 * kFloatedWindowPaddingDp ||
      minimum_size.height() >
          work_area.height() - 2 * kFloatedWindowPaddingDp) {
    return false;
  }
  return true;
}

bool CanFloatWindowInTablet(aura::Window* window) {
  return !GetFloatedWindowTabletSize(window).IsEmpty();
}

}  // namespace

bool IsLandscapeOrientationForWindow(aura::Window* window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const OrientationType orientation = RotationToOrientation(
      GetDisplayNaturalOrientation(display), display.rotation());
  return IsLandscapeOrientation(orientation);
}

gfx::Size GetFloatedWindowTabletSize(aura::Window* window) {
  CHECK(window);

  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0) {
    return gfx::Size();
  }

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  const bool landscape = IsLandscapeOrientationForWindow(window);

  const gfx::Size preferred_size =
      GetPreferredFloatedWindowTabletSize(work_area, landscape);
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  if (minimum_size.height() > preferred_size.height()) {
    return gfx::Size();
  }

  const int landscape_width =
      landscape ? work_area.width() : work_area.height();

  // The maximum size for a floated window is half the landscape width minus
  // some space for the split view divider and padding.
  const int maximum_float_width =
      (landscape_width - kSplitviewDividerShortSideLength) / 2 -
      kFloatedWindowPaddingDp * 2;
  if (minimum_size.width() > maximum_float_width) {
    return gfx::Size();
  }

  // For browsers, we need to add some padding to the minimum size since the
  // browser returns a minimum size that makes the omnibox untappable for many
  // websites. However, we don't add this padding if it causes an otherwise
  // floatable window to not be floatable anymore.
  // TODO(b/278769645): Remove this workaround once browser returns a viable
  // minimum size.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const int minimum_width_padding = kBrowserExtraPaddingDp;
#else
  const int minimum_width_padding =
      window->GetProperty(chromeos::kAppTypeKey) == chromeos::AppType::BROWSER
          ? kBrowserExtraPaddingDp
          : 0;
#endif

  // If the preferred width is less than the minimum width, use the minimum
  // width. Add padding to the preferred width if the window is a browser, but
  // don't exceed the maximum float width.
  int width = std::max(preferred_size.width(),
                       minimum_size.width() + minimum_width_padding);
  width = std::min(width, maximum_float_width);
  return gfx::Size(width, preferred_size.height());
}

bool CanFloatWindow(aura::Window* window) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only app window can be floated. All windows on lacros side are expected to
  // be lacros, so this check is not needed.
  if (window->GetProperty(chromeos::kAppTypeKey) ==
      chromeos::AppType::NON_APP) {
    return false;
  }

  if (!window->GetProperty(kSupportsFloatedStateKey)) {
    return false;
  }

  const auto state_type = window->GetProperty(chromeos::kWindowStateTypeKey);
  const bool unresizable =
      (window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0;
  // Windows which occupy the entire display should not be the target of
  // unresizable floating.
  if (unresizable && (state_type == chromeos::WindowStateType::kFullscreen ||
                      state_type == chromeos::WindowStateType::kMaximized)) {
    return false;
  }
#endif

  if (window->GetProperty(aura::client::kZOrderingKey) !=
      ui::ZOrderLevel::kNormal) {
    return false;
  }

  return display::Screen::GetScreen()->InTabletMode()
             ? CanFloatWindowInTablet(window)
             : CanFloatWindowInClamshell(window);
}

}  // namespace chromeos::wm
