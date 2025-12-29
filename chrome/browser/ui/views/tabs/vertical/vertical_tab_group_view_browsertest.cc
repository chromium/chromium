// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"

#include "base/test/run_until.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/image_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"

class VerticalTabGroupViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  RootTabCollectionNode* root_node() {
    VerticalTabStripRegionView* region_view =
        browser()
            ->GetBrowserView()
            .vertical_tab_strip_region_view_for_testing();
    return region_view->root_node_for_testing();
  }

  void CreateActiveTabGroup() {
    AppendTab();
    AppendTab();

    browser()->tab_strip_model()->ActivateTabAt(
        1, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));

    browser()->tab_strip_model()->AddToNewGroup({1});
  }

  void CreateInactiveTabGroup() {
    AppendTab();
    AppendTab();

    browser()->tab_strip_model()->AddToNewGroup({1});

    browser()->tab_strip_model()->ActivateTabAt(
        2, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
  }

  void ClickTabGroupHeaderToToggleCollapse() {
    views::View* const tab_group_header =
        BrowserElementsViews::From(browser())->GetView(
            kTabGroupHeaderElementId);
    auto mouse_button =
        base::FeatureList::IsEnabled(tab_groups::kLeftClickOpensTabGroupBubble)
            ? ui::EF_RIGHT_MOUSE_BUTTON
            : ui::EF_LEFT_MOUSE_BUTTON;
    ui::MouseEvent mouse_release_event(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), mouse_button, mouse_button);
    tab_group_header->OnMouseReleased(mouse_release_event);
  }

  const tabs::TabInterface* GetTabInterfaceForNode(TabCollectionNode* node) {
    return std::get<const tabs::TabInterface*>(node->GetNodeData());
  }

 protected:
  // Appends a new tab to the end of the tab strip.
  content::WebContents* AppendTab() {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    tab_strip_model()->AppendWebContents(std::move(contents), true);
    return raw_contents;
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       ClickingTabGroupHeaderTogglesCollapse) {
  CreateInactiveTabGroup();

  // The grouped tab is the first child of the group collection, which is the
  // second child of the unpinned collection which is the second child of the
  // root node.
  TabCollectionNode* tab_node =
      root_node()->children()[1]->children()[1]->children()[0].get();
  VerticalTabView* tab =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing());
  // Verify the tab in the group is visible.
  EXPECT_TRUE(tab->GetVisible());

  // Collapse the tab group and verify the bounds of the group and the
  // visibility of the tab in the group.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));

  // Uncollapse the tab group and verify the tab in the group is visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return tab->GetVisible(); }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       HeaderCollapseIconUpdatesWithCollapseState) {
  CreateInactiveTabGroup();

  // The group is the second child of the unpinned collection which is the
  // second child of the root node. Verify the collapse icon is correct.
  TabCollectionNode* group_node =
      root_node()->children()[1]->children()[1].get();
  VerticalTabGroupHeaderView* group_header =
      static_cast<VerticalTabGroupView*>(group_node->get_view_for_testing())
          ->group_header_for_testing();
  EXPECT_EQ(group_header->collapse_icon_for_testing()
                ->GetImageModel()
                .GetVectorIcon()
                .vector_icon()
                ->name,
            kKeyboardArrowUpChromeRefreshIcon.name);

  // Collapse the tab group and verify the collapse icon is correctly updated.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return group_header->collapse_icon_for_testing()
               ->GetImageModel()
               .GetVectorIcon()
               .vector_icon()
               ->name == kKeyboardArrowDownChromeRefreshIcon.name;
  }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       CollapsingGroupWithActiveTabActivatesNextTab) {
  CreateActiveTabGroup();

  // The grouped tab is the first child of the group collection, which is the
  // second child of the unpinned collection which is the second child of the
  // root node.
  TabCollectionNode* tab_node =
      root_node()->children()[1]->children()[1]->children()[0].get();
  VerticalTabView* tab =
      static_cast<VerticalTabView*>(tab_node->get_view_for_testing());
  const tabs::TabInterface* tab_interface = GetTabInterfaceForNode(tab_node);
  // Verify the tab in the group is visible and active.
  EXPECT_TRUE(tab->GetVisible());
  EXPECT_TRUE(tab_interface->IsActivated());

  // Collapse the tab group and verify the tab in the group is not visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));
  EXPECT_FALSE(tab_interface->IsActivated());

  // The tab after the group will be the third child of the unpinned collection
  // which is the second child of the root node.
  TabCollectionNode* next_tab_node =
      root_node()->children()[1]->children()[2].get();
  EXPECT_TRUE(GetTabInterfaceForNode(next_tab_node)->IsActivated());
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       CollapsingGroupWithOnlyTabInStripAddsNewTab) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  // The unpinned collection is the second child of the
  // root node, it should only have one child, the tab group.
  TabCollectionNode* unpinned_collection_node =
      root_node()->children()[1].get();
  EXPECT_EQ(unpinned_collection_node->children().size(), 1u);

  // Collapse the tab group and verify there are now two children and the
  // ungrouped tab is active.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return unpinned_collection_node->children().size() == 2u; }));
  // The tab after the group will be the second child of the unpinned collection
  // which is the second child of the root node.
  TabCollectionNode* next_tab_node =
      root_node()->children()[1]->children()[1].get();
  EXPECT_TRUE(GetTabInterfaceForNode(next_tab_node)->IsActivated());
}
