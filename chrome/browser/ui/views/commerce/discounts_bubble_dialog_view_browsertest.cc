// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_bubble_dialog_view.h"

#include "base/build_time.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/commerce/core/test_utils.h"
#include "content/public/test/browser_test.h"

class DiscountBubbleViewBrowserTest : public DialogBrowserTest {
 public:
  DiscountBubbleViewBrowserTest() = default;

  DiscountBubbleViewBrowserTest(const DiscountBubbleViewBrowserTest&) = delete;
  DiscountBubbleViewBrowserTest& operator=(
      const DiscountBubbleViewBrowserTest&) = delete;

  void ShowUi(const std::string& name) override {
    commerce::DiscountInfo discount_info;
    const std::string detail =
        "10% off on laptop stands, valid for purchase of $40 or more";
    const std::string terms_and_conditions = "Seller's terms and conditions.";
    const std::string value_in_text = "value_in_text";
    const std::string discount_code = "WELCOME10";
    const double expiry_time_sec =
    base::Time::FromSecondsSinceUnixEpoch(3376512000)
            .InSecondsFSinceUnixEpoch();  // Dec 30, 2076
    if (name == "CrawledDeal") {
      discount_info = commerce::CreateValidDiscountInfo(
          detail, terms_and_conditions, value_in_text, discount_code, /*id=*/1,
          /*is_merchant_wide=*/false, std::nullopt);
      discount_info.terms_and_conditions = std::nullopt;
    } else if (name == "FreeListingDealWithoutTermsAndConditions") {
      discount_info = commerce::CreateValidDiscountInfo(
          detail, "", value_in_text, discount_code, /*id=*/1,
          /*is_merchant_wide=*/false, expiry_time_sec);
      discount_info.terms_and_conditions = std::nullopt;
    } else {  // Default case
      discount_info = commerce::CreateValidDiscountInfo(
          detail, terms_and_conditions, value_in_text, discount_code, /*id=*/1,
          /*is_merchant_wide=*/false, expiry_time_sec);
    }

    views::View* const anchor_view =
        BrowserView::GetBrowserViewForBrowser(browser())->top_container();
    // Create the coordinator using the anchor view.
    // The coordinator manages the bubble's lifecycle.
    coordinator_ = std::make_unique<DiscountsBubbleCoordinator>(anchor_view);

    coordinator_->Show(web_contents(), discount_info, base::DoNothing());
  }

  void TearDownOnMainThread() override {
    coordinator_.reset();
    DialogBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }
  std::unique_ptr<DiscountsBubbleCoordinator> coordinator_;
};

IN_PROC_BROWSER_TEST_F(DiscountBubbleViewBrowserTest,
                       InvokeUi_FreelistingDeal) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DiscountBubbleViewBrowserTest, InvokeUi_CrawledDeal) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DiscountBubbleViewBrowserTest,
                       InvokeUi_FreeListingDealWithoutTermsAndConditions) {
  ShowAndVerifyUi();
}
