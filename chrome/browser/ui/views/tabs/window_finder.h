// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_

#include <set>

#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

// Class used by the tabstrip to find chrome windows that we can attach tabs to.
class WindowFinder {
 public:
  WindowFinder() = default;
  virtual ~WindowFinder() = default;

  // Finds the topmost visible chrome window at |screen_point|. This should
  // return nullptr if |screen_point| is in another program's window which
  // occludes the topmost chrome window. Ignores the windows in |ignore|, which
  // contain windows such as the tab being dragged right now.
  virtual gfx::NativeWindow GetLocalProcessWindowAtPoint(
      const gfx::Point& screen_point,
      const std::set<gfx::NativeWindow>& ignore);

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowFinder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_WINDOW_FINDER_H_
