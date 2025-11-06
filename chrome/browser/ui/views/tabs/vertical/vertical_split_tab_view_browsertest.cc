
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class VerticalSplitTabViewTest : public InProcessBrowserTest {
 public:
  VerticalSplitTabViewTest() = default;
  ~VerticalSplitTabViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {tabs::kVerticalTabs, features::kSideBySide}, {});
    InProcessBrowserTest::SetUp();
  }

  void CreateSplitTab() {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();

    // Add pinned split tabs.
    content::WebContents* contents1 = AppendTab();
    content::WebContents* contents2 = AppendTab();

    const int index1 = tab_strip_model->GetIndexOfWebContents(contents1);
    const int index2 = tab_strip_model->GetIndexOfWebContents(contents2);

    tab_strip_model->ActivateTabAt(
        index1, TabStripUserGestureDetails(
                    TabStripUserGestureDetails::GestureType::kOther));

    tab_strip_model->AddToNewSplit(
        {index2}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewTest, ProposedLayout_Unbounded) {
  CreateSplitTab();
  // // Create view hierarchy from an arbitrary parent view since we don't
  // // currently support updates from the API.
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));
  auto split = root_node.children()[1]->get_view_for_testing()->children()[1];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(split));
  VerticalSplitTabView* split_tab_view =
      static_cast<VerticalSplitTabView*>(split);

  auto children = split_tab_view->children();
  EXPECT_EQ(children.size(), 2);
  auto child1 = children[0];
  auto child2 = children[1];

  auto proposed_layout =
      split_tab_view->CalculateProposedLayout(views::SizeBounds());
  auto* child1_layout = proposed_layout.GetLayoutFor(child1);
  auto* child2_layout = proposed_layout.GetLayoutFor(child2);
  // Expect children to be on the same row.
  EXPECT_EQ(child1_layout->bounds.y(), child2_layout->bounds.y());
  // Expect children to be next to each other with no gap.
  EXPECT_EQ(child1_layout->bounds.right(), child2_layout->bounds.x());
  // Expect total width to just hold the two children
  EXPECT_EQ(proposed_layout.host_size.width(), child2_layout->bounds.right());
  EXPECT_EQ(proposed_layout.host_size.width(),
            child1_layout->bounds.width() + child2_layout->bounds.width());
}

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewTest, ProposedLayout_LargeBounds) {
  CreateSplitTab();
  // // Create view hierarchy from an arbitrary parent view since we don't
  // // currently support updates from the API.
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));
  auto split = root_node.children()[1]->get_view_for_testing()->children()[1];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(split));
  VerticalSplitTabView* split_tab_view =
      static_cast<VerticalSplitTabView*>(split);

  auto children = split_tab_view->children();
  EXPECT_EQ(children.size(), 2);
  auto child1 = children[0];
  auto child2 = children[1];

  // Needs to be larger than 2 * kVerticalTabExpandedMinWidth.
  int available_width = 200;
  auto proposed_layout = split_tab_view->CalculateProposedLayout(
      views::SizeBounds(available_width, {}));
  auto* child1_layout = proposed_layout.GetLayoutFor(child1);
  auto* child2_layout = proposed_layout.GetLayoutFor(child2);
  // Expect children to be on the same row.
  EXPECT_EQ(child1_layout->bounds.y(), child2_layout->bounds.y());
  // Expect children to be next to each other with no gap.
  EXPECT_EQ(child1_layout->bounds.right(), child2_layout->bounds.x());
  // Expect total width to just hold the two children.
  EXPECT_EQ(proposed_layout.host_size.width(), child2_layout->bounds.right());
  EXPECT_EQ(proposed_layout.host_size.width(),
            child1_layout->bounds.width() + child2_layout->bounds.width());
  // Expect children to share total width.
  EXPECT_EQ((available_width / 2), child1_layout->bounds.width());
  EXPECT_EQ((available_width / 2), child2_layout->bounds.width());
}

IN_PROC_BROWSER_TEST_F(VerticalSplitTabViewTest, ProposedLayout_LimitedBounds) {
  CreateSplitTab();
  // // Create view hierarchy from an arbitrary parent view since we don't
  // // currently support updates from the API.
  std::unique_ptr<views::View> parent_view = std::make_unique<views::View>();
  RootTabCollectionNode root_node(
      browser()
          ->GetFeatures()
          .tab_strip_service_feature()
          ->GetTabStripService(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));
  auto split = root_node.children()[1]->get_view_for_testing()->children()[1];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(split));
  VerticalSplitTabView* split_tab_view =
      static_cast<VerticalSplitTabView*>(split);

  auto children = split_tab_view->children();
  EXPECT_EQ(children.size(), 2);
  auto child1 = children[0];
  auto child2 = children[1];

  // Needs to be smaller than 2 * kVerticalTabExpandedMinWidth.
  int available_width = 75;
  auto proposed_layout = split_tab_view->CalculateProposedLayout(
      views::SizeBounds(available_width, {}));
  auto* child1_layout = proposed_layout.GetLayoutFor(child1);
  auto* child2_layout = proposed_layout.GetLayoutFor(child2);
  // Expect children to be on different rows.
  EXPECT_NE(child1_layout->bounds.y(), child2_layout->bounds.y());
  // Expect children to be next to each other vertically with no gap.
  EXPECT_EQ(child1_layout->bounds.bottom(), child2_layout->bounds.y());
  // Expect total height to just hold the two children.
  EXPECT_EQ(proposed_layout.host_size.height(), child2_layout->bounds.bottom());
  EXPECT_EQ(proposed_layout.host_size.height(),
            child1_layout->bounds.height() + child2_layout->bounds.height());
  // Expect children to fill width.
  EXPECT_EQ(available_width, child1_layout->bounds.width());
  EXPECT_EQ(available_width, child2_layout->bounds.width());
}
