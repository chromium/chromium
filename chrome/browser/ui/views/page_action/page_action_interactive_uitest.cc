// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/browser_actions.h"
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
#include "content/public/test/test_utils.h"
#include "ui/views/test/views_test_utils.h"

namespace page_actions {
namespace {

// Constants for available space adjustments.
constexpr size_t kFullSpaceTextLength = 0;
constexpr size_t kReducedSpaceTextLength = 500;

bool IsLabelVisible(PageActionView* page_action) {
  return page_action->GetLabelForTesting()->size().width() != 0;
}

bool IsAtMinimumSize(PageActionView* page_action) {
  return page_action->size() == page_action->GetMinimumSize();
}

class PageActionUiTestBase {
 public:
  PageActionUiTestBase() {
    feature_list_.InitWithFeatures(
        {lens::features::kLensOverlay, ::features::kPageActionsMigration}, {});
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

  PageActionView* GetPageActionView(const actions::ActionId& action_id) const {
    return page_action_container()->GetPageActionView(action_id);
  }

  PageActionView* GetLensPageActionView() const {
    return GetPageActionView(kActionSidePanelShowLensOverlayResults);
  }

  void ShowSuggestionChip(const actions::ActionId& action_id) const {
    page_action_controller()->ShowSuggestionChip(action_id);
  }

  void ShowPageAction(const actions::ActionId& action_id) const {
    page_action_controller()->Show(action_id);
  }

  void ShowLensPageActionIcon() const {
    ShowPageAction(kActionSidePanelShowLensOverlayResults);
  }

  void ShowLensSuggestionChip() const {
    ShowSuggestionChip(kActionSidePanelShowLensOverlayResults);
  }

  // Dynamically adjust the available space in the location bar by setting
  // the omnibox text length. A larger `text_length` will reduce available
  // space, while a smaller text_length (or 0) will increase available space.
  void AdjustAvailableSpace(size_t text_length) const {
    omnibox_view()->SetUserText(std::u16string(text_length, 'a'));
    views::test::RunScheduledLayout(
        BrowserView::GetBrowserViewForBrowser(GetBrowser()));
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
  ShowLensSuggestionChip();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetLensPageActionView();

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
}

// Tests that increasing available space from reduced to full restores the
// suggestion chip label (expanding from icon-only to label mode).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipRestoresLabelWhenSpaceIsRestored) {
  AdjustAvailableSpace(kReducedSpaceTextLength);
  ShowLensSuggestionChip();

  PageActionView* view = GetLensPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));
}

// Tests that transitioning from full available space to reduced and then back
// to full toggles the suggestion chip between label and icon modes.
IN_PROC_BROWSER_TEST_F(
    PageActionInteractiveUiTest,
    SuggestionChipTransitionsBetweenLabelAndIconWhenSpaceChanges) {
  ShowLensSuggestionChip();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetLensPageActionView();

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));
}

// Tests that starting with reduced space, moving to full space, and then
// reverting to reduced space toggles the suggestion chip between icon-only and
// label modes repeatedly.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       SuggestionChipSwitchesModesOnMultipleSpaceAdjustments) {
  ShowLensSuggestionChip();
  AdjustAvailableSpace(kReducedSpaceTextLength);

  PageActionView* view = GetLensPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
}

// Tests that calling ShowPageAction on a lens overlay results in an icon-only
// view, ignoring any extra available space.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionDisplaysIconOnlyRegardlessOfAvailableSpace) {
  ShowLensPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetLensPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
}

// Tests that once a page action is shown as an icon-only view, it remains
// icon-only through available space adjustments (both increased and reduced).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionIconRemainsUnchangedThroughSpaceAdjustments) {
  ShowLensPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetLensPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
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
    PageActionView* lens_view = GetLensPageActionView();
    EXPECT_TRUE(lens_view->GetVisible());
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
    ShowLensPageActionIcon();
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    PageActionView* lens_view = GetLensPageActionView();
    EXPECT_TRUE(lens_view->GetVisible());
    EXPECT_FALSE(IsLabelVisible(lens_view));
    EXPECT_TRUE(IsAtMinimumSize(lens_view));
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
    ShowLensSuggestionChip();
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    PageActionView* lens_view = GetLensPageActionView();
    EXPECT_TRUE(lens_view->GetVisible());
    EXPECT_TRUE(IsLabelVisible(lens_view));
    EXPECT_FALSE(IsAtMinimumSize(lens_view));
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
    ShowLensSuggestionChip();
    PageActionPixelTestBase::ShowUi(name);
  }

  bool VerifyUi() override {
    PageActionView* lens_view = GetLensPageActionView();
    EXPECT_TRUE(lens_view->GetVisible());
    EXPECT_FALSE(IsLabelVisible(lens_view));
    EXPECT_TRUE(IsAtMinimumSize(lens_view));
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(PageActionPixelShowChipReducedTest, InvokeUi_Default) {
  ShowAndVerifyUi();
}

}  // namespace
}  // namespace page_actions
