// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_desktop_window_tree_host_win.h"

#include <windows.h>

#include "base/win/windows_version.h"
#include "chrome/browser/ui/views/apps/app_window_frame_view_win.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_win.h"
#include "ui/base/theme_provider.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/insets_conversions.h"
#include "ui/views/controls/menu/native_menu_win.h"

AppWindowDesktopWindowTreeHostWin::AppWindowDesktopWindowTreeHostWin(
    ChromeNativeAppWindowViewsWin* app_window,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura)
    : DesktopWindowTreeHostWin(app_window->widget(),
                               desktop_native_widget_aura),
      app_window_(app_window) {
}

AppWindowDesktopWindowTreeHostWin::~AppWindowDesktopWindowTreeHostWin() {
}

bool AppWindowDesktopWindowTreeHostWin::GetClientAreaInsets(
    gfx::Insets* insets,
    HMONITOR monitor) const {
  // The inset added below is only necessary for the native glass frame, i.e.
  // not for colored frames drawn by Chrome, or when DWM is disabled.
  // In fullscreen the frame is not visible.
  if (!app_window_->frame_view() || IsFullscreen()) {
    return false;
  }

  *insets = app_window_->frame_view()->GetClientAreaInsets(monitor);

  return true;
}

bool AppWindowDesktopWindowTreeHostWin::GetDwmFrameInsetsInPixels(
    gfx::Insets* insets) const {
  // If there's no frame view we never need to change DWM frame insets.
  if (!GetWidget()->client_view() || !app_window_->frame_view() ||
      !DesktopWindowTreeHostWin::ShouldUseNativeFrame()) {
    return false;
  }

  if (GetWidget()->IsFullscreen()) {
    *insets = gfx::Insets();
  } else {
    // If the opaque frame is visible, we use the default (zero) margins.
    // Otherwise, we need to figure out how to extend the glass in.
    *insets = app_window_->frame_view()->GetInsets();
    // The DWM API's expect values in pixels. We need to convert from DIP to
    // pixels here.
    *insets = gfx::ToFlooredInsets(
        gfx::ConvertInsetsToPixels(*insets, display::win::GetDPIScale()));
  }
  return true;
}

void AppWindowDesktopWindowTreeHostWin::HandleFrameChanged() {
  // We need to update the glass region on or off before the base class adjusts
  // the window region.
  app_window_->OnCanHaveAlphaEnabledChanged();
  DesktopWindowTreeHostWin::HandleFrameChanged();
}
