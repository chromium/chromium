// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_glic_container.h"

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
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/slide_animation.h"

class TabGlicContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabGlicContainerBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton,
         features::kTabstripDeclutter},
        {});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabGlicContainer* tab_glic_container() {
    return browser_view()->tab_strip_region_view()->GetTabGlicContainer();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabGlicContainerBrowserTest, ShowsDeclutterChip) {
  ASSERT_FALSE(tab_glic_container()->animation_session_for_testing());

  tab_glic_container()->ShowTabStripNudge(
      tab_glic_container()->tab_declutter_button());

  ASSERT_TRUE(tab_glic_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabGlicContainerBrowserTest,
                       ShowsAndHidesDeclutterChip) {
  ASSERT_FALSE(tab_glic_container()->animation_session_for_testing());

  tab_glic_container()->ShowTabStripNudge(
      tab_glic_container()->tab_declutter_button());

  ASSERT_TRUE(tab_glic_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  // Finish showing declutter chip.
  tab_glic_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_glic_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Hide the declutter chip.
  tab_glic_container()->HideTabStripNudge(
      tab_glic_container()->tab_declutter_button());

  ASSERT_TRUE(tab_glic_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabGlicContainerBrowserTest,
                       LogsWhenDeclutterButtonClicked) {
  base::HistogramTester histogram_tester;

  tab_glic_container()->ShowTabStripNudge(
      tab_glic_container()->tab_declutter_button());

  tab_glic_container()->OnTabDeclutterButtonClicked();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 0, 1);
  // Bucketed CTR metric should reflect one show and one click, with fewer than
  // 15 total tabs.
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 10, 1);
}

IN_PROC_BROWSER_TEST_F(TabGlicContainerBrowserTest,
                       LogsWhenDeclutterButtonDismissed) {
  base::HistogramTester histogram_tester;

  tab_glic_container()->ShowTabStripNudge(
      tab_glic_container()->tab_declutter_button());

  tab_glic_container()->OnTabDeclutterButtonDismissed();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabGlicContainerBrowserTest,
                       LogsWhenDeclutterButtonTimeout) {
  base::HistogramTester histogram_tester;

  tab_glic_container()->ShowTabStripNudge(
      tab_glic_container()->tab_declutter_button());

  tab_glic_container()->OnTabStripNudgeButtonTimeout(
      tab_glic_container()->tab_declutter_button());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 2, 1);
}
