// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_icon_view.h"

#include <string>

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/interaction/element_tracker_views.h"

class ProductSpecificationsIconViewBrowserTest : public UiBrowserTest {
 public:
  ProductSpecificationsIconViewBrowserTest() {
    test_features_.InitAndEnableFeature(commerce::kProductSpecifications);
  }

  void SetUp() override {
    replace_commerce_ui_tab_helper_ = MockCommerceUiTabHelper::ReplaceFactory();
    UiBrowserTest::SetUp();
  }

  // UiBrowserTest:
  void PreShow() override {
    MockCommerceUiTabHelper* mock_tab_helper =
        static_cast<MockCommerceUiTabHelper*>(browser()
                                                  ->GetActiveTabInterface()
                                                  ->GetTabFeatures()
                                                  ->commerce_ui_tab_helper());

    ON_CALL(*mock_tab_helper, ShouldShowProductSpecificationsIconView)
        .WillByDefault(testing::Return(true));
    ON_CALL(*mock_tab_helper, ShouldExpandPageActionIcon)
        .WillByDefault(testing::Return(true));

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    if (test_name == "InvokeUi_forced_show_add") {
      ON_CALL(*mock_tab_helper, IsInRecommendedSet)
          .WillByDefault(testing::Return(false));
    } else if (test_name == "InvokeUi_forced_show_added") {
      ON_CALL(*mock_tab_helper, IsInRecommendedSet)
          .WillByDefault(testing::Return(true));
    } else {
      NOTREACHED();
    }

    // Manually trigger the discounts page action.
    browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->commerce_ui_tab_helper()
        ->UpdateProductSpecificationsIconView();
  }

  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

  bool VerifyUi() override {
    auto* product_specifications_chip = GetChip();
    // TODO(b/325660810): Add more detailed test about different states of the
    // icon after implementation.
    return product_specifications_chip;
  }

  void WaitForUserDismissal() override {
    ui_test_utils::WaitForBrowserToClose();
  }

 private:
  base::test::ScopedFeatureList test_features_;
  ui::UserDataFactory::ScopedOverride replace_commerce_ui_tab_helper_;

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->toolbar()->location_bar();
  }

  IconLabelBubbleView* GetChip() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(GetLocationBarView());
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kProductSpecificationsChipElementId, context);

    return matched_view ? views::AsViewClass<IconLabelBubbleView>(matched_view)
                        : nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsIconViewBrowserTest,
                       InvokeUi_forced_show_add) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsIconViewBrowserTest,
                       InvokeUi_forced_show_added) {
  ShowAndVerifyUi();
}
