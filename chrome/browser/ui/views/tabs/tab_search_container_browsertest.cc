// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_organization_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"

class TabSearchContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchContainerBrowserTest() {
    feature_list_.InitWithFeatures({features::kTabOrganization}, {});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

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

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);
  tab_strip_model()->ForceShowingModalUIForTesting(false);
  service->OnTriggerOccured(browser());

  ASSERT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       TogglesActionUIStateOnlyInCorrectBrowser) {
  const Browser* const second_browser = CreateBrowser(browser()->profile());
  TabSearchContainer* const second_search_container =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->tab_strip_region_view()
          ->tab_search_container();

  ASSERT_FALSE(
      second_search_container->expansion_animation_for_testing()->IsShowing());

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();
  // Same profile -> same service.
  ASSERT_EQ(service,
            second_search_container->tab_organization_service_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);
  tab_strip_model()->ForceShowingModalUIForTesting(false);
  second_search_container->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);
  second_browser->tab_strip_model()->ForceShowingModalUIForTesting(false);
  service->OnTriggerOccured(browser());

  EXPECT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());
  EXPECT_FALSE(
      second_search_container->expansion_animation_for_testing()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       DoesntShowIfTabStripModalUIExists) {
  ASSERT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());

  tab_strip_model()->ForceShowingModalUIForTesting(true);
  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);
  tab_search_container()->ShowTabOrganization();

  EXPECT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());

  tab_strip_model()->ForceShowingModalUIForTesting(false);
  tab_search_container()->ShowTabOrganization();

  EXPECT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       BlocksTabStripModalUIWhileShown) {
  ASSERT_TRUE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->ShowTabOrganization();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->expansion_animation_for_testing()->Reset(1);

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->HideTabOrganization();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->expansion_animation_for_testing()->Reset(0);

  EXPECT_TRUE(browser()->tab_strip_model()->CanShowModalUI());
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

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonClicked) {
  tab_search_container()->expansion_animation_for_testing()->Reset(1);
  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide);

  tab_search_container()->OnOrganizeButtonClicked();

  EXPECT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonDismissed) {
  tab_search_container()->expansion_animation_for_testing()->Reset(1);
  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide);

  tab_search_container()->OnOrganizeButtonDismissed();

  EXPECT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       DelayedHidesWhenOrganizeButtonTimesOut) {
  tab_search_container()->expansion_animation_for_testing()->Reset(1);
  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide);

  tab_search_container()->OnOrganizeButtonTimeout();

  EXPECT_FALSE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone);

  ASSERT_TRUE(
      tab_search_container()->expansion_animation_for_testing()->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsSuccessWhenButtonClicked) {
  base::HistogramTester histogram_tester;

  tab_search_container()->expansion_animation_for_testing()->Reset(1);

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnOrganizeButtonClicked();

  histogram_tester.ExpectUniqueSample("Tab.Organization.AllEntrypoints.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 0, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsFailureWhenButtonDismissed) {
  base::HistogramTester histogram_tester;

  tab_search_container()->expansion_animation_for_testing()->Reset(1);

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnOrganizeButtonDismissed();

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsFailureWhenButtonTimeout) {
  base::HistogramTester histogram_tester;

  tab_search_container()->expansion_animation_for_testing()->Reset(1);

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnOrganizeButtonTimeout();

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);

  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 2, 1);
}
