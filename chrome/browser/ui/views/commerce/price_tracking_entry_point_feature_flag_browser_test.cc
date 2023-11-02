// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_icon_view.h"

#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kTestURL[] = "about:blank";
}  // namespace

class PriceTrackingEntryPointFeatureFlagTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  PriceTrackingEntryPointFeatureFlagTest() {
    const bool is_feature_enabled = GetParam();
    if (is_feature_enabled) {
      test_features_.InitAndEnableFeature(commerce::kShoppingList);
    } else {
      test_features_.InitAndDisableFeature(commerce::kShoppingList);
    }
  }

  PriceTrackingEntryPointFeatureFlagTest(
      const PriceTrackingEntryPointFeatureFlagTest&) = delete;
  PriceTrackingEntryPointFeatureFlagTest& operator=(
      const PriceTrackingEntryPointFeatureFlagTest&) = delete;

  ~PriceTrackingEntryPointFeatureFlagTest() override = default;

  static std::string DescribeParams(
      const ::testing::TestParamInfo<ParamType>& info) {
    return info.param ? "ShoppingListEnabled" : "ShoppingListDisabled";
  }

  PriceTrackingIconView* GetChip() {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* location_bar_view = browser_view->toolbar()->location_bar();
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(location_bar_view);
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kPriceTrackingChipElementId, context);

    return matched_view
               ? views::AsViewClass<PriceTrackingIconView>(matched_view)
               : nullptr;
  }

 private:
  base::test::ScopedFeatureList test_features_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PriceTrackingEntryPointFeatureFlagTest,
    testing::Bool(),
    &PriceTrackingEntryPointFeatureFlagTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(PriceTrackingEntryPointFeatureFlagTest,
                       ShoppingListUiTabHelperCreation) {
  ASSERT_TRUE(AddTabAtIndex(0, GURL(kTestURL), ui::PAGE_TRANSITION_TYPED));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* tab_helper =
      commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);

  const bool is_feature_enabled = GetParam();
  if (is_feature_enabled) {
    EXPECT_TRUE(tab_helper);
  } else {
    EXPECT_FALSE(tab_helper);
  }
}

IN_PROC_BROWSER_TEST_P(PriceTrackingEntryPointFeatureFlagTest,
                       PriceTrackingPageActionIconCreation) {
  auto* chip = GetChip();

  const bool is_feature_enabled = GetParam();
  if (is_feature_enabled) {
    EXPECT_TRUE(chip);
  } else {
    EXPECT_FALSE(chip);
  }
}
