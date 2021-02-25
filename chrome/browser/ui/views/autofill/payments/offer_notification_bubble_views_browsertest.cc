// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace autofill {

class OfferNotificationBubbleViewsBrowserTest
    : public OfferNotificationBubbleViewsTestBase {
 public:
  OfferNotificationBubbleViewsBrowserTest() = default;
  ~OfferNotificationBubbleViewsBrowserTest() override = default;
  OfferNotificationBubbleViewsBrowserTest(
      const OfferNotificationBubbleViewsBrowserTest&) = delete;
  OfferNotificationBubbleViewsBrowserTest& operator=(
      const OfferNotificationBubbleViewsBrowserTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest, Navigation) {
  static const struct {
    std::string url_navigated_to;
    bool bubble_should_be_visible;
  } test_cases[] = {
      // Different page on same domain keeps bubble.
      {"https://www.example.com/second/", true},
      // Different domain not in offer's list dismisses bubble.
      {"https://www.about.com/", false},
      // Subdomain not in offer's list dismisses bubble.
      {"https://support.example.com/first/", false},
      // http vs. https mismatch dismisses bubble.
      {"http://www.example.com/first/", false},
      // Different domain in the offer's list keeps bubble.
      {"https://www.test.com/first/", true},
  };

  // Set the initial origin that the bubble will be displayed on.
  SetUpOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});

  for (const auto& test_case : test_cases) {
    NavigateTo(chrome::kChromeUINewTabURL);

    ResetEventWaiterForSequence({DialogEvent::BUBBLE_SHOWN});
    NavigateTo("https://www.example.com/first");
    WaitForObservedEvent();

    // Bubble should be visible.
    EXPECT_TRUE(IsIconVisible());
    EXPECT_TRUE(GetOfferNotificationBubbleViews());

    // Navigate to a different url, and verify bubble/icon visibility.
    if (test_case.bubble_should_be_visible) {
      NavigateTo(test_case.url_navigated_to);
    } else {
      views::test::WidgetDestroyedWaiter destroyed_waiter(
          GetOfferNotificationBubbleViews()->GetWidget());
      NavigateTo(test_case.url_navigated_to);
      destroyed_waiter.Wait();
    }

    EXPECT_EQ(test_case.bubble_should_be_visible, IsIconVisible());
    EXPECT_EQ(test_case.bubble_should_be_visible,
              !!GetOfferNotificationBubbleViews());
  }
}

// Tests that the offer notification bubble will not be shown if the offer data
// is invalid (does not have a linked card).
IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest,
                       InvalidOfferData) {
  auto offer_data = CreateOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});
  offer_data->eligible_instrument_id.clear();
  personal_data()->AddOfferDataForTest(std::move(offer_data));
  personal_data()->NotifyPersonalDataObserver();

  // Neither icon nor bubble should be visible.
  NavigateTo("https://www.example.com/first/");
  EXPECT_FALSE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

}  // namespace autofill
