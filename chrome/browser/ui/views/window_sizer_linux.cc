// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/window_sizer_linux.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_linux.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/ozone/public/ozone_platform.h"

// static
void WindowSizer::GetBrowserWindowBoundsAndShowState(
    std::unique_ptr<StateProvider> state_provider,
    const gfx::Rect& specified_bounds,
    const Browser* browser,
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) {
  DCHECK(bounds);
  DCHECK(show_state);
  WindowSizerLinux sizer(std::move(state_provider), browser);
  // Pre-populate the window state with our default.
  *show_state = GetWindowDefaultShowState(browser);
  *bounds = specified_bounds;
  sizer.DetermineWindowBoundsAndShowState(specified_bounds, bounds, show_state);
}

WindowSizerLinux::WindowSizerLinux(
    std::unique_ptr<StateProvider> state_provider,
    const Browser* browser)
    : WindowSizer(std::move(state_provider), browser) {}

WindowSizerLinux::~WindowSizerLinux() = default;

void WindowSizerLinux::AdjustWorkAreaForPlatform(gfx::Rect& work_area) {
  // On Linux it is possible to use client-side decorations that effectively
  // inflate window bounds.  To adjust the window size to the work area
  // properly, we first check if the window is going to have CSD, and if yes,
  // we add the appropriate margins to the work area.
  // See https://crbug.com/1260832
  if (browser() && (!ui::OzonePlatform::GetInstance()
                         ->GetPlatformRuntimeProperties()
                         .supports_server_side_window_decorations ||
                    browser()->profile()->GetPrefs()->GetBoolean(
                        prefs::kUseCustomChromeFrame))) {
    work_area.Inset(gfx::ShadowValue::GetMargin(
        BrowserFrameViewLinux::GetShadowValues(true)));
  }
}
