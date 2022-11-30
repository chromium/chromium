// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_window_easy_resize_window_targeter.h"

#include "ui/aura/window.h"
#include "ui/base/base_window.h"

AppWindowEasyResizeWindowTargeter::AppWindowEasyResizeWindowTargeter(
    const gfx::Insets& insets,
    ui::BaseWindow* native_app_window)
    : wm::EasyResizeWindowTargeter(insets, insets),
      native_app_window_(native_app_window) {}

AppWindowEasyResizeWindowTargeter::~AppWindowEasyResizeWindowTargeter() {}

bool AppWindowEasyResizeWindowTargeter::GetHitTestRects(
    aura::Window* window,
    gfx::Rect* rect_mouse,
    gfx::Rect* rect_touch) const {
  // EasyResizeWindowTargeter intercepts events landing at the edges of the
  // window. Since maximized and fullscreen windows can't be resized anyway,
  // skip EasyResizeWindowTargeter so that the web contents receive all mouse
  // events.
  if (native_app_window_->IsMaximized() || native_app_window_->IsFullscreen())
    return WindowTargeter::GetHitTestRects(window, rect_mouse, rect_touch);

  return EasyResizeWindowTargeter::GetHitTestRects(window, rect_mouse,
                                                   rect_touch);
}
