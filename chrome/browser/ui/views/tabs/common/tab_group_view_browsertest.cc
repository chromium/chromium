// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_attention_indicator.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

class TabGroupViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{data_sharing::features::kDataSharingFeature, {}},
            {features::kTabGroupsFocusing, {}}};
  }

  RootTabCollectionNode* root_node() {
    VerticalTabStripRegionView* region_view =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->vertical_tab_strip_region_view_for_testing();
    return region_view->root_node_for_testing();
  }

  void ActivateTab(const tabs::TabInterface* tab) {
    int index = browser()->GetTabStripModel()->GetIndexOfTab(tab);
    CHECK(index != TabStripModel::kNoTab);
    browser()->GetTabStripModel()->ActivateTabAt(
        index, TabStripUserGestureDetails(
                   TabStripUserGestureDetails::GestureType::kOther));
    RunScheduledLayouts();
  }

  tab_groups::TabGroupId CreateActiveTabGroup() {
    AppendTab();
    AppendTab();

    browser()->GetTabStripModel()->ActivateTabAt(
        1, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));

    tab_groups::TabGroupId group_id =
        browser()->GetTabStripModel()->AddToNewGroup({1});
    RunScheduledLayouts();
    return group_id;
  }

  tab_groups::TabGroupId CreateInactiveTabGroup() {
    AppendTab();
    AppendTab();

    tab_groups::TabGroupId group_id =
        browser()->GetTabStripModel()->AddToNewGroup({1});

    browser()->GetTabStripModel()->ActivateTabAt(
        2, TabStripUserGestureDetails(
               TabStripUserGestureDetails::GestureType::kOther));
    RunScheduledLayouts();
    return group_id;
  }

  void UngroupTabGroup(tab_groups::TabGroupId group_id) {
    const gfx::Range tab_range = browser()
                                     ->GetTabStripModel()
                                     ->group_model()
                                     ->GetTabGroup(group_id)
                                     ->ListTabs();

    std::vector<int> tab_indices;
    tab_indices.reserve(tab_range.length());
    for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
      tab_indices.push_back(i);
    }

    browser()->GetTabStripModel()->RemoveFromGroup(tab_indices);
    RunScheduledLayouts();
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

  const tabs::TabInterface* GetTabInterfaceForNode(
      const TabCollectionNode* node) {
    return std::get<tabs::ConstDanglingUntriagedTabInterface>(
        node->GetNodeData());
  }
};

IN_PROC_BROWSER_TEST_F(TabGroupViewTest,
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
  TabView* tab = views::AsViewClass<TabView>(tab_node->view());
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

IN_PROC_BROWSER_TEST_F(TabGroupViewTest,
                       HeaderCollapseIconUpdatesWithCollapseState) {
  CreateInactiveTabGroup();

  // Verify the collapse icon is correct.
  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  TabGroupHeaderView* group_header =
      views::AsViewClass<TabGroupView>(group_node->view())->group_header();
  EXPECT_EQ(group_header->collapse_icon_for_testing()
                ->GetImageModel()
                .GetVectorIcon()
                .vector_icon()
                ->name,
            features::IsRoundedIconsEnabled()
                ? vector_icons::kKeyboardArrowUpIcon.name
                : kKeyboardArrowUpChromeRefreshOldIcon.name);

  // Collapse the tab group and verify the collapse icon is correctly updated.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return group_header->collapse_icon_for_testing()
               ->GetImageModel()
               .GetVectorIcon()
               .vector_icon()
               ->name == (features::IsRoundedIconsEnabled()
                              ? kKeyboardArrowDownIcon.name
                              : kKeyboardArrowDownChromeRefreshOldIcon.name);
  }));
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest,
                       CollapsingGroupWithActiveTabActivatesNextTab) {
  CreateActiveTabGroup();

  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->children()[0]
          .get();
  TabView* tab = views::AsViewClass<TabView>(tab_node->view());
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

IN_PROC_BROWSER_TEST_F(TabGroupViewTest,
                       CollapsingGroupWithOnlyTabInStripAddsNewTab) {
  browser()->GetTabStripModel()->AddToNewGroup({0});

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

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, UngroupingTabsFromCollapsedGroup) {
  tab_groups::TabGroupId group_id = CreateActiveTabGroup();

  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->children()[0]
          .get();
  TabView* tab = static_cast<TabView*>(tab_node->view());

  // Collapse the tab group and verify the tab is not visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));

  // Ungroup the tab group and verify that the tab is now visible.
  UngroupTabGroup(group_id);
  EXPECT_TRUE(base::test::RunUntil([&]() { return tab->GetVisible(); }));
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, ActiveTabFromCollapsedGroup) {
  CreateActiveTabGroup();

  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->children()[0]
          .get();
  TabView* tab = static_cast<TabView*>(tab_node->view());

  // Collapse the tab group and verify the tab is not visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));

  // Activate the tab within the group and verify that the tab is now visible.
  ActivateTab(GetTabInterfaceForNode(tab->collection_node()));
  EXPECT_TRUE(base::test::RunUntil([&]() { return tab->GetVisible(); }));
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, OpenEditorBubble) {
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

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, MousePressFalseWhileEditorBubbleOpen) {
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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_EditorBubbleOpensOnEditorBubbleButtonPress \
  EditorBubbleOpensOnEditorBubbleButtonPress
#else
#define MAYBE_EditorBubbleOpensOnEditorBubbleButtonPress \
  DISABLED_EditorBubbleOpensOnEditorBubbleButtonPress
#endif
IN_PROC_BROWSER_TEST_F(TabGroupViewTest,
                       MAYBE_EditorBubbleOpensOnEditorBubbleButtonPress) {
#if BUILDFLAG(IS_OZONE)
  if (ui::OzonePlatform::GetInstance()->RunningOnWaylandForTest()) {
    // The test constantly failing on wayland.
    return;
  }
#endif

  CreateInactiveTabGroup();

  TabGroupHeaderView* const tab_group_header =
      views::AsViewClass<TabGroupHeaderView>(
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
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, AttentionIndicator) {
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();

  TabCollectionNode* tab_node =
      root_node()->children()[1]->children()[1]->children()[0].get();
  TabView* tab = views::AsViewClass<TabView>(tab_node->view());
  // Verify the tab in the group is visible.
  EXPECT_TRUE(tab->GetVisible());

  // Collapse the tab group and verify the tab in the group is not visible.
  ClickTabGroupHeaderToToggleCollapse();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !tab->GetVisible(); }));
  // Set the attention indicator to true and verify its visibility.
  browser()
      ->GetTabStripModel()
      ->group_model()
      ->GetTabGroup(group_id)
      ->GetTabGroupFeatures()
      ->attention_indicator()
      ->SetHasAttention(true);
  TabGroupHeaderView* const tab_group_header =
      views::AsViewClass<TabGroupHeaderView>(
          BrowserElementsViews::From(browser())->GetView(
              kTabGroupHeaderElementId));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return tab_group_header->attention_indicator_for_testing()->GetVisible();
  }));
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, ShiftGroupUp_PastSingleTab) {
  TabStripModel* model = browser()->GetTabStripModel();

  AppendTab();
  AppendTab();
  ASSERT_EQ(3, model->count());

  // Create a group with the second and third tabs (indices 1 and 2).
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2});

  // Verify initial state: [Ungrouped Tab 0], [Grouped Tab 1, Grouped Tab 2]
  EXPECT_FALSE(model->GetTabGroupForTab(0).has_value());
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_EQ(group, model->GetTabGroupForTab(2));

  // Shift the group up past the first un-grouped tab.
  vertical_tab_strip_controller()->ShiftGroupUp(group);

  // Verify the group is now at the beginning.
  EXPECT_EQ(group, model->GetTabGroupForTab(0));
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_FALSE(model->GetTabGroupForTab(2).has_value());
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, ShiftGroupDown_PastTabGroup) {
  TabStripModel* model = browser()->GetTabStripModel();

  AppendTab();
  AppendTab();
  AppendTab();
  ASSERT_EQ(4, model->count());

  // Create Group A (indices 0 and 1) and Group B (indices 2 and 3).
  tab_groups::TabGroupId group_a = model->AddToNewGroup({0, 1});
  tab_groups::TabGroupId group_b = model->AddToNewGroup({2, 3});

  // Verify initial state: [Group A (0, 1)], [Group B (2, 3)]
  EXPECT_EQ(group_a, model->GetTabGroupForTab(0));
  EXPECT_EQ(group_a, model->GetTabGroupForTab(1));
  EXPECT_EQ(group_b, model->GetTabGroupForTab(2));
  EXPECT_EQ(group_b, model->GetTabGroupForTab(3));

  // Shift Group A down, skipping group_b.
  vertical_tab_strip_controller()->ShiftGroupDown(group_a);

  // Verify the groups swapped positions: [Group B (0, 1)], [Group A (2, 3)]
  EXPECT_EQ(group_b, model->GetTabGroupForTab(0));
  EXPECT_EQ(group_b, model->GetTabGroupForTab(1));
  EXPECT_EQ(group_a, model->GetTabGroupForTab(2));
  EXPECT_EQ(group_a, model->GetTabGroupForTab(3));
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, ShiftGroupUp_AlreadyAtTop) {
  TabStripModel* model = browser()->GetTabStripModel();

  AppendTab();
  AppendTab();
  ASSERT_EQ(3, model->count());

  // Create a group with the first and second tabs (indices 0 and 1).
  tab_groups::TabGroupId group = model->AddToNewGroup({0, 1});

  // Verify initial state: [Grouped Tab 0, Grouped Tab 1], [Ungrouped Tab 2]
  EXPECT_EQ(group, model->GetTabGroupForTab(0));
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_FALSE(model->GetTabGroupForTab(2).has_value());

  // Attempt to shift the group up when it is already at the top.
  vertical_tab_strip_controller()->ShiftGroupUp(group);

  // Verify the group has not moved ([Grouped Tab 0, Grouped Tab 1], [Ungrouped
  // Tab 2]).
  EXPECT_EQ(group, model->GetTabGroupForTab(0));
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_FALSE(model->GetTabGroupForTab(2).has_value());
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest, ShiftGroupDown_AlreadyAtBottom) {
  TabStripModel* model = browser()->GetTabStripModel();

  AppendTab();
  AppendTab();
  ASSERT_EQ(3, model->count());

  // Create a group with the second and third tabs (indices 1 and 2).
  tab_groups::TabGroupId group = model->AddToNewGroup({1, 2});

  // Verify initial state: [Ungrouped Tab 0], [Grouped Tab 1, Grouped Tab 2]
  EXPECT_FALSE(model->GetTabGroupForTab(0).has_value());
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_EQ(group, model->GetTabGroupForTab(2));

  // Attempt to shift the group down when it is already at the bottom.
  vertical_tab_strip_controller()->ShiftGroupDown(group);

  // Verify the group has not moved ([Ungrouped Tab 0], [Grouped Tab 1, Grouped
  // Tab 2]).
  EXPECT_FALSE(model->GetTabGroupForTab(0).has_value());
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_EQ(group, model->GetTabGroupForTab(2));
}

IN_PROC_BROWSER_TEST_F(TabGroupViewTest,
                       GroupLineHiddenAndTabsAlignedInFocusMode) {
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();

  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  TabGroupView* group_view =
      views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_TRUE(group_view);

  TabCollectionNode* tab_node = group_node->children()[0].get();
  TabView* tab = views::AsViewClass<TabView>(tab_node->view());
  ASSERT_TRUE(tab);

  // Initially in expanded mode (not focused), group line is visible and tab is
  // indented.
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(tab->x(), TabGroupView::kTabLeadingPadding);

  // Focus the group.
  browser()->GetTabStripModel()->SetFocusedGroup(group_id);
  RunScheduledLayouts();

  // In focus mode, group line should be hidden and tab should be aligned at x =
  // 0.
  EXPECT_FALSE(group_view->group_line()->GetVisible());
  EXPECT_EQ(tab->x(), 0);

  // Unfocus the group.
  browser()->GetTabStripModel()->SetFocusedGroup(std::nullopt);
  RunScheduledLayouts();

  // Group line should be restored and tab indented again.
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(tab->x(), TabGroupView::kTabLeadingPadding);
}

class HorizontalTabGroupViewBrowserTest : public InProcessBrowserTest {
 public:
  HorizontalTabGroupViewBrowserTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }

 protected:
  RootTabCollectionNode* root_node() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view ? base_region_view->root_node_for_testing()
                            : nullptr;
  }

  TabStripModel* GetTabStripModel() { return browser()->GetTabStripModel(); }

  void AppendTab() {
    chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/-1,
                     /*foreground=*/true);
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       UnboundedLayoutQueryDoesNotClearAvailableSpace) {
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = GetTabStripModel()->AddToNewGroup({1, 2});

  auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
      BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  ASSERT_NE(base_region_view, nullptr);

  auto* tab_strip_view =
      views::AsViewClass<TabStripView>(base_region_view->GetTabStripView());
  ASSERT_NE(tab_strip_view, nullptr);

  auto* unpinned_container = tab_strip_view->GetUnpinnedTabsContainer();
  ASSERT_NE(unpinned_container, nullptr);

  auto* group_node = root_node()->GetNodeForHandle(GetTabStripModel()
                                                       ->group_model()
                                                       ->GetTabGroup(group_id)
                                                       ->GetCollectionHandle());
  ASSERT_NE(group_node, nullptr);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_NE(group_view, nullptr);

  // Perform a standard layout pass so bounded available space is established.
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(unpinned_container->available_space().is_bounded());
  const int original_unpinned_available =
      unpinned_container->available_space().value();
  EXPECT_GT(original_unpinned_available, 0);

  EXPECT_TRUE(group_view->available_space().is_bounded());
  const int original_group_available = group_view->available_space().value();
  EXPECT_GT(original_group_available, 0);

  // Perform unconstrained layout measurement queries on TabStripView and
  // UnpinnedTabContainerView.
  tab_strip_view->GetPreferredSize(views::SizeBounds());
  unpinned_container->GetPreferredSize(views::SizeBounds());

  // Verify that unbounded size queries did not clear or overwrite the bounded
  // available space on either container.
  EXPECT_TRUE(unpinned_container->available_space().is_bounded());
  EXPECT_EQ(unpinned_container->available_space().value(),
            original_unpinned_available);

  EXPECT_TRUE(group_view->available_space().is_bounded());
  EXPECT_EQ(group_view->available_space().value(), original_group_available);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       ZeroSizedLayoutQueryDoesNotClearAvailableSpace) {
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = GetTabStripModel()->AddToNewGroup({1, 2});

  auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
      BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  auto* tab_strip_view =
      views::AsViewClass<TabStripView>(base_region_view->GetTabStripView());
  auto* unpinned_container = tab_strip_view->GetUnpinnedTabsContainer();
  auto* group_node = root_node()->GetNodeForHandle(GetTabStripModel()
                                                       ->group_model()
                                                       ->GetTabGroup(group_id)
                                                       ->GetCollectionHandle());
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());

  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
  const int original_unpinned_available =
      unpinned_container->available_space().value();
  const int original_group_available = group_view->available_space().value();
  ASSERT_GT(original_unpinned_available, 0);
  ASSERT_GT(original_group_available, 0);

  // Perform zero-sized layout measurement queries on TabStripView and
  // UnpinnedTabContainerView (e.g. minimum size calculations).
  tab_strip_view->GetMinimumSize();
  tab_strip_view->GetPreferredSize(views::SizeBounds(0, 0));
  unpinned_container->GetMinimumSize();
  unpinned_container->GetPreferredSize(views::SizeBounds(0, 0));

  EXPECT_EQ(unpinned_container->available_space().value(),
            original_unpinned_available);
  EXPECT_EQ(group_view->available_space().value(), original_group_available);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       UncollapsingGroupInHorizontalStripRestoresTabWidths) {
  AppendTab();
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = GetTabStripModel()->AddToNewGroup({2, 3});
  GetTabStripModel()->ActivateTabAt(0);

  auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
      BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  ASSERT_NE(base_region_view, nullptr);

  auto* tab_strip_view =
      views::AsViewClass<TabStripView>(base_region_view->GetTabStripView());
  ASSERT_NE(tab_strip_view, nullptr);

  auto* group_node = root_node()->GetNodeForHandle(GetTabStripModel()
                                                       ->group_model()
                                                       ->GetTabGroup(group_id)
                                                       ->GetCollectionHandle());
  ASSERT_NE(group_node, nullptr);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_NE(group_view, nullptr);

  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  // Collapse the group.
  group_view->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return group_view->IsCollapsed(); }));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  // Uncollapse the group.
  group_view->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !group_view->IsCollapsed(); }));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  // Tabs within the group should be restored to the same width as ungrouped
  // tabs, allowing a 1px difference for proportional space allocation.
  auto* unpinned_node =
      root_node()->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  ASSERT_NE(unpinned_node, nullptr);
  ASSERT_GE(unpinned_node->children().size(), 2u);
  auto* ungrouped_tab_node = unpinned_node->children()[1].get();
  auto* grouped_tab_node = group_node->children()[0].get();

  auto* ungrouped_tab_view =
      views::AsViewClass<TabView>(ungrouped_tab_node->view());
  auto* grouped_tab_view =
      views::AsViewClass<TabView>(grouped_tab_node->view());
  ASSERT_NE(ungrouped_tab_view, nullptr);
  ASSERT_NE(grouped_tab_view, nullptr);

  EXPECT_TRUE(base::test::RunUntil([&]() {
    RunScheduledLayouts();
    return std::abs(grouped_tab_view->width() - ungrouped_tab_view->width()) <=
           1;
  }));
  EXPECT_GT(grouped_tab_view->width(), 50);
}

// TODO(crbug.com/490428062): Create Tests to Verify Focus Order of Tab Group
// Header w/ Editor Bubble Button.
