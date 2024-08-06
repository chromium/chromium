// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/discounts_icon_view.h"

#include "base/time/default_clock.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kTestURL[] = "about:blank";
}  // namespace

class DiscountsIconViewBrowserTest : public UiBrowserTest {
 public:
  void SetUp() override {
    MockCommerceUiTabHelper::ReplaceFactory();
    UiBrowserTest::SetUp();
  }
  // UiBrowserTest:
  void PreShow() override {
    std::string detail =
        "10% off on laptop stands, valid for purchase of $40 or more";
    std::string terms_and_conditions = "Seller's terms and conditions.";
    std::string value_in_text = "value_in_text";
    std::string discount_code = "WELCOME10";
    double expiry_time_sec =
        (base::DefaultClock::GetInstance()->Now() + base::Days(2))
            .InSecondsFSinceUnixEpoch();
    commerce::DiscountInfo discount_info = commerce::CreateValidDiscountInfo(
        detail, terms_and_conditions, value_in_text, discount_code, /*id=*/1,
        /*is_merchant_wide=*/false, expiry_time_sec);

    discount_infos_ = {discount_info};

    // Setup discount response in tab helper
    MockCommerceUiTabHelper* mock_tab_helper =
        static_cast<MockCommerceUiTabHelper*>(browser()
                                                  ->GetActiveTabInterface()
                                                  ->GetTabFeatures()
                                                  ->commerce_ui_tab_helper());
    ON_CALL(*mock_tab_helper, ShouldShowDiscountsIconView)
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_tab_helper, GetDiscounts)
        .WillByDefault(testing::ReturnRef(discount_infos_));

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    if (test_name == "InvokeUi_show_discounts_icon_with_label") {
      EXPECT_CALL(*mock_tab_helper, ShouldExpandPageActionIcon)
          .WillRepeatedly(testing::Return(true));
    } else if (test_name == "InvokeUi_show_discounts_icon_only") {
      EXPECT_CALL(*mock_tab_helper, ShouldExpandPageActionIcon)
          .WillRepeatedly(testing::Return(false));
    }
  }

  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestURL)));
  }

  bool VerifyUi() override {
    auto* icon = GetIcon();

    if (!icon) {
      return false;
    }

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name == "InvokeUi_show_discounts_icon_with_label") {
      EXPECT_TRUE(icon->ShouldShowLabel());
      EXPECT_EQ(icon->GetText(),
                l10n_util::GetStringUTF16(IDS_DISCOUNT_ICON_EXPANDED_TEXT));
    } else if (test_name == "InvokeUi_show_discounts_icon_only") {
      EXPECT_FALSE(icon->ShouldShowLabel());
    }
    return true;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal. This is useful when using
    // the test-launcher-interactive option.
    ui_test_utils::WaitForBrowserToClose();
  }

 protected:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  DiscountsIconView* GetIcon() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(GetLocationBarView());
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kDiscountsChipElementId, context);

    return matched_view ? views::AsViewClass<DiscountsIconView>(matched_view)
                        : nullptr;
  }

 private:
  base::test::ScopedFeatureList test_features_;

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->toolbar()->location_bar();
  }

  std::vector<commerce::DiscountInfo> discount_infos_;
};

IN_PROC_BROWSER_TEST_F(DiscountsIconViewBrowserTest,
                       InvokeUi_show_discounts_icon_with_label) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(DiscountsIconViewBrowserTest,
                       InvokeUi_show_discounts_icon_only) {
  ShowAndVerifyUi();
}
