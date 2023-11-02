// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/visibility_timer_tab_helper.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"

class VisibilityTimerTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  VisibilityTimerTabHelperTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(VisibilityTimerTabHelperTest, Delay) {
  bool task_executed = false;
  VisibilityTimerTabHelper::CreateForWebContents(web_contents());
  VisibilityTimerTabHelper::FromWebContents(web_contents())
      ->PostTaskAfterVisibleDelay(FROM_HERE,
                                  base::BindLambdaForTesting([&task_executed] {
                                    EXPECT_FALSE(task_executed);
                                    task_executed = true;
                                  }),
                                  base::Seconds(1));

  EXPECT_FALSE(task_executed);

  // The task will be executed after 1 second, but the timer is reset whenever
  // the tab is not visible, so these 500ms never add up to >= 1 second.
  for (int n = 0; n < 10; n++) {
    web_contents()->WasShown();
    task_environment()->FastForwardBy(base::Milliseconds(500));
    web_contents()->WasHidden();
  }

  EXPECT_FALSE(task_executed);

  // Time elapsed whilst hidden is not counted.
  // n.b. This line also clears out any old scheduled timer tasks. This is
  // important, because otherwise Timer::Reset (triggered by
  // VisibilityTimerTabHelper::WasShown) may choose to re-use an existing
  // scheduled task, and when it fires Timer::RunScheduledTask will call
  // TimeTicks::Now() (which unlike task_environment()->NowTicks(), we can't
  // fake), and miscalculate the remaining delay at which to fire the timer.
  task_environment()->FastForwardBy(base::Days(1));

  EXPECT_FALSE(task_executed);

  // So 500ms is still not enough.
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_FALSE(task_executed);

  // But 5*500ms > 1 second, so it should now be blocked.
  for (int n = 0; n < 4; n++)
    task_environment()->FastForwardBy(base::Milliseconds(500));

  EXPECT_TRUE(task_executed);
}

TEST_F(VisibilityTimerTabHelperTest, TasksAreQueuedInDormantState) {
  std::string tasks_executed;
  VisibilityTimerTabHelper::CreateForWebContents(web_contents());

  VisibilityTimerTabHelper::FromWebContents(web_contents())
      ->PostTaskAfterVisibleDelay(
          FROM_HERE, base::BindLambdaForTesting([&] { tasks_executed += "1"; }),
          base::Seconds(1));
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Milliseconds(500));

  // Add second task. Its timer does not advance until after the first task
  // completes.
  VisibilityTimerTabHelper::FromWebContents(web_contents())
      ->PostTaskAfterVisibleDelay(
          FROM_HERE, base::BindLambdaForTesting([&] { tasks_executed += "2"; }),
          base::Seconds(1));

  web_contents()->WasHidden();
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Milliseconds(990));

  EXPECT_EQ("", tasks_executed);

  task_environment()->FastForwardBy(base::Milliseconds(11));

  EXPECT_EQ("1", tasks_executed);

  task_environment()->FastForwardBy(base::Milliseconds(1000));

  EXPECT_EQ("12", tasks_executed);
}
