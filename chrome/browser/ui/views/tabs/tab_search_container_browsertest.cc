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
#include "ui/gfx/animation/slide_animation.h"

class TabSearchContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchContainerBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kTabstripDeclutter}, {});
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

// TODO(crbug.com/338649929): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TogglesActionUIState DISABLED_TogglesActionUIState
#else
#define MAYBE_TogglesActionUIState TogglesActionUIState
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_TogglesActionUIState) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);
  tab_strip_model()->ForceShowingModalUIForTesting(false);
  service->OnTriggerOccured(browser());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

// TODO(crbug.com/338649929): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TogglesActionUIStateOnlyInCorrectBrowser \
  DISABLED_TogglesActionUIStateOnlyInCorrectBrowser
#else
#define MAYBE_TogglesActionUIStateOnlyInCorrectBrowser \
  TogglesActionUIStateOnlyInCorrectBrowser
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_TogglesActionUIStateOnlyInCorrectBrowser) {
  const Browser* const second_browser = CreateBrowser(browser()->profile());
  TabSearchContainer* const second_search_container =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->tab_strip_region_view()
          ->tab_search_container();

  ASSERT_FALSE(second_search_container->animation_session_for_testing());

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();
  // Same profile -> same service.
  ASSERT_EQ(service,
            second_search_container->tab_organization_service_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);
  tab_strip_model()->ForceShowingModalUIForTesting(false);
  second_search_container->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);
  second_browser->tab_strip_model()->ForceShowingModalUIForTesting(false);
  service->OnTriggerOccured(browser());

  EXPECT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
  EXPECT_FALSE(second_search_container->animation_session_for_testing());
}

// TODO(crbug.com/338649929): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DoesntShowIfTabStripModalUIExists \
  DISABLED_DoesntShowIfTabStripModalUIExists
#else
#define MAYBE_DoesntShowIfTabStripModalUIExists \
  DoesntShowIfTabStripModalUIExists
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_DoesntShowIfTabStripModalUIExists) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_strip_model()->ForceShowingModalUIForTesting(true);
  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_strip_model()->ForceShowingModalUIForTesting(false);
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       BlocksTabStripModalUIWhileShown) {
  ASSERT_TRUE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);

  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->HideTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(0);

  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(browser()->tab_strip_model()->CanShowModalUI());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, DelaysShow) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillShow,
      tab_search_container()->auto_tab_group_button());
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, DelaysHide) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide,
      tab_search_container()->auto_tab_group_button());
  tab_search_container()->HideTabOrganization(
      tab_search_container()->auto_tab_group_button());

  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonClicked) {
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide,
      tab_search_container()->auto_tab_group_button());

  tab_search_container()->OnAutoTabGroupButtonClicked();

  EXPECT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonDismissed) {
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide,
      tab_search_container()->auto_tab_group_button());

  tab_search_container()->OnAutoTabGroupButtonDismissed();

  EXPECT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       DelayedHidesWhenOrganizeButtonTimesOut) {
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide,
      tab_search_container()->auto_tab_group_button());

  tab_search_container()->OnOrganizeButtonTimeout(
      tab_search_container()->auto_tab_group_button());

  EXPECT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone,
      tab_search_container()->auto_tab_group_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsSuccessWhenButtonClicked) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnAutoTabGroupButtonClicked();

  histogram_tester.ExpectUniqueSample("Tab.Organization.AllEntrypoints.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 0, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsFailureWhenButtonDismissed) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnAutoTabGroupButtonDismissed();

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsFailureWhenButtonTimeout) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnOrganizeButtonTimeout(
      tab_search_container()->auto_tab_group_button());

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);

  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 2, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       HidesAutoTabGroupButtonFromHalfway) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  gfx::SlideAnimation* expansion_animation =
      tab_search_container()
          ->animation_session_for_testing()
          ->expansion_animation();

  gfx::AnimationTestApi animation_api(expansion_animation);
  base::TimeTicks now = base::TimeTicks::Now();
  animation_api.SetStartTime(now);
  animation_api.Step(now + (expansion_animation->GetSlideDuration() / 2));

  double expanded_value = expansion_animation->GetCurrentValue();
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  tab_search_container()->HideTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());

  EXPECT_EQ(tab_search_container()
                ->animation_session_for_testing()
                ->expansion_animation()
                ->GetCurrentValue(),
            expanded_value);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, ShowsDeclutterChip) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       ShowsAndHidesDeclutterChip) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  // Finish showing declutter chip.
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Hide the declutter chip.
  tab_search_container()->HideTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       DoesNotShowDeclutterChipWhenAutoTabGroupChipIsShown) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  // Show the auto-tab group chip.
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
  tab_search_container()
      ->animation_session_for_testing()
      ->ResetAnimationForTesting(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Try to show the declutter chip while auto-tab group chip is already shown.
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());
}
