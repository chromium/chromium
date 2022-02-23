// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/help_bubble_view.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/user_education/feature_promo_specification.h"
#include "chrome/browser/ui/user_education/help_bubble_params.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/data/grit/chrome_test_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget_observer.h"

class HelpBubbleViewTest : public TestWithBrowserView {
 public:
  HelpBubbleViewTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  // If |button_callback| is non-nil, creates the bubble with one button calling
  // that callback. Otherwise has no buttons.
  HelpBubbleParams GetBubbleParams(
      base::RepeatingClosure button_callback = base::DoNothing()) {
    HelpBubbleParams params;
    params.body_text = u"To X, do Y";
    params.arrow = HelpBubbleArrow::kTopRight;

    if (button_callback) {
      HelpBubbleButtonParams button_params;
      button_params.text = u"Go away";
      button_params.is_default = true;
      button_params.callback = std::move(button_callback);
      params.buttons.push_back(std::move(button_params));
    }

    return params;
  }

  HelpBubbleView* CreateHelpBubbleView(HelpBubbleParams params) {
    return new HelpBubbleView(browser_view()->contents_container(),
                              std::move(params));
  }

  HelpBubbleView* CreateHelpBubbleView(
      base::RepeatingClosure button_callback = base::DoNothing()) {
    return CreateHelpBubbleView(GetBubbleParams(button_callback));
  }
};

class MockWidgetObserver : public views::WidgetObserver {
 public:
  MOCK_METHOD(void, OnWidgetClosing, (views::Widget*), ());
};

TEST_F(HelpBubbleViewTest, CallButtonCallback) {
  base::MockRepeatingClosure mock_callback;

  EXPECT_CALL(mock_callback, Run()).Times(1);

  HelpBubbleView* const bubble = CreateHelpBubbleView(mock_callback.Get());

  // Simulate clicks on dismiss button.
  ui::MouseEvent mouse_press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent mouse_release(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  bubble->GetButtonForTesting(0)->OnMouseEvent(&mouse_press);
  bubble->GetButtonForTesting(0)->OnMouseEvent(&mouse_release);

  bubble->GetWidget()->Close();
}

TEST_F(HelpBubbleViewTest, DismissOnTimeout) {
  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Seconds(30);
  HelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));
  MockWidgetObserver dismiss_observer;
  EXPECT_CALL(dismiss_observer, OnWidgetClosing(testing::_)).Times(1);
  bubble->GetWidget()->AddObserver(&dismiss_observer);
  task_environment()->FastForwardBy(base::Minutes(1));
  task_environment()->RunUntilIdle();
}

TEST_F(HelpBubbleViewTest, NoAutoDismissWithoutTimeout) {
  HelpBubbleView* const bubble = CreateHelpBubbleView();
  MockWidgetObserver dismiss_observer;
  EXPECT_CALL(dismiss_observer, OnWidgetClosing(testing::_)).Times(0);
  bubble->GetWidget()->AddObserver(&dismiss_observer);
  task_environment()->FastForwardBy(base::Minutes(1));
  task_environment()->RunUntilIdle();
  // WidgetObserver checks if it is in an observer list in its destructor.
  // Need to remove it from widget manually.
  bubble->GetWidget()->RemoveObserver(&dismiss_observer);
}

TEST_F(HelpBubbleViewTest, TimeoutCallback) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams();
  params.timeout = base::Seconds(10);
  params.timeout_callback = timeout_callback.Get();

  CreateHelpBubbleView(std::move(params));

  EXPECT_CALL(timeout_callback, Run()).Times(1);
  task_environment()->FastForwardBy(base::Seconds(10));
}

TEST_F(HelpBubbleViewTest, NoTimeoutIfSetToZero) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams(base::RepeatingClosure());
  params.timeout = base::TimeDelta();
  params.timeout_callback = timeout_callback.Get();

  CreateHelpBubbleView(std::move(params));

  EXPECT_CALL(timeout_callback, Run()).Times(0);

  // Fast forward by a long time to check bubble does not time out.
  task_environment()->FastForwardBy(base::Hours(1));
}

TEST_F(HelpBubbleViewTest, RespectsProvidedTimeoutBeforeActivate) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams(base::RepeatingClosure());
  params.timeout = base::Seconds(20);
  params.timeout_callback = timeout_callback.Get();

  CreateHelpBubbleView(std::move(params));

  EXPECT_CALL(timeout_callback, Run()).Times(0);
  task_environment()->FastForwardBy(base::Seconds(19));

  EXPECT_CALL(timeout_callback, Run()).Times(1);
  task_environment()->FastForwardBy(base::Seconds(1));
}

TEST_F(HelpBubbleViewTest, RespectsProvidedTimeoutAfterActivate) {
  base::MockRepeatingClosure timeout_callback;

  HelpBubbleParams params = GetBubbleParams(base::RepeatingClosure());
  params.timeout = base::Seconds(10);
  params.timeout_callback = timeout_callback.Get();

  EXPECT_CALL(timeout_callback, Run()).Times(0);

  HelpBubbleView* const bubble = CreateHelpBubbleView(std::move(params));

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
