// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observation.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/base/test/ui_controls.h"
#include "ui/views/interaction/polling_view_observer.h"
#include "ui/views/view_observer.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTabContents);

constexpr char kDocumentWithTitle[] = "/title3.html";

}  // namespace

class TabStripInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabStripInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{tabs::kTabStripUnification});
  }
  ~TabStripInteractiveUiTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  BrowserView* GetBrowserView() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripInteractiveUiTest, HoverEffectShowsOnMouseOver) {
  using Observer = views::test::PollingViewObserver<bool, TabStrip>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(Observer, kTabStripHoverState);
  RunTestSequence(
      InstrumentTab(kFirstTabContents, 0),
      NavigateWebContents(kFirstTabContents,
                          embedded_test_server()->GetURL(kDocumentWithTitle)),
      AddInstrumentedTab(kSecondTabContents,
                         embedded_test_server()->GetURL(kDocumentWithTitle)),
      HoverTabAt(0), FinishTabstripAnimations(),
      PollView(kTabStripHoverState, kTabStripElementId,
               [](const TabStrip* tab_strip) -> bool {
                 return tab_strip->tab_at(0)
                            ->tab_style_views()
                            ->GetHoverAnimationValue() > 0;
               }),
      WaitForState(kTabStripHoverState, true));
}

DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<int>,
                                    kTabCountState);

class TestNewTabButtonContextMenu : public TabStripInteractiveUiTest {
 public:
  TestNewTabButtonContextMenu() {
    scoped_feature_list_.InitWithFeatures(
        {features::kTabGroupMenuMoreEntryPoints}, {tabs::kTabStripUnification});
  }

  TabStrip* tabstrip() {
    return views::AsViewClass<HorizontalTabStripRegionView>(
               BrowserView::GetBrowserViewForBrowser(browser())
                   ->tab_strip_view())
        ->tab_strip();
  }
  TabStripController* controller() { return tabstrip()->controller(); }

  auto WaitForTabCount(Browser* browser, int expected_count) {
    return Steps(
        PollState(kTabCountState,
                  [browser]() { return browser->tab_strip_model()->count(); }),
        WaitForState(kTabCountState, expected_count),
        StopObservingState(kTabCountState));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO (crbug.com/447617263) rewrite these tests so that they work on mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VerifyNewTabButtonContextMenu \
  DISABLED_VerifyNewTabButtonContextMenu
#else
#define MAYBE_VerifyNewTabButtonContextMenu VerifyNewTabButtonContextMenu
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TestNewTabButtonContextMenu,
                       MAYBE_VerifyNewTabButtonContextMenu) {
  RunTestSequence(
      FinishTabstripAnimations(), EnsurePresent(kNewTabButtonElementId),
      MoveMouseTo(kNewTabButtonElementId), ClickMouse(ui_controls::RIGHT),
      EnsurePresent(NewTabButtonMenuModel::kNewTab),
      EnsurePresent(NewTabButtonMenuModel::kNewTabInGroup),
      EnsurePresent(NewTabButtonMenuModel::kNewSplitView),
      EnsurePresent(NewTabButtonMenuModel::kCreateNewTabGroup),
      SendAccelerator(NewTabButtonMenuModel::kNewTab,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_NewTabButtonContextMenuSplitView \
  DISABLED_NewTabButtonContextMenuSplitView
#else
#define MAYBE_NewTabButtonContextMenuSplitView NewTabButtonContextMenuSplitView
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TestNewTabButtonContextMenu,
                       MAYBE_NewTabButtonContextMenuSplitView) {
  RunTestSequence(
      FinishTabstripAnimations(), EnsurePresent(kNewTabButtonElementId),
      MoveMouseTo(kNewTabButtonElementId), ClickMouse(ui_controls::RIGHT),
      EnsurePresent(NewTabButtonMenuModel::kNewTab),
      EnsurePresent(NewTabButtonMenuModel::kNewSplitView),
      SelectMenuItem(NewTabButtonMenuModel::kNewSplitView));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Split view should be open
  EXPECT_TRUE(browser_view->IsInSplitView());
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_NewTabButtonContextMenuSplitViewDisabled \
  DISABLED_NewTabButtonContextMenuSplitViewDisabled
#else
#define MAYBE_NewTabButtonContextMenuSplitViewDisabled \
  NewTabButtonContextMenuSplitViewDisabled
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TestNewTabButtonContextMenu,
                       MAYBE_NewTabButtonContextMenuSplitViewDisabled) {
  chrome::NewSplitTab(browser(), split_tabs::SplitTabLayout::kSideBySide,
                      split_tabs::SplitTabCreatedSource::kNewTabButton);
  RunTestSequence(
      FinishTabstripAnimations(), EnsurePresent(kNewTabButtonElementId),
      MoveMouseTo(kNewTabButtonElementId), ClickMouse(ui_controls::RIGHT),
      EnsurePresent(NewTabButtonMenuModel::kNewTab),
      EnsurePresent(NewTabButtonMenuModel::kNewSplitView),
      WaitForViewProperty(NewTabButtonMenuModel::kNewSplitView, views::View,
                          Enabled, false),
      SendAccelerator(NewTabButtonMenuModel::kNewTab,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)));

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Split view should be open
  EXPECT_TRUE(browser_view->IsInSplitView());
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_NewTabButtonNewTabInGroupDisabledWhenNoOpenGroups \
  DISABLED_NewTabButtonNewTabInGroupDisabledWhenNoOpenGroups
#else
#define MAYBE_NewTabButtonNewTabInGroupDisabledWhenNoOpenGroups \
  NewTabButtonNewTabInGroupDisabledWhenNoOpenGroups
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(
    TestNewTabButtonContextMenu,
    MAYBE_NewTabButtonNewTabInGroupDisabledWhenNoOpenGroups) {
  RunTestSequence(
      EnsurePresent(kNewTabButtonElementId),
      MoveMouseTo(kNewTabButtonElementId), ClickMouse(ui_controls::RIGHT),
      WaitForShow(NewTabButtonMenuModel::kNewTabInGroup),
      WaitForViewProperty(NewTabButtonMenuModel::kNewTabInGroup, views::View,
                          Enabled, false),
      SendAccelerator(NewTabButtonMenuModel::kNewTab,
                      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE)));
}

#if BUILDFLAG(IS_MAC)
#define MAYBE_NewTabButtonNewTabInMostRecentGroup \
  DISABLED_NewTabButtonNewTabInMostRecentGroup
#else
#define MAYBE_NewTabButtonNewTabInMostRecentGroup \
  NewTabButtonNewTabInMostRecentGroup
#endif  // BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(TestNewTabButtonContextMenu,
                       MAYBE_NewTabButtonNewTabInMostRecentGroup) {
  controller()->CreateNewTab(NewTabTypes::kNewTabCommand);
  controller()->CreateNewTab(NewTabTypes::kNewTabCommand);
  controller()->CreateNewTab(NewTabTypes::kNewTabCommand);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 4);

  browser()->tab_strip_model()->AddToNewGroup({1});
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({2});
  browser()->tab_strip_model()->AddToNewGroup({3});

  RunTestSequence(
      FinishTabstripAnimations(), SelectTab(kTabStripElementId, 1),
      SelectTab(kTabStripElementId, 2), SelectTab(kTabStripElementId, 3),
      SelectTab(kTabStripElementId, 2), SelectTab(kTabStripElementId, 0),
      MoveMouseTo(kNewTabButtonElementId), ClickMouse(ui_controls::RIGHT),
      SelectMenuItem(NewTabButtonMenuModel::kNewTabInGroup),
      WaitForTabCount(browser(), 5),
      CheckResult(
          [&]() {
            // Check that the most recent group got an extra tab.
            return tab_strip_model->group_model()
                ->GetTabGroup(group)
                ->tab_count();
          },
          2));
}

namespace {

// Helper class to observe bounds changes on the New Tab Button and assert
// that it only moves in a stable, jitter-free direction.
class NewTabButtonBoundsObserver : public views::ViewObserver {
 public:
  enum class Mode { kOnlyGrow, kOnlyShrink, kConstant };

  NewTabButtonBoundsObserver(views::View* view, Mode mode) : mode_(mode) {
    observation_.Observe(view);
  }
  ~NewTabButtonBoundsObserver() override = default;

  void OnViewBoundsChanged(views::View* observed_view) override {
    int current_x = observed_view->bounds().x();
    if (last_x_.has_value()) {
      if (mode_ == Mode::kOnlyGrow) {
        // Since we are only expanding the tab group, the New Tab Button
        // should only move to the right (increase x) or stay in place.
        // It should NEVER move to the left (jitter/flicker).
        EXPECT_GE(current_x, last_x_.value())
            << "New Tab Button moved to the left (flickered)! "
            << "Previous x: " << last_x_.value()
            << ", Current x: " << current_x;
      } else if (mode_ == Mode::kOnlyShrink) {
        // Since we are only shrinking (like during tab close), the New Tab
        // Button should only move to the left (decrease x) or stay in place.
        // It should NEVER move to the right (jitter/flicker).
        EXPECT_LE(current_x, last_x_.value())
            << "New Tab Button moved to the right (flickered)! "
            << "Previous x: " << last_x_.value()
            << ", Current x: " << current_x;
      } else if (mode_ == Mode::kConstant) {
        // In a constrained layout where the tab strip is already full, the
        // New Tab Button should remain completely stationary.
        EXPECT_EQ(current_x, last_x_.value())
            << "New Tab Button moved in constrained layout (flickered)! "
            << "Previous x: " << last_x_.value()
            << ", Current x: " << current_x;
      }
    }
    last_x_ = current_x;
  }

 private:
  Mode mode_;
  base::ScopedObservation<views::View, views::ViewObserver> observation_{this};
  std::optional<int> last_x_;
};

}  // namespace

// Verifies that in an unconstrained layout (wide window, few tabs), as a tab
// group name is expanded, the New Tab Button slides smoothly to the right
// without any backward jitter/flicker.
IN_PROC_BROWSER_TEST_F(
    TabStripInteractiveUiTest,
    NewTabButtonPositionStableDuringGroupRenameUnconstrained) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(tab_strip_model->count(), 1);

  // Create a tab group containing the single tab.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0});

  HorizontalTabStripRegionView* region_view =
      views::AsViewClass<HorizontalTabStripRegionView>(
          GetBrowserView()->tab_strip_view());
  ASSERT_NE(region_view, nullptr);

  views::View* new_tab_button =
      BrowserElementsViews::From(browser())->GetViewAs<views::View>(
          kNewTabButtonElementId);
  ASSERT_NE(new_tab_button, nullptr);

  // Set up the bounds observer to verify that it only moves right (grows).
  NewTabButtonBoundsObserver observer(
      new_tab_button, NewTabButtonBoundsObserver::Mode::kOnlyGrow);

  // Simulate typing a long name character by character.
  std::string name = "";
  for (int i = 0; i < 30; ++i) {
    name += "A";
    tab_groups::TabGroupVisualData visual_data(
        base::ASCIIToUTF16(name), tab_groups::TabGroupColorId::kGrey);
    tab_strip_model->group_model()->GetTabGroup(group)->SetVisualData(
        visual_data);

    // Instantly complete the animation and force layout to verify bounds
    // synchronously.
    region_view->tab_strip()->StopAnimating();
  }
}

// Verifies that in a constrained layout (narrow window or many tabs), as a tab
// group name is expanded, the New Tab Button remains completely stationary
// at the right edge without any 1-2px jitter.
IN_PROC_BROWSER_TEST_F(TabStripInteractiveUiTest,
                       NewTabButtonPositionStableDuringGroupRenameConstrained) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // Add many tabs to fill up the tab strip and force it into a shrunk state.
  for (int i = 0; i < 15; ++i) {
    chrome::NewTab(browser(), NewTabTypes::kNewTabCommand);
  }
  ASSERT_EQ(tab_strip_model->count(), 16);

  // Create a tab group.
  tab_groups::TabGroupId group = tab_strip_model->AddToNewGroup({0});

  HorizontalTabStripRegionView* region_view =
      views::AsViewClass<HorizontalTabStripRegionView>(
          GetBrowserView()->tab_strip_view());
  ASSERT_NE(region_view, nullptr);

  views::View* new_tab_button =
      BrowserElementsViews::From(browser())->GetViewAs<views::View>(
          kNewTabButtonElementId);
  ASSERT_NE(new_tab_button, nullptr);

  // Ensure the layout has settled into steady state first.
  region_view->tab_strip()->StopAnimating();

  // Set up the bounds observer to verify that the New Tab Button's position
  // remains completely constant.
  NewTabButtonBoundsObserver observer(
      new_tab_button, NewTabButtonBoundsObserver::Mode::kConstant);

  // Simulate typing a long name character by character.
  std::string name = "";
  for (int i = 0; i < 30; ++i) {
    name += "A";
    tab_groups::TabGroupVisualData visual_data(
        base::ASCIIToUTF16(name), tab_groups::TabGroupColorId::kGrey);
    tab_strip_model->group_model()->GetTabGroup(group)->SetVisualData(
        visual_data);

    // Instantly complete the animation and force layout to verify bounds
    // synchronously.
    region_view->tab_strip()->StopAnimating();
  }
}

// Verifies that during tab closure, the New Tab Button slides smoothly to the
// left without any backward jitter (jumping to the right) on any frame.
IN_PROC_BROWSER_TEST_F(TabStripInteractiveUiTest,
                       NewTabButtonPositionStableDuringTabClose) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Add a second tab so we can close one.
  chrome::NewTab(browser(), NewTabTypes::kNewTabCommand);
  ASSERT_EQ(tab_strip_model->count(), 2);

  HorizontalTabStripRegionView* region_view =
      views::AsViewClass<HorizontalTabStripRegionView>(
          GetBrowserView()->tab_strip_view());
  ASSERT_NE(region_view, nullptr);

  views::View* new_tab_button =
      BrowserElementsViews::From(browser())->GetViewAs<views::View>(
          kNewTabButtonElementId);
  ASSERT_NE(new_tab_button, nullptr);

  // Ensure layout is settled first.
  region_view->tab_strip()->StopAnimating();

  // Set up the bounds observer to verify that it only moves left (shrinks).
  NewTabButtonBoundsObserver observer(
      new_tab_button, NewTabButtonBoundsObserver::Mode::kOnlyShrink);

  // Close the second tab. This starts the tab closure animation in real-time.
  tab_strip_model->CloseWebContentsAt(1, TabCloseTypes::CLOSE_USER_GESTURE);

  // Wait for the tab strip animation to complete naturally.
  // During this real-time animation, the observer will assert bounds safety on
  // every single frame.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !region_view->tab_strip()->IsAnimatingInTabStrip(); }));
}
