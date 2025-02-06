// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
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
#include "ui/views/test/views_test_utils.h"

namespace page_actions {
namespace {

// Constants for available space adjustments.
constexpr size_t kFullSpaceTextLength = 0;
constexpr size_t kReducedSpaceTextLength = 500;

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

  // Dynamically adjust the available space in the location bar by setting
  // the omnibox text length. A larger `text_length` will reduce available
  // space, while a smaller text_length (or 0) will increase available space.
  void AdjustAvailableSpace(size_t text_length) {
    location_bar()->omnibox_view()->SetUserText(
        std::u16string(text_length, 'a'));
    views::test::RunScheduledLayout(
        BrowserView::GetBrowserViewForBrowser(browser()));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that switching from a full available space to a reduced available space
// collapses the suggestion chip from label mode to icon-only mode.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipCollapsesToIconWhenSpaceIsReduced) {
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);

  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);
  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

// Tests that increasing available space from reduced to full restores the
// suggestion chip label (expanding from icon-only to label mode).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipRestoresLabelWhenSpaceIsRestored) {
  AdjustAvailableSpace(kReducedSpaceTextLength);
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));
}

// Tests that transitioning from full available space to reduced and then back
// to full correctly toggles the suggestion chip between label and icon modes.
IN_PROC_BROWSER_TEST_F(
    PageActionInteractiveUiTest,
    SuggestionChipTransitionsBetweenLabelAndIconWhenSpaceChanges) {
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);
  AdjustAvailableSpace(kFullSpaceTextLength);
  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));
}

// Tests that starting with reduced space, moving to full space, and then
// reverting to reduced space toggles the suggestion chip between icon-only and
// label modes repeatedly.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipSwitchesModesOnMultipleSpaceAdjustments) {
  ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);
  AdjustAvailableSpace(kReducedSpaceTextLength);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(label_visible(view));
  EXPECT_FALSE(is_at_minimum_size(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

// Tests that calling ShowPageAction on a lens overlay results in an icon-only
// view, ignoring any extra available space.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionDisplaysIconOnlyRegardlessOfAvailableSpace) {
  ShowPageAction(kActionSidePanelShowLensOverlayResults);
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

// Tests that once a page action is shown as an icon-only view, it remains
// icon-only through available space adjustments (both increased and reduced).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionIconRemainsUnchangedThroughSpaceAdjustments) {
  ShowPageAction(kActionSidePanelShowLensOverlayResults);
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view =
      get_page_action_view(kActionSidePanelShowLensOverlayResults);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_FALSE(label_visible(view));
  EXPECT_TRUE(is_at_minimum_size(view));
}

}  // namespace
}  // namespace page_actions
