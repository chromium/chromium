// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class OfferNotificationBubbleViewsInteractiveUiTest
    : public OfferNotificationBubbleViewsTestBase {
 public:
  OfferNotificationBubbleViewsInteractiveUiTest() = default;
  ~OfferNotificationBubbleViewsInteractiveUiTest() override = default;
  OfferNotificationBubbleViewsInteractiveUiTest(
      const OfferNotificationBubbleViewsInteractiveUiTest&) = delete;
  OfferNotificationBubbleViewsInteractiveUiTest& operator=(
      const OfferNotificationBubbleViewsInteractiveUiTest&) = delete;

  // TODO(crbug.com/1181615): Move shared functions to some utils.
  void ClickOnView(views::View* view) {
    GetOfferNotificationBubbleViews()->ResetViewShownTimeStampForTesting();
    base::RunLoop closure_loop;
    ui_test_utils::MoveMouseToCenterAndPress(
        view, ui_controls::LEFT, ui_controls::DOWN | ui_controls::UP,
        closure_loop.QuitClosure());
    closure_loop.Run();
  }
};

// Tests that bubble behaves correctly after user dismisses it.
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsInteractiveUiTest,
                       DismissBubble) {
  // Set the initial origin that the bubble will be displayed on.
  SetUpOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
  ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
  NavigateTo("https://www.example.com/first");
  WaitForObservedEvent();

  // Bubble should be visible.
  EXPECT_TRUE(IsIconVisible());
  EXPECT_TRUE(GetOfferNotificationBubbleViews());

  // Dismiss the bubble by clicking the button.
  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetOfferNotificationBubbleViews()->GetWidget());
  auto* ok_button = GetOfferNotificationBubbleViews()->GetOkButton();
  EXPECT_TRUE(ok_button);
  ClickOnView(ok_button);
  destroyed_waiter.Wait();
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Navigates to another valid domain will not reshow the bubble.
  NavigateTo("https://www.example.com/second");
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Navigates to an invalid domain will dismiss the icon.
  NavigateTo("https://www.about.com/");
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
  EXPECT_FALSE(IsIconVisible());
}

}  // namespace autofill
