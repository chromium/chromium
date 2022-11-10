// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/window_util.h"

#include "ash/constants/app_types.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/tablet_state.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace chromeos::wm {

namespace {

bool CanFloatWindowInClamshell(aura::Window* window) {
  DCHECK(window);
  DCHECK(features::IsFloatWindowEnabled());

  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0) {
    return false;
  }

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
  DCHECK(window);
  DCHECK(features::IsFloatWindowEnabled());

  if ((window->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanResize) == 0) {
    return false;
  }

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window).work_area();
  const bool landscape = IsLandscapeOrientationForWindow(window);

  const int preferred_height =
      GetPreferredFloatedWindowTabletSize(work_area, landscape).height();
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  if (minimum_size.height() > preferred_height)
    return false;

  const int landscape_width =
      landscape ? work_area.width() : work_area.height();

  // The maximize size for a floated window is half the landscape width minus
  // some space for the split view divider and padding.
  if (minimum_size.width() >
      ((landscape_width - kSplitviewDividerShortSideLength) / 2 -
       kFloatedWindowPaddingDp * 2)) {
    return false;
  }
  return true;
}

}  // namespace

bool IsLandscapeOrientationForWindow(aura::Window* window) {
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const OrientationType orientation = RotationToOrientation(
      GetDisplayNaturalOrientation(display), display.rotation());
  return IsLandscapeOrientation(orientation);
}

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

bool CanFloatWindow(aura::Window* window) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Only app window can be floated. All windows on lacros side are expected to
  // be lacros, so this check is not needed.
  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::NON_APP)) {
    return false;
  }
#endif
  return TabletState::Get()->InTabletMode() ? CanFloatWindowInTablet(window)
                                            : CanFloatWindowInClamshell(window);
}

}  // namespace chromeos::wm
