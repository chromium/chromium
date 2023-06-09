// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kTestURL[] = "about:blank";
}  // namespace

class PriceInsightsIconViewBrowserTest : public UiBrowserTest {
 public:
  // UiBrowserTest:
  void PreShow() override {
    MockShoppingListUiTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    MockShoppingListUiTabHelper* mock_tab_helper =
        static_cast<MockShoppingListUiTabHelper*>(
            MockShoppingListUiTabHelper::FromWebContents(
                browser()->tab_strip_model()->GetActiveWebContents()));
    EXPECT_CALL(*mock_tab_helper, ShouldShowPriceInsightsIconView)
        .Times(testing::AnyNumber());
    ON_CALL(*mock_tab_helper, ShouldShowPriceInsightsIconView)
        .WillByDefault(testing::Return(true));
  }

  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestURL)));
  }

  bool VerifyUi() override {
    auto* price_tracking_chip = GetChip();
    if (!price_tracking_chip) {
      return false;
    }

    // TODO(meiliang): call VerifyPixelUi here after PriceInsightsIconView is
    // finished implementing.
    return true;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList test_features_{commerce::kPriceInsights};

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->toolbar()->location_bar();
  }

  PriceInsightsIconView* GetChip() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(GetLocationBarView());
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kPriceInsightsChipElementId, context);

    return matched_view
               ? views::AsViewClass<PriceInsightsIconView>(matched_view)
               : nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewBrowserTest,
                       InvokeUi_show_price_insights_icon) {
  ShowAndVerifyUi();
}
