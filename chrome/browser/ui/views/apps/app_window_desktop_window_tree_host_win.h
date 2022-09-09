// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_win.h"

namespace views {
class DesktopNativeWidgetAura;
}

class ChromeNativeAppWindowViewsWin;

// AppWindowDesktopWindowTreeHostWin handles updating the glass of app frames on
// Windows. It is used for all desktop app windows on Windows, but is only
// actively doing anything when a glass window frame is being used.
class AppWindowDesktopWindowTreeHostWin
    : public views::DesktopWindowTreeHostWin {
 public:
  AppWindowDesktopWindowTreeHostWin(
      ChromeNativeAppWindowViewsWin* app_window,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura);

  AppWindowDesktopWindowTreeHostWin(const AppWindowDesktopWindowTreeHostWin&) =
      delete;
  AppWindowDesktopWindowTreeHostWin& operator=(
      const AppWindowDesktopWindowTreeHostWin&) = delete;

  ~AppWindowDesktopWindowTreeHostWin() override;

 private:
  // Overridden from DesktopWindowTreeHostWin:
  bool GetClientAreaInsets(gfx::Insets* insets,
                           HMONITOR monitor) const override;
  bool GetDwmFrameInsetsInPixels(gfx::Insets* insets) const override;
  void HandleFrameChanged() override;

  raw_ptr<ChromeNativeAppWindowViewsWin> app_window_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_WINDOW_TREE_HOST_WIN_H_
