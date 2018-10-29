// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/ui_features.h"
#include "ui/views/focus/focus_manager.h"

namespace ui_test_utils {

bool IsViewFocused(const Browser* browser, ViewID vid) {
  BrowserWindow* browser_window = browser->window();
  DCHECK(browser_window);
  gfx::NativeWindow window = browser_window->GetNativeWindow();
  DCHECK(window);
  const views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  DCHECK(widget);
  const views::FocusManager* focus_manager = widget->GetFocusManager();
  DCHECK(focus_manager);
  DCHECK(focus_manager->GetFocusedView());
  return focus_manager->GetFocusedView()->id() == vid;
}

void ClickOnView(const Browser* browser, ViewID vid) {
  views::View* view =
      BrowserView::GetBrowserViewForBrowser(browser)->GetViewByID(vid);
  DCHECK(view);
  MoveMouseToCenterAndPress(
      view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
      base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
  content::RunMessageLoop();
}

void FocusView(const Browser* browser, ViewID vid) {
  views::View* view =
      BrowserView::GetBrowserViewForBrowser(browser)->GetViewByID(vid);
  DCHECK(view);
  view->RequestFocus();
}

gfx::Point GetCenterInScreenCoordinates(const views::View* view) {
  gfx::Point center(view->width() / 2, view->height() / 2);
  views::View::ConvertPointToScreen(view, &center);
  return center;
}

}  // namespace ui_test_utils
