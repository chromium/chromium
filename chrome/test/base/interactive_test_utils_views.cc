// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/buildflags.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ui_test_utils {

namespace {

// A helper to wait until a view either gains or loses focus.
class ViewFocusWaiter : public views::ViewObserver {
 public:
  ViewFocusWaiter(views::View* view, bool focused)
      : view_(view), target_focused_(focused) {
    view->AddObserver(this);
  }
  ViewFocusWaiter(const ViewFocusWaiter&) = delete;
  ViewFocusWaiter& operator=(const ViewFocusWaiter&) = delete;

  ~ViewFocusWaiter() override { view_->RemoveObserver(this); }

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override {
    if (run_loop_.running() && target_focused_)
      run_loop_.Quit();
  }

  void OnViewBlurred(views::View* observed_view) override {
    if (run_loop_.running() && !target_focused_)
      run_loop_.Quit();
  }

  void Wait() {
    if (view_->HasFocus() != target_focused_)
      run_loop_.Run();
  }

 private:
  base::RunLoop run_loop_;
  raw_ptr<views::View> view_;
  const bool target_focused_;
};

}  // namespace

bool IsViewFocused(const BrowserWindowInterface* browser, ViewID vid) {
  if (vid == VIEW_ID_OMNIBOX) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    if (browser_view && browser_view->GetLocationBar()) {
      OmniboxController* omnibox_controller =
          browser_view->GetLocationBar()->GetOmniboxController();
      if (omnibox_controller && omnibox_controller->edit_model()) {
        return omnibox_controller->edit_model()->has_focus();
      }
    }
  }
  gfx::NativeWindow window = browser->GetWindow()->GetNativeWindow();
  DCHECK(window);
  const views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  const views::FocusManager* focus_manager = widget->GetFocusManager();
  DCHECK(focus_manager);
  return focus_manager->GetFocusedView() &&
         focus_manager->GetFocusedView()->GetID() == vid;
}

void ClickOnView(views::View* view) {
  DCHECK(view);
  base::RunLoop loop;
  MoveMouseToCenterAndClick(view, ui_controls::LEFT,
                            ui_controls::DOWN | ui_controls::UP,
                            loop.QuitClosure());
  loop.Run();
}

void ClickOnView(const BrowserWindowInterface* browser, ViewID vid) {
  ClickOnView(BrowserView::GetBrowserViewForBrowser(browser)->GetViewByID(vid));
}

void FocusView(const BrowserWindowInterface* browser, ViewID vid) {
  views::View* view =
      BrowserView::GetBrowserViewForBrowser(browser)->GetViewByID(vid);
  DCHECK(view);
  view->RequestFocus();
}

void MoveMouseToCenterAndClick(views::View* view,
                               ui_controls::MouseButton button,
                               int button_state,
                               base::OnceClosure closure,
                               int accelerator_state) {
  MoveMouseToCenterWithOffsetAndClick(view, /*offset=*/{}, button, button_state,
                                      std::move(closure), accelerator_state);
}

void MoveMouseToCenterWithOffsetAndClick(views::View* view,
                                         const gfx::Vector2d& offset,
                                         ui_controls::MouseButton button,
                                         int button_state,
                                         base::OnceClosure closure,
                                         int accelerator_state) {
  DCHECK(view);
  DCHECK(view->GetWidget());
  // Complete any in-progress animation before sending the events so that the
  // mouse-event targeting happens reliably, and does not flake because of
  // unreliable animation state.
  ui::Layer* layer = view->GetWidget()->GetLayer();
  if (layer) {
    ui::LayerAnimator* animator = layer->GetAnimator();
    if (animator && animator->is_animating())
      animator->StopAnimating();
  }

  gfx::Point view_center = GetCenterInScreenCoordinates(view);
  view_center += offset;
  ui_controls::SendMouseMoveNotifyWhenDone(
      view_center.x(), view_center.y(),
      base::BindOnce(&internal::ClickTask, button, button_state,
                     std::move(closure), accelerator_state));
}

gfx::Point GetCenterInScreenCoordinates(const views::View* view) {
  gfx::Point center = view->GetLocalBounds().CenterPoint();
  views::View::ConvertPointToScreen(view, &center);
  return center;
}

void WaitForViewFocus(BrowserWindowInterface* browser,
                      ViewID vid,
                      bool focused) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser->GetWindow()->GetNativeWindow());
  views::View* view = widget && widget->GetContentsView()
                          ? widget->GetContentsView()->GetViewByID(vid)
                          : nullptr;
  if (view) {
    WaitForViewFocus(browser, view, focused);
  } else {
    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return IsViewFocused(browser, vid) == focused; }));
  }
}

void WaitForViewFocus(BrowserWindowInterface* browser,
                      views::View* view,
                      bool focused) {
  ASSERT_TRUE(view);
  ViewFocusWaiter(view, focused).Wait();
}

}  // namespace ui_test_utils
