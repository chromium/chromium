// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_EASY_RESIZE_WINDOW_TARGETER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_EASY_RESIZE_WINDOW_TARGETER_H_

#include "base/memory/raw_ptr.h"
#include "ui/wm/core/easy_resize_window_targeter.h"

namespace ui {
class BaseWindow;
}

// An EasyResizeEventTargeter whose behavior depends on the state of the app
// window.
class AppWindowEasyResizeWindowTargeter : public wm::EasyResizeWindowTargeter {
 public:
  AppWindowEasyResizeWindowTargeter(const gfx::Insets& insets,
                                    ui::BaseWindow* native_app_window);

  AppWindowEasyResizeWindowTargeter(const AppWindowEasyResizeWindowTargeter&) =
      delete;
  AppWindowEasyResizeWindowTargeter& operator=(
      const AppWindowEasyResizeWindowTargeter&) = delete;

  ~AppWindowEasyResizeWindowTargeter() override;

 protected:
  // aura::WindowTargeter:
  bool GetHitTestRects(aura::Window* window,
                       gfx::Rect* rect_mouse,
                       gfx::Rect* rect_touch) const override;

 private:
  raw_ptr<ui::BaseWindow> native_app_window_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_EASY_RESIZE_WINDOW_TARGETER_H_
