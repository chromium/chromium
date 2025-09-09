// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_search_container.h"

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
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
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

namespace {

using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;

class TabSearchContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabSearchContainerBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kTabOrganization, features::kTabstripDeclutter},
        {features::kTabstripComboButton});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabSearchContainer* tab_search_container() {
    return BrowserElementsViews::From(browser())->GetViewAs<TabSearchContainer>(
        kTabSearchContainerElementId);
  }

  // Returns an assertion result that the expansion animation is closing.
  AssertionResult ExpansionAnimationIsClosing() {
    if (!tab_search_container()) {
      return AssertionFailure() << "tab_search_container is null.";
    }
    if (!tab_search_container()->animation_session_for_testing()) {
      return AssertionFailure() << "animation_session_for_testing is null.";
    }
    if (!tab_search_container()
             ->animation_session_for_testing()
             ->expansion_animation()) {
      return AssertionFailure() << "expansion_animation is null.";
    }
    return tab_search_container()
                   ->animation_session_for_testing()
                   ->expansion_animation()
                   ->IsClosing()
               ? AssertionSuccess()
               : AssertionFailure() << "expansion_animation is not closing.";
  }

 protected:
  void ResetAnimation(int value) {
    if (tab_search_container()->animation_session_for_testing()) {
      tab_search_container()
          ->animation_session_for_testing()
          ->ResetOpacityAnimationForTesting(value);
    }
    if (tab_search_container()->animation_session_for_testing()) {
      tab_search_container()
          ->animation_session_for_testing()
          ->ResetExpansionAnimationForTesting(value);
    }
    if (tab_search_container()->animation_session_for_testing()) {
      tab_search_container()
          ->animation_session_for_testing()
          ->ResetFlatEdgeAnimationForTesting(value);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
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

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TogglesActionUIStateOnlyInCorrectBrowser \
  DISABLED_TogglesActionUIStateOnlyInCorrectBrowser
#else
#define MAYBE_TogglesActionUIStateOnlyInCorrectBrowser \
  TogglesActionUIStateOnlyInCorrectBrowser
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_TogglesActionUIStateOnlyInCorrectBrowser) {
  Browser* const second_browser = CreateBrowser(browser()->profile());
  TabSearchContainer* const second_search_container =
      BrowserElementsViews::From(second_browser)
          ->GetViewAs<TabSearchContainer>(kTabSearchContainerElementId);

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

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
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

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_BlocksTabStripModalUIWhileShown \
  DISABLED_BlocksTabStripModalUIWhileShown
#else
#define MAYBE_BlocksTabStripModalUIWhileShown BlocksTabStripModalUIWhileShown
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_BlocksTabStripModalUIWhileShown) {
  ASSERT_TRUE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kNone, nullptr);
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  ResetAnimation(1);

  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  tab_search_container()->HideTabOrganization(
      tab_search_container()->auto_tab_group_button());

  EXPECT_FALSE(browser()->tab_strip_model()->CanShowModalUI());

  ResetAnimation(0);

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

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DelaysHide DISABLED_DelaysHide
#else
#define MAYBE_DelaysHide DelaysHide
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest, MAYBE_DelaysHide) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
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

  EXPECT_TRUE(ExpansionAnimationIsClosing());
}

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ImmediatelyHidesWhenOrganizeButtonClicked \
  DISABLED_ImmediatelyHidesWhenOrganizeButtonClicked
#else
#define MAYBE_ImmediatelyHidesWhenOrganizeButtonClicked \
  ImmediatelyHidesWhenOrganizeButtonClicked
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_ImmediatelyHidesWhenOrganizeButtonClicked) {
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide,
      tab_search_container()->auto_tab_group_button());

  tab_search_container()->OnAutoTabGroupButtonClicked();

  EXPECT_TRUE(ExpansionAnimationIsClosing());
}

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ImmediatelyHidesWhenOrganizeButtonDismissed \
  DISABLED_ImmediatelyHidesWhenOrganizeButtonDismissed
#else
#define MAYBE_ImmediatelyHidesWhenOrganizeButtonDismissed \
  ImmediatelyHidesWhenOrganizeButtonDismissed
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_ImmediatelyHidesWhenOrganizeButtonDismissed) {
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  tab_search_container()->SetLockedExpansionModeForTesting(
      LockedExpansionMode::kWillHide,
      tab_search_container()->auto_tab_group_button());

  tab_search_container()->OnAutoTabGroupButtonDismissed();

  EXPECT_TRUE(ExpansionAnimationIsClosing());
}

// TODO(crbug.com/414839512): Fix flaky test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DelayedHidesWhenOrganizeButtonTimesOut \
  DISABLED_DelayedHidesWhenOrganizeButtonTimesOut
#else
#define MAYBE_DelayedHidesWhenOrganizeButtonTimesOut \
  DelayedHidesWhenOrganizeButtonTimesOut
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_DelayedHidesWhenOrganizeButtonTimesOut) {
  // RunScheduledLayout() is needed due to widget auto-resize.
  views::test::RunScheduledLayout(tab_search_container());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
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

  EXPECT_TRUE(ExpansionAnimationIsClosing());
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsSuccessWhenAutoTabGroupsButtonClicked) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
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
                       LogsFailureWhenAutoTabGroupsButtonDismissed) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_search_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  tab_search_container()->OnAutoTabGroupButtonDismissed();

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 1, 1);
}

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_LogsFailureWhenAutoTabGroupsButtonTimeout \
  DISABLED_LogsFailureWhenAutoTabGroupsButtonTimeout
#else
#define MAYBE_LogsFailureWhenAutoTabGroupsButtonTimeout \
  LogsFailureWhenAutoTabGroupsButtonTimeout
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_LogsFailureWhenAutoTabGroupsButtonTimeout) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());
  ResetAnimation(1);
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

// TODO(crbug.com/409311762): This test is flaky, fix and re-enable if work on
// declutter resumes.
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       DISABLED_LogsWhenDeclutterButtonClicked) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  tab_search_container()->OnTabDeclutterButtonClicked();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 0, 1);
  // Bucketed CTR metric should reflect one show and one click, with fewer than
  // 15 total tabs.
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 10, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsWhenDeclutterButtonDismissed) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  tab_search_container()->OnTabDeclutterButtonDismissed();

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       LogsWhenDeclutterButtonTimeout) {
  base::HistogramTester histogram_tester;

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  tab_search_container()->OnOrganizeButtonTimeout(
      tab_search_container()->tab_declutter_button());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 2, 1);
}

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HidesAutoTabGroupButtonFromHalfway \
  DISABLED_HidesAutoTabGroupButtonFromHalfway
#else
#define MAYBE_HidesAutoTabGroupButtonFromHalfway \
  HidesAutoTabGroupButtonFromHalfway
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_HidesAutoTabGroupButtonFromHalfway) {
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

  EXPECT_TRUE(ExpansionAnimationIsClosing());

  EXPECT_EQ(tab_search_container()
                ->animation_session_for_testing()
                ->expansion_animation()
                ->GetCurrentValue(),
            expanded_value);
}

// TODO(crbug.com/414839512): Fix flaky test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowsDeclutterChip DISABLED_ShowsDeclutterChip
#else
#define MAYBE_ShowsDeclutterChip ShowsDeclutterChip
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_ShowsDeclutterChip) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ShowsAndHidesDeclutterChip DISABLED_ShowsAndHidesDeclutterChip
#else
#define MAYBE_ShowsAndHidesDeclutterChip ShowsAndHidesDeclutterChip
#endif
IN_PROC_BROWSER_TEST_F(TabSearchContainerBrowserTest,
                       MAYBE_ShowsAndHidesDeclutterChip) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  // Finish showing declutter chip.
  ResetAnimation(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Hide the declutter chip.
  tab_search_container()->HideTabOrganization(
      tab_search_container()->tab_declutter_button());

  EXPECT_TRUE(ExpansionAnimationIsClosing());
}

// TODO(crbug.com/413441658): Flaky on Windows 10 builds.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DoesNotShowDeclutterChipWhenAutoTabGroupChipIsShown \
  DISABLED_DoesNotShowDeclutterChipWhenAutoTabGroupChipIsShown
#else
#define MAYBE_DoesNotShowDeclutterChipWhenAutoTabGroupChipIsShown \
  DoesNotShowDeclutterChipWhenAutoTabGroupChipIsShown
#endif
IN_PROC_BROWSER_TEST_F(
    TabSearchContainerBrowserTest,
    MAYBE_DoesNotShowDeclutterChipWhenAutoTabGroupChipIsShown) {
  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());

  // Show the auto-tab group chip.
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->auto_tab_group_button());

  ASSERT_TRUE(tab_search_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
  ResetAnimation(1);
  tab_search_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Try to show the declutter chip while auto-tab group chip is already shown.
  tab_search_container()->ShowTabOrganization(
      tab_search_container()->tab_declutter_button());

  ASSERT_FALSE(tab_search_container()->animation_session_for_testing());
}

}  // namespace
