// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class ExclusiveAccessBubbleViewsTest : public ExclusiveAccessTest,
                                       public views::WidgetObserver {
 public:
  ExclusiveAccessBubbleViewsTest() {}

  ExclusiveAccessBubbleViewsTest(const ExclusiveAccessBubbleViewsTest&) =
      delete;
  ExclusiveAccessBubbleViewsTest& operator=(
      const ExclusiveAccessBubbleViewsTest&) = delete;

  void ClearSnooze() {
    GetExclusiveAccessBubbleView()->snooze_until_ = base::TimeTicks::Min();
  }

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    was_observing_in_destroying_ =
        widget->HasObserver(GetExclusiveAccessBubbleView());
    was_destroying_ = true;
    widget->RemoveObserver(this);
  }

 protected:
  bool was_destroying_ = false;
  bool was_observing_in_destroying_ = false;
};

// Simulate obscure codepaths resulting in the bubble Widget being closed before
// the ExclusiveAccessBubbleViews destructor asks for it. If a close bypasses
// the destructor, animations could still be running that attempt to manipulate
// a destroyed Widget and crash.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, NativeClose) {
  EXPECT_FALSE(GetExclusiveAccessBubbleView());
  EnterActiveTabFullscreen();
  EXPECT_TRUE(GetExclusiveAccessBubbleView());

  GetExclusiveAccessBubbleView()->GetView()->GetWidget()->AddObserver(this);

  // Simulate the bubble being closed out from under its controller, which seems
  // to happen in some odd corner cases, like system log-off while the bubble is
  // showing.
  GetExclusiveAccessBubbleView()->GetView()->GetWidget()->CloseNow();
  EXPECT_FALSE(GetExclusiveAccessBubbleView());

  // Verify that teardown is really happening via OnWidgetDestroyed() rather
  // than the usual path via the ExclusiveAccessBubbleViews destructor. Since
  // the destructor always first removes ExclusiveAccessBubbleViews as an
  // observer before starting the close, checking in OnWidgetDestroyed that it's
  // still observing achieves this.
  EXPECT_TRUE(was_observing_in_destroying_);
  EXPECT_TRUE(was_destroying_);
}

// Tests that creating an exclusive access bubble for a download does not crash,
// despite the type being EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE. See
// crbug.com/1472150.
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, CreateForDownload) {
  ExclusiveAccessBubbleViews bubble(
      BrowserView::GetBrowserViewForBrowser(browser()), {.has_download = true},
      base::NullCallback());
  EXPECT_TRUE(IsBubbleDownloadNotification(&bubble));
}

// Ensure the bubble reshows on mouse move events after a suppression period.
// TODO(crbug.com/336399260): Enable on macOS
// TODO(crbug.com/372814576): Enable on Wayland
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE_WAYLAND)
#define MAYBE_ReshowOnMove DISABLED_ReshowOnMove
#else
#define MAYBE_ReshowOnMove ReshowOnMove
#endif
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, MAYBE_ReshowOnMove) {
  // Click on the tab now, so test events are sent to that target later.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Show the bubble, wait for it to hide, and clear the 15min snooze signal.
  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());

    ExclusiveAccessBubbleHideCallback callback =
        base::BindLambdaForTesting([&](ExclusiveAccessBubbleHideReason reason) {
          EXPECT_EQ(reason, ExclusiveAccessBubbleHideReason::kTimeout);
        });
    GetExclusiveAccessManager()->context()->UpdateExclusiveAccessBubble(
        {.url = GURL("http://example.com"),
         .type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION},
        std::move(callback));
    EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());

    task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime * 2);
  }

  FinishExclusiveAccessBubbleAnimation();
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ClearSnooze();

  // The bubble reshows on a mouse move event.
  const auto point = BrowserView::GetBrowserViewForBrowser(browser())
                         ->GetBoundsInScreen()
                         .CenterPoint();
  base::RunLoop move_run_loop;
  ui_controls::SendMouseMoveNotifyWhenDone(point.x(), point.y(),
                                           move_run_loop.QuitClosure());
  move_run_loop.Run();
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

// Ensure the bubble reshows on mouse click events after a suppression period.
// TODO(crbug.com/336399260): Enable on macOS
// TODO(crbug.com/372814576): Enable on Wayland
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_OZONE_WAYLAND)
#define MAYBE_ReshowOnClick DISABLED_ReshowOnClick
#else
#define MAYBE_ReshowOnClick ReshowOnClick
#endif
IN_PROC_BROWSER_TEST_F(ExclusiveAccessBubbleViewsTest, MAYBE_ReshowOnClick) {
  // Click on the tab now, so test events are sent to that target later.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);

  // Show the bubble, wait for it to hide, and clear the 15min snooze signal.
  {
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());

    ExclusiveAccessBubbleHideCallback callback =
        base::BindLambdaForTesting([&](ExclusiveAccessBubbleHideReason reason) {
          EXPECT_EQ(reason, ExclusiveAccessBubbleHideReason::kTimeout);
        });

    GetExclusiveAccessManager()->context()->UpdateExclusiveAccessBubble(
        {.url = GURL("http://example.com"),
         .type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION},
        std::move(callback));
    EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());

    task_runner->FastForwardBy(ExclusiveAccessBubble::kShowTime * 2);
  }

  FinishExclusiveAccessBubbleAnimation();
  EXPECT_FALSE(IsExclusiveAccessBubbleDisplayed());
  ClearSnooze();

  // The bubble reshows on a mouse click event; avoid sending a move event.
  base::RunLoop click_run_loop;
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN, click_run_loop.QuitClosure());
  click_run_loop.Run();
  EXPECT_TRUE(IsExclusiveAccessBubbleDisplayed());
}
