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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  // Appends a new tab to the end of the tab strip.
  content::WebContents* AppendTab() {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    return raw_contents;
  }

  // Appends a new pinned tab to the end of the pinned tabs.
  content::WebContents* AppendPinnedTab() {
    content::WebContents* contents = AppendTab();
    const int index =
        browser()->tab_strip_model()->GetIndexOfWebContents(contents);
    browser()->tab_strip_model()->SetTabPinned(index, true);
    return contents;
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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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

IN_PROC_BROWSER_TEST_F(TabCollectionNodeBrowserTest,
                       RootNodePopulatesWithTabs_ViewHierarchy) {
  AppendTab();
  auto parent_view = std::make_unique<views::View>();

  RootTabCollectionNode root_node(
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

  // The root node should contain two nodes: one for pinned, one for unpinned.
  ASSERT_EQ(root_node.children().size(), 2u);

  // The parent_view should have one child, the root_node's view.
  ASSERT_EQ(parent_view->children().size(), 1u);
  const auto root_node_view = parent_view->children()[0];

  // The root_node_view should have two children, the pinned and unpinned views.
  ASSERT_EQ(root_node_view->children().size(), 2u);
  const auto pinned_node_view = root_node_view->children()[0];
  const auto unpinned_node_view = root_node_view->children()[1];

  // The pinned_node_view should have no children.
  ASSERT_EQ(pinned_node_view->children().size(), 0u);

  // The unpinned_node_view should have two children, the two tab views.
  ASSERT_EQ(unpinned_node_view->children().size(), 2u);
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
        base::BindRepeating(static_cast<views::View* (
                                views::View::*)(std::unique_ptr<views::View>)>(
                                &views::View::AddChildView),
                            base::Unretained(*middle_view_ptr)));
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
      browser()->GetFeatures().tab_strip_service(), parent_view.get(),
      base::BindRepeating(static_cast<views::View* (
                              views::View::*)(std::unique_ptr<views::View>)>(
                              &views::View::AddChildView),
                          base::Unretained(parent_view.get())));

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !root_node.children().empty(); }));

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
