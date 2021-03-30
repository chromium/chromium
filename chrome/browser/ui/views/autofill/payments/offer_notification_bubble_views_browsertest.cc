// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views_test_base.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
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

IN_PROC_BROWSER_TEST_F(OfferNotificationBubbleViewsBrowserTest, OpenNewTab) {
  SetUpOfferDataWithDomains(
      {GURL("https://www.example.com/"), GURL("https://www.test.com/")});

  NavigateTo(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.example.com/"),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(IsIconVisible());
  EXPECT_FALSE(GetOfferNotificationBubbleViews());
}

}  // namespace autofill
