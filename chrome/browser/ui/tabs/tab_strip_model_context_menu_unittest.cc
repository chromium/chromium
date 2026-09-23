// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  MOCK_METHOD(void,
              WillCloseSplit,
              (const split_tabs::SplitTabId& split_id),
              (override));
  MOCK_METHOD(void,
              CreateHistoricalSplit,
              (const split_tabs::SplitTabId& split_id),
              (override));
  MOCK_METHOD(void,
              OnGroupsDestruction,
              (const std::vector<tab_groups::TabGroupId>& group_ids,
               base::OnceCallback<void()> callback,
               bool delete_groups),
              (override));
};

class TabStripModelContextMenuTest : public testing::Test {
 public:
  TabStripModelContextMenuTest() : profile_(new TestingProfile()) {}
  TabStripModelContextMenuTest(const TabStripModelContextMenuTest&) = delete;
  TabStripModelContextMenuTest& operator=(const TabStripModelContextMenuTest&) =
      delete;

  void SetUp() override {
    testing::Test::SetUp();
    tab_strip_model_ =
        std::make_unique<TabStripModel>(&delegate_, profile_.get());
  }

  TabStripModel* tab_strip_model() { return tab_strip_model_.get(); }
  MockTabStripModelDelegate& delegate() { return delegate_; }

  TestingProfile* profile() { return profile_.get(); }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    return content::WebContentsTester::CreateTestWebContents(profile(),
                                                             nullptr);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
  const tabs::TabModel::PreventFeatureInitializationForTesting
      prevent_feature_init_for_testing_;
  MockTabStripModelDelegate delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
};

TEST_F(TabStripModelContextMenuTest, CommandCloseTabLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and close it.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.CloseTab.SelectedTabsCount", 1, 1);

  // Select two tabs and close them.
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->SelectTabAt(1);
  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);
  histogram_tester.ExpectBucketCount(
      "Tab.ContextMenu.CloseTab.SelectedTabsCount", 2, 1);
}

TEST_F(TabStripModelContextMenuTest, CommandCloseOtherTabsLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and close others.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandCloseOtherTabs);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.CloseOtherTabs.SelectedTabsCount", 1, 1);
}

TEST_F(TabStripModelContextMenuTest, CommandCloseTabsToRightLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and close tabs to the right.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandCloseTabsToRight);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.CloseTabsToRight.SelectedTabsCount", 1, 1);
}

TEST_F(TabStripModelContextMenuTest, CommandTogglePinnedLogsHistograms) {
  base::HistogramTester histogram_tester;
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Select a single tab and pin it.
  tab_strip_model()->SelectTabAt(0);
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandTogglePinned);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.TogglePinned.SelectedTabsCount", 1, 1);
}

TEST_F(TabStripModelContextMenuTest,
       CommandCloseTabCallsCreateHistoricalSplitOnceForSplitTab) {
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Make tab 0 active.
  tab_strip_model()->SelectTabAt(0);

  // Create a split with tab 1 (which will split with the active tab 0).
  std::vector<int> indices = {1};
  tab_strip_model()->AddToNewSplit(
      indices, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);

  // Expect CreateHistoricalSplit exactly once (via
  // CreateHistoricalSplitIfClosing) and no eager WillCloseSplit call (which
  // would record a duplicate entry).
  EXPECT_CALL(delegate(), CreateHistoricalSplit(testing::_)).Times(1);
  EXPECT_CALL(delegate(), WillCloseSplit(testing::_)).Times(0);

  // Execute close command on the first tab.
  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);
}

TEST_F(TabStripModelContextMenuTest,
       CommandCloseTabDoesNotCallCreateHistoricalSplitForRegularTab) {
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Expect no calls to CreateHistoricalSplit or WillCloseSplit.
  EXPECT_CALL(delegate(), CreateHistoricalSplit(testing::_)).Times(0);
  EXPECT_CALL(delegate(), WillCloseSplit(testing::_)).Times(0);

  // Execute close command on the tab.
  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);
}

TEST_F(TabStripModelContextMenuTest, CommandCloseTabHandlesGroupDestruction) {
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);
  tab_groups::TabGroupId group_id = tab_strip_model()->AddToNewGroup({0});
  TabGroup* tab_group = tab_strip_model()->group_model()->GetTabGroup(group_id);
  ASSERT_TRUE(tab_group);
  EXPECT_FALSE(tab_group->IsGroupClosing());

  base::OnceCallback<void()> close_callback;
  EXPECT_CALL(delegate(),
              OnGroupsDestruction(testing::ElementsAre(group_id), testing::_,
                                  /*delete_groups=*/true))
      .WillOnce([&](const std::vector<tab_groups::TabGroupId>&,
                    base::OnceCallback<void()> callback, bool) {
        EXPECT_TRUE(tab_group->IsGroupClosing());
        close_callback = std::move(callback);
      });

  tab_strip_model()->ExecuteContextMenuCommand(0,
                                               TabStripModel::CommandCloseTab);

  // Until the group destruction callback runs, the tab and group remain open.
  ASSERT_TRUE(close_callback);
  EXPECT_EQ(1, tab_strip_model()->count());
  EXPECT_TRUE(tab_strip_model()->group_model()->ContainsTabGroup(group_id));

  std::move(close_callback).Run();
  EXPECT_EQ(0, tab_strip_model()->count());
  EXPECT_FALSE(tab_strip_model()->group_model()->ContainsTabGroup(group_id));
}

TEST_F(TabStripModelContextMenuTest, CommandToggleFocusGroupEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kTabGroupsFocusing);

  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Tab 0 is not in a group -> command disabled.
  EXPECT_FALSE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Add tab 0 to a group -> command enabled.
  tab_strip_model()->AddToNewGroup({0});
  EXPECT_TRUE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Tab 1 is not in a group -> command disabled.
  EXPECT_FALSE(tab_strip_model()->IsContextMenuCommandEnabled(
      1, TabStripModel::CommandToggleFocusGroup));
}

TEST_F(TabStripModelContextMenuTest, CommandToggleFocusGroupLogsMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kTabGroupsFocusing);
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Tab 0 is not in a group. Executing command should not focus or log
  // entry/exit metrics.
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  histogram_tester.ExpectUniqueSample(
      "Tab.ContextMenu.ToggleFocusGroup.SelectedTabsCount", 1, 1);
  histogram_tester.ExpectTotalCount("TabGroups.Focus.EntryPoint", 0);
  histogram_tester.ExpectTotalCount("TabGroups.Focus.ExitReason", 0);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_FocusTabGroup"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_UnfocusTabGroup"),
            0);

  tab_groups::TabGroupId group = tab_strip_model()->AddToNewGroup({0, 1});

  // Focus the group from tab 0.
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), group);
  histogram_tester.ExpectBucketCount(
      "Tab.ContextMenu.ToggleFocusGroup.SelectedTabsCount", 1, 2);
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.EntryPoint",
                                      TabGroupFocusEntryPoint::kTabContextMenu,
                                      1);
  histogram_tester.ExpectTotalCount("TabGroups.Focus.ExitReason", 0);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_FocusTabGroup"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_UnfocusTabGroup"),
            0);

  // Unfocus the group from tab 0.
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  histogram_tester.ExpectBucketCount(
      "Tab.ContextMenu.ToggleFocusGroup.SelectedTabsCount", 1, 3);
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.ExitReason",
                                      TabGroupFocusExitReason::kTabContextMenu,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_FocusTabGroup"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_UnfocusTabGroup"),
            1);

  // Select multiple tabs and focus the group.
  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(0);
  selection.AddIndexToSelection(1);
  tab_strip_model()->SetSelectionFromModel(selection);
  ASSERT_EQ(2u, tab_strip_model()->selection_model().size());

  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), group);
  histogram_tester.ExpectBucketCount(
      "Tab.ContextMenu.ToggleFocusGroup.SelectedTabsCount", 2, 1);
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.EntryPoint",
                                      TabGroupFocusEntryPoint::kTabContextMenu,
                                      2);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_FocusTabGroup"),
            2);
}

TEST_F(TabStripModelContextMenuTest, NonGroupFocusEnabledSingleTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});

  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);

  // Tab 0 is not in a group, but kNonGroupFocus is enabled -> command enabled.
  EXPECT_TRUE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Executing the command creates an ephemeral group and focuses it.
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  std::optional<tab_groups::TabGroupId> focused_group =
      tab_strip_model()->GetFocusedGroup();
  ASSERT_TRUE(focused_group.has_value());
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(0), focused_group);
  EXPECT_TRUE(tab_strip_model()->IsEphemeralTabGroup(focused_group.value()));

  histogram_tester.ExpectUniqueSample(
      "TabGroups.Focus.EntryPoint",
      TabGroupFocusEntryPoint::kTabContextMenuNonGroup, 1);
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.NonGroupTabsCount", 1,
                                      1);
  EXPECT_EQ(
      user_action_tester.GetActionCount("TabContextMenu_FocusNonGroupTabs"), 1);

  EXPECT_TRUE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Executing the command unfocuses the group, thus deleting it.
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(0), std::nullopt);
  EXPECT_FALSE(tab_strip_model()->group_model()->ContainsTabGroup(
      focused_group.value()));
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.ExitReason",
                                      TabGroupFocusExitReason::kTabContextMenu,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_UnfocusTabGroup"),
            1);
}

TEST_F(TabStripModelContextMenuTest, NonGroupFocusEnabledMultipleTabs) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});

  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(0);
  selection.AddIndexToSelection(1);
  tab_strip_model()->SetSelectionFromModel(selection);

  // Both tabs are non-grouped -> command enabled.
  EXPECT_TRUE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  std::optional<tab_groups::TabGroupId> focused_group =
      tab_strip_model()->GetFocusedGroup();
  ASSERT_TRUE(focused_group.has_value());
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(0), focused_group);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(1), focused_group);
  EXPECT_TRUE(tab_strip_model()->IsEphemeralTabGroup(focused_group.value()));

  histogram_tester.ExpectUniqueSample(
      "TabGroups.Focus.EntryPoint",
      TabGroupFocusEntryPoint::kTabContextMenuNonGroup, 1);
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.NonGroupTabsCount", 2,
                                      1);
  EXPECT_EQ(
      user_action_tester.GetActionCount("TabContextMenu_FocusNonGroupTabs"), 1);

  EXPECT_TRUE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Executing the command unfocuses the group, thus deleting it.
  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandToggleFocusGroup);
  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(0), std::nullopt);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(1), std::nullopt);
  EXPECT_FALSE(tab_strip_model()->group_model()->ContainsTabGroup(
      focused_group.value()));
  histogram_tester.ExpectUniqueSample("TabGroups.Focus.ExitReason",
                                      TabGroupFocusExitReason::kTabContextMenu,
                                      1);
  EXPECT_EQ(user_action_tester.GetActionCount("TabContextMenu_UnfocusTabGroup"),
            1);
}

TEST_F(TabStripModelContextMenuTest, NonGroupFocusGreyedOutWhenMixture) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});

  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);
  tab_strip_model()->AppendWebContents(CreateTestWebContents(), false);

  // Mixture of grouped and non-grouped tabs.
  tab_strip_model()->AddToNewGroup({0});
  ui::ListSelectionModel selection_grouped_and_ungrouped;
  selection_grouped_and_ungrouped.SetSelectedIndex(0);
  selection_grouped_and_ungrouped.AddIndexToSelection(1);
  tab_strip_model()->SetSelectionFromModel(selection_grouped_and_ungrouped);

  EXPECT_FALSE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Mixture of different tab groups.
  tab_strip_model()->AddToNewGroup({1});
  ui::ListSelectionModel selection_different_groups;
  selection_different_groups.SetSelectedIndex(0);
  selection_different_groups.AddIndexToSelection(1);
  tab_strip_model()->SetSelectionFromModel(selection_different_groups);

  EXPECT_FALSE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));

  // Mixture of pinned and non-pinned tabs.
  tab_strip_model()->RemoveFromGroup({0, 1});
  tab_strip_model()->SetTabPinned(0, true);
  ui::ListSelectionModel selection_pinned_and_unpinned;
  selection_pinned_and_unpinned.SetSelectedIndex(0);
  selection_pinned_and_unpinned.AddIndexToSelection(1);
  tab_strip_model()->SetSelectionFromModel(selection_pinned_and_unpinned);

  EXPECT_FALSE(tab_strip_model()->IsContextMenuCommandEnabled(
      0, TabStripModel::CommandToggleFocusGroup));
}

class NonGroupFocusDissolutionTest : public TabStripModelContextMenuTest {
 public:
  void SetUp() override {
    TabStripModelContextMenuTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});
  }

  // Appends `tab_count` tabs, then starts a non-group focus session over the
  // first `focused_tab_count` of them. Returns the ephemeral group.
  tab_groups::TabGroupId StartNonGroupFocus(int tab_count,
                                            int focused_tab_count) {
    for (int i = 0; i < tab_count; ++i) {
      tab_strip_model()->AppendWebContents(CreateTestWebContents(), i == 0);
    }

    ui::ListSelectionModel selection;
    selection.SetSelectedIndex(0);
    for (int i = 1; i < focused_tab_count; ++i) {
      selection.AddIndexToSelection(i);
    }
    tab_strip_model()->SetSelectionFromModel(selection);

    tab_strip_model()->ExecuteContextMenuCommand(
        0, TabStripModel::CommandToggleFocusGroup);

    const std::optional<tab_groups::TabGroupId> group =
        tab_strip_model()->GetFocusedGroup();
    CHECK(group.has_value());
    CHECK(tab_strip_model()->IsEphemeralTabGroup(group.value()));
    return group.value();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NonGroupFocusDissolutionTest,
       AddToNewGroupDissolvesEphemeralFocusedGroup) {
  const tab_groups::TabGroupId ephemeral_group =
      StartNonGroupFocus(/*tab_count=*/3, /*focused_tab_count=*/2);

  ui::ListSelectionModel single_selection;
  single_selection.SetSelectedIndex(0);
  tab_strip_model()->SetSelectionFromModel(single_selection);
  ASSERT_EQ(tab_strip_model()->GetFocusedGroup(), ephemeral_group);

  tab_strip_model()->ExecuteContextMenuCommand(
      0, TabStripModel::CommandAddToNewGroup);

  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  EXPECT_FALSE(
      tab_strip_model()->group_model()->ContainsTabGroup(ephemeral_group));

  // Tab 0 lands in a new permanent group.
  const std::optional<tab_groups::TabGroupId> new_group =
      tab_strip_model()->GetTabGroupForTab(0);
  ASSERT_TRUE(new_group.has_value());
  EXPECT_NE(new_group.value(), ephemeral_group);
  EXPECT_FALSE(tab_strip_model()->IsEphemeralTabGroup(new_group.value()));

  // Tab 1 was in the ephemeral group but not selected, so it is now ungrouped
  // rather than stranded in the ephemeral group.
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(1), std::nullopt);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(2), std::nullopt);
}

TEST_F(NonGroupFocusDissolutionTest,
       ActivatingTabOutsideEphemeralGroupDissolvesIt) {
  const tab_groups::TabGroupId ephemeral_group =
      StartNonGroupFocus(/*tab_count=*/2, /*focused_tab_count=*/1);

  tab_strip_model()->ActivateTabAt(1);

  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  EXPECT_FALSE(
      tab_strip_model()->group_model()->ContainsTabGroup(ephemeral_group));
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(0), std::nullopt);
}

TEST_F(NonGroupFocusDissolutionTest,
       InsertingTabOutsideEphemeralGroupDissolvesIt) {
  const tab_groups::TabGroupId ephemeral_group =
      StartNonGroupFocus(/*tab_count=*/2, /*focused_tab_count=*/2);

  tab_strip_model()->AppendWebContents(CreateTestWebContents(), true);

  EXPECT_EQ(tab_strip_model()->GetFocusedGroup(), std::nullopt);
  EXPECT_FALSE(
      tab_strip_model()->group_model()->ContainsTabGroup(ephemeral_group));
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(0), std::nullopt);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(1), std::nullopt);
  EXPECT_EQ(tab_strip_model()->GetTabGroupForTab(2), std::nullopt);
}
