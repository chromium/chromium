// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  gfx::NativeWindow window =
      display::Screen::GetScreen()->GetWindowAtScreenPoint(screen_point);
  for (; window; window = window->parent()) {
    if (views::Widget::GetWidgetForNativeWindow(window))
      break;
  }
  return (window && !base::Contains(ignore, window)) ? window : nullptr;
}
