// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_

#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/views/glic/glic_web_view.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

class Browser;
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

  void DragFromPoint(gfx::Vector2d mouse_location);

  views::WebView* web_view() { return web_view_; }

 private:
  // observes the pinned target
  class PinnedTargetWidgetObserver : public views::WidgetObserver {
   public:
    explicit PinnedTargetWidgetObserver(GlicView* glic);
    PinnedTargetWidgetObserver(const PinnedTargetWidgetObserver&) = delete;
    PinnedTargetWidgetObserver& operator=(const PinnedTargetWidgetObserver&) =
        delete;
    ~PinnedTargetWidgetObserver() override;
    void SetPinnedTargetWidget(views::Widget* widget);

    void OnWidgetBoundsChanged(views::Widget* widget,
                               const gfx::Rect& new_bounds) override;
    void OnWidgetDestroying(views::Widget* widget) override;

   private:
    const raw_ptr<GlicView> glic_view_;
    raw_ptr<views::Widget> pinned_target_widget_;
  };

  PinnedTargetWidgetObserver pinned_target_widget_observer_{this};

  // Used to monitor key and mouse events from native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;
  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;
  raw_ptr<GlicWebView> web_view_;

  // Ensures that the profile associated with this view isn't destroyed while
  // it is visible, and nor is the browser process.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // If the mouse is in snapping distance of a browser's glic button, it snaps
  // glic to the top right of the browser's glic button.
  void HandleBrowserPinning(gfx::Vector2d mouse_location);

  // When glic is unpinned, reparent to empty holder widget. Initializes the
  // empty holder widget if it hasn't been created yet.g
  void MaybeCreateHolderWindowAndReparent(views::Widget* widget);

  // Moves glic view to the pin target of the specified browser.
  void MoveToBrowserPinTarget(Browser* browser);

  // Empty holder widget to reparent to when unpinned.
  std::unique_ptr<views::Widget> holder_widget_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_UI_VIEWS_GLIC_GLIC_VIEW_H_
