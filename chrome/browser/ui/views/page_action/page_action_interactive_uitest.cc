// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
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

void EnsurePageActionEnabled(actions::ActionId action_id) {
  auto* action = actions::ActionManager::Get().FindAction(action_id);
  CHECK(action);
  action->SetEnabled(true);
  action->SetVisible(true);
}

class PageActionUiTestBase {
 public:
  PageActionUiTestBase() {
    feature_list_.InitWithFeatures({features::kPageActionsMigration},
                                   {lens::features::kLensOverlay});
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

  void ShowSuggestionChip(actions::ActionId action_id) const {
    EnsurePageActionEnabled(action_id);
    page_action_controller()->ShowSuggestionChip(action_id);
  }

  PageActionView* GetTranslatePageActionView() const {
    return GetPageActionView(kActionShowTranslate);
  }

  PageActionView* GetMemorySaverPageActionView() const {
    return GetPageActionView(kActionShowMemorySaverChip);
  }

  void ShowPageAction(actions::ActionId action_id) const {
    EnsurePageActionEnabled(kActionShowTranslate);
    page_action_controller()->Show(action_id);
  }

  void ShowTestPageActionIcon() const { ShowPageAction(kActionShowTranslate); }

  void ShowTestSuggestionChip() const {
    ShowPageAction(kActionShowTranslate);
    ShowSuggestionChip(kActionShowTranslate);
  }

  void ShowTranslatePageActionIcon() const {
    ShowPageAction(kActionShowTranslate);
  }

  void ShowTranslateSuggestionChip() const {
    ShowPageAction(kActionShowTranslate);
    ShowSuggestionChip(kActionShowTranslate);
  }

  void ShowMemorySaverPageActionIcon() const {
    ShowSuggestionChip(kActionShowMemorySaverChip);
  }

  void ShowMemorySaverSuggestionChip() const {
    ShowPageAction(kActionShowMemorySaverChip);
    ShowSuggestionChip(kActionShowMemorySaverChip);
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
  ShowTestSuggestionChip();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

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
  ShowTestSuggestionChip();

  PageActionView* view = GetTestPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));
}

// Tests that transitioning from full available space to reduced and then back
// to full toggles the suggestion chip between label and icon modes.
IN_PROC_BROWSER_TEST_F(
    PageActionInteractiveUiTest,
    SuggestionChipTransitionsBetweenLabelAndIconWhenSpaceChanges) {
  ShowTestSuggestionChip();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

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
  ShowTestSuggestionChip();
  AdjustAvailableSpace(kReducedSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kFullSpaceTextLength);

  EXPECT_TRUE(IsLabelVisible(view));
  EXPECT_FALSE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
}

// Tests that calling ShowPageAction on a page action results in an icon-only
// view, ignoring any extra available space.
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionDisplaysIconOnlyRegardlessOfAvailableSpace) {
  ShowTestPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));
}

// Tests that once a page action is shown as an icon-only view, it remains
// icon-only through available space adjustments (both increased and reduced).
IN_PROC_BROWSER_TEST_F(PageActionInteractiveUiTest,
                       PageActionIconRemainsUnchangedThroughSpaceAdjustments) {
  ShowTestPageActionIcon();
  AdjustAvailableSpace(kFullSpaceTextLength);

  PageActionView* view = GetTestPageActionView();

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

  AdjustAvailableSpace(kReducedSpaceTextLength);

  EXPECT_FALSE(IsLabelVisible(view));
  EXPECT_TRUE(IsAtMinimumSize(view));

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
  // memory saver page action.
  EXPECT_LT(initial_translate_index.value(),
            initial_memory_saver_index.value());

  // Step 1: Activate suggestion chip for the translate action only.
  ShowTranslateSuggestionChip();

  // Expect translate view to move to the front (index 0).
  {
    auto new_translate_index = container->GetIndexOf(translate_view);
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 0u);
  }
  // The memory saver should at the initial index.
  {
    auto new_memory_saver_index = container->GetIndexOf(memory_saver_view);
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(),
              initial_memory_saver_index.value());
  }

  // Step 2: Activate suggestion chip for the memory saver page action as well.
  ShowMemorySaverSuggestionChip();

  // Now the memory saver view should move to the front.
  {
    auto new_memory_saver_index = container->GetIndexOf(memory_saver_view);
    ASSERT_TRUE(new_memory_saver_index.has_value());
    EXPECT_EQ(new_memory_saver_index.value(), 0u);
  }
  // And the translate view should now be at index 1.
  {
    auto new_translate_index = container->GetIndexOf(translate_view);
    ASSERT_TRUE(new_translate_index.has_value());
    EXPECT_EQ(new_translate_index.value(), 1u);
  }
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
    ShowSuggestionChip(kActionShowTranslate);

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
