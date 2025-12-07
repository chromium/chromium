// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_utils.h"
#include "url/gurl.h"

namespace page_actions {
namespace {

// Constants for available space adjustments.
constexpr size_t kFullSpaceTextLength = 0;
constexpr size_t kReducedSpaceTextLength = 500;

bool IsLabelVisible(PageActionView* page_action) {
  return page_action->IsChipVisible() &&
         page_action->GetLabelForTesting()->width() != 0;
}

bool IsAtMinimumSize(PageActionView* page_action) {
  return page_action->size() == page_action->GetMinimumSize();
}

bool IsIconCentered(PageActionView* page_action) {
  const auto* const image_container = page_action->GetImageContainerView();
  return image_container->x() ==
         page_action->width() - image_container->bounds().right();
}

void EnsurePageActionEnabled(actions::ActionId action_id) {
  auto* action = actions::ActionManager::Get().FindAction(action_id);
  CHECK(action);
  action->SetEnabled(true);
  action->SetVisible(true);
}

MATCHER(IsChipExpanded, "Check if the chip is expanded") {
  if (arg == nullptr) {
    *result_listener << "Page action is null";
    return false;
  }
  if (!IsLabelVisible(arg)) {
    *result_listener << "Label is not visible";
    return false;
  }
  if (IsAtMinimumSize(arg)) {
    *result_listener << "Chip is at minimum size, Size: "
                     << arg->size().ToString();
    return false;
  }
  if (arg->is_animating_label()) {
    *result_listener << "Page action is animating";
    return false;
  }
  if (IsIconCentered(arg)) {
    *result_listener << "Chip is centered, Insets: "
                     << arg->GetInsets().ToString();
    return false;
  }

  return true;
}

MATCHER(IsChipCollapsed, "Check if the chip is collapsed") {
  if (arg == nullptr) {
    *result_listener << "Page action is null";
    return false;
  }
  if (IsLabelVisible(arg)) {
    *result_listener << "Label is visible";
    return false;
  }
  if (!IsAtMinimumSize(arg)) {
    *result_listener << "Chip is not at minimum size, Size: "
                     << arg->size().ToString();
    return false;
  }
  if (arg->is_animating_label()) {
    *result_listener << "Page action is animating";
    return false;
  }
  if (!IsIconCentered(arg)) {
    *result_listener << "Chip is not centered, Insets: "
                     << arg->GetInsets().ToString();
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
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {
                features::kPageActionsMigration,
                {
                    {features::kPageActionsMigrationZoom.name, "true"},
                    {features::kPageActionsMigrationTranslate.name, "true"},
                    {features::kPageActionsMigrationMemorySaver.name, "true"},
                },
            },
            {lens::features::kLensOverlayOmniboxEntryPoint, {}},
        },
        /*disabled_features=*/{
            lens::features::kLensOverlay,
        });
  }

  virtual ~PageActionUiTestBase() = default;

  virtual Browser* GetBrowser() const = 0;

  page_actions::PageActionController* page_action_controller() const {
    return GetBrowser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->page_action_controller();
  }

  LocationBarView* location_bar() const {
    return BrowserView::GetBrowserViewForBrowser(GetBrowser())
        ->toolbar()
        ->location_bar();
  }

  OmniboxViewViews* omnibox_view() const {
    return static_cast<OmniboxViewViews*>(location_bar()->omnibox_view());
  }

  PageActionContainerView* page_action_container() const {
    return location_bar()->page_action_container();
  }

  PageActionView* GetPageActionView(actions::ActionId action_id) const {
    return page_action_container()->GetPageActionView(action_id);
  }

  PageActionView* GetTestPageActionView() const {
    return GetPageActionView(kActionShowTranslate);
  }

  void FastForwardAnimation(PageActionView* view) {
    auto animation = std::make_unique<gfx::AnimationTestApi>(
        &view->GetSlideAnimationForTesting());
    auto now = base::TimeTicks::Now();
    animation->SetStartTime(now);
    animation->Step(now + base::Minutes(1));
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

  PageActionView* GetTranslatePageActionView() const {
    return GetPageActionView(kActionShowTranslate);
  }

  PageActionView* GetMemorySaverPageActionView() const {
    return GetPageActionView(kActionShowMemorySaverChip);
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

    // Step 2: Immediately unhide the page actions.
    page_action_controller()->SetShouldHidePageActions(false);

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
  Browser* GetBrowser() const override { return browser(); }
};

// Tests that switching from a full available space to a reduced available space
// collapses the suggestion chip from label mode to icon-only mode.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipCollapsesToIconWhenSpaceIsReduced) {
  PageActionView* view = GetTestPageActionView();

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipExpanded());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipCollapsed());
}

// Tests that increasing available space from reduced to full restores the
// suggestion chip label (expanding from icon-only to label mode).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipRestoresLabelWhenSpaceIsRestored) {
  AdjustAvailableSpace(kReducedSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipExpanded());
}

// Tests that transitioning from full available space to reduced and then back
// to full toggles the suggestion chip between label and icon modes.
IN_PROC_BROWSER_TEST_F(
    PageActionInteractiveUiTest,
    SuggestionChipTransitionsBetweenLabelAndIconWhenSpaceChanges) {
  PageActionView* view = GetTestPageActionView();

  AdjustAvailableSpace(kFullSpaceTextLength);
  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipExpanded());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipExpanded());
}

// Tests that starting with reduced space, moving to full space, and then
// reverting to reduced space toggles the suggestion chip between icon-only and
// label modes repeatedly.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipSwitchesModesOnMultipleSpaceAdjustments) {
  PageActionView* view = GetTestPageActionView();
  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipExpanded());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  ShowTestSuggestionChip();
  FastForwardAnimation(view);

  EXPECT_THAT(view, IsChipCollapsed());
}

// Tests that calling ShowPageAction on a page action results in an icon-only
// view, ignoring any extra available space.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionDisplaysIconOnlyRegardlessOfAvailableSpace) {
  ShowTestPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

  EXPECT_THAT(view, IsChipCollapsed());
}

// Tests that once a page action is shown as an icon-only view, it remains
// icon-only through available space adjustments (both increased and reduced).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionIconRemainsUnchangedThroughSpaceAdjustments) {
  ShowTestPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

  EXPECT_THAT(view, IsChipCollapsed());

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_THAT(view, IsChipCollapsed());

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
}

// Tests that toggling the suggestion chip state for two actions reorders their
// views appropriately.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipReordersMultipleActions) {
  PageActionContainerView* container = page_action_container();
  ASSERT_TRUE(container);

  PageActionView* memory_saver_view = GetMemorySaverPageActionView();
  ASSERT_TRUE(memory_saver_view);
  PageActionView* translate_view = GetTranslatePageActionView();
  ASSERT_TRUE(translate_view);

  auto initial_memory_saver_index = container->GetIndexOf(memory_saver_view);
  ASSERT_TRUE(initial_memory_saver_index.has_value());
  auto initial_translate_index = container->GetIndexOf(translate_view);
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
    auto new_translate_index = container->GetIndexOf(translate_view);
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 0u);
  }
  // The memory saver view, now a non-chip, should follow the chip.
  // Since translate is at 0, memory saver (if it was initially at 1) should be
  // at 1.
  {
    auto new_memory_saver_index = container->GetIndexOf(memory_saver_view);
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(), 1u);
  }

  // Step 2: Activate suggestion chip for the memory saver page action as well.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder
  // again.
  ShowMemorySaverSuggestionChip();

  // Now both are chips. The new logic sorts chips by their initial insertion
  // order. Since translate was initially before memory saver, translate should
  // remain at index 0.
  {
    auto new_translate_index = container->GetIndexOf(translate_view);
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 0u);
  }
  // And the memory saver view should now be at index 1, immediately after
  // the translate chip, preserving its relative initial order among chips.
  {
    auto new_memory_saver_index = container->GetIndexOf(memory_saver_view);
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(), 1u);
  }

  // Step 3: Hide the translate suggestion chip.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder.
  // Only memory saver is a chip now.
  HideSuggestionChip(kActionShowTranslate);

  // Memory saver should now be the only active chip and move to index 0.
  {
    auto new_memory_saver_index = container->GetIndexOf(memory_saver_view);
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(), 0u);
  }
  // Translate is no longer a chip. It should be placed after the memory saver
  // chip, maintaining its initial relative order among non-chips.
  // In this case, it will be at index 1.
  {
    auto new_translate_index = container->GetIndexOf(translate_view);
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 1u);
  }

  // Step 4: Hide the memory saver suggestion chip.
  // This should trigger PageActionContainerView::NormalizePageActionViewOrder.
  // No chips are active.
  HidePageAction(kActionShowMemorySaverChip);

  // With no active chips, all icons should revert to their original relative
  // order.
  {
    auto final_translate_index = container->GetIndexOf(translate_view);
    ASSERT_TRUE(final_translate_index.has_value());
    EXPECT_EQ(final_translate_index.value(), initial_translate_index.value());
  }
  {
    auto final_memory_saver_index = container->GetIndexOf(memory_saver_view);
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
  PerformBackNavigation(browser()->tab_strip_model()->GetActiveWebContents());

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
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 1);
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      PageActionIconType::kTranslate, 1);

  // 2) Hide and re-show the same Translate icon within the same page context
  //    (same tab, same navigation). Because it's ephemeral and already shown,
  //    the histogram should not increment again.
  HidePageAction(kActionShowTranslate);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 1);
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      PageActionIconType::kTranslate, 1);

  // 3) Navigate to a new URL in the same tab (tab[0]). This is now a new page
  //    context. Showing the ephemeral Translate action again in this context
  //    should increment the histogram by 1.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 2);
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      PageActionIconType::kTranslate, 2);

  // 4) Open a brand new tab (tab[1]) and activate it. Because each tab
  // maintains its own context, showing ephemeral actions for the first time in
  // tab[1] should log again. Then, show both the Translate icon and the Memory
  // Saver chip here, which should each increment the histogram for their
  // respective actions.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://version"), ui::PAGE_TRANSITION_LINK));
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Show ephemeral Translate action in tab[1].
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 3);
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      PageActionIconType::kTranslate, 3);

  // Show ephemeral Memory Saver chip in tab[1].
  ShowPageAction(kActionShowMemorySaverChip);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 4);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kTranslate, 3);
  histogram_tester.ExpectBucketCount("PageActionController.ActionTypeShown2",
                                     PageActionIconType::kMemorySaver, 1);

  // 5) Switch back to tab[0] (where the Translate action was already shown
  // after navigation). Re-showing the ephemeral icon should NOT increment the
  // metric, since it's the same context in tab[0].
  browser()->tab_strip_model()->ActivateTabAt(0);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 4);
}

// Verifies that "…Icon.CTR2" histograms emit kShown once-per-context.
// The test mirrors EphemeralPageActionUmaLoggedOncePerContext.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       CTR2HistogramsLoggedOncePerContext) {
  base::HistogramTester histogram_tester;

  constexpr char kGeneralHistogram[] = "PageActionController.Icon.CTR2";
  constexpr char kTranslateHistogram[] =
      "PageActionController.Translate.Icon.CTR2";

  // 1. Initial page-context (tab[0], first navigation).
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectUniqueSample(kGeneralHistogram,
                                      PageActionCTREvent::kShown, 1);
  histogram_tester.ExpectUniqueSample(kTranslateHistogram,
                                      PageActionCTREvent::kShown, 1);

  // 2. Hide + re-show in the SAME context → no additional logging.
  HidePageAction(kActionShowTranslate);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount(kGeneralHistogram, 1);
  histogram_tester.ExpectTotalCount(kTranslateHistogram, 1);

  // 3. New navigation in the SAME tab → new context, logs again.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://settings")));
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount(kGeneralHistogram, 2);
  histogram_tester.ExpectBucketCount(kTranslateHistogram,
                                     PageActionCTREvent::kShown, 2);

  // 4. Open a new tab → brand-new context.
  ASSERT_TRUE(
      AddTabAtIndex(1, GURL("chrome://version"), ui::PAGE_TRANSITION_LINK));
  browser()->tab_strip_model()->ActivateTabAt(1);

  // 4-a) First show of Translate in tab[1] logs again.
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount(kGeneralHistogram, 3);
  histogram_tester.ExpectBucketCount(kTranslateHistogram,
                                     PageActionCTREvent::kShown, 3);

  browser()->tab_strip_model()->ActivateTabAt(0);
  ShowPageAction(kActionShowTranslate);
  histogram_tester.ExpectTotalCount(kGeneralHistogram, 3);
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
  Browser* GetBrowser() const override { return browser(); }

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
  Browser* GetBrowser() const final { return browser(); }

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
    PageActionView* test_view = GetTestPageActionView();
    EXPECT_FALSE(test_view->GetVisible());
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
    PageActionView* test_view = GetTestPageActionView();
    EXPECT_TRUE(test_view->GetVisible());
    EXPECT_FALSE(IsLabelVisible(test_view));
    EXPECT_TRUE(IsAtMinimumSize(test_view));
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
    FastForwardAnimation(GetTestPageActionView());
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    PageActionView* test_view = GetTestPageActionView();
    EXPECT_TRUE(test_view->GetVisible());
    EXPECT_TRUE(IsLabelVisible(test_view));
    EXPECT_FALSE(IsAtMinimumSize(test_view));
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
    FastForwardAnimation(GetTestPageActionView());
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    PageActionView* test_view = GetTestPageActionView();
    EXPECT_TRUE(test_view->GetVisible());
    EXPECT_FALSE(IsLabelVisible(test_view));
    EXPECT_TRUE(IsAtMinimumSize(test_view));
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
    PageActionContainerView* container = page_action_container();
    PageActionView* memory_saver_view = GetMemorySaverPageActionView();
    PageActionView* translate_view = GetTranslatePageActionView();

    // Get the current indices as optionals.
    auto memory_saver_index = container->GetIndexOf(memory_saver_view);
    auto translate_index = container->GetIndexOf(translate_view);
    if (!memory_saver_index.has_value() || !translate_index.has_value()) {
      return false;
    }

    // Expect the Translate action (suggestion chip) to be at index 0,
    // and the memory saver page action to be at index 1.
    EXPECT_EQ(translate_index.value(), 0u);
    EXPECT_EQ(memory_saver_index.value(), 1u);

    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelReorderTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

}  // namespace
}  // namespace page_actions
