// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/test_utils.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kTestURL[] = "about:blank";
}  // namespace

class PriceInsightsIconViewBrowserTest : public UiBrowserTest {
 public:
  PriceInsightsIconViewBrowserTest() {
    MockCommerceUiTabHelper::ReplaceFactory();
    test_features_.InitWithFeatures(
        {commerce::kPriceInsights, commerce::kCommerceAllowChipExpansion}, {});
  }

  // UiBrowserTest:
  void PreShow() override {
    MockCommerceUiTabHelper* mock_tab_helper = getTabHelper();
    EXPECT_CALL(*mock_tab_helper, ShouldShowPriceInsightsIconView)
        .Times(testing::AnyNumber());
    ON_CALL(*mock_tab_helper, ShouldShowPriceInsightsIconView)
        .WillByDefault(testing::Return(true));

    EXPECT_CALL(*mock_tab_helper, GetPriceInsightsInfo)
        .Times(testing::AnyNumber());
    ON_CALL(*mock_tab_helper, GetPriceInsightsInfo)
        .WillByDefault(testing::ReturnRef(price_insights_info_));
    EXPECT_CALL(*mock_tab_helper, ShouldExpandPageActionIcon)
        .WillRepeatedly(testing::Return(true));

    PriceInsightsIconLabelType label_type = PriceInsightsIconLabelType::kNone;
    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name == "InvokeUi_show_price_insights_icon_with_low_price_label") {
      label_type = PriceInsightsIconLabelType::kPriceIsLow;
    } else if (test_name ==
               "InvokeUi_show_price_insights_icon_with_high_price_label") {
      label_type = PriceInsightsIconLabelType::kPriceIsHigh;
    }

    EXPECT_CALL(*mock_tab_helper, GetPriceInsightsIconLabelTypeForPage)
        .WillRepeatedly(testing::Return(label_type));
  }

  MockCommerceUiTabHelper* getTabHelper() {
    return static_cast<MockCommerceUiTabHelper*>(
        browser()
            ->GetActiveTabInterface()
            ->GetTabFeatures()
            ->commerce_ui_tab_helper());
  }

  void ShowUi(const std::string& name) override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kTestURL)));
  }

  bool VerifyUi() override {
    auto* price_insights_chip = GetChip();
    if (!price_insights_chip) {
      return false;
    }
    EXPECT_EQ(base::ToLowerASCII(
                  price_insights_chip->GetViewAccessibility().GetCachedName()),
              base::ToLowerASCII(l10n_util::GetStringUTF16(
                  IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT)));

    // TODO(meiliang): call VerifyPixelUi here after PriceInsightsIconView is
    // finished implementing.
    return true;
  }

  void WaitForUserDismissal() override {
    // Consider closing the browser to be dismissal.
    ui_test_utils::WaitForBrowserToClose();
  }

 protected:
  std::optional<commerce::PriceInsightsInfo> price_insights_info_;

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

 private:
  base::test::ScopedFeatureList test_features_;

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  LocationBarView* GetLocationBarView() {
    return GetBrowserView()->toolbar()->location_bar();
  }
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewBrowserTest,
                       InvokeUi_show_price_insights_icon) {
  ShowAndVerifyUi();
}

class PriceInsightsIconViewWithLabelBrowserTest
    : public PriceInsightsIconViewBrowserTest {
 public:
  PriceInsightsIconViewWithLabelBrowserTest() {
    test_features_.InitAndEnableFeaturesWithParameters(
        {{commerce::kPriceInsights,
          {{commerce::kPriceInsightsChipLabelExpandOnHighPriceParam, "true"}}},
         {feature_engagement::kIPHPriceInsightsPageActionIconLabelFeature, {}}},
        {});
  }
  // UiBrowserTest:
  void PreShow() override {
    PriceInsightsIconViewBrowserTest::PreShow();

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name == "InvokeUi_show_price_insights_icon_with_low_price_label") {
      price_insights_info_ = commerce::CreateValidPriceInsightsInfo(
          true, true, commerce::PriceBucket::kLowPrice);
    } else if (test_name ==
               "InvokeUi_show_price_insights_icon_with_high_price_label") {
      price_insights_info_ = commerce::CreateValidPriceInsightsInfo(
          true, true, commerce::PriceBucket::kHighPrice);
    } else {
      price_insights_info_ = commerce::CreateValidPriceInsightsInfo(
          true, true, commerce::PriceBucket::kTypicalPrice);
    }
  }

  bool VerifyUi() override {
    auto* price_insights_chip = GetChip();
    if (!price_insights_chip) {
      return false;
    }

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name == "InvokeUi_show_price_insights_icon_with_low_price_label") {
      EXPECT_TRUE(price_insights_chip->ShouldShowLabel());
      EXPECT_EQ(
          base::ToLowerASCII(price_insights_chip->GetIconLabelForTesting()),
          u"price is low");

      // TODO(meiliang): Add pixel test.
    } else if (test_name ==
               "InvokeUi_show_price_insights_icon_with_high_price_label") {
      EXPECT_TRUE(price_insights_chip->ShouldShowLabel());
      EXPECT_EQ(
          base::ToLowerASCII(price_insights_chip->GetIconLabelForTesting()),
          u"price is high");

      // TODO(meiliang): Add pixel test.
    }
    EXPECT_EQ(base::ToLowerASCII(
                  price_insights_chip->GetViewAccessibility().GetCachedName()),
              base::ToLowerASCII(l10n_util::GetStringUTF16(
                  IDS_SHOPPING_INSIGHTS_ICON_TOOLTIP_TEXT)));
    return true;
  }

 private:
  feature_engagement::test::ScopedIphFeatureList test_features_;
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewWithLabelBrowserTest,
                       InvokeUi_show_price_insights_icon_with_low_price_label) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(
    PriceInsightsIconViewWithLabelBrowserTest,
    InvokeUi_show_price_insights_icon_with_high_price_label) {
  ShowAndVerifyUi();
}
