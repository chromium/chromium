// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/views/help_bubble_view.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget_observer.h"

using user_education::HelpBubbleArrow;
using user_education::HelpBubbleButtonParams;
using user_education::HelpBubbleParams;
using user_education::HelpBubbleView;

namespace {
class TestHelpBubbleView : public HelpBubbleView {
 public:
  using HelpBubbleView::HelpBubbleView;
  using HelpBubbleView::OnWidgetActivationChanged;
};
}  // namespace

// Testing timeouts can be flaky on some platforms without the full browser view
// and its message pump, so we do these tests here rather than in the
// user_education component.
class HelpBubbleViewTimeoutTest : public TestWithBrowserView {
 public:
  HelpBubbleViewTimeoutTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}
  ~HelpBubbleViewTimeoutTest() override = default;

 protected:
  HelpBubbleParams GetBubbleParams() {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;
    return params;
  }

  TestHelpBubbleView* CreateHelpBubbleView(HelpBubbleParams params) {
    return new TestHelpBubbleView(GetHelpBubbleDelegate(),
                                  {browser_view()->contents_container()},
                                  std::move(params));
  }
};

class MockWidgetObserver : public views::WidgetObserver {
 public:
  MOCK_METHOD(void, OnWidgetClosing, (views::Widget*), ());
};

TEST_F(HelpBubbleViewTimeoutTest, DismissOnTimeout) {
  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Seconds(30);
  HelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));
  MockWidgetObserver dismiss_observer;
  EXPECT_CALL(dismiss_observer, OnWidgetClosing(testing::_)).Times(1);
  bubble->GetWidget()->AddObserver(&dismiss_observer);
  task_environment()->FastForwardBy(base::Minutes(1));
  task_environment()->RunUntilIdle();
}

TEST_F(HelpBubbleViewTimeoutTest, NoAutoDismissWithoutTimeout) {
  // Without a button, there is a default timeout; with a button there is none.
  HelpBubbleParams params = GetBubbleParams();
  HelpBubbleButtonParams button_params;
  button_params.text = u"button";
  params.buttons.push_back(std::move(button_params));
  HelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));
  MockWidgetObserver dismiss_observer;
  EXPECT_CALL(dismiss_observer, OnWidgetClosing(testing::_)).Times(0);
  bubble->GetWidget()->AddObserver(&dismiss_observer);
  task_environment()->FastForwardBy(base::Minutes(1));
  task_environment()->RunUntilIdle();
  // WidgetObserver checks if it is in an observer list in its destructor.
  // Need to remove it from widget manually.
  bubble->GetWidget()->RemoveObserver(&dismiss_observer);
}

TEST_F(HelpBubbleViewTimeoutTest, TimeoutCallback) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Seconds(10);
  params.timeout_callback = timeout_callback.Get();

  CreateHelpBubbleView(std::move(params));

  EXPECT_CALL(timeout_callback, Run()).Times(1);
  task_environment()->FastForwardBy(base::Seconds(10));
}

TEST_F(HelpBubbleViewTimeoutTest, NoTimeoutIfSetToZero) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::TimeDelta();
  params.timeout_callback = timeout_callback.Get();

  CreateHelpBubbleView(std::move(params));

  EXPECT_CALL(timeout_callback, Run()).Times(0);

  // Fast forward by a long time to check bubble does not time out.
  task_environment()->FastForwardBy(base::Hours(1));
}

TEST_F(HelpBubbleViewTimeoutTest, RespectsProvidedTimeoutBeforeActivate) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Seconds(20);
  params.timeout_callback = timeout_callback.Get();

  CreateHelpBubbleView(std::move(params));

  EXPECT_CALL(timeout_callback, Run()).Times(0);
  task_environment()->FastForwardBy(base::Seconds(19));

  EXPECT_CALL(timeout_callback, Run()).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(HelpBubbleViewTimeoutTest, RespectsProvidedTimeoutAfterActivate) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Seconds(10);
  params.timeout_callback = timeout_callback.Get();

  EXPECT_CALL(timeout_callback, Run()).Times(0);

  TestHelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));

  task_environment()->FastForwardBy(base::Seconds(9));

  // Simulate bubble activation. We won't actually activate the bubble since
  // bubble visibility and activation don't work well in this mock environment.
  bubble->OnWidgetActivationChanged(bubble->GetWidget(), true);

  // The bubble should not time out since it is active.
  task_environment()->FastForwardBy(base::Seconds(4));

  // Deactivating the widget should restart the timer.
  bubble->OnWidgetActivationChanged(bubble->GetWidget(), false);

  // Wait most of the timeout, but not all of it.
  task_environment()->FastForwardBy(base::Seconds(9));

  EXPECT_CALL(timeout_callback, Run()).Times(1);

  // Finishing the timeout should dismiss the bubble.
  task_environment()->FastForwardBy(base::Seconds(1));
}
