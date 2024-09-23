// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/commerce/mock_commerce_ui_tab_helper.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/views/commerce/product_specifications_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/interaction/element_tracker_views.h"

namespace {
const char kUrlA[] = "chrome://flags";
const char kUrlB[] = "about:blank";

}  // namespace

class ProductSpecificationsIconViewIntegrationTest
    : public TestWithBrowserView {
 public:
  ProductSpecificationsIconViewIntegrationTest() {
    MockCommerceUiTabHelper::ReplaceFactory();
  }

  ProductSpecificationsIconViewIntegrationTest(
      const ProductSpecificationsIconViewIntegrationTest&) = delete;
  ProductSpecificationsIconViewIntegrationTest& operator=(
      const ProductSpecificationsIconViewIntegrationTest&) = delete;

  ~ProductSpecificationsIconViewIntegrationTest() override = default;

  void SetUp() override {
    test_features_.InitAndEnableFeature(commerce::kProductSpecifications);
    TestWithBrowserView::SetUp();

    account_checker_ = std::make_unique<commerce::MockAccountChecker>();
    shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetForBrowserContext(
            browser()->profile()));
    shopping_service_->SetAccountChecker(account_checker_.get());
    AddTab(browser(), GURL(kUrlA));
    mock_tab_helper_ =
        static_cast<MockCommerceUiTabHelper*>(browser()
                                                  ->GetActiveTabInterface()
                                                  ->GetTabFeatures()
                                                  ->commerce_ui_tab_helper());
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        commerce::ShoppingServiceFactory::GetInstance(),
        base::BindRepeating(&ProductSpecificationsIconViewIntegrationTest::
                                BuildMockShoppingService));
    return factories;
  }

  static std::unique_ptr<KeyedService> BuildMockShoppingService(
      content::BrowserContext* context) {
    std::unique_ptr<commerce::MockShoppingService> service =
        std::make_unique<commerce ::MockShoppingService>();
    return service;
  }

  ProductSpecificationsIconView* GetChip() {
    auto* location_bar_view = browser_view()->toolbar()->location_bar();
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(location_bar_view);
    views::View* matched_view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kProductSpecificationsChipElementId, context);
    return matched_view
               ? views::AsViewClass<ProductSpecificationsIconView>(matched_view)
               : nullptr;
  }

  MockCommerceUiTabHelper* GetTabHelper() { return mock_tab_helper_.get(); }

 protected:
  raw_ptr<MockCommerceUiTabHelper, DanglingUntriaged> mock_tab_helper_;

 private:
  base::test::ScopedFeatureList test_features_;
  raw_ptr<commerce::MockShoppingService, AcrossTasksDanglingUntriaged>
      shopping_service_;
  std::unique_ptr<commerce::MockAccountChecker> account_checker_;
};

TEST_F(ProductSpecificationsIconViewIntegrationTest, IconVisibility) {
  ON_CALL(*GetTabHelper(), ShouldShowProductSpecificationsIconView)
      .WillByDefault(testing::Return(true));

  NavigateAndCommitActiveTab(GURL(kUrlA));
  auto* icon_view = GetChip();
  EXPECT_TRUE(icon_view->GetVisible());

  ON_CALL(*GetTabHelper(), ShouldShowProductSpecificationsIconView)
      .WillByDefault(testing::Return(false));
  NavigateAndCommitActiveTab(GURL(kUrlB));
  EXPECT_FALSE(icon_view->GetVisible());
}

TEST_F(ProductSpecificationsIconViewIntegrationTest, IconExecution) {
  ON_CALL(*GetTabHelper(), ShouldShowProductSpecificationsIconView)
      .WillByDefault(testing::Return(true));

  NavigateAndCommitActiveTab(GURL(kUrlB));
  auto* icon_view = GetChip();
  EXPECT_TRUE(icon_view->GetVisible());

  EXPECT_CALL(*GetTabHelper(), OnProductSpecificationsIconClicked).Times(1);
  icon_view->ExecuteForTesting();
}

TEST_F(ProductSpecificationsIconViewIntegrationTest, TestVisualState) {
  std::u16string added_title = u"Added to set";
  std::u16string add_title = u"Add to set";

  ON_CALL(*GetTabHelper(), ShouldShowProductSpecificationsIconView)
      .WillByDefault(testing::Return(true));
  ON_CALL(*GetTabHelper(), IsInRecommendedSet)
      .WillByDefault(testing::Return(true));
  ON_CALL(*GetTabHelper(), GetProductSpecificationsLabel)
      .WillByDefault(testing::Return(added_title));

  NavigateAndCommitActiveTab(GURL(kUrlB));
  auto* icon_view = GetChip();
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_EQ(icon_view->GetText(), added_title);

  ON_CALL(*GetTabHelper(), ShouldShowProductSpecificationsIconView)
      .WillByDefault(testing::Return(true));
  ON_CALL(*GetTabHelper(), IsInRecommendedSet)
      .WillByDefault(testing::Return(false));
  ON_CALL(*GetTabHelper(), GetProductSpecificationsLabel)
      .WillByDefault(testing::Return(add_title));

  NavigateAndCommitActiveTab(GURL(kUrlA));
  icon_view = GetChip();
  EXPECT_TRUE(icon_view->GetVisible());
  EXPECT_EQ(icon_view->GetText(), add_title);
}
