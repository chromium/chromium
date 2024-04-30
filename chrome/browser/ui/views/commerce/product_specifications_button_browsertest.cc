// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/product_specifications_button.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

class ProductSpecificationsButtonBrowserTest : public InProcessBrowserTest {
 public:
  ProductSpecificationsButtonBrowserTest() {
    feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
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

  bool GetRenderTabSearchBeforeTabStrip() {
    return TabSearchBubbleHost::ShouldTabSearchRenderBeforeTabStrip();
  }

  void SetLockedExpansionModeForTesting(LockedExpansionMode mode) {
    product_specifications_button()->SetLockedExpansionMode(mode);
  }

  void OnDismissed() { product_specifications_button()->OnDismissed(); }

  void OnTimeout() { product_specifications_button()->OnTimeout(); }

 private:
  base::test::ScopedFeatureList feature_list_;
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
  product_specifications_button()->Show();

  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  SetLockedExpansionModeForTesting(LockedExpansionMode::kNone);

  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest, DelaysHide) {
  product_specifications_button()->expansion_animation_for_testing()->Reset(1);
  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsClosing());

  SetLockedExpansionModeForTesting(LockedExpansionMode::kWillHide);
  product_specifications_button()->Hide();

  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsClosing());

  SetLockedExpansionModeForTesting(LockedExpansionMode::kNone);

  ASSERT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       ImmediatelyHidesWhenButtonDismissed) {
  product_specifications_button()->expansion_animation_for_testing()->Reset(1);
  SetLockedExpansionModeForTesting(LockedExpansionMode::kWillHide);

  OnDismissed();

  EXPECT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       DelaysHideWhenButtonTimesOut) {
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
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       DoesntShowIfTabStripModalUIExists) {
  ASSERT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  std::unique_ptr<ScopedTabStripModalUI> scoped_tab_strip_modal_ui =
      browser()->tab_strip_model()->ShowModalUI();
  product_specifications_button()->Show();

  EXPECT_FALSE(product_specifications_button()
                   ->expansion_animation_for_testing()
                   ->IsShowing());

  scoped_tab_strip_modal_ui.reset();
  product_specifications_button()->Show();

  EXPECT_TRUE(product_specifications_button()
                  ->expansion_animation_for_testing()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(ProductSpecificationsButtonBrowserTest,
                       BlocksTabStripModalUIWhileShown) {
  ASSERT_TRUE(browser()->tab_strip_model()->CanShowModalUI());

  product_specifications_button()->Show();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  product_specifications_button()->expansion_animation_for_testing()->Reset(1);

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  product_specifications_button()->Hide();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  product_specifications_button()->expansion_animation_for_testing()->Reset(0);

  EXPECT_TRUE(browser()->tab_strip_model()->CanShowModalUI());
}
