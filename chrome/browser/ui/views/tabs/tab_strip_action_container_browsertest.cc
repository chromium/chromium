// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

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

class TabStripActionContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabStripActionContainerBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kGlic,
         features::kTabstripComboButton, features::kTabstripDeclutter},
        {});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripActionContainer* tab_strip_action_container() {
    return browser_view()
        ->tab_strip_region_view()
        ->GetTabStripActionContainer();
  }

 protected:
  TabStripNudgeButton* TabDeclutterButton() {
    return tab_strip_action_container()->tab_declutter_button();
  }
  TabStripNudgeButton* AutoTabGroupButton() {
    return tab_strip_action_container()->auto_tab_group_button();
  }
  void ShowTabStripNudgeButton(TabStripNudgeButton* button) {
    tab_strip_action_container()->ShowTabStripNudge(button);
  }
  void HideTabStripNudgeButton(TabStripNudgeButton* button) {
    tab_strip_action_container()->HideTabStripNudge(button);
  }

  void OnTabStripNudgeButtonTimeout(TabStripNudgeButton* button) {
    tab_strip_action_container()->OnTabStripNudgeButtonTimeout(button);
  }

  void SetLockedExpansionMode(LockedExpansionMode mode,
                              TabStripNudgeButton* button) {
    tab_strip_action_container()->SetLockedExpansionMode(mode, button);
  }
  void OnButtonClicked(TabStripNudgeButton* button) {
    if (button == TabDeclutterButton()) {
      tab_strip_action_container()->OnTabDeclutterButtonClicked();
    } else if (button == AutoTabGroupButton()) {
      tab_strip_action_container()->OnAutoTabGroupButtonClicked();
    }
  }
  void OnButtonDismissed(TabStripNudgeButton* button) {
    if (button == TabDeclutterButton()) {
      tab_strip_action_container()->OnTabDeclutterButtonDismissed();
    } else if (button == AutoTabGroupButton()) {
      tab_strip_action_container()->OnAutoTabGroupButtonDismissed();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, ShowsDeclutterChip) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ShowsAndHidesDeclutterChip) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  // Finish showing declutter chip.
  tab_strip_action_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Hide the declutter chip.
  HideTabStripNudgeButton(TabDeclutterButton());

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenDeclutterButtonClicked) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(TabDeclutterButton());

  OnButtonClicked(TabDeclutterButton());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 0, 1);
  // Bucketed CTR metric should reflect one show and one click, with fewer than
  // 15 total tabs.
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 10, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenDeclutterButtonDismissed) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(TabDeclutterButton());

  OnButtonDismissed(TabDeclutterButton());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenDeclutterButtonTimeout) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(TabDeclutterButton());

  OnTabStripNudgeButtonTimeout(TabDeclutterButton());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 2, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsSuccessWhenAutoTabGroupsButtonClicked) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(AutoTabGroupButton());

  tab_strip_action_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);

  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_strip_action_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  OnButtonClicked(AutoTabGroupButton());

  histogram_tester.ExpectUniqueSample("Tab.Organization.AllEntrypoints.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 0, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsFailureWhenAutoTabGroupsButtonDismissed) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(AutoTabGroupButton());

  tab_strip_action_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);

  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_strip_action_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  OnButtonDismissed(AutoTabGroupButton());

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, DelaysShow) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kWillShow, TabDeclutterButton());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, DelaysHide) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  ShowTabStripNudgeButton(TabDeclutterButton());

  tab_strip_action_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, TabDeclutterButton());

  HideTabStripNudgeButton(TabDeclutterButton());

  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonClicked) {
  ShowTabStripNudgeButton(TabDeclutterButton());
  tab_strip_action_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, TabDeclutterButton());

  OnButtonClicked(TabDeclutterButton());

  EXPECT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonDismissed) {
  ShowTabStripNudgeButton(TabDeclutterButton());
  tab_strip_action_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, TabDeclutterButton());

  OnButtonDismissed(TabDeclutterButton());

  EXPECT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}
