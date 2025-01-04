// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/views/glic/glic_web_view.h"
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
class BrowserFrameBoundsChangeAnimation;

namespace glic {

class GlicView : public views::View {
 public:
  GlicView(Profile* profile, const gfx::Size& initial_size);
  GlicView(const GlicView&) = delete;
  GlicView& operator=(const GlicView&) = delete;
  ~GlicView() override;

  // Creates a menu widget that contains a `GlicView`, configured with the
  // given `initial_bounds`.
  static views::UniqueWidgetPtr CreateWidget(Profile* profile,
                                             const gfx::Rect& initial_bounds);
  // Returns the `GlicView` from the widget returned by
  // `GlicView::CreateWidget()`.
  static GlicView* FromWidget(views::Widget& widget);

  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  bool IsPointWithinDraggableArea(const gfx::Point& point);

  views::WebView* web_view() { return web_view_; }

  // Sets the bounds of the widget with animation
  void AnimateFrameBounds(const gfx::Rect& bounds);

 private:
  raw_ptr<GlicWebView> web_view_;
  // Defines the areas of the view from which it can be dragged. These areas can
  // be updated by the glic web client.
  std::vector<gfx::Rect> draggable_areas_;

  // Animates programmatic changes to bounds (e.g. via `resizeTo()`
  // `resizeBy()` and `setContentsSize()` calls).
  std::unique_ptr<BrowserFrameBoundsChangeAnimation> bounds_change_animation_;

  // Ensures that the profile associated with this view isn't destroyed while
  // it is visible.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
