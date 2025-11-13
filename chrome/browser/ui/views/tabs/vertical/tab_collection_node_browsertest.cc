// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

class TabCollectionNodeBrowserTest : public InProcessBrowserTest {
 public:
  TabCollectionNodeBrowserTest() = default;
  ~TabCollectionNodeBrowserTest() override = default;

  void TearDown() override {
    TabCollectionNode::SetViewFactoryForTesting(
        TabCollectionNode::ViewFactory());
    InProcessBrowserTest::TearDown();
  }

 protected:
  // Appends a new unpinned tab to the end of the tab strip.
  content::WebContents* AppendTab() {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    return raw_contents;
  }

  // Inserts a new unpinned tab to the specified index in the tab strip.
  content::WebContents* InsertTab(int index) {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->InsertWebContentsAt(
        index, std::move(contents), ADD_INHERIT_OPENER | ADD_ACTIVE);
    return raw_contents;
  }

  // Appends a new pinned tab to the end of the pinned tabs.
  content::WebContents* AppendPinnedTab() {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->InsertWebContentsAt(
        browser()->tab_strip_model()->count(), std::move(contents),
        ADD_INHERIT_OPENER | ADD_ACTIVE | ADD_PINNED);
    return raw_contents;
  }

  // Appends a new tab and adds it to a new group.
  std::pair<content::WebContents*, tab_groups::TabGroupId>
  AppendTabToNewGroup() {
    content::WebContents* contents = AppendTab();
    const int index =
        browser()->tab_strip_model()->GetIndexOfWebContents(contents);
    const tab_groups::TabGroupId group_id =
        browser()->tab_strip_model()->AddToNewGroup({index});
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
      indices.push_back(
          browser()->tab_strip_model()->GetIndexOfWebContents(wc));
    }

    const tab_groups::TabGroupId group_id =
        browser()->tab_strip_model()->AddToNewGroup(indices);
    return {contents, group_id};
  }

  // Appends two new tabs and adds them to a new split group.
  std::pair<content::WebContents*, content::WebContents*> AppendSplitTab() {
    content::WebContents* contents1 = AppendTab();
    content::WebContents* contents2 = AppendTab();

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
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

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
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
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The pinned Node should be empty.
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // The unpinned Node should contain two tabs (the initial one and the new
  // one).
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_PinnedTab) {
  AppendPinnedTab();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The pinned Node should have one tab.
  ASSERT_EQ(pinned_node->children().size(), 1u);
  EXPECT_EQ(pinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);

  // The unpinned Node should have one tab (the initial one).
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_TabGroup) {
  AppendTabToNewGroup();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The pinned Node should be empty.
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // Unpinned Node -> Tab, Group
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  const auto& group_node = unpinned_node->children()[1];
  EXPECT_EQ(group_node->GetType(), TabCollectionNode::Type::kTabGroup);

  // Group -> Tab
  ASSERT_EQ(group_node->children().size(), 1u);
  EXPECT_EQ(group_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_MultiTabGroup) {
  AppendTabsToNewGroup(2);
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The pinned Node should be empty.
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // Unpinned Node -> Tab, Group
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  const auto& group_node = unpinned_node->children()[1];
  EXPECT_EQ(group_node->GetType(), TabCollectionNode::Type::kTabGroup);

  // Group -> Tab, Tab
  ASSERT_EQ(group_node->children().size(), 2u);
  EXPECT_EQ(group_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(group_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
}

class TabCollectionNodeWithSplitTabBrowserTest
    : public TabCollectionNodeBrowserTest {
 public:
  TabCollectionNodeWithSplitTabBrowserTest() {
    feature_list_.InitAndEnableFeature(features::kSideBySide);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabCollectionNodeWithSplitTabBrowserTest,
                       RootNodePopulatesWithTabs_SplitTab) {
  AppendSplitTab();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The pinned Node should be empty.
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // Unpinned Node -> Tab, Split
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  const auto& split_node = unpinned_node->children()[1];
  EXPECT_EQ(split_node->GetType(), TabCollectionNode::Type::kSplitTab);

  // Split -> Tab, Tab
  ASSERT_EQ(split_node->children().size(), 2u);
  EXPECT_EQ(split_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(split_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeWithSplitTabBrowserTest,
                       RootNodePopulatesWithTabs_PinnedSplitTab) {
  AppendPinnedSplitTab();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Root -> Pinned Node, Unpinned Node
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // Pinned Node -> Split
  ASSERT_EQ(pinned_node->children().size(), 1u);
  const auto& split_node = pinned_node->children()[0];
  EXPECT_EQ(split_node->GetType(), TabCollectionNode::Type::kSplitTab);

  // Split -> Tab, Tab
  ASSERT_EQ(split_node->children().size(), 2u);
  EXPECT_EQ(split_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(split_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);

  // Unpinned Node -> Tab
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeWithSplitTabBrowserTest,
                       RootNodePopulatesWithTabs_ViewClasses) {
  AppendPinnedTab();
  AppendTabToNewGroup();
  AppendSplitTab();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // The root node should contain two nodes: one for pinned
  // (VerticalPinnedTabContainerView), one for unpinned
  // (VerticalUnpinnedTabContainerView).
  ASSERT_EQ(root_node.children().size(), 2u);
  EXPECT_TRUE(views::IsViewClass<VerticalTabStripView>(
      root_node.get_view_for_testing()));
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_TRUE(views::IsViewClass<VerticalPinnedTabContainerView>(
      pinned_node->get_view_for_testing()));
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);
  EXPECT_TRUE(views::IsViewClass<VerticalUnpinnedTabContainerView>(
      unpinned_node->get_view_for_testing()));

  // The pinned Node should be have one tab.
  ASSERT_EQ(pinned_node->children().size(), 1u);
  EXPECT_EQ(pinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      pinned_node->children()[0]->get_view_for_testing()));

  // The unpinned Node should contain a tab, a tab group, and a split tab.
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      unpinned_node->children()[0]->get_view_for_testing()));

  const auto& group_node = unpinned_node->children()[1];
  EXPECT_EQ(group_node->GetType(), TabCollectionNode::Type::kTabGroup);
  // TODO(crbug.com/442567916): Verify tab group view is created.
  ASSERT_EQ(group_node->children().size(), 1u);
  EXPECT_EQ(group_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      group_node->children()[0]->get_view_for_testing()));

  const auto& split_node = unpinned_node->children()[2];
  EXPECT_EQ(split_node->GetType(), TabCollectionNode::Type::kSplitTab);
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(
      split_node->get_view_for_testing()));
  ASSERT_EQ(split_node->children().size(), 2u);
  EXPECT_EQ(split_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      split_node->children()[0]->get_view_for_testing()));
  EXPECT_EQ(split_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      split_node->children()[1]->get_view_for_testing()));
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_ViewHierarchy) {
  AppendTab();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);

  // The parent_view should have one child, the root_node's view.
  ASSERT_EQ(parent_view->children().size(), 1u);
  const auto root_node_view = parent_view->children()[0];

  // The root_node_view should have three children, the pinned and unpinned
  // views and a separator.
  ASSERT_EQ(root_node_view->children().size(), 3u);
  const auto pinned_node_scroll_view = root_node_view->children()[0];
  ASSERT_TRUE(
      views::IsViewClass<views::Separator>(root_node_view->children()[1]));
  const auto unpinned_node_scroll_view = root_node_view->children()[2];

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

namespace {

std::unique_ptr<views::View> CreateViewWithMiddleView(
    views::View** middle_view_ptr,
    TabCollectionNode* node) {
  auto view = std::make_unique<views::View>();
  // The root node is the first one created.
  if (!*middle_view_ptr) {
    *middle_view_ptr = view->AddChildView(std::make_unique<views::View>());
    node->set_add_child_to_node(
        base::BindRepeating<TabCollectionNode::CustomAddChildView>(
            &views::View::AddChildView, base::Unretained(*middle_view_ptr)));
  }
  return view;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_ViewHierarchyCustomCallback) {
  AppendTab();
  auto parent_view = std::make_unique<views::View>();

  views::View* middle_view = nullptr;
  TabCollectionNode::SetViewFactoryForTesting(
      base::BindRepeating(&CreateViewWithMiddleView, &middle_view));

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);

  // The parent_view should have one child, the root_node's view.
  ASSERT_EQ(parent_view->children().size(), 1u);
  const auto root_node_view = parent_view->children()[0];

  // The root_node_view should have one child, the middle_view.
  ASSERT_EQ(root_node_view->children().size(), 1u);
  const auto middle_view_from_hierarchy = root_node_view->children()[0];
  ASSERT_EQ(middle_view, middle_view_from_hierarchy);

  // The middle_view should have two children, the pinned and unpinned views.
  ASSERT_EQ(middle_view->children().size(), 2u);
  const auto pinned_node_view = middle_view->children()[0];
  const auto unpinned_node_view = middle_view->children()[1];

  // The pinned_node_view should have no children.
  ASSERT_EQ(pinned_node_view->children().size(), 0u);

  // The unpinned_node_view should have two children, the two tab views.
  ASSERT_EQ(unpinned_node_view->children().size(), 2u);
}

namespace {

std::unique_ptr<views::View> CreateView(TabCollectionNode* node) {
  auto view = std::make_unique<views::View>();
  node->set_add_child_to_node(
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(view.get())));
  return view;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, GetDirectChildren) {
  AppendTab();
  auto parent_view = std::make_unique<views::View>();
  TabCollectionNode::SetViewFactoryForTesting(base::BindRepeating(&CreateView));

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);

  // The parent_view should have one child, the root_node's view.
  ASSERT_EQ(parent_view->children().size(), 1u);
  const auto root_node_view = parent_view->children()[0];

  // The root_node_view should have two children, the pinned and unpinned views.
  ASSERT_EQ(root_node_view->children().size(), 2u);

  const auto& child_views = root_node.GetDirectChildren();
  ASSERT_EQ(child_views.size(), 2u);
  EXPECT_EQ(child_views[0], root_node_view->children()[0]);
  EXPECT_EQ(child_views[1], root_node_view->children()[1]);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       CollectionReturnsOnlyCollectionItems) {
  AppendTab();
  auto parent_view = std::make_unique<views::View>();
  TabCollectionNode::SetViewFactoryForTesting(base::BindRepeating(&CreateView));
  views::View* non_collection_view =
      parent_view->AddChildView(std::make_unique<views::View>());

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);

  // The parent_view should have two children, the non-collection view, and the
  // root_node's view.
  ASSERT_EQ(parent_view->children().size(), 2u);
  const auto root_node_view = parent_view->children()[1];

  views::View* non_collection_view_2 =
      root_node_view->AddChildView(std::make_unique<views::View>());

  // The root_node_view should have three children, the pinned and unpinned
  // views, and the non-collection view.
  ASSERT_EQ(root_node_view->children().size(), 3u);

  const auto& child_views = root_node.GetDirectChildren();
  ASSERT_EQ(child_views.size(), 2u);
  EXPECT_NE(child_views[0], non_collection_view);
  EXPECT_NE(child_views[1], non_collection_view);
  EXPECT_NE(child_views[0], non_collection_view_2);
  EXPECT_NE(child_views[1], non_collection_view_2);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       VerticalTabViewIsCreatedForTabs) {
  // Add an unpinned tab.
  AppendTab();
  // Add a pinned tab.
  AppendPinnedTab();

  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Wait for the root node to populate its children.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // Get the pinned and unpinned tab container nodes.
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];

  // Verify the pinned node contains a single child.
  ASSERT_EQ(pinned_node->children().size(), 1u);
  // Verify that child is a VerticalTabView.
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      pinned_node->children()[0]->get_view_for_testing()));

  // Verify the unpinned node contains two children: the initial empty tab and
  // the newly appended tab.
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  // Verify both children are VerticalTabView instances.
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      unpinned_node->children()[0]->get_view_for_testing()));
  EXPECT_TRUE(views::IsViewClass<VerticalTabView>(
      unpinned_node->children()[1]->get_view_for_testing()));
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, TabsCreatedEvent) {
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];
  EXPECT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  EXPECT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The pinned Node should be empty.
  ASSERT_EQ(pinned_node->children().size(), 0u);

  // The unpinned Node should have one tab (the initial one).
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);

  AppendPinnedTab();

  // The pinned Node should have one tab.
  ASSERT_EQ(pinned_node->children().size(), 1u);
  EXPECT_EQ(pinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);

  AppendTab();

  // The unpinned Node should contain two tabs (the initial one and the new
  // one).
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);

  TabCollectionNode* initial_unpinned_tab_node =
      unpinned_node->children()[0].get();
  TabCollectionNode* appended_unpinned_tab_node =
      unpinned_node->children()[1].get();

  // Insert a tab between the two unpinned tabs.
  InsertTab(2);

  // The unpinned Node should contain three tabs (the initial one, then the new
  // one added by InsertTab, then the previous one that was added by AppendTab).
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[0].get(), initial_unpinned_tab_node);
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[2]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[2].get(), appended_unpinned_tab_node);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, DataChangedEvent) {
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);
  const auto& unpinned_node = root_node.children()[1];
  ASSERT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // The unpinned Node should have one tab (the initial one).
  ASSERT_EQ(unpinned_node->children().size(), 1u);
  const auto& tab = unpinned_node->children()[0];
  ASSERT_EQ(tab->GetType(), TabCollectionNode::Type::kTab);

  // Send out an update to change the title of the tab.
  const std::string new_title = "New Title";
  ASSERT_NE(new_title, tab->data()->get_tab()->title);

  auto event = tabs_api::mojom::OnDataChangedEvent::New();
  auto tab_data = tab->data()->get_tab()->Clone();
  tab_data->title = new_title;
  event->data = tabs_api::mojom::Data::NewTab(std::move(tab_data));
  root_node.OnDataChanged(event);

  // Title should be changed.
  EXPECT_EQ(new_title, tab->data()->get_tab()->title);
}
IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, CloseTabInteraction) {
  // 1. Setup: Have three tabs unpinned.
  AppendTab();
  AppendTab();

  auto parent_view = std::make_unique<views::View>();

  // 2. Initialize the RootTabCollectionNode, which observes the
  // TabStripService.
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // Get the Unpinned Container Node.
  const auto& unpinned_node = root_node.children()[1];
  ASSERT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // Initial structure: three tabs
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);

  // Close a tab.
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(1);

  // After detaching, the unpinned node should only have 2 tabs.
  ASSERT_EQ(unpinned_node->children().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, DetachAndReattachGroup) {
  // 1. Setup: Create an initial tab and a tab group to be detached.
  auto [contents_vector, group_id] = AppendTabsToNewGroup(2);

  auto parent_view = std::make_unique<views::View>();

  // 2. Initialize the RootTabCollectionNode, which observes the
  // TabStripService.
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // Get the Unpinned Container Node.
  const auto& unpinned_node = root_node.children()[1];
  ASSERT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // Initial structure: [Tab, GroupA] -> children size 2
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTabGroup);

  // Detached GroupA node to simulate moving to another window/position.
  std::unique_ptr<DetachedTabCollection> detached_group =
      browser()->tab_strip_model()->DetachTabGroupForInsertion(group_id);

  // After detaching, the unpinned node should only have 1 child (the initial
  // tab).
  ASSERT_EQ(unpinned_node->children().size(), 1u);

  // 3. Re-insert the detached group at index 0.
  browser()->tab_strip_model()->InsertDetachedTabGroupAt(
      std::move(detached_group), 0);

  // 4. Verification: The hierarchy should now be updated: [GroupA, Tab].
  // Since the detached group was inserted at index 0, it should be the first
  // child.
  ASSERT_EQ(unpinned_node->children().size(), 2u);

  // The first child should now be the Tab Group.
  const auto& reinserted_group_node = unpinned_node->children()[0];
  EXPECT_EQ(reinserted_group_node->GetType(),
            TabCollectionNode::Type::kTabGroup);

  // The second child should be the original Tab.
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);

  // Verify the group itself contains the correct number of children (2 tabs).
  ASSERT_EQ(reinserted_group_node->children().size(), 2u);
  EXPECT_EQ(reinserted_group_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(reinserted_group_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest, GroupContiguousTabs) {
  // 1. Setup: Start with three unpinned tabs.
  AppendTab();  // Tab 1 (index 1)
  AppendTab();  // Tab 2 (index 2)

  auto parent_view = std::make_unique<views::View>();

  // 2. Initialize the RootTabCollectionNode, which observes the
  // TabStripService.
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // Get the Unpinned Container Node.
  const auto& unpinned_node = root_node.children()[1];
  ASSERT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // Initial structure verification: All 3 tabs are direct children of the
  // Unpinned Container. [Tab, Tab, Tab] -> children size 3
  ASSERT_EQ(unpinned_node->children().size(), 3u);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(unpinned_node->children()[2]->GetType(),
            TabCollectionNode::Type::kTab);

  browser()->tab_strip_model()->AddToNewGroup({0, 1});

  // 4. Verification: The hierarchy should now be updated to [Tab, Group].
  // The two tabs are replaced by a single group node.
  ASSERT_EQ(unpinned_node->children().size(), 2u);

  // The first child is the group.
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTabGroup);
  const auto& new_group_node = unpinned_node->children()[0];

  // Verify the group itself contains the correct number of children (2 tabs).
  ASSERT_EQ(new_group_node->children().size(), 2u);
  EXPECT_EQ(new_group_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);
  EXPECT_EQ(new_group_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       SingleMoveWithinCollection) {
  // 1. Setup: Start with three unpinned tabs.
  AppendTab();  // Tab 1 (index 1)
  AppendTab();  // Tab 2 (index 2)

  auto parent_view = std::make_unique<views::View>();

  // 2. Initialize the RootTabCollectionNode, which observes the
  // TabStripService.
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // Get the Unpinned Container Node.
  const auto& unpinned_node = root_node.children()[1];
  ASSERT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // Initial structure verification and saving pointers to nodes.
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
  browser()->tab_strip_model()->SetSelectionFromModel(selection_model);
  browser()->tab_strip_model()->MoveSelectedTabsTo(2, std::nullopt);

  // 4. Validation: Check the final node order.
  ASSERT_EQ(unpinned_node->children().size(), 3u);

  // Expected Final Order: [B, C, A]
  // Node at index 0 is Tab B (originally index 1).
  EXPECT_EQ(unpinned_node->children()[0].get(), tab_b_node);
  EXPECT_EQ(unpinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);

  // Node at index 1 is Tab C (originally index 2).
  EXPECT_EQ(unpinned_node->children()[1].get(), tab_c_node);
  EXPECT_EQ(unpinned_node->children()[1]->GetType(),
            TabCollectionNode::Type::kTab);

  // Node at index 2 is Tab A (originally index 0).
  EXPECT_EQ(unpinned_node->children()[2].get(), tab_a_node);
  EXPECT_EQ(unpinned_node->children()[2]->GetType(),
            TabCollectionNode::Type::kTab);
}

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       SingleMoveAcrossCollection) {
  // 1. Setup: Start with three unpinned tabs.
  // Tab 0: Initial tab
  // Tab 1: AppendTab() -> This is the tab we will pin.
  // Tab 2: AppendTab()
  AppendTab();
  AppendTab();

  auto parent_view = std::make_unique<views::View>();

  // 2. Initialize the RootTabCollectionNode, which observes the
  // TabStripService.
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  // Wait for the initial structure to be populated.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // Get the Pinned and Unpinned Container Nodes.
  const auto& pinned_node = root_node.children()[0];
  const auto& unpinned_node = root_node.children()[1];

  ASSERT_EQ(pinned_node->GetType(), TabCollectionNode::Type::kPinnedTabs);
  ASSERT_EQ(unpinned_node->GetType(), TabCollectionNode::Type::kUnpinnedTabs);

  // Initial State Validation:
  // Pinned: 0 children.
  ASSERT_EQ(pinned_node->children().size(), 0u);
  // Unpinned: 3 children.
  ASSERT_EQ(unpinned_node->children().size(), 3u);

  // Save a pointer to the tab we intend to pin (the second one, at index 1).
  TabCollectionNode* tab_to_pin_node = unpinned_node->children()[1].get();

  // 3. Perform the Move (Pin the tab).
  // Pinning the tab at index 1 moves it from the Unpinned collection
  // (where it was at index 1) to the Pinned collection (at index 0).
  browser()->tab_strip_model()->SetTabPinned(1, true);

  // 4. Verification: The RootTabCollectionNode should process the OnNodeMoved
  // event. We check that the Pinned container now has the expected child.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return pinned_node->children().size() == 1u; }));

  // Final State Validation:

  // Pinned Node verification:
  // Pinned: 1 child (the moved tab).
  ASSERT_EQ(pinned_node->children().size(), 1u);
  // The first child of the Pinned node must be the tab we moved.
  EXPECT_EQ(pinned_node->children()[0].get(), tab_to_pin_node);
  EXPECT_EQ(pinned_node->children()[0]->GetType(),
            TabCollectionNode::Type::kTab);

  // Unpinned Node verification:
  // Unpinned: 2 children remaining.
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  // Ensure the tab is no longer a child of the Unpinned node.
  EXPECT_NE(unpinned_node->children()[0].get(), tab_to_pin_node);
  EXPECT_NE(unpinned_node->children()[1].get(), tab_to_pin_node);
}
