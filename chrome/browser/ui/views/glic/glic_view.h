// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_

#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class WebView;
}  // namespace views

class Profile;

namespace {
class WindowEventObserver;
}

namespace glic {

class GlicView : public views::View {
 public:
  GlicView(Profile* profile, const gfx::Size& initial_size);
  GlicView(const GlicView&) = delete;
  GlicView& operator=(const GlicView&) = delete;
  ~GlicView() override;

  // Creates a menu widget that contains a `GlicView`, configured with the
  // given `initial_bounds`.
  static std::pair<views::UniqueWidgetPtr, GlicView*> CreateWidget(
      Profile* profile,
      const gfx::Rect& initial_bounds);

  // views::View
  void AddedToWidget() override;

  void DragFromPoint(gfx::Vector2d mousePoint);

  views::WebView* web_view() { return web_view_; }

 private:
  // Used to monitor key and mouse events from native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;
  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;
  raw_ptr<views::WebView> web_view_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
