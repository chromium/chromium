// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <optional>
#include <string>

#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/page_action/page_action_enums.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/anchored_message_view.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/native_theme/mock_os_settings_provider.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"

namespace page_actions {
namespace {

// Constants for available space adjustments.
constexpr size_t kFullSpaceTextLength = 0;
constexpr size_t kReducedSpaceTextLength = 500;

void EnsurePageActionEnabled(actions::ActionId action_id) {
  auto* action = actions::ActionManager::Get().FindAction(action_id);
  CHECK(action);
  action->SetEnabled(true);
  action->SetVisible(true);
}

MATCHER(IsChipExpanded, "Check if the chip is expanded") {
  PageActionTestAccessor accessor = arg;
  if (!accessor.GetVisible()) {
    *result_listener << "Page action is not visible";
    return false;
  }
  if (!accessor.IsLabelVisible()) {
    *result_listener << "Label is not visible";
    return false;
  }
  if (accessor.IsAtMinimumSize()) {
    *result_listener << "Chip is at minimum size";
    return false;
  }
  if (accessor.IsAnimating()) {
    *result_listener << "Page action is animating";
    return false;
  }
  if (accessor.IsIconCentered()) {
    *result_listener << "Chip icon is centered";
    return false;
  }

  return true;
}

MATCHER(IsChipCollapsed, "Check if the chip is collapsed") {
  PageActionTestAccessor accessor = arg;
  if (!accessor.GetVisible()) {
    *result_listener << "Page action is not visible";
    return false;
  }
  if (accessor.IsLabelVisible()) {
    *result_listener << "Label is visible";
    return false;
  }
  if (!accessor.IsAtMinimumSize()) {
    *result_listener << "Chip is not at minimum size";
    return false;
  }
  if (accessor.IsAnimating()) {
    *result_listener << "Page action is animating";
    return false;
  }
  if (!accessor.IsIconCentered()) {
    *result_listener << "Chip icon is not centered";
    return false;
  }

  return true;
}

class PageActionUiTestBase {
 public:
  PageActionUiTestBase() {
    // TODO(crbug.com/424806660): These tests should not be reliant on
    // kLensOverlayOmniboxEntryPoint being enabled, but disabling it causes them
    // to fail.
    // TODO(crbug.com/482339938): SuggestionChipReordersMultipleActions is
    // failing when kPageActionsPrioritySelector is enabled, since those 2 chips
    // are no longer allowed to show at the same time.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {lens::features::kLensOverlayOmniboxEntryPoint, {}},
        },
        /*disabled_features=*/{
            lens::features::kLensOverlay,
            features::kPageActionsPrioritySelector,
            features::kWebUILocationBar,
        });
  }

  virtual ~PageActionUiTestBase() = default;

  virtual BrowserWindowInterface* GetBrowser() const = 0;

  page_actions::PageActionController* page_action_controller() const {
    return GetBrowser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->page_action_controller();
  }

  LocationBar* location_bar() const {
    return BrowserView::GetBrowserViewForBrowser(GetBrowser())
        ->GetLocationBar();
  }

  OmniboxView* omnibox_view() const { return location_bar()->GetOmniboxView(); }

  PageActionTestAccessor GetPageAction(actions::ActionId action_id) const {
    return PageActionTestAccessor(GetBrowser(), action_id);
  }

  PageActionTestAccessor GetTestPageAction() const {
    return GetPageAction(kActionShowTranslate);
  }

  PageActionTestAccessor GetTranslatePageAction() const {
    return GetPageAction(kActionShowTranslate);
  }

  PageActionTestAccessor GetMemorySaverPageAction() const {
    return GetPageAction(kActionShowMemorySaverChip);
  }

  void FastForwardAnimation(PageActionTestAccessor action) {
    action.FinishAnimation();
    EnsureLayout();
  }

  void ShowSuggestionChip(actions::ActionId action_id) const {
    EnsurePageActionEnabled(action_id);
    page_action_controller()->ShowSuggestionChip(
        action_id, {.should_animate = false, .should_announce_chip = false});
  }

  void HideSuggestionChip(actions::ActionId action_id) const {
    page_action_controller()->HideSuggestionChip(action_id);
  }

  void ShowAnchoredMessage(actions::ActionId action_id,
                           std::u16string text,
                           AnchoredMessageActionIconType icon_type,
                           std::optional<ui::ImageModel> anchored_message_icon,
                           std::unique_ptr<ui::SimpleMenuModel> menu) const {
    EnsurePageActionEnabled(action_id);
    page_action_controller()->SetAnchoredMessageText(action_id, text);
    page_action_controller()->SetAnchoredMessageAction(action_id, icon_type,
                                                       std::move(menu));
    if (anchored_message_icon == std::nullopt) {
      page_action_controller()->ClearAnchoredMessageIcon(action_id);
    } else {
      page_action_controller()->SetAnchoredMessageIcon(
          action_id, anchored_message_icon.value());
    }
    page_action_controller()->ShowAnchoredMessage(action_id, {});
  }

  void HideAnchoredMessage(actions::ActionId action_id) const {
    page_action_controller()->HideAnchoredMessage(action_id);
  }

  void ShowPageAction(actions::ActionId action_id) const {
    EnsurePageActionEnabled(action_id);
    page_action_controller()->Show(action_id);
  }

  void HidePageAction(actions::ActionId action_id) const {
    EnsurePageActionEnabled(action_id);
    page_action_controller()->Hide(action_id);
  }

  void ShowTestPageActionIcon() const { ShowPageAction(kActionShowTranslate); }

  void ShowTestSuggestionChip() const {
    ShowPageAction(kActionShowTranslate);
    ShowSuggestionChip(kActionShowTranslate);
  }

  void ShowTranslatePageActionIcon() const {
    HideSuggestionChip(kActionShowTranslate);
    ShowPageAction(kActionShowTranslate);
  }

  void ShowTranslateSuggestionChip() const {
    ShowPageAction(kActionShowTranslate);
    ShowSuggestionChip(kActionShowTranslate);
  }

  void ShowMemorySaverPageActionIcon() const {
    HideSuggestionChip(kActionShowMemorySaverChip);
    ShowPageAction(kActionShowMemorySaverChip);
  }

  void ShowMemorySaverSuggestionChip() const {
    ShowPageAction(kActionShowMemorySaverChip);
    ShowSuggestionChip(kActionShowMemorySaverChip);
  }

  // Dynamically adjust the available space in the location bar by setting
  // the omnibox text length. A larger `text_length` will reduce available
  // space, while a smaller text_length (or 0) will increase available space.
  void AdjustAvailableSpace(size_t text_length) {
    omnibox_view()->SetUserText(std::u16string(text_length, 'a'));

    // Immediately unhide the page actions.
    page_action_controller()->SetShouldHidePageActions(false);
    if (features::IsWebUILocationBarEnabled()) {
      if (auto* browser_view =
              BrowserView::GetBrowserViewForBrowser(GetBrowser())) {
        if (auto* webui_view = browser_view->toolbar_button_provider()
                                   ->GetWebUIToolbarViewForTesting()) {
          if (auto* loc_bar = webui_view->GetLocationBar()) {
            loc_bar->page_action_control().SetShouldHidePageActions(false);
          }
        }
      }
    }

    EnsureLayout();
  }

  void EnsureLayout() {
    views::test::RunScheduledLayout(
        BrowserView::GetBrowserViewForBrowser(GetBrowser()));
  }

 protected:
  void PerformBackNavigation(content::WebContents* web_contents) {
    content::NavigationController& controller = web_contents->GetController();
    ASSERT_TRUE(controller.CanGoBack());
    content::TestNavigationObserver back_observer(web_contents);
    controller.GoBack();
    back_observer.Wait();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PageActionInteractiveUiTest : public InteractiveBrowserTest,
                                    public PageActionUiTestBase {
 public:
  PageActionInteractiveUiTest() = default;
  PageActionInteractiveUiTest(const PageActionInteractiveUiTest&) = delete;
  PageActionInteractiveUiTest& operator=(const PageActionInteractiveUiTest&) =
      delete;
  ~PageActionInteractiveUiTest() override = default;

  // PageActionUiTestBase:
  BrowserWindowInterface* GetBrowser() const override { return browser(); }
};

// Tests that switching from a full available space to a reduced available space
// collapses the suggestion chip from label mode to icon-only mode.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipCollapsesToIconWhenSpaceIsReduced) {
  auto action = GetTestPageAction();

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipExpanded());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipCollapsed());
}

// Tests that increasing available space from reduced to full restores the
// suggestion chip label (expanding from icon-only to label mode).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipRestoresLabelWhenSpaceIsRestored) {
  AdjustAvailableSpace(kReducedSpaceTextLength);

  auto action = GetTestPageAction();

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipExpanded());
}

// Tests that transitioning from full available space to reduced and then back
// to full toggles the suggestion chip between label and icon modes.
IN_PROC_BROWSER_TEST_F(
    PageActionInteractiveUiTest,
    SuggestionChipTransitionsBetweenLabelAndIconWhenSpaceChanges) {
  auto action = GetTestPageAction();

  AdjustAvailableSpace(kFullSpaceTextLength);
  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipExpanded());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipExpanded());
}

// Tests that starting with reduced space, moving to full space, and then
// reverting to reduced space toggles the suggestion chip between icon-only and
// label modes repeatedly.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipSwitchesModesOnMultipleSpaceAdjustments) {
  auto action = GetTestPageAction();
  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipExpanded());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(action);

  EXPECT_THAT(action, IsChipCollapsed());
}

// Tests that calling ShowPageAction on a page action results in an icon-only
// view, ignoring any extra available space.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionDisplaysIconOnlyRegardlessOfAvailableSpace) {
  ShowTestPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  auto action = GetTestPageAction();

  EXPECT_THAT(action, IsChipCollapsed());
}

// Tests that once a page action is shown as an icon-only view, it remains
// icon-only through available space adjustments (both increased and reduced).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionIconRemainsUnchangedThroughSpaceAdjustments) {
  ShowTestPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  auto action = GetTestPageAction();

  EXPECT_THAT(action, IsChipCollapsed());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_THAT(action, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_THAT(action, IsChipCollapsed());
}

// Tests that toggling the suggestion chip state for two actions reorders their
// views appropriately.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipReordersMultipleActions) {
  ShowTranslatePageActionIcon();
  ShowMemorySaverPageActionIcon();

  auto memory_saver_action = GetMemorySaverPageAction();
  auto translate_action = GetTranslatePageAction();

  auto initial_memory_saver_index = memory_saver_action.GetIndex();
  ASSERT_TRUE(initial_memory_saver_index.has_value());
  auto initial_translate_index = translate_action.GetIndex();
  ASSERT_TRUE(initial_translate_index.has_value());

  // For this test, we assume that the translate page action appears before the
  // memory saver page action initially. This is crucial for the new logic,
  // as the initial order determines the relative order of chips.
  EXPECT_LT(initial_translate_index.value(),
            initial_memory_saver_index.value());

  // Step 1: Activate suggestion chip for the translate action only.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder.
  ShowTranslateSuggestionChip();

  // Expect translate view to move to the front (index 0) as it's the only chip.
  {
    auto new_translate_index = translate_action.GetIndex();
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 0u);
  }
  // The memory saver view, now a non-chip, should follow the chip.
  // Since translate is at index 0, the memory saver should maintain its
  // relative order among non-chips.
  {
    auto new_memory_saver_index = memory_saver_action.GetIndex();
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(),
              initial_memory_saver_index.value());
  }

  // Step 2: Activate suggestion chip for the memory saver page action as well.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder
  // again.
  ShowMemorySaverSuggestionChip();

  // Now both are chips. The new logic sorts chips by their initial insertion
  // order. Since translate was initially before memory saver, translate should
  // remain at index 0.
  {
    auto new_translate_index = translate_action.GetIndex();
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 0u);
  }
  // And the memory saver view should now be at index 1, immediately after
  // the translate chip, preserving its relative initial order among chips.
  {
    auto new_memory_saver_index = memory_saver_action.GetIndex();
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(), 1u);
  }

  // Step 3: Hide the translate suggestion chip.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder.
  // Only memory saver is a chip now.
  HideSuggestionChip(kActionShowTranslate);

  // Memory saver should now be the only active chip and move to index 0.
  {
    auto new_memory_saver_index = memory_saver_action.GetIndex();
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(), 0u);
  }
  // Translate is no longer a chip. It should be placed after the memory saver
  // chip, maintaining its initial relative order among non-chips.
  // In this case, it will be at index 1 + its initial index (since Memory Saver
  // is the only chip at index 0, and it was initially after Translate).
  {
    auto new_translate_index = translate_action.GetIndex();
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(),
              1u + initial_translate_index.value());
  }

  // Step 4: Hide the memory saver suggestion chip.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder.
  // No chips are active.
  HidePageAction(kActionShowMemorySaverChip);

  // With no active chips, all icons should revert to their original relative
  // order.
  {
    auto final_translate_index = translate_action.GetIndex();
    ASSERT_TRUE(final_translate_index.has_value());
    EXPECT_EQ(final_translate_index.value(), initial_translate_index.value());
  }
  {
    auto final_memory_saver_index = memory_saver_action.GetIndex();
    ASSERT_TRUE(final_memory_saver_index.has_value());
    EXPECT_EQ(final_memory_saver_index.value(),
              initial_memory_saver_index.value());
  }
}

IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       EphemeralPageActionUmaNotLoggedOnBackNavigation) {
  // This test verifies that when we navigate back to a previously visited URL
  // in the same tab, ephemeral actions are *not* re-logged to
  // "PageActionController.ActionTypeShown2". The ephemeral action has already
  // been logged for that page context, so it shouldn't increment again.

  base::HistogramTester histogram_tester;

  // Step 1: Show ephemeral Translate action in our initial context (tab[0]).
  //         This should increment the histogram by 1.
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 1);
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      PageActionIconType::kTranslate, 1);

  // Step 2: Navigate forward to a new URL. This new navigation is a different
  //         page context, so showing the ephemeral action again logs a second
  //         time.
  GURL next_url("chrome://version");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), next_url));
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 2);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 2);

  // Step 3: Go back to the previous URL in the same tab. This *reverts* to the
  //         old page context that already had ephemeral actions shown/logged.
  //         Therefore, re-showing the ephemeral action now should NOT increment
  //         the histogram again.
  PerformBackNavigation(browser()->GetTabStripModel()->GetActiveWebContents());

  ShowPageAction(kActionShowTranslate);

  // Histogram should increase at 3 total samples; since the url have changed in
  // the same page.
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 3);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 3);
}

IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       EphemeralPageActionUmaLoggedOncePerContext) {
  // This test verifies that ephemeral page actions (like a Translate icon or
  // Memory Saver chip) only log to "PageActionController.ActionTypeShown2" the
  // first time they appear in a given page context. A "page context" is
  // determined by the combination of (tab, navigation). Re-showing the same
  // ephemeral action in the same context should NOT increment the histogram,
  // whereas switching tabs or navigating creates a new context that does log
  // again.

  base::HistogramTester histogram_tester;

  // 1) Show the ephemeral Translate action in the initial tab (tab[0]) for the
  //    very first time. This should increment the histogram by 1.
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 1);

  // 2) Hide and re-show the same Translate icon within the same page context
  //    (same tab, same navigation). Because it's ephemeral and already shown,
  //    the histogram should not increment again.
  HidePageAction(kActionShowTranslate);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 1);

  // 3) Navigate to a new URL in the same tab (tab[0]). This is now a new page
  //    context. Showing the ephemeral Translate action again in this context
  //    should increment the histogram by 1.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 2);

  // 4) Open a brand new tab (tab[1]) and activate it. Because each tab
  // maintains its own context, showing ephemeral actions for the first time in
  // tab[1] should log again. Then, show both the Translate icon and the Memory
  // Saver chip here, which should each increment the histogram for their
  // respective actions.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://version"), ui::PAGE_TRANSITION_LINK));
  browser()->GetTabStripModel()->ActivateTabAt(1);

  // Show ephemeral Translate action in tab[1].
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 3);

  // Show ephemeral Memory Saver chip in tab[1].
  ShowPageAction(kActionShowMemorySaverChip);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 3);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kMemorySaver, 1);

  // 5) Switch back to tab[0] (where the Translate action was already shown
  // after navigation). Re-showing the ephemeral icon should NOT increment the
  // metric, since it's the same context in tab[0].
  browser()->GetTabStripModel()->ActivateTabAt(0);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 3);
}

// Verifies that "…Icon.CTR2" histograms emit kShown once-per-context.
// The test mirrors EphemeralPageActionUmaLoggedOncePerContext.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       CTR2HistogramsLoggedOncePerContext) {
  base::HistogramTester histogram_tester;

  constexpr char kTranslateHistogram[] =
      "PageActionController.Translate.Icon.CTR2";

  // 1. Initial page-context (tab[0], first navigation).
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectUniqueSample(kTranslateHistogram,
                                      PageActionCTREvent::kShown, 1);

  // 2. Hide + re-show in the SAME context → no additional logging.
  HidePageAction(kActionShowTranslate);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount(kTranslateHistogram, 1);

  // 3. New navigation in the SAME tab → new context, logs again.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount(kTranslateHistogram,
                                     PageActionCTREvent::kShown, 2);

  // 4. Open a new tab → brand-new context.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://version"), ui::PAGE_TRANSITION_LINK));
  browser()->GetTabStripModel()->ActivateTabAt(1);

  // 4-a) First show of Translate in tab[1] logs again.
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount(kTranslateHistogram,
                                     PageActionCTREvent::kShown, 3);

  // 5. Switch back to tab[0] and show again → no additional logging.
  browser()->GetTabStripModel()->ActivateTabAt(0);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectBucketCount(kTranslateHistogram,
                                     PageActionCTREvent::kShown, 3);
}

class PageActionMetricsInteractiveUiTest : public InteractiveBrowserTest,
                                           public PageActionUiTestBase {
 public:
  PageActionMetricsInteractiveUiTest() = default;

  PageActionMetricsInteractiveUiTest(
      const PageActionMetricsInteractiveUiTest&) = delete;
  PageActionMetricsInteractiveUiTest& operator=(
      const PageActionInteractiveUiTest&) = delete;
  ~PageActionMetricsInteractiveUiTest() override = default;

  // PageActionUiTestBase:
  BrowserWindowInterface* GetBrowser() const override { return browser(); }

 protected:
  void SetZoomLevel(content::PageZoom zoom_level) {
    chrome::Zoom(GetBrowser(), zoom_level);
  }

  auto DoZoomIn() {
    return Do([&]() { SetZoomLevel(content::PAGE_ZOOM_IN); });
  }

  auto DoZoomOut() {
    return Do([&]() { SetZoomLevel(content::PAGE_ZOOM_OUT); });
  }
};

IN_PROC_BROWSER_TEST_F(PageActionMetricsInteractiveUiTest, ClickHistogramLogs) {
  base::HistogramTester histogram_tester;
  const char* general_histogram = "PageActionController.Icon.CTR2";
  const std::string specific_histogram = "PageActionController.Zoom.Icon.CTR2";

  RunTestSequence(
      DoZoomIn(), WaitForShow(kActionItemZoomElementId),

      CheckResult(
          [&]() { return histogram_tester.GetTotalSum(general_histogram); },
          testing::Eq(0)),
      CheckResult(
          [&]() { return histogram_tester.GetTotalSum(specific_histogram); },
          testing::Eq(0)),

      PressButton(kActionItemZoomElementId),

      CheckResult(
          [&]() {
            return histogram_tester.GetBucketCount(
                general_histogram, PageActionCTREvent::kClicked);
          },
          testing::Eq(1)),
      CheckResult(
          [&]() {
            return histogram_tester.GetBucketCount(
                specific_histogram, PageActionCTREvent::kClicked);
          },
          testing::Eq(1)),

      PressButton(kActionItemZoomElementId),

      CheckResult(
          [&]() {
            return histogram_tester.GetBucketCount(
                general_histogram, PageActionCTREvent::kClicked);
          },
          testing::Eq(2)),
      CheckResult(
          [&]() {
            return histogram_tester.GetBucketCount(
                specific_histogram, PageActionCTREvent::kClicked);
          },
          testing::Eq(2)));
}

// Verifies that the "NumberActionsShown3" exact-linear histogram records
// the correct bucket for one vs. two simultaneously visible ephemeral actions.
IN_PROC_BROWSER_TEST_F(PageActionMetricsInteractiveUiTest,
                       NumberActionsShown3HistogramLogged) {
  base::HistogramTester histogram_tester;

  // 1) Show the Translate suggestion chip (1 visible ephemeral action).
  ShowPageAction(kActionShowTranslate);

  // 2) Show the Memory Saver suggestion chip (now 2 visible ephemeral actions).
  ShowPageAction(kActionShowMemorySaverChip);

  // Expect exactly one sample in bucket “1” and one in bucket “2”.
  histogram_tester.ExpectBucketCount("PageActionController.NumberActionsShown3",
                                     1, 1);
  histogram_tester.ExpectBucketCount("PageActionController.NumberActionsShown3",
                                     2, 1);
}

// Verifies that the "PagesWithActionsShown3" enumeration histogram records
// a kPageShown on navigation, a kActionShown on the first ephemeral action,
// and a kMultipleActionsShown once two appear.
IN_PROC_BROWSER_TEST_F(PageActionMetricsInteractiveUiTest,
                       PagesWithActionsShown3EventsLogged) {
  base::HistogramTester histogram_tester;

  // Navigate to a fresh URL to trigger a kPageShown event.
  GURL test_url("chrome://version");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));

  // Show two ephemeral suggestion chips in sequence.
  ShowPageAction(kActionShowTranslate);        // logs kActionShown
  ShowPageAction(kActionShowMemorySaverChip);  // logs kMultipleActionsShown

  // Verify each enumeration event was recorded exactly once.
  histogram_tester.ExpectBucketCount(
      "PageActionController.PagesWithActionsShown3",
      PageActionPageEvent::kPageShown, 1);
  histogram_tester.ExpectBucketCount(
      "PageActionController.PagesWithActionsShown3",
      PageActionPageEvent::kActionShown, 1);
  histogram_tester.ExpectBucketCount(
      "PageActionController.PagesWithActionsShown3",
      PageActionPageEvent::kMultipleActionsShown, 1);
}

// TODO(crbug.com/411078148): Re-enable on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SuggestionChipWithAnnouncement \
  DISABLED_SuggestionChipWithAnnouncement
#else
#define MAYBE_SuggestionChipWithAnnouncement SuggestionChipWithAnnouncement
#endif
// Tests that showing a suggestion chip with announcements enabled will
// announce the chip on a screen reader.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       MAYBE_SuggestionChipWithAnnouncement) {
  views::test::AXEventCounter counter(views::AXUpdateNotifier::Get());
  ASSERT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  ShowTranslatePageActionIcon();
  page_action_controller()->ShowSuggestionChip(
      kActionShowTranslate, {
                                .should_animate = false,
                                .should_announce_chip = false,
                            });
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kAlert));

  // Reshow the chip with announcements enabled.
  HideSuggestionChip(kActionShowTranslate);
  page_action_controller()->ShowSuggestionChip(kActionShowTranslate,
                                               {
                                                   .should_animate = false,
                                                   .should_announce_chip = true,
                                               });
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kAlert));
}

class PageActionPixelTestBase : public UiBrowserTest,
                                public PageActionUiTestBase {
 public:
  PageActionPixelTestBase() = default;
  PageActionPixelTestBase(const PageActionPixelTestBase&) = delete;
  PageActionPixelTestBase& operator=(const PageActionPixelTestBase&) = delete;
  ~PageActionPixelTestBase() override = default;

  // PageActionUiTestBase:
  BrowserWindowInterface* GetBrowser() const final { return browser(); }

  // UiBrowserTest:
  void ShowUi(const std::string& /*name*/) override {
    views::test::RunScheduledLayout(
        BrowserView::GetBrowserViewForBrowser(GetBrowser()));
  }

  void WaitForUserDismissal() final {}
};

class PageActionPixelIconsHiddenTest : public PageActionPixelTestBase {
 public:
  PageActionPixelIconsHiddenTest() = default;
  PageActionPixelIconsHiddenTest(const PageActionPixelIconsHiddenTest&) =
      delete;
  PageActionPixelIconsHiddenTest& operator=(
      const PageActionPixelIconsHiddenTest&) = delete;
  ~PageActionPixelIconsHiddenTest() override = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    // Default scenario: do nothing.
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    auto test_action = GetTestPageAction();
    EXPECT_FALSE(test_action.GetVisible());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelIconsHiddenTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

class PageActionPixelShowIconTest : public PageActionPixelTestBase {
 public:
  PageActionPixelShowIconTest() = default;
  PageActionPixelShowIconTest(const PageActionPixelShowIconTest&) = delete;
  PageActionPixelShowIconTest& operator=(const PageActionPixelShowIconTest&) =
      delete;
  ~PageActionPixelShowIconTest() override = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowTestPageActionIcon();
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    auto test_action = GetTestPageAction();
    EXPECT_THAT(test_action, IsChipCollapsed());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelShowIconTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

class PageActionPixelShowChipTest : public PageActionPixelTestBase {
 public:
  PageActionPixelShowChipTest() = default;
  PageActionPixelShowChipTest(const PageActionPixelShowChipTest&) = delete;
  PageActionPixelShowChipTest& operator=(const PageActionPixelShowChipTest&) =
      delete;
  ~PageActionPixelShowChipTest() override = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    AdjustAvailableSpace(kFullSpaceTextLength);
    ShowTestSuggestionChip();
    FastForwardAnimation(GetTestPageAction());
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    auto test_action = GetTestPageAction();
    EXPECT_THAT(test_action, IsChipExpanded());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelShowChipTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

class PageActionPixelShowChipReducedTest : public PageActionPixelTestBase {
 public:
  PageActionPixelShowChipReducedTest() = default;
  PageActionPixelShowChipReducedTest(
      const PageActionPixelShowChipReducedTest&) = delete;
  PageActionPixelShowChipReducedTest& operator=(
      const PageActionPixelShowChipReducedTest&) = delete;
  ~PageActionPixelShowChipReducedTest() override = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    AdjustAvailableSpace(kReducedSpaceTextLength);
    ShowTestSuggestionChip();
    FastForwardAnimation(GetTestPageAction());
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    auto test_action = GetTestPageAction();
    EXPECT_THAT(test_action, IsChipCollapsed());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelShowChipReducedTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

class PageActionPixelReorderTest : public PageActionPixelTestBase {
 public:
  PageActionPixelReorderTest() = default;
  PageActionPixelReorderTest(const PageActionPixelReorderTest&) = delete;
  PageActionPixelReorderTest& operator=(const PageActionPixelReorderTest&) =
      delete;
  ~PageActionPixelReorderTest() override = default;

  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    ShowMemorySaverPageActionIcon();

    // Now, activate the suggestion chip for the translate action.
    ShowTranslateSuggestionChip();

    // Run any pending layout tasks.
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    auto memory_saver_action = GetMemorySaverPageAction();
    auto translate_action = GetTranslatePageAction();

    // Get the current indices as optionals.
    auto memory_saver_index = memory_saver_action.GetIndex();
    auto translate_index = translate_action.GetIndex();
    if (!memory_saver_index.has_value() || !translate_index.has_value()) {
      return false;
    }

    // Expect the Translate action (suggestion chip) to be at index 0.
    EXPECT_EQ(translate_index.value(), 0u);
    // The memory saver page action should follow the chip.
    EXPECT_GT(memory_saver_index.value(), 0u);

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelReorderTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

class AnchoredMessageInteractiveTestBase : public InteractiveBrowserTest,
                                           public PageActionUiTestBase {
 public:
  AnchoredMessageInteractiveTestBase() = default;
  ~AnchoredMessageInteractiveTestBase() override = default;

  // Implements PageActionUiTestBase:
  BrowserWindowInterface* GetBrowser() const override { return browser(); }

  void ShowTestAnchoredMessage(
      std::u16string text,
      AnchoredMessageActionIconType icon_type,
      std::optional<ui::ImageModel> anchored_message_icon,
      std::unique_ptr<ui::SimpleMenuModel> menu) const {
    ShowPageAction(kActionShowTranslate);
    ShowAnchoredMessage(kActionShowTranslate, text, icon_type,
                        anchored_message_icon, std::move(menu));
  }

  void ShowTestAnchoredMessageWithExpandableContent(
      std::u16string text,
      std::optional<AnchoredMessageExpandableContent> expandable_content,
      std::unique_ptr<ui::SimpleMenuModel> menu = nullptr) const {
    ShowPageAction(kActionShowTranslate);
    page_action_controller()->SetAnchoredMessageExpandableContent(
        kActionShowTranslate, expandable_content);
    const AnchoredMessageActionIconType icon_type =
        menu ? AnchoredMessageActionIconType::kMenu
             : AnchoredMessageActionIconType::kNone;
    ShowAnchoredMessage(kActionShowTranslate, text, icon_type, std::nullopt,
                        std::move(menu));
  }

  auto WaitForDrawerAnimation() {
    return Do([this]() {
      auto* anchored_message_bubble =
          views::ElementTrackerViews::GetInstance()->GetUniqueView(
              AnchoredMessageBubbleView::kAnchoredMessageBubbleId,
              BrowserView::GetBrowserViewForBrowser(browser())
                  ->GetElementContext());
      if (anchored_message_bubble) {
        views::test::WaitForAnimatingLayoutManager(anchored_message_bubble);
      }
    });
  }
};

class PageActionPixelShowAnchoredMessageTest
    : public AnchoredMessageInteractiveTestBase {
 public:
  PageActionPixelShowAnchoredMessageTest() {
    os_settings_provider_.SetPreferredColorScheme(
        ui::NativeTheme::PreferredColorScheme::kLight);
  }
  ~PageActionPixelShowAnchoredMessageTest() override = default;

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
};

IN_PROC_BROWSER_TEST_F(PageActionPixelShowAnchoredMessageTest,
                       InvokeUi_Default) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot can only run in pixel_tests."),
      Do([this]() {
        ShowTestAnchoredMessage(u"", AnchoredMessageActionIconType::kNone,
                                std::nullopt, nullptr);
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId, "default",
                 "20260324"));
}

IN_PROC_BROWSER_TEST_F(PageActionPixelShowAnchoredMessageTest,
                       InvokeUi_CloseIcon) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot can only run in pixel_tests."),
      Do([this]() {
        ShowTestAnchoredMessage(u"", AnchoredMessageActionIconType::kClose,
                                std::nullopt, nullptr);
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId,
                 "close_icon", "20260324"),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageCloseIconId),
      WaitForHide(AnchoredMessageBubbleView::kAnchoredMessageBubbleId));
}

IN_PROC_BROWSER_TEST_F(PageActionPixelShowAnchoredMessageTest,
                       InvokeUi_NoChip) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot can only run in pixel_tests."),
      Do([this]() {
        ShowTestAnchoredMessage(
            u"", AnchoredMessageActionIconType::kNone,
            ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                               ? vector_icons::kEditFilledIcon
                                               : vector_icons::kEditOldIcon),
            nullptr);
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId, "no_chip",
                 "20260324"));
}

IN_PROC_BROWSER_TEST_F(PageActionPixelShowAnchoredMessageTest, InvokeUi_Menu) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kMenuItemId);

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(0, u"Menu Item 1");
  menu_model->SetElementIdentifierAt(0, kMenuItemId);
  menu_model->AddItem(1, u"Menu Item 2");

  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot can only run in pixel_tests."),
      Do([this, &menu_model]() {
        ShowTestAnchoredMessage(u"Menu Test",
                                AnchoredMessageActionIconType::kMenu,
                                std::nullopt, std::move(menu_model));
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId, "menu",
                 "20260324"),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageMenuIconId),
      WaitForShow(kMenuItemId),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId,
                 "menu_clicked", "20260324"),
      Do([this]() { HideAnchoredMessage(kActionShowTranslate); }),
      WaitForHide(kMenuItemId));
}

IN_PROC_BROWSER_TEST_F(PageActionPixelShowAnchoredMessageTest, InvokeUi_Text) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kIgnoreAndContinue,
                              "Screenshot can only run in pixel_tests."),
      Do([this]() {
        ShowTestAnchoredMessage(u"Anchored Message Text",
                                AnchoredMessageActionIconType::kNone,
                                std::nullopt, nullptr);
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId, "text",
                 "20260324"),
      Do([this]() -> void { HideAnchoredMessage(kActionShowTranslate); }),
      WaitForHide(AnchoredMessageBubbleView::kAnchoredMessageBubbleId));
}

IN_PROC_BROWSER_TEST_F(PageActionPixelShowAnchoredMessageTest,
                       ShowAndHideExpandedContent) {
  AnchoredMessageExpandableContent content;
  content.items.push_back({ui::ImageModel(), u"Item"});

  RunTestSequence(
      Do([this, content]() {
        ShowTestAnchoredMessageWithExpandableContent(u"Anchored with expand",
                                                     content);
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      EnsureNotPresent(
          AnchoredMessageBubbleView::kAnchoredMessageExpandedContentId),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      WaitForDrawerAnimation(),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageExpandedContentId),
      PressButton(AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
      WaitForDrawerAnimation(),
      WaitForHide(
          AnchoredMessageBubbleView::kAnchoredMessageExpandedContentId));
}

struct PageActionPixelTestParams {
  ui::NativeTheme::PreferredColorScheme color_scheme =
      ui::NativeTheme::PreferredColorScheme::kLight;
  bool rtl = false;
  int expandable_content_items = 0;

  std::string ToString() const {
    std::string name;
    if (expandable_content_items > 0) {
      name += base::StringPrintf("%dItems", expandable_content_items);
    }
    if (color_scheme == ui::NativeTheme::PreferredColorScheme::kDark) {
      name += "Dark";
    }
    if (rtl) {
      name += "Rtl";
    }
    if (name.empty()) {
      name = "Default";
    }
    return name;
  }
};

#if BUILDFLAG(IS_WIN)
// A flexible, parameterized screenshotting test class to cover any anchored
// message configuration. Extend the parameter class as needed.
// These tests run only on Windows, since they are dedicated to screenshot
// capture; tests that need to cover interactivity must be separate.
class PageActionAnchoredMessagePixelTest
    : public AnchoredMessageInteractiveTestBase,
      public testing::WithParamInterface<PageActionPixelTestParams> {
 public:
  PageActionAnchoredMessagePixelTest() {
    os_settings_provider_.SetPreferredColorScheme(GetParam().color_scheme);
  }
  ~PageActionAnchoredMessagePixelTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    scoped_rtl_.emplace(GetParam().rtl);
  }
  void TearDownOnMainThread() override {
    scoped_rtl_.reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  std::optional<AnchoredMessageExpandableContent> GetExpandableContent() const {
    const int num_items = GetParam().expandable_content_items;
    if (num_items <= 0) {
      return std::nullopt;
    }

    AnchoredMessageExpandableContent content;
    content.heading = base::ASCIIToUTF16(
        base::StringPrintf("Will share %d items", num_items));
    const std::array<std::u16string, 4> kItems = {{
        u"Site with sample items",
        u"Another site with more sample items",
        u"Sample items galore",
        u"A site that requires elision because it has a distinctly longer"
        u" description that will almost certainly overflow available space",
    }};
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 16);
    bitmap.eraseColor(SK_ColorRED);
    for (int i = 0; i < num_items; ++i) {
      const auto& text = kItems[static_cast<size_t>(i) % kItems.size()];
      content.items.push_back({ui::ImageModel::FromImageSkia(
                                   gfx::ImageSkia::CreateFrom1xBitmap(bitmap)),
                               text});
    }
    return content;
  }

  auto ExpandContentIfPresent() {
    return If(
        []() { return GetParam().expandable_content_items > 0; },
        Then(
            PressButton(
                AnchoredMessageBubbleView::kAnchoredMessageExpandButtonId),
            WaitForDrawerAnimation(),
            WaitForShow(
                AnchoredMessageBubbleView::kAnchoredMessageExpandedContentId)));
  }

 private:
  ui::MockOsSettingsProvider os_settings_provider_;
  std::optional<base::i18n::ScopedRTLForTesting> scoped_rtl_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    PageActionAnchoredMessagePixelTest,
    testing::ValuesIn(std::vector<PageActionPixelTestParams>{
        {},
        {
            .expandable_content_items = 4,
        },
        {
            .color_scheme = ui::NativeTheme::PreferredColorScheme::kDark,
            .expandable_content_items = 4,
        },
        {
            .rtl = true,
            .expandable_content_items = 4,
        },
        // Single item.
        {
            .expandable_content_items = 1,
        },
        // Multiple items, but not overflowing the expand button.
        {
            .expandable_content_items = 3,
        },
    }),
    [](const testing::TestParamInfo<PageActionPixelTestParams>& info) {
      return info.param.ToString();
    });

IN_PROC_BROWSER_TEST_P(PageActionAnchoredMessagePixelTest, Screenshots) {
  RunTestSequence(
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              "Screenshots not possible"),
      Do([this]() {
        auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
        menu_model->AddItem(0, u"Menu Item 1");
        ShowTestAnchoredMessageWithExpandableContent(u"Anchored with expand",
                                                     GetExpandableContent(),
                                                     std::move(menu_model));
      }),
      WaitForShow(AnchoredMessageBubbleView::kAnchoredMessageBubbleId),
      EnsureNotPresent(
          AnchoredMessageBubbleView::kAnchoredMessageExpandedContentId),
      ExpandContentIfPresent(),
      Screenshot(AnchoredMessageBubbleView::kAnchoredMessageBubbleId, "",
                 "7915728"));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace page_actions
