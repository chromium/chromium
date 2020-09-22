// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/chromedriver/chrome/ui_events.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/views/widget/widget_observer.h"

class FeaturePromoBubbleDelegate {
 public:
  virtual void OnDismiss() = 0;
  virtual void OnSnooze() = 0;
};

class MockFeaturePromoBubbleDelegate : public FeaturePromoBubbleDelegate {
 public:
  MOCK_METHOD(void, OnDismiss, (), ());
  MOCK_METHOD(void, OnSnooze, (), ());
};

class FeaturePromoBubbleViewTest : public TestWithBrowserView {
 public:
  FeaturePromoBubbleViewTest()
      : TestWithBrowserView(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  FeaturePromoBubbleParams GetBubbleParams(bool snoozable) {
    FeaturePromoBubbleParams params;
    params.body_string_specifier = IDS_REOPEN_TAB_PROMO;
    params.anchor_view = browser_view()->contents_container();
    params.arrow = views::BubbleBorder::TOP_RIGHT;
    params.allow_focus = snoozable;
    params.allow_snooze = snoozable;
    return params;
  }
};

class MockWidgetObserver : public views::WidgetObserver {
 public:
  MOCK_METHOD(void, OnWidgetClosing, (views::Widget*), ());
};

TEST_F(FeaturePromoBubbleViewTest, CallDismiss) {
  MockFeaturePromoBubbleDelegate callback;

  EXPECT_CALL(callback, OnDismiss()).Times(1);

  FeaturePromoBubbleView* bubble = FeaturePromoBubbleView::Create(
      GetBubbleParams(true), base::RepeatingClosure(),
      base::BindRepeating(&FeaturePromoBubbleDelegate::OnDismiss,
                          base::Unretained(&callback)));

  // Simulate clicks on dismiss button.
  ui::MouseEvent mouse_press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent mouse_release(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  bubble->GetDismissButtonForTesting()->OnMouseEvent(&mouse_press);
  bubble->GetDismissButtonForTesting()->OnMouseEvent(&mouse_release);

  bubble->GetWidget()->Close();
}

TEST_F(FeaturePromoBubbleViewTest, CallSnooze) {
  MockFeaturePromoBubbleDelegate callback;

  EXPECT_CALL(callback, OnSnooze()).Times(1);

  FeaturePromoBubbleView* bubble = FeaturePromoBubbleView::Create(
      GetBubbleParams(true),
      base::BindRepeating(&FeaturePromoBubbleDelegate::OnSnooze,
                          base::Unretained(&callback)));

  // Simulate clicks on snooze button.
  ui::MouseEvent mouse_press(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent mouse_release(
      ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  bubble->GetSnoozeButtonForTesting()->OnMouseEvent(&mouse_press);
  bubble->GetSnoozeButtonForTesting()->OnMouseEvent(&mouse_release);

  bubble->GetWidget()->Close();
}

TEST_F(FeaturePromoBubbleViewTest, NoButtonIfNotSnoozable) {
  FeaturePromoBubbleView* bubble =
      FeaturePromoBubbleView::Create(GetBubbleParams(false));
  EXPECT_FALSE(bubble->GetSnoozeButtonForTesting());
  EXPECT_FALSE(bubble->GetDismissButtonForTesting());

  bubble->GetWidget()->Close();
}

TEST_F(FeaturePromoBubbleViewTest, AutoDismissIfNotSnoozable) {
  FeaturePromoBubbleView* bubble =
      FeaturePromoBubbleView::Create(GetBubbleParams(false));
  MockWidgetObserver dismiss_observer;
  EXPECT_CALL(dismiss_observer, OnWidgetClosing(testing::_)).Times(1);
  bubble->GetWidget()->AddObserver(&dismiss_observer);
  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  task_environment()->RunUntilIdle();
}

TEST_F(FeaturePromoBubbleViewTest, NoAutoDismissIfSnoozable) {
  FeaturePromoBubbleView* bubble =
      FeaturePromoBubbleView::Create(GetBubbleParams(true));
  MockWidgetObserver dismiss_observer;
  EXPECT_CALL(dismiss_observer, OnWidgetClosing(testing::_)).Times(0);
  bubble->GetWidget()->AddObserver(&dismiss_observer);
  task_environment()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  task_environment()->RunUntilIdle();
  // WidgetObserver checks if it is in an observer list in its destructor.
  // Need to remove it from widget manually.
  bubble->GetWidget()->RemoveObserver(&dismiss_observer);
}
