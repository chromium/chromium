// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/split_tab_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/test/mock_activation_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

std::string OrientationTestName(
    const testing::TestParamInfo<TabStripOrientation>& info) {
  switch (info.param) {
    case TabStripOrientation::kVertical:
      return "Vertical";
    case TabStripOrientation::kHorizontal:
      return "Horizontal";
  }
}

}  // namespace

class BaseTabStripRegionViewBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  void SetUp() override {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
    activation_controller_ =
        std::make_unique<views::test::MockActivationController>();
#endif
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::SetUp();
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
#if BUILDFLAG(IS_MAC)
    // MockActivationController bypasses NativeWidgetMacNSWindowHost, preventing
    // InProcessBrowserTest's ScopedFakeFullKeyboardAccess from propagating to
    // FocusManager.
    BrowserView::GetBrowserViewForBrowser(browser())
        ->GetFocusManager()
        ->SetKeyboardAccessible(true);
#endif
  }

  void TearDownOnMainThread() override {
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
    activation_controller_.reset();
#endif
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::TearDownOnMainThread();
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsBrowserTestMixin<
        InProcessBrowserTest>::GetEnabledFeatures();
    enabled.emplace_back(tabs::kTabStripUnification, base::FieldTrialParams());
    return enabled;
  }

  TabStripOrientation orientation() const { return GetParam(); }

  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  BaseTabStripRegionView* region_view() {
    return views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  BaseTabStripRegionView* vertical_region_view() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->vertical_tab_strip_region_view_for_testing();
  }

  BaseTabStripRegionView* horizontal_region_view() {
    return views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->horizontal_tab_strip_region_view_for_testing());
  }

  views::View* GetTabViewAt(int index) {
    return region_view()->GetTabAnchorView(
        browser()->GetTabStripModel()->GetTabAtIndex(index)->GetHandle());
  }

  // Append tabs synchronously without waiting for load stop so multi-step tab
  // setup (e.g. grouping and splitting) completes before a layout pass runs.
  void AppendTab() {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), -1, true);
  }

 private:
#if defined(USE_MOCK_ACTIVATION_CONTROLLER)
  // Emulates widget activation in-process via RAII so parallel browser_tests
  // shards cannot steal OS window activation and clobber FocusManager focus.
  std::unique_ptr<views::test::MockActivationController> activation_controller_;
#endif
};

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest,
                       GetTabAnchorViewReturnsCorrectView) {
  AppendTab();
  AppendTab();
  AppendTab();

  const int tab_index = 1;
  tabs::TabInterface* tab =
      browser()->GetTabStripModel()->GetTabAtIndex(tab_index);
  views::View* tab_anchor_view =
      region_view()->GetTabAnchorView(tab->GetHandle());
  EXPECT_NE(nullptr, tab_anchor_view);

  const TabCollectionNode* node =
      root_node()->GetNodeForHandle(tab->GetHandle());
  EXPECT_EQ(node->view(), tab_anchor_view);
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest,
                       GetTabGroupAnchorViewReturnsCorrectView) {
  AppendTab();
  AppendTab();
  AppendTab();

  TabStripModel* model = browser()->GetTabStripModel();

  std::vector<int> tab_indices = {0, 1};
  std::optional<tab_groups::TabGroupId> group_id =
      model->AddToNewGroup(tab_indices);
  ASSERT_TRUE(group_id.has_value());

  views::View* group_anchor_view =
      region_view()->GetTabGroupAnchorView(group_id.value());
  EXPECT_NE(nullptr, group_anchor_view);

  const TabGroup* tab_group =
      model->group_model()->GetTabGroup(group_id.value());
  const TabCollectionNode* node =
      root_node()->GetNodeForHandle(tab_group->GetCollectionHandle());
  EXPECT_EQ(node->view(), group_anchor_view);
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest, CanFocusTabView) {
  AppendTab();

  views::View* first_tab_view = GetTabViewAt(0);
  ASSERT_TRUE(first_tab_view);

  views::FocusManager* focus_manager =
      BrowserView::GetBrowserViewForBrowser(browser())->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);
  focus_manager->SetFocusedView(first_tab_view);

  // Setting focus is only synchronous if the widget is active. Activating the
  // widget is an asynchronous process so wait until the view has focus.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return first_tab_view->HasFocus(); }));
  EXPECT_EQ(first_tab_view, focus_manager->GetFocusedView());
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest,
                       GetFocusedTabIndexReturnsCorrectIndex) {
  EXPECT_EQ(std::nullopt, region_view()->GetFocusedTabIndex());

  AppendTab();
  AppendTab();
  AppendTab();

  TabStripModel* model = browser()->GetTabStripModel();

  const int focused_tab_index = 1;
  model->ActivateTabAt(focused_tab_index,
                       TabStripUserGestureDetails(
                           TabStripUserGestureDetails::GestureType::kOther));

  views::View* tab_view = GetTabViewAt(focused_tab_index);

  views::FocusManager* focus_manager =
      BrowserView::GetBrowserViewForBrowser(browser())->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);

  focus_manager->SetFocusedView(tab_view);
  ASSERT_TRUE(base::test::RunUntil([&]() { return tab_view->HasFocus(); }));

  std::optional<int> focused_index = region_view()->GetFocusedTabIndex();
  ASSERT_TRUE(focused_index.has_value());
  EXPECT_EQ(focused_tab_index, focused_index.value());

  focus_manager->SetFocusedView(nullptr);
  ASSERT_TRUE(base::test::RunUntil([&]() { return !tab_view->HasFocus(); }));

  EXPECT_EQ(std::nullopt, region_view()->GetFocusedTabIndex());
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest,
                       AccessiblePaneViewDefaultFocusableChild) {
  AppendTab();
  AppendTab();

  browser()->GetTabStripModel()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  views::View* active_tab_view = GetTabViewAt(1);
  ASSERT_TRUE(active_tab_view);
  EXPECT_EQ(active_tab_view, region_view()->GetDefaultFocusableChild());

  region_view()->RequestFocus();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return active_tab_view->HasFocus(); }));
  EXPECT_TRUE(region_view()->pane_has_focus());

  active_tab_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  EXPECT_EQ(nullptr, region_view()->GetDefaultFocusableChild());

  active_tab_view->SetFocusBehavior(
      views::View::FocusBehavior::ACCESSIBLE_ONLY);
  EXPECT_EQ(active_tab_view, region_view()->GetDefaultFocusableChild());

  active_tab_view->parent()->SetVisible(false);
  EXPECT_EQ(nullptr, region_view()->GetDefaultFocusableChild());
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest, SplitTabsShareSpace) {
  TabStripModel* model = browser()->GetTabStripModel();
  AppendTab();
  const int index1 = model->active_index();
  AppendTab();
  const int index2 = model->active_index();

  model->ActivateTabAt(index1,
                       TabStripUserGestureDetails(
                           TabStripUserGestureDetails::GestureType::kOther));

  model->AddToNewSplit({index2}, {},
                       split_tabs::SplitTabCreatedSource::kTabContextMenu);

  AppendPinnedTab();
  const int index3 = model->active_index();
  AppendPinnedTab();
  const int index4 = model->active_index();

  model->ActivateTabAt(index3,
                       TabStripUserGestureDetails(
                           TabStripUserGestureDetails::GestureType::kOther));

  model->AddToNewSplit({index4}, {},
                       split_tabs::SplitTabCreatedSource::kTabContextMenu);

  auto* pinned_tabs = root_node()->children()[0]->view();
  EXPECT_TRUE(views::IsViewClass<PinnedTabContainerView>(pinned_tabs));
  EXPECT_EQ(pinned_tabs->children().size(), 1u);
  auto* unpinned_tabs = root_node()->children()[1]->view();
  EXPECT_TRUE(views::IsViewClass<UnpinnedTabContainerView>(unpinned_tabs));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return unpinned_tabs->children().size() == 2; }));

  // Split tabs share available width, allowing 1px rounding on odd widths.
  auto pinned_split_tab = pinned_tabs->children()[0];
  EXPECT_TRUE(views::IsViewClass<SplitTabView>(pinned_split_tab));
  EXPECT_EQ(pinned_split_tab->children().size(), 2u);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return std::abs(pinned_split_tab->children()[0]->size().width() -
                    pinned_split_tab->children()[1]->size().width()) <= 1 &&
           pinned_split_tab->children()[0]->size().width() > 0;
  }));

  auto unpinned_split_tab = unpinned_tabs->children()[1];
  EXPECT_TRUE(views::IsViewClass<SplitTabView>(unpinned_split_tab));
  EXPECT_EQ(unpinned_split_tab->children().size(), 2u);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return std::abs(unpinned_split_tab->children()[0]->size().width() -
                    unpinned_split_tab->children()[1]->size().width()) <= 1 &&
           unpinned_split_tab->children()[0]->size().width() > 0;
  }));
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest,
                       TabStripEditableState) {
  EXPECT_TRUE(region_view()->IsTabStripEditable());

  region_view()->DisableTabStripEditingForTesting();
  EXPECT_FALSE(region_view()->IsTabStripEditable());
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest,
                       TabStripCloseableState) {
  EXPECT_TRUE(region_view()->IsTabStripCloseable());
}

IN_PROC_BROWSER_TEST_P(BaseTabStripRegionViewBrowserTest, SwitchModes) {
  BaseTabStripRegionView* initial_region = region_view();
  BaseTabStripRegionView* target_region =
      is_horizontal() ? vertical_region_view() : horizontal_region_view();
  EXPECT_TRUE(initial_region->root_node_for_testing());

  TabStripModel* model = browser()->GetTabStripModel();

  AppendPinnedTab();

  {
    AppendTab();
    const int i1 = model->active_index();
    AppendTab();
    const int i2 = model->active_index();
    model->ActivateTabAt(i1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
    model->AddToNewSplit({i2}, {},
                         split_tabs::SplitTabCreatedSource::kTabContextMenu);
  }

  {
    AppendPinnedTab();
    const int i1 = model->active_index();
    AppendPinnedTab();
    const int i2 = model->active_index();
    model->ActivateTabAt(i1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
    model->AddToNewSplit({i2}, {},
                         split_tabs::SplitTabCreatedSource::kTabContextMenu);
  }

  {
    AppendTab();
    model->AddToNewGroup({model->active_index()});
  }

  {
    AppendTab();
    const int i1 = model->active_index();
    AppendTab();
    const int i2 = model->active_index();
    model->AddToNewGroup({i1, i2});
    model->ActivateTabAt(i1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
    model->AddToNewSplit({i2}, {},
                         split_tabs::SplitTabCreatedSource::kTabContextMenu);
  }

  // Inactive region should not own any tab nodes.
  EXPECT_FALSE(target_region->root_node_for_testing());

  // Pinned: 1 single + 1 split = 2 children.
  // Unpinned: 1 single + 1 split + 2 groups = 4 children.
  auto* initial_pinned =
      initial_region->root_node_for_testing()->children()[0]->view();
  auto* initial_unpinned =
      initial_region->root_node_for_testing()->children()[1]->view();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return initial_pinned->children().size() == 2 &&
           initial_unpinned->children().size() == 4;
  }));

  if (is_horizontal()) {
    EnterVerticalTabsMode();
  } else {
    ExitVerticalTabsMode();
  }

  EXPECT_FALSE(initial_region->root_node_for_testing());

  RootTabCollectionNode* target_root = target_region->root_node_for_testing();
  ASSERT_TRUE(target_root);
  auto* target_pinned = target_root->children()[0]->view();
  auto* target_unpinned = target_root->children()[1]->view();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return target_pinned->children().size() == 2 &&
           target_unpinned->children().size() == 4;
  }));
}

INSTANTIATE_TEST_SUITE_P(All,
                         BaseTabStripRegionViewBrowserTest,
                         testing::Values(TabStripOrientation::kVertical,
                                         TabStripOrientation::kHorizontal),
                         &OrientationTestName);

class BaseTabStripFocusModeTest : public BaseTabStripRegionViewBrowserTest {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = BaseTabStripRegionViewBrowserTest::GetEnabledFeatures();
    enabled.emplace_back(features::kTabGroupsFocusing,
                         base::FieldTrialParams());
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_P(BaseTabStripFocusModeTest,
                       MoveFocusedGroupToNewWindowPreservesFocus) {
  AppendTab();
  AppendTab();
  ASSERT_EQ(3, tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      tab_strip_model()->AddToNewGroup({1, 2});

  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model()->EnterFocusMode(group_id);
  ASSERT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);

  ui_test_utils::BrowserCreatedObserver observer;
  chrome::MoveGroupToNewWindow(browser(), group_id);
  BrowserWindowInterface* new_browser = observer.Wait();
  ASSERT_TRUE(new_browser);

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(tab_strip_model()->GetFocusedGroup().has_value());

  EXPECT_EQ(2, new_browser->GetTabStripModel()->count());
  EXPECT_EQ(new_browser->GetTabStripModel()->GetFocusedGroup(), group_id);
}

IN_PROC_BROWSER_TEST_P(BaseTabStripFocusModeTest,
                       MoveAllTabsIngroupToNewWindowPreservesFocus) {
  AppendTab();
  AppendTab();
  ASSERT_EQ(3, tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      tab_strip_model()->AddToNewGroup({1, 2});

  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model()->EnterFocusMode(group_id);
  ASSERT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);

  ui_test_utils::BrowserCreatedObserver observer;
  chrome::MoveTabsToNewWindow(browser(), {1, 2});
  BrowserWindowInterface* new_browser = observer.Wait();
  ASSERT_TRUE(new_browser);

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(tab_strip_model()->GetFocusedGroup().has_value());

  EXPECT_EQ(2, new_browser->GetTabStripModel()->count());
  EXPECT_EQ(new_browser->GetTabStripModel()->GetFocusedGroup(), group_id);
}

IN_PROC_BROWSER_TEST_P(BaseTabStripFocusModeTest,
                       MoveGroupAndPinnedTabsToNewWindowPreservesFocus) {
  AppendTab();
  AppendTab();
  AppendTab();
  ASSERT_EQ(4, tab_strip_model()->count());

  tab_strip_model()->SetTabPinned(0, true);
  ASSERT_TRUE(tab_strip_model()->IsTabPinned(0));

  const tab_groups::TabGroupId group_id =
      tab_strip_model()->AddToNewGroup({1, 2});

  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model()->EnterFocusMode(group_id);
  ASSERT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);

  ui_test_utils::BrowserCreatedObserver observer;
  chrome::MoveTabsToNewWindow(browser(), {0, 1, 2});
  BrowserWindowInterface* new_browser = observer.Wait();
  ASSERT_TRUE(new_browser);

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(tab_strip_model()->GetFocusedGroup().has_value());

  EXPECT_EQ(3, new_browser->GetTabStripModel()->count());
  EXPECT_TRUE(new_browser->GetTabStripModel()->IsTabPinned(0));
  EXPECT_EQ(new_browser->GetTabStripModel()->GetFocusedGroup(), group_id);
}

IN_PROC_BROWSER_TEST_P(BaseTabStripFocusModeTest,
                       MoveFocusedGroupToExistingWindowDoesNotFocus) {
  AppendTab();
  AppendTab();
  ASSERT_EQ(3, tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      tab_strip_model()->AddToNewGroup({1, 2});

  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model()->EnterFocusMode(group_id);
  ASSERT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);

  BrowserWindowInterface* target_browser =
      CreateBrowser(browser()->GetProfile());
  ASSERT_TRUE(target_browser);
  ASSERT_EQ(1, target_browser->GetTabStripModel()->count());

  chrome::MoveGroupToExistingWindow(browser(), target_browser, group_id);

  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_FALSE(tab_strip_model()->GetFocusedGroup().has_value());

  // Target browser contains the moved group along with its original tab, so the
  // group should not be focused in the target window.
  EXPECT_EQ(3, target_browser->GetTabStripModel()->count());
  EXPECT_TRUE(
      target_browser->GetTabStripModel()->group_model()->ContainsTabGroup(
          group_id));
  EXPECT_FALSE(
      target_browser->GetTabStripModel()->GetFocusedGroup().has_value());
}

IN_PROC_BROWSER_TEST_P(BaseTabStripFocusModeTest,
                       ToggleOrientationPreservesFocusMode) {
  AppendTab();
  AppendTab();
  ASSERT_EQ(3, tab_strip_model()->count());

  const tab_groups::TabGroupId group_id =
      tab_strip_model()->AddToNewGroup({1, 2});

  tab_strip_model()->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model()->EnterFocusMode(group_id);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);

  if (is_horizontal()) {
    EnterVerticalTabsMode();
  } else {
    ExitVerticalTabsMode();
  }
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);

  if (is_horizontal()) {
    ExitVerticalTabsMode();
  } else {
    EnterVerticalTabsMode();
  }
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), group_id);
}

INSTANTIATE_TEST_SUITE_P(All,
                         BaseTabStripFocusModeTest,
                         testing::Values(TabStripOrientation::kVertical,
                                         TabStripOrientation::kHorizontal),
                         &OrientationTestName);
