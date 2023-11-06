// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"

class TabSearchContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchContainerBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kChromeRefresh2023}, {});
  }

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStrip* tab_strip() { return browser_view()->tabstrip(); }

  TabSearchContainer* tab_search_container() {
    return browser_view()->tab_strip_region_view()->tab_search_container();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, TogglesActionUIState) {
  ASSERT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  ASSERT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, DelaysShow) {
  ASSERT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillShow);
  tab_search_container()->ShowTabOrganization();

  ASSERT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);

  ASSERT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, DelaysHide) {
  tab_search_container()->expansion_animation_for_testing()->Reset(1);
  ASSERT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide);
  tab_search_container()->HideTabOrganization();

  ASSERT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);

  ASSERT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());
}
