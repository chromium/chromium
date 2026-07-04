// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_GUIDED_SETTER_OVERLAY_WINDOW_WIN_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_GUIDED_SETTER_OVERLAY_WINDOW_WIN_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_ui_types.h"

namespace views {
class View;
class Widget;
}  // namespace views

// A click-through, layered, topmost window used to draw the arrow that guides
// the user from the WebUI stage to the Settings "Set default" button. It owns
// no application state; the controller feeds it absolute screen coordinates.
class GuidedSetterOverlayWindowWin {
 public:
  explicit GuidedSetterOverlayWindowWin(gfx::NativeWindow parent_widget);

  GuidedSetterOverlayWindowWin(const GuidedSetterOverlayWindowWin&) = delete;
  GuidedSetterOverlayWindowWin& operator=(const GuidedSetterOverlayWindowWin&) =
      delete;

  ~GuidedSetterOverlayWindowWin();

  void Hide();

  // `bounds_screen` is the window rect (physical screen coords); `start_screen`
  // and `end_screen` are the arrow endpoints (physical screen coords). They
  // will be converted to DIPs for widget positioning and arrow rendering.
  void UpdateAndShow(const gfx::Rect& bounds_screen,
                     const gfx::Point& start_screen,
                     const gfx::Point& end_screen);

  void SetArrowColor(SkColor color);

  views::Widget* widget_for_testing() const { return widget_.get(); }

 private:
  void CreateWidget(gfx::NativeWindow parent_context);

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> arrow_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_GUIDED_SETTER_OVERLAY_WINDOW_WIN_H_
