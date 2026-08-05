// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_view_info.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_observer.h"

using user_education::HelpBubbleArrow;
using user_education::HelpBubbleButtonParams;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;
using user_education::HelpBubbleViewInfo;

class HelpBubbleViewTimeoutTest : public InProcessBrowserTest {
 public:
  HelpBubbleViewTimeoutTest() = default;
  ~HelpBubbleViewTimeoutTest() override = default;

 protected:
  HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }

  [[nodiscard]] HelpBubbleViewInfo CreateHelpBubbleView(
      HelpBubbleParams params) {
    return HelpBubbleView::Create(
        GetHelpBubbleDelegate(),
        {BrowserView::GetBrowserViewForBrowser(browser())
             ->contents_container()},
        std::move(params));
  }

  void SimulateActivation(const HelpBubbleViewInfo& info, bool active) {
    static_cast<HelpBubbleView*>(info.bubble_view)
        ->OnWidgetActivationChanged(info.widget.get(), active);
  }
};

IN_PROC_BROWSER_TEST_F(HelpBubbleViewTimeoutTest, DismissOnTimeout) {
  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Milliseconds(200);
  auto info = CreateHelpBubbleView(std::move(params));
  views::test::WidgetDestroyedWaiter(info.widget.get()).Wait();
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewTimeoutTest, NoAutoDismissWithoutTimeout) {
  // Without a button, there is a default timeout; with a button there is none.
  HelpBubbleParams params = GetBubbleParams();
  HelpBubbleButtonParams button_params;
  button_params.text = u"button";
  params.buttons.push_back(std::move(button_params));
  auto info = CreateHelpBubbleView(std::move(params));
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();
  EXPECT_FALSE(info.widget->IsClosed());
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewTimeoutTest, TimeoutCallback) {
  base::RunLoop run_loop;
  base::MockRepeatingClosure timeout_callback;
  EXPECT_CALL(timeout_callback, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Milliseconds(200);
  params.timeout_callback = timeout_callback.Get();

  auto bubble = CreateHelpBubbleView(std::move(params));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewTimeoutTest, NoTimeoutIfSetToZero) {
  base::MockRepeatingClosure timeout_callback;
  EXPECT_CALL(timeout_callback, Run()).Times(0);

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::TimeDelta();
  params.timeout_callback = timeout_callback.Get();

  auto bubble = CreateHelpBubbleView(std::move(params));
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(200));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewTimeoutTest,
                       RespectsProvidedTimeoutBeforeActivate) {
  base::RunLoop run_loop;
  base::MockRepeatingClosure timeout_callback;
  EXPECT_CALL(timeout_callback, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Milliseconds(200);
  params.timeout_callback = timeout_callback.Get();

  auto bubble = CreateHelpBubbleView(std::move(params));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(HelpBubbleViewTimeoutTest,
                       RespectsProvidedTimeoutAfterActivate) {
  base::RunLoop run_loop;
  base::MockRepeatingClosure timeout_callback;
  EXPECT_CALL(timeout_callback, Run())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Milliseconds(200);
  params.timeout_callback = timeout_callback.Get();

  auto info = CreateHelpBubbleView(std::move(params));

  // Simulate bubble activation. We won't actually activate the bubble since
  // bubble visibility and activation don't work well in this mock environment.
  SimulateActivation(info, true);

  // The bubble should not time out since it is active.
  base::PlatformThread::Sleep(base::Milliseconds(300));

  // Deactivating the widget should restart the timer.
  SimulateActivation(info, false);

  run_loop.Run();
}
