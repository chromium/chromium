// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_view.h"

#include "base/test/run_until.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_attention_indicator.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/image_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"

class VerticalTabGroupViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{tabs::kVerticalTabs, {}},
            {data_sharing::features::kDataSharingFeature, {}}};
  }

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
    RunScheduledLayouts();
  }

  tab_groups::TabGroupId CreateInactiveTabGroup() {
    AppendTab();
    AppendTab();

    tab_groups::TabGroupId group_id =
        browser()->tab_strip_model()->AddToNewGroup({1});

    browser()->tab_strip_model()->ActivateTabAt(
        2, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
    RunScheduledLayouts();
    return group_id;
  }

  void ClickTabGroupHeaderToToggleCollapse() {
    views::View* const tab_group_header =
        BrowserElementsViews::From(browser())->GetView(
            kTabGroupHeaderElementId);
    ui::MouseEvent mouse_release_event(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
        ui::EF_LEFT_MOUSE_BUTTON);
    tab_group_header->OnMouseReleased(mouse_release_event);
  }

  void ClickTabGroupHeaderToOpenEditorBubble() {
    views::View* const tab_group_header =
        BrowserElementsViews::From(browser())->GetView(
            kTabGroupHeaderElementId);
    ui::MouseEvent mouse_release_event(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_RIGHT_MOUSE_BUTTON,
        ui::EF_RIGHT_MOUSE_BUTTON);
    tab_group_header->OnMouseReleased(mouse_release_event);
  }

  const tabs::TabInterface* GetTabInterfaceForNode(TabCollectionNode* node) {
    return std::get<const tabs::TabInterface*>(node->GetNodeData());
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       ClickingTabGroupHeaderTogglesCollapse) {
  CreateInactiveTabGroup();

  // The grouped tab is the first child of the group collection, which is the
  // second child of the unpinned collection which is the second child of the
  // root node.
  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->children()[0]
          .get();
  VerticalTabView* tab = static_cast<VerticalTabView*>(tab_node->view());
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

  // Verify the collapse icon is correct.
  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  VerticalTabGroupHeaderView* group_header =
      static_cast<VerticalTabGroupView*>(group_node->view())->group_header();
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

  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->children()[0]
          .get();
  VerticalTabView* tab = static_cast<VerticalTabView*>(tab_node->view());
  const tabs::TabInterface* tab_interface = GetTabInterfaceForNode(tab_node);
  // Verify the tab in the group is visible and active.
  EXPECT_TRUE(tab->GetVisible());
  EXPECT_TRUE(tab_interface->IsActivated());

  // Collapse the tab group and verify the tab in the group is not visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));
  EXPECT_FALSE(tab_interface->IsActivated());

  // The tab after the group will be the third child of the unpinned collection.
  TabCollectionNode* next_tab_node =
      unpinned_collection_node()->children()[2].get();
  EXPECT_TRUE(GetTabInterfaceForNode(next_tab_node)->IsActivated());
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       CollapsingGroupWithOnlyTabInStripAddsNewTab) {
  browser()->tab_strip_model()->AddToNewGroup({0});

  // The unpinned collection should only have one child, the tab group.
  EXPECT_EQ(unpinned_collection_node()->children().size(), 1u);

  // Collapse the tab group and verify there are now two children and the
  // ungrouped tab is active.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return unpinned_collection_node()->children().size() == 2u; }));
  // The tab after the group will be the second child of the unpinned
  // collection.
  TabCollectionNode* next_tab_node =
      unpinned_collection_node()->children()[1].get();
  EXPECT_TRUE(GetTabInterfaceForNode(next_tab_node)->IsActivated());
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest, OpenEditorBubble) {
  CreateInactiveTabGroup();

  // The editor dialog should not be visible.
  auto* editor_dialog =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTabGroupEditorBubbleId);
  EXPECT_FALSE(editor_dialog);

  // The editor dialog should be visible after activating it via the tab group
  // header.
  ClickTabGroupHeaderToOpenEditorBubble();
  editor_dialog =
      ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
          kTabGroupEditorBubbleId);
  EXPECT_TRUE(editor_dialog);
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       MousePressFalseWhileEditorBubbleOpen) {
  CreateInactiveTabGroup();

  views::View* const tab_group_header =
      BrowserElementsViews::From(browser())->GetView(kTabGroupHeaderElementId);
  ui::MouseEvent mouse_press_event(ui::EventType::kMousePressed, gfx::Point(),
                                   gfx::Point(), ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);

  // Verify press events return true when the editor dialog does not exist.
  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTabGroupEditorBubbleId));
  EXPECT_TRUE(tab_group_header->OnMousePressed(mouse_press_event));

  // The editor dialog should be visible after activating it via the tab group
  // header.
  ClickTabGroupHeaderToOpenEditorBubble();
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTabGroupEditorBubbleId));

  // Verify mouse press events return false while the editor dialog is visible.
  EXPECT_FALSE(tab_group_header->OnMousePressed(mouse_press_event));
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest,
                       EditorBubbleOpensOnEditorBubbleButtonPress) {
  CreateInactiveTabGroup();

  VerticalTabGroupHeaderView* const tab_group_header =
      views::AsViewClass<VerticalTabGroupHeaderView>(
          BrowserElementsViews::From(browser())->GetView(
              kTabGroupHeaderElementId));
  ui::MouseEvent mouse_press_event(ui::EventType::kMousePressed, gfx::Point(),
                                   gfx::Point(), ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON,
                                   ui::EF_LEFT_MOUSE_BUTTON);

  EXPECT_FALSE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTabGroupEditorBubbleId));
  tab_group_header->editor_bubble_button()->OnMousePressed(mouse_press_event);
  // The editor dialog should be visible after activating it via the tab group
  // header's editor bubble button.
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
      kTabGroupEditorBubbleId));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return tab_group_header->editor_bubble_button()->GetVisible();
  }));
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewTest, AttentionIndicator) {
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();

  TabCollectionNode* tab_node =
      root_node()->children()[1]->children()[1]->children()[0].get();
  VerticalTabView* tab = static_cast<VerticalTabView*>(tab_node->view());
  // Verify the tab in the group is visible.
  EXPECT_TRUE(tab->GetVisible());

  // Collapse the tab group and verify the tab in the group is not visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));
  // Set the attention indicator to true and verify its visibility.
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group_id)
      ->GetTabGroupFeatures()
      ->attention_indicator()
      ->SetHasAttention(true);
  VerticalTabGroupHeaderView* const tab_group_header =
      static_cast<VerticalTabGroupHeaderView*>(
          BrowserElementsViews::From(browser())->GetView(
              kTabGroupHeaderElementId));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return tab_group_header->attention_indicator_for_testing()->GetVisible();
  }));
}
