// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"

namespace page_actions {
namespace {

// Wide window: Chip shows with full insets.
constexpr gfx::Rect kWideWindowBounds(0, 0, 1400, 920);
// Mid window: Chip shows with full insets.
constexpr gfx::Rect kMidWindowBounds(0, 0, 1024, 920);
// Very small window: Chip shows with icon-only insets.
constexpr gfx::Rect kVerySmallWindowBounds(0, 0, 100, 100);
// Tiny window: Chip shows with icon-only insets.
constexpr gfx::Rect kTinyWindowBounds(0, 0, 10, 10);

class PageActionInteractiveUiTest : public InteractiveBrowserTest {
 public:
  PageActionInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        {lens::features::kLensOverlay, ::features::kPageActionsMigration}, {});
  }

  PageActionInteractiveUiTest(const PageActionInteractiveUiTest&) = delete;
  PageActionInteractiveUiTest& operator=(const PageActionInteractiveUiTest&) =
      delete;

  ~PageActionInteractiveUiTest() override = default;

  page_actions::PageActionController* page_action_controller() {
    return browser()
        ->GetActiveTabInterface()
        ->GetTabFeatures()
        ->page_action_controller();
  }

  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar();
  }

  PageActionContainerView* page_action_container() {
    return location_bar()->page_action_container();
  }

  PageActionView* get_page_action_view(const actions::ActionId& action_id) {
    return page_action_container()->GetPageActionView(action_id);
  }

  bool label_visible(PageActionView* page_action) {
    return page_action->GetLabelForTesting()->size().width() != 0;
  }

  bool is_at_minimum_size(PageActionView* page_action) {
    return page_action->size() == page_action->GetMinimumSize();
  }

  void ShowSuggestionChip(const actions::ActionId& action_id) {
    page_action_controller()->ShowSuggestionChip(action_id);
  }

  void ShowPageAction(const actions::ActionId& action_id) {
    page_action_controller()->Show(action_id);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that switching from a mid-sized window to a tiny window collapses the
// suggestion chip from label mode to icon-only mode (and correct insets).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       OpenWideWindowAndResizeItToSmallWindow) {
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);
  browser()->window()->SetBounds(kMidWindowBounds);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);
  ASSERT_TRUE(view);

  EXPECT_TRUE(label_visible(view));

  browser()->window()->SetBounds(kTinyWindowBounds);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

// Tests that moving from a very small window to mid-sized restores the label
// for a suggestion chip.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest, ResizeSmallToLargeWindow) {
  browser()->window()->SetBounds(kVerySmallWindowBounds);
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kMidWindowBounds);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));
}

// Tests that transitioning from a wide window to a very small window collapses
// the suggestion chip from label mode to icon mode, and then re-expands back
// to label mode when returning to a wide window.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       WindowResizingTransitionsWideToSmall) {
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);
  browser()->window()->SetBounds(kMidWindowBounds);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kVerySmallWindowBounds);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kMidWindowBounds);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));
}

// Tests that starting small, moving to a mid-sized window, and then going back
// small again toggles the suggestion chip between icon and label modes.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       WindowResizingTransitionsSmallToWideToSmall) {
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);
  browser()->window()->SetBounds(kVerySmallWindowBounds);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kMidWindowBounds);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kVerySmallWindowBounds);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

// Tests that calling ShowPageAction(...) on a lens overlay results in an
// icon-only view, ignoring extra space.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest, ShowPageActionOnlyIcon) {
  browser()->window()->SetBounds(kWideWindowBounds);
  ShowPageAction(kActionSidePanelShowLensOverlayResults);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

// Tests that once we show a page action as an icon-only view, it remains
// icon-only through window resizing (wide or narrow).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       ShowPageActionIconRemainsOnResize) {
  browser()->window()->SetBounds(kWideWindowBounds);
  ShowPageAction(kActionSidePanelShowLensOverlayResults);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  // Confirm icon-only state in a wide window.
  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kVerySmallWindowBounds);

  // Expect the view to remain icon-only.
  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  browser()->window()->SetBounds(kWideWindowBounds);

  // Confirm it is still icon-only (unchanged by resizing).
  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

}  // namespace
}  // namespace page_actions
