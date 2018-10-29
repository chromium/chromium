// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class ExclusiveAccessBubbleViewsTest : public FullscreenControllerTest,
                                       public views::WidgetObserver {
 public:
  ExclusiveAccessBubbleViewsTest() {}

  ExclusiveAccessBubbleViews* bubble() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view->exclusive_access_bubble();
  }

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    was_observing_in_destroying_ = widget->HasObserver(bubble());
    was_destroying_ = true;
    widget->RemoveObserver(this);
  }

 protected:
  bool was_destroying_ = false;
  bool was_observing_in_destroying_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExclusiveAccessBubbleViewsTest);
};

// Simulate obscure codepaths resulting in the bubble Widget being closed before
// the ExclusiveAccessBubbleViews destructor asks for it. If a close bypasses
// the destructor, animations could still be running that attempt to manipulate
// a destroyed Widget and crash.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, NativeClose) {
  EXPECT_FALSE(bubble());
  EnterActiveTabFullscreen();
  EXPECT_TRUE(bubble());

  bubble()->GetView()->GetWidget()->AddObserver(this);

  // Simulate the bubble being closed out from under its controller, which seems
  // to happen in some odd corner cases, like system log-off while the bubble is
  // showing.
  bubble()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(bubble());

  // Verify that teardown is really happening via OnWidgetDestroyed() rather
  // than the usual path via the ExclusiveAccessBubbleViews destructor. Since
  // the destructor always first removes ExclusiveAccessBubbleViews as an
  // observer before starting the close, checking in OnWidgetDestroyed that it's
  // still observing achieves this.
  EXPECT_TRUE(was_observing_in_destroying_);
  EXPECT_TRUE(was_destroying_);
}
