// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/status_bubble_views.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/status_bubble_views_browsertest_mac.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/gfx/animation/animation.h"
#include "ui/views/widget/widget.h"

class StatusBubbleViewsTest : public InProcessBrowserTest {
 public:
  StatusBubbleViews* GetBubble() {
    return static_cast<StatusBubbleViews*>(
        browser()->window()->GetStatusBubble());
  }
  views::Widget* GetWidget() { return GetBubble()->popup(); }
  bool IsDestroyPopupTimerRunning() {
    return GetBubble()->IsDestroyPopupTimerRunningForTest();
  }
  gfx::Animation* GetShowHideAnimationForTesting() {
    return GetBubble()->GetShowHideAnimationForTest();
  }
  void SetTaskRunner(base::SequencedTaskRunner* task_runner) {
    GetBubble()->task_runner_ = task_runner;
  }
};

IN_PROC_BROWSER_TEST_F(StatusBubbleViewsTest, WidgetLifetime) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  SetTaskRunner(task_runner.get());

  // The widget does not exist until it needs to be shown.
  StatusBubble* bubble = GetBubble();
  ASSERT_TRUE(bubble);
  EXPECT_FALSE(GetWidget());

  // Setting status text shows the widget.
  bubble->SetStatus(base::ASCIIToUTF16("test"));
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  // Changing status text keeps the widget visible.
  bubble->SetStatus(base::ASCIIToUTF16("foo"));
  EXPECT_TRUE(widget->IsVisible());

  // Setting the URL keeps the widget visible.
  bubble->SetURL(GURL("http://www.foo.com"));
  EXPECT_TRUE(widget->IsVisible());

#if !defined(OS_MACOSX)
  // Clearing the URL and status closes the widget on platforms other than Mac.
  EXPECT_FALSE(IsDestroyPopupTimerRunning());
  bubble->SetStatus(base::string16());
  bubble->SetURL(GURL());
  // The widget is not hidden immediately, instead a task is scheduled. Run that
  // now.
  task_runner->RunPendingTasks();
  // After the task, a timer is created that animates hidden. Advance that.
  ASSERT_TRUE(GetShowHideAnimationForTesting());
  // Advance well past the time for the animation to ensure it completes.
  static_cast<gfx::AnimationContainerElement*>(GetShowHideAnimationForTesting())
      ->Step(base::TimeTicks::Now() + base::TimeDelta::FromMinutes(1));
  // Widget should still exist.
  ASSERT_TRUE(GetWidget());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(IsDestroyPopupTimerRunning());

  // Run until idle, which should trigger deleting the widget.
  task_runner->RunUntilIdle();
  EXPECT_FALSE(IsDestroyPopupTimerRunning());
  EXPECT_FALSE(GetWidget());
#endif
  SetTaskRunner(base::ThreadTaskRunnerHandle::Get().get());
}

// Mac does not delete the widget after a delay, so this test only runs on
// non-mac platforms.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(StatusBubbleViewsTest, ShowHideDestroyShow) {
  scoped_refptr<base::TestSimpleTaskRunner> task_runner =
      base::MakeRefCounted<base::TestSimpleTaskRunner>();
  SetTaskRunner(task_runner.get());

  // The widget does not exist until it needs to be shown.
  StatusBubble* bubble = GetBubble();
  ASSERT_TRUE(bubble);

  // Setting status text shows the widget.
  bubble->SetStatus(base::ASCIIToUTF16("test"));
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  bubble->SetStatus(base::string16());
  // The widget is not hidden immediately, instead a task is scheduled. Run that
  // now.
  task_runner->RunPendingTasks();
  // After the task, a timer is created that animates hidden. Advance that.
  ASSERT_TRUE(GetShowHideAnimationForTesting());
  // Advance well past the time for the animation to ensure it completes.
  static_cast<gfx::AnimationContainerElement*>(GetShowHideAnimationForTesting())
      ->Step(base::TimeTicks::Now() + base::TimeDelta::FromMinutes(1));
  // Widget should still exist.
  ASSERT_TRUE(GetWidget());
  EXPECT_FALSE(widget->IsVisible());
  EXPECT_TRUE(IsDestroyPopupTimerRunning());

  // Run until idle, which should trigger deleting the widget.
  task_runner->RunUntilIdle();
  EXPECT_FALSE(IsDestroyPopupTimerRunning());
  EXPECT_FALSE(GetWidget());

  // Setting status text shows the widget.
  bubble->SetStatus(base::ASCIIToUTF16("test"));
  widget = GetWidget();
  ASSERT_TRUE(widget);
  EXPECT_TRUE(widget->IsVisible());

  SetTaskRunner(base::ThreadTaskRunnerHandle::Get().get());
}
#endif
