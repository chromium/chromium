// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_button.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

class MockProductSpecificationsEntryPointController
    : public commerce::ProductSpecificationsEntryPointController {
 public:
  explicit MockProductSpecificationsEntryPointController(Browser* browser)
      : commerce::ProductSpecificationsEntryPointController(browser) {}
  ~MockProductSpecificationsEntryPointController() override = default;

  MOCK_METHOD(void, OnEntryPointExecuted, (), (override));
  MOCK_METHOD(void, OnEntryPointDismissed, (), (override));
  MOCK_METHOD(void, OnEntryPointHidden, (), (override));
  MOCK_METHOD(bool, ShouldExecuteEntryPointShow, (), (override));
};

class ProductSpecificationsButtonBrowserTest : public InProcessBrowserTest {
 public:
  ProductSpecificationsButtonBrowserTest() {
    feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProductSpecificationsButtonBrowserTest::SetTestingFactory,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    controller_ =
        std::make_unique<MockProductSpecificationsEntryPointController>(
            browser());
    product_specifications_button()->SetEntryPointControllerForTesting(
        controller_.get());
    ON_CALL(*controller(), ShouldExecuteEntryPointShow)
        .WillByDefault(testing::Return(true));
  }

  void SetTestingFactory(content::BrowserContext* context) {
    commerce::ProductSpecificationsServiceFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext* context)
                                             -> std::unique_ptr<KeyedService> {
              return nullptr;
            }));
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchContainer* tab_search_container() {
    return browser_view()->tab_strip_region_view()->tab_search_container();
  }

  ProductSpecificationsButton* product_specifications_button() {
    return browser_view()
        ->tab_strip_region_view()
        ->product_specifications_button();
  }

  MockProductSpecificationsEntryPointController* controller() {
    return controller_.get();
  }

  bool GetRenderTabSearchBeforeTabStrip() {
    return !tabs::GetTabSearchTrailingTabstrip(browser()->profile());
  }

  void SetLockedExpansionModeForTesting(LockedExpansionMode mode) {
    product_specifications_button()->SetLockedExpansionMode(mode);
  }

  void ShowButton() { product_specifications_button()->Show(); }

  void ClickButton() { product_specifications_button()->OnClicked(); }

  void OnDismissed() { product_specifications_button()->OnDismissed(); }

  void OnTimeout() { product_specifications_button()->OnTimeout(); }

 protected:
  base::UserActionTester user_action_tester_;

 private:
  base::CallbackListSubscription dependency_manager_subscription_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<MockProductSpecificationsEntryPointController> controller_;
};

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       ProductSpecificationsButtonOrder) {
  auto* tab_strip_region_view = browser_view()->tab_strip_region_view();
  if (GetRenderTabSearchBeforeTabStrip()) {
    ASSERT_EQ(tab_search_container(), tab_strip_region_view->children()[0]);
    ASSERT_EQ(product_specifications_button(),
              tab_strip_region_view->children()[1]);
  } else {
    auto tab_search_index =
        tab_strip_region_view->GetIndexOf(tab_search_container());
    auto product_specifications_index =
        tab_strip_region_view->GetIndexOf(product_specifications_button());
    ASSERT_TRUE(tab_search_index.has_value());
    ASSERT_TRUE(product_specifications_index.has_value());
    ASSERT_EQ(1u,
              tab_search_index.value() - product_specifications_index.value());
  }
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest, DelaysShow) {
  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  SetLockedExpansionModeForTesting(LockedExpansionMode::kWillShow);
  ShowButton();

  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   "Commerce.Compare.ProactiveChipShown"));

  EXPECT_CALL(*controller(), ShouldExecuteEntryPointShow()).Times(1);
  SetLockedExpansionModeForTesting(LockedExpansionMode::kNone);

  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsShowing());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.Compare.ProactiveChipShown"));
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       StopIneligibleDelayedShow) {
  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  SetLockedExpansionModeForTesting(LockedExpansionMode::kWillShow);
  ShowButton();

  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  EXPECT_CALL(*controller(), ShouldExecuteEntryPointShow()).Times(1);
  ON_CALL(*controller(), ShouldExecuteEntryPointShow)
      .WillByDefault(testing::Return(false));
  SetLockedExpansionModeForTesting(LockedExpansionMode::kNone);

  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       ShowNotBlockedByCurrentPageEligibility) {
  EXPECT_CALL(*controller(), ShouldExecuteEntryPointShow()).Times(0);

  ShowButton();

  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       ImmediatelyHidesWhenButtonDismissed) {
  EXPECT_CALL(*controller(), OnEntryPointDismissed()).Times(1);
  EXPECT_CALL(*controller(), OnEntryPointHidden()).Times(1);
  product_specifications_button()->expansion_animation_for_testing()->Reset(1);
  SetLockedExpansionModeForTesting(LockedExpansionMode::kWillHide);

  OnDismissed();

  EXPECT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsClosing());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.Compare.ProactiveChipDismissed"));
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       DelaysHideWhenButtonTimesOut) {
  EXPECT_CALL(*controller(), OnEntryPointHidden()).Times(1);
  EXPECT_CALL(*controller(), OnEntryPointDismissed()).Times(0);
  product_specifications_button()->expansion_animation_for_testing()->Reset(1);
  SetLockedExpansionModeForTesting(LockedExpansionMode::kWillHide);

  OnTimeout();

  EXPECT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsClosing());

  SetLockedExpansionModeForTesting(LockedExpansionMode::kNone);

  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsClosing());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.Compare.ProactiveChipIgnored"));
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       DoesntShowIfTabStripModalUIExists) {
  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  std::unique_ptr<ScopedTabStripModalUI> scoped_tab_strip_modal_ui =
      browser()->tab_strip_model()->ShowModalUI();
  ShowButton();

  EXPECT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  scoped_tab_strip_modal_ui.reset();
  ShowButton();

  EXPECT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       BlocksTabStripModalUIWhileShown) {
  ASSERT_TRUE(browser()->tab_strip_model()->CanShowModalUI());

  ShowButton();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  product_specifications_button()->expansion_animation_for_testing()->Reset(1);

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  OnDismissed();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  product_specifications_button()->expansion_animation_for_testing()->Reset(0);

  EXPECT_TRUE(browser()->tab_strip_model()->CanShowModalUI());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest, ClickButton) {
  EXPECT_CALL(*controller(), OnEntryPointExecuted()).Times(1);
  EXPECT_CALL(*controller(), OnEntryPointHidden()).Times(1);

  ShowButton();
  product_specifications_button()->expansion_animation_for_testing()->Reset(1);

  ClickButton();
  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsClosing());
  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "Commerce.Compare.ProactiveChipClicked"));
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       NotifyShowEntryPoint) {
  product_specifications_button()->ShowEntryPointWithTitle(u"title");

  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsShowing());
  ASSERT_EQ(product_specifications_button()->GetText(), u"title");
  ASSERT_EQ(product_specifications_button()->GetTooltipText(), u"title");
}
