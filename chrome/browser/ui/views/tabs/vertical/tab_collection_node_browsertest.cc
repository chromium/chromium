// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

class TabCollectionNodeBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  TabCollectionNodeBrowserTest() = default;
  ~TabCollectionNodeBrowserTest() override = default;

 protected:
  RootTabCollectionNode* root_node() {
    return browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view_for_testing()
        ->root_node_for_testing();
  }

  views::View* root_node_view() { return root_node()->view(); }

  views::View* parent_view() { return root_node()->view()->parent(); }

  TabStripModel* GetTabStripModel() { return browser()->tab_strip_model(); }

  // Appends a new unpinned tab to the end of the tab strip.
  content::WebContents* AppendTab() {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/-1,
                     /*foreground=*/true);
    return GetTabStripModel()->GetWebContentsAt(GetTabStripModel()->count() -
                                                1);
  }

  // Inserts a new unpinned tab to the specified index in the tab strip.
  content::WebContents* InsertTab(int index) {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/index,
                     /*foreground=*/true);
    return GetTabStripModel()->GetWebContentsAt(index);
  }

  // Appends a new pinned tab to the end of the pinned tabs.
  content::WebContents* AppendPinnedTab() {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL),
                     /*index=*/-1,
                     /*foreground=*/true,
                     /*group=*/std::nullopt,
                     /*pinned=*/true);
    return GetTabStripModel()->GetWebContentsAt(
        GetTabStripModel()->IndexOfFirstNonPinnedTab() - 1);
  }

  // Appends a new tab and adds it to a new group.
  std::pair<content::WebContents*, tab_groups::TabGroupId>
  AppendTabToNewGroup() {
    content::WebContents* contents = AppendTab();
    const int index = GetTabStripModel()->GetIndexOfWebContents(contents);
    const tab_groups::TabGroupId group_id =
        GetTabStripModel()->AddToNewGroup({index});
    return {contents, group_id};
  }

  // Appends `num_tabs` new tabs and adds them to a new group.
  std::pair<std::vector<content::WebContents*>, tab_groups::TabGroupId>
  AppendTabsToNewGroup(int num_tabs) {
    std::vector<content::WebContents*> contents;
    std::vector<int> indices;
    for (int i = 0; i < num_tabs; ++i) {
      content::WebContents* wc = AppendTab();
      contents.push_back(wc);
      indices.push_back(GetTabStripModel()->GetIndexOfWebContents(wc));
    }

    const tab_groups::TabGroupId group_id =
        GetTabStripModel()->AddToNewGroup(indices);
    return {contents, group_id};
  }

  // Appends two new tabs and adds them to a new split group.
  std::pair<content::WebContents*, content::WebContents*> AppendSplitTab() {
    content::WebContents* contents1 = AppendTab();
    content::WebContents* contents2 = AppendTab();

    TabStripModel* tab_strip_model = GetTabStripModel();
    const int index1 = tab_strip_model->GetIndexOfWebContents(contents1);
    const int index2 = tab_strip_model->GetIndexOfWebContents(contents2);

    tab_strip_model->ActivateTabAt(
        index1, TabStripUserGestureDetails(
                    TabStripUserGestureDetails::GestureType::kOther));

    tab_strip_model->AddToNewSplit(
        {index2}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);

    return {contents1, contents2};
  }

  // Appends two new pinned tabs and adds them to a new split group.
  std::pair<content::WebContents*, content::WebContents*>
  AppendPinnedSplitTab() {
    content::WebContents* contents1 = AppendPinnedTab();
    content::WebContents* contents2 = AppendPinnedTab();

    TabStripModel* tab_strip_model = GetTabStripModel();
    const int index1 = tab_strip_model->GetIndexOfWebContents(contents1);
    const int index2 = tab_strip_model->GetIndexOfWebContents(contents2);

    tab_strip_model->ActivateTabAt(
        index1, TabStripUserGestureDetails(
                    TabStripUserGestureDetails::GestureType::kOther));

    tab_strip_model->AddToNewSplit(
        {index2}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);

    return {contents1, contents2};
  }
};

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_UnpinnedTab) {
  AppendTab();

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node()->children().size(), 2u);

  const auto* pinned_node = pinned_collection_node();
  ASSERT_NE(pinned_node, nullptr);

  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_NE(unpinned_node, nullptr);

  // The pinned Node should be empty.
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // The unpinned Node should contain two tabs (the initial one and the new
  // one).
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[1]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_PinnedTab) {
  AppendPinnedTab();

  // The pinned Node should have one tab.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 1u);
  EXPECT_EQ(pinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);

  // The unpinned Node should have one tab (the initial one).
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_TabGroup) {
  AppendTabToNewGroup();

  // The pinned Node should be empty.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // Unpinned Node -> Tab, Group
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  const auto& group_node = unpinned_node->children()[1];
  EXPECT_EQ(group_node->type(), TabCollectionNode::Type::GROUP);

  // Group -> Tab
  ASSERT_EQ(group_node->children().size(), 1u);
  EXPECT_EQ(group_node->children()[0]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_MultiTabGroup) {
  AppendTabsToNewGroup(2);

  // The pinned Node should be empty.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // Unpinned Node -> Tab, Group
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  const auto& group_node = unpinned_node->children()[1];
  EXPECT_EQ(group_node->type(), TabCollectionNode::Type::GROUP);

  // Group -> Tab, Tab
  ASSERT_EQ(group_node->children().size(), 2u);
  EXPECT_EQ(group_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(group_node->children()[1]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_SplitTab) {
  AppendSplitTab();

  // The pinned Node should be empty.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // Unpinned Node -> Tab, Split
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  const auto& split_node = unpinned_node->children()[1];
  EXPECT_EQ(split_node->type(), TabCollectionNode::Type::SPLIT);

  // Split -> Tab, Tab
  ASSERT_EQ(split_node->children().size(), 2u);
  EXPECT_EQ(split_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(split_node->children()[1]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_PinnedSplitTab) {
  AppendPinnedSplitTab();

  // Pinned Node -> Split
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 1u);
  const auto& split_node = pinned_node->children()[0];
  EXPECT_EQ(split_node->type(), TabCollectionNode::Type::SPLIT);

  // Split -> Tab, Tab
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(split_node->children().size(), 2u);
  EXPECT_EQ(split_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(split_node->children()[1]->type(), TabCollectionNode::Type::TAB);

  // Unpinned Node -> Tab
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_ViewClasses) {
  AppendPinnedTab();
  AppendTabToNewGroup();
  AppendSplitTab();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // The root node should contain two nodes: one for pinned
  // (VerticalPinnedTabContainerView), one for unpinned
  // (VerticalUnpinnedTabContainerView).
  EXPECT_TRUE(views::IsViewClass<VerticalTabStripView>(root_node_view()));
  const auto* pinned_node = pinned_collection_node();
  EXPECT_TRUE(
      views::IsViewClass<VerticalPinnedTabContainerView>(pinned_node->view()));
  const auto* unpinned_node = unpinned_collection_node();
  EXPECT_TRUE(views::IsViewClass<VerticalUnpinnedTabContainerView>(
      unpinned_node->view()));

  // The pinned Node should be have one tab.
  ASSERT_EQ(pinned_node->children().size(), 1u);
  EXPECT_EQ(pinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_TRUE(
      views::IsViewClass<VerticalTabView>(pinned_node->children()[0]->view()));

  // The unpinned Node should contain a tab, a tab group, and a split tab.
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      unpinned_node->children()[0]->view()));

  const auto& group_node = unpinned_node->children()[1];
  EXPECT_EQ(group_node->type(), TabCollectionNode::Type::GROUP);
  // TODO(crbug.com/442567916): Verify tab group view is created.
  ASSERT_EQ(group_node->children().size(), 1u);
  EXPECT_EQ(group_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_TRUE(
      views::IsViewClass<VerticalTabView>(group_node->children()[0]->view()));

  const auto& split_node = unpinned_node->children()[2];
  EXPECT_EQ(split_node->type(), TabCollectionNode::Type::SPLIT);
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(split_node->view()));
  ASSERT_EQ(split_node->children().size(), 2u);
  EXPECT_EQ(split_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_TRUE(
      views::IsViewClass<VerticalTabView>(split_node->children()[0]->view()));
  EXPECT_EQ(split_node->children()[1]->type(), TabCollectionNode::Type::TAB);
  EXPECT_TRUE(
      views::IsViewClass<VerticalTabView>(split_node->children()[1]->view()));
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_ViewHierarchy) {
  AppendTab();

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node()->children().size(), 2u);

  // The root_node_view should have three children, the pinned and unpinned
  // views and a separator.
  ASSERT_EQ(root_node_view()->children().size(), 3u);
  const auto pinned_node_scroll_view = root_node_view()->children()[0];
  ASSERT_TRUE(
      views::IsViewClass<views::Separator>(root_node_view()->children()[1]));
  const auto unpinned_node_scroll_view = root_node_view()->children()[2];

  // The pinned_node_scroll_view's contents should have no children.
  ASSERT_TRUE(views::IsViewClass<views::ScrollView>(pinned_node_scroll_view));
  ASSERT_EQ(static_cast<views::ScrollView*>(pinned_node_scroll_view)
                ->contents()
                ->children()
                .size(),
            0u);

  // The unpinned_node_scroll_view's contents should have two children, the two
  // tab views.
  ASSERT_TRUE(views::IsViewClass<views::ScrollView>(unpinned_node_scroll_view));
  ASSERT_EQ(static_cast<views::ScrollView*>(unpinned_node_scroll_view)
                ->contents()
                ->children()
                .size(),
            2u);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, GetDirectChildren) {
  AppendTab();

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node()->children().size(), 2u);

  // The root_node_view should have three children, the pinned and unpinned
  // views and a separator.
  ASSERT_EQ(root_node_view()->children().size(), 3u);
  const auto pinned_node_scroll_view = root_node_view()->children()[0];
  ASSERT_TRUE(
      views::IsViewClass<views::Separator>(root_node_view()->children()[1]));
  const auto unpinned_node_scroll_view = root_node_view()->children()[2];

  const auto& child_views = root_node()->GetDirectChildren();
  ASSERT_EQ(child_views.size(), 2u);
  EXPECT_EQ(child_views[0],
            views::AsViewClass<views::ScrollView>(pinned_node_scroll_view)
                ->contents());
  EXPECT_EQ(child_views[1],
            views::AsViewClass<views::ScrollView>(unpinned_node_scroll_view)
                ->contents());
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       CollectionReturnsOnlyCollectionItems) {
  AppendTab();

  views::View* non_collection_view =
      parent_view()->AddChildView(std::make_unique<views::View>());

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node()->children().size(), 2u);

  // The parent_view should have multiple children in addition to the root view.
  ASSERT_GE(parent_view()->children().size(), 2u);

  views::View* non_collection_view_2 =
      root_node_view()->AddChildView(std::make_unique<views::View>());

  // The root_node_view should have four children, the pinned and unpinned
  // views, a separator and the non-collection view.
  ASSERT_EQ(root_node_view()->children().size(), 4u);

  const auto& child_views = root_node()->GetDirectChildren();
  ASSERT_GE(child_views.size(), 2u);
  EXPECT_FALSE(std::ranges::contains(child_views, non_collection_view));
  EXPECT_FALSE(std::ranges::contains(child_views, non_collection_view_2));
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       VerticalTabViewIsCreatedForTabs) {
  // Add an unpinned tab.
  AppendTab();
  // Add a pinned tab.
  AppendPinnedTab();

  // Wait for the root node to populate its children.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // Verify the pinned node contains a single child.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 1u);
  // Verify that child is a VerticalTabView.
  EXPECT_TRUE(
      views::IsViewClass<VerticalTabView>(pinned_node->children()[0]->view()));

  // Verify the unpinned node contains two children: the initial empty tab and
  // the newly appended tab.
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  // Verify both children are VerticalTabView instances.
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      unpinned_node->children()[0]->view()));
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      unpinned_node->children()[1]->view()));
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, TabsCreatedEvent) {
  // The pinned Node should be empty.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // The unpinned Node should have one tab (the initial one).
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);

  AppendPinnedTab();

  // The pinned Node should have one tab.
  ASSERT_EQ(pinned_node->children().size(), 1u);
  EXPECT_EQ(pinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);

  AppendTab();

  // The unpinned Node should contain two tabs (the initial one and the new
  // one).
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[1]->type(), TabCollectionNode::Type::TAB);

  TabCollectionNode* initial_unpinned_tab_node =
      unpinned_node->children()[0].get();
  TabCollectionNode* appended_unpinned_tab_node =
      unpinned_node->children()[1].get();

  // Insert a tab between the two unpinned tabs.
  InsertTab(2);

  // The unpinned Node should contain three tabs (the initial one, then the new
  // one added by InsertTab, then the previous one that was added by AppendTab).
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[0].get(), initial_unpinned_tab_node);
  EXPECT_EQ(unpinned_node->children()[1]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[2]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[2].get(), appended_unpinned_tab_node);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, CloseTabInteraction) {
  // 1. Setup: Have three tabs unpinned.
  AppendTab();
  AppendTab();

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // Initial structure: three tabs
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);

  // Close a tab.
  GetTabStripModel()->DetachAndDeleteWebContentsAt(1);

  // After detaching, the unpinned node should only have 2 tabs.
  ASSERT_EQ(unpinned_node->children().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, DetachAndReattachGroup) {
  // 1. Setup: Create an initial tab and a tab group to be detached.
  auto [contents_vector, group_id] = AppendTabsToNewGroup(2);

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // Initial structure: [Tab, GroupA] -> children size 2
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[1]->type(),
            TabCollectionNode::Type::GROUP);

  // Detached GroupA node to simulate moving to another window/position.
  std::unique_ptr<DetachedTabCollection> detached_group =
      GetTabStripModel()->DetachTabGroupForInsertion(group_id);

  // After detaching, the unpinned node should only have 1 child (the initial
  // tab).
  ASSERT_EQ(unpinned_node->children().size(), 1u);

  // 3. Re-insert the detached group at index 0.
  GetTabStripModel()->InsertDetachedTabGroupAt(std::move(detached_group), 0);

  // 4. Verification: The hierarchy should now be updated: [GroupA, Tab].
  // Since the detached group was inserted at index 0, it should be the first
  // child.
  ASSERT_EQ(unpinned_node->children().size(), 2u);

  // The first child should now be the Tab Group.
  const auto& reinserted_group_node = unpinned_node->children()[0];
  EXPECT_EQ(reinserted_group_node->type(), TabCollectionNode::Type::GROUP);

  // The second child should be the original Tab.
  EXPECT_EQ(unpinned_node->children()[1]->type(), TabCollectionNode::Type::TAB);

  // Verify the group itself contains the correct number of children (2 tabs).
  ASSERT_EQ(reinserted_group_node->children().size(), 2u);
  EXPECT_EQ(reinserted_group_node->children()[0]->type(),
            TabCollectionNode::Type::TAB);
  EXPECT_EQ(reinserted_group_node->children()[1]->type(),
            TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, GroupContiguousTabs) {
  // 1. Setup: Start with three unpinned tabs.
  AppendTab();  // Tab 1 (index 1)
  AppendTab();  // Tab 2 (index 2)

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // Initial structure verification: All 3 tabs are direct children of the
  // Unpinned Container. [Tab, Tab, Tab] -> children size 3
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[1]->type(), TabCollectionNode::Type::TAB);
  EXPECT_EQ(unpinned_node->children()[2]->type(), TabCollectionNode::Type::TAB);

  GetTabStripModel()->AddToNewGroup({0, 1});

  // 2. Verification: The hierarchy should now be updated to [Tab, Group].
  // The two tabs are replaced by a single group node.
  ASSERT_EQ(unpinned_node->children().size(), 2u);

  // The first child is the group.
  EXPECT_EQ(unpinned_node->children()[0]->type(),
            TabCollectionNode::Type::GROUP);
  const auto& new_group_node = unpinned_node->children()[0];

  // Verify the group itself contains the correct number of children (2 tabs).
  ASSERT_EQ(new_group_node->children().size(), 2u);
  EXPECT_EQ(new_group_node->children()[0]->type(),
            TabCollectionNode::Type::TAB);
  EXPECT_EQ(new_group_node->children()[1]->type(),
            TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       SingleMoveWithinCollection) {
  // 1. Setup: Start with three unpinned tabs.
  AppendTab();  // Tab 1 (index 1)
  AppendTab();  // Tab 2 (index 2)

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // Initial structure verification and saving pointers to nodes.
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 3u);

  // A = Initial Tab (index 0)
  TabCollectionNode* tab_a_node = unpinned_node->children()[0].get();
  // B = Tab 1 (index 1)
  TabCollectionNode* tab_b_node = unpinned_node->children()[1].get();
  // C = Tab 2 (index 2)
  TabCollectionNode* tab_c_node = unpinned_node->children()[2].get();

  // Initial Order: [A, B, C]
  EXPECT_EQ(unpinned_node->children()[0].get(), tab_a_node);
  EXPECT_EQ(unpinned_node->children()[1].get(), tab_b_node);
  EXPECT_EQ(unpinned_node->children()[2].get(), tab_c_node);

  ui::ListSelectionModel selection_model;
  selection_model.AddIndexToSelection(0);
  selection_model.set_anchor(std::nullopt);
  selection_model.set_active(0);
  GetTabStripModel()->SetSelectionFromModel(selection_model);
  GetTabStripModel()->MoveSelectedTabsTo(2, std::nullopt);

  // 2. Validation: Check the final node order.
  ASSERT_EQ(unpinned_node->children().size(), 3u);

  // Expected Final Order: [B, C, A]
  // Node at index 0 is Tab B (originally index 1).
  EXPECT_EQ(unpinned_node->children()[0].get(), tab_b_node);
  EXPECT_EQ(unpinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);

  // Node at index 1 is Tab C (originally index 2).
  EXPECT_EQ(unpinned_node->children()[1].get(), tab_c_node);
  EXPECT_EQ(unpinned_node->children()[1]->type(), TabCollectionNode::Type::TAB);

  // Node at index 2 is Tab A (originally index 0).
  EXPECT_EQ(unpinned_node->children()[2].get(), tab_a_node);
  EXPECT_EQ(unpinned_node->children()[2]->type(), TabCollectionNode::Type::TAB);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       SingleMoveAcrossCollection) {
  // 1. Setup: Start with three unpinned tabs.
  // Tab 0: Initial tab
  // Tab 1: AppendTab() -> This is the tab we will pin.
  // Tab 2: AppendTab()
  AppendTab();
  AppendTab();

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node()->children().empty(); }));

  // Initial State Validation:
  // Pinned: 0 children.
  const auto* pinned_node = pinned_collection_node();
  ASSERT_EQ(pinned_node->children().size(), 0u);
  // Unpinned: 3 children.
  const auto* unpinned_node = unpinned_collection_node();
  ASSERT_EQ(unpinned_node->children().size(), 3u);

  // Save a pointer to the tab we intend to pin (the second one, at index 1).
  TabCollectionNode* tab_to_pin_node = unpinned_node->children()[1].get();

  // 3. Perform the Move (Pin the tab).
  // Pinning the tab at index 1 moves it from the Unpinned collection
  // (where it was at index 1) to the Pinned collection (at index 0).
  GetTabStripModel()->SetTabPinned(1, true);

  // 4. Verification: The RootTabCollectionNode should process the OnNodeMoved
  // event. We check that the Pinned container now has the expected child.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return pinned_node->children().size() == 1u; }));

  // Final State Validation:

  // Pinned Node verification:
  // Pinned: 1 child (the moved tab).
  ASSERT_EQ(pinned_node->children().size(), 1u);
  // The first child of the Pinned node must be the tab we moved.
  EXPECT_EQ(pinned_node->children()[0]->type(), TabCollectionNode::Type::TAB);

  // Unpinned Node verification:
  // Unpinned: 2 children remaining.
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  // Ensure the tab is no longer a child of the Unpinned node.
  EXPECT_NE(unpinned_node->children()[0].get(), tab_to_pin_node);
  EXPECT_NE(unpinned_node->children()[1].get(), tab_to_pin_node);
}
