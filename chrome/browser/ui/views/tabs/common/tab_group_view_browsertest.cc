// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"

#include <algorithm>

#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_attention_indicator.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/animations/focus_mode_animations.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_line_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_style.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_unfocus_button.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_collection_types.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget_utils.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

class TabGroupViewTestBase
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsBrowserTestMixin<
        InProcessBrowserTest>::GetEnabledFeatures();
    enabled.push_back({data_sharing::features::kDataSharingFeature, {}});
    enabled.push_back({features::kTabGroupsFocusing, {}});
    enabled.push_back({features::kNonGroupFocus, {}});
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }

  RootTabCollectionNode* root_node() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view ? base_region_view->root_node_for_testing()
                            : nullptr;
  }

  TabStripCollectionController* collection_controller() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view
               ? base_region_view->GetTabStripCollectionController()
               : nullptr;
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

  TabGroupView* GetTabGroupView(tab_groups::TabGroupId group_id) {
    TabCollectionNode* group_node =
        root_node()->GetNodeForHandle(browser()
                                          ->GetTabStripModel()
                                          ->group_model()
                                          ->GetTabGroup(group_id)
                                          ->GetCollectionHandle());
    return group_node ? views::AsViewClass<TabGroupView>(group_node->view())
                      : nullptr;
  }

  // Enables rich animations and drives `kFocusMode` from a container whose
  // clock only advances through `AdvanceFocusMotion()`.
  void EnableFocusAnimations() {
    disable_rich_animations_.reset();
    enable_rich_animations_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
    normal_duration_mode_ =
        std::make_unique<gfx::ScopedAnimationDurationScaleMode>(
            gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
    animation_container_ = base::MakeRefCounted<gfx::AnimationContainer>();
    BrowserAnimationController::From(browser())
        ->SetAnimationContainerForTesting(FocusModeAnimations::kFocusMode,
                                          animation_container_.get());
    animation_container_test_api_ =
        std::make_unique<gfx::AnimationContainerTestApi>(
            animation_container_.get());
  }

  // Disables rich animations so that setup happens synchronously.
  void DisableFocusAnimations() {
    disable_rich_animations_ =
        gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  }

  bool IsAnyTabLayoutAnimating() {
    views::View* const unpinned_view = unpinned_collection_node()->view();
    if (static_cast<TabCollectionAnimatingLayoutManager*>(
            unpinned_view->GetLayoutManager())
            ->is_animating()) {
      return true;
    }
    for (views::View* child : unpinned_view->children()) {
      if (auto* group_view = views::AsViewClass<TabGroupView>(child);
          group_view && static_cast<TabCollectionAnimatingLayoutManager*>(
                            group_view->GetLayoutManager())
                            ->is_animating()) {
        return true;
      }
    }
    return false;
  }

  void AdvanceFocusMotion(base::TimeDelta delta) {
    animation_container_test_api_->IncrementTime(delta);
    RunScheduledLayouts();
  }

  // Runs the focus motion to its end, then drains any tab layout animation
  // that is still in flight.
  void FinishFocusMotion() {
    const BrowserAnimationController* const controller =
        BrowserAnimationController::From(browser());
    if (controller->IsAnimating(FocusModeAnimations::kFocusMode)) {
      AdvanceFocusMotion(
          controller->GetMotionDuration(FocusModeAnimations::kFocusMode));
    }
    EXPECT_FALSE(controller->IsAnimating(FocusModeAnimations::kFocusMode));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      RunScheduledLayouts();
      return !IsAnyTabLayoutAnimating();
    }));
    RunScheduledLayouts();
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      disable_rich_animations_;
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      enable_rich_animations_;
  std::unique_ptr<gfx::ScopedAnimationDurationScaleMode> normal_duration_mode_;
  scoped_refptr<gfx::AnimationContainer> animation_container_;
  std::unique_ptr<gfx::AnimationContainerTestApi> animation_container_test_api_;
};

class TabGroupViewTest
    : public TabGroupViewTestBase,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  TabStripOrientation orientation() const { return GetParam(); }
  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  void SetUpOnMainThread() override {
    TabGroupViewTestBase::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }
};

IN_PROC_BROWSER_TEST_P(TabGroupViewTest,
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest,
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest,
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, UngroupingTabsFromCollapsedGroup) {
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, ActiveTabFromCollapsedGroup) {
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, OpenEditorBubble) {
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, MousePressFalseWhileEditorBubbleOpen) {
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, AttentionIndicator) {
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();

  TabCollectionNode* tab_node =
      unpinned_collection_node()
          ->GetChildNodeOfType(TabCollectionNode::Type::GROUP)
          ->children()[0]
          .get();
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

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, ShiftGroupPrevious_PastSingleTab) {
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

  // Shift the group previous past the first un-grouped tab.
  vertical_tab_strip_controller()->ShiftGroupPrevious(group);

  // Verify the group is now at the beginning.
  EXPECT_EQ(group, model->GetTabGroupForTab(0));
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_FALSE(model->GetTabGroupForTab(2).has_value());
}

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, ShiftGroupNext_PastTabGroup) {
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

  // Shift Group A next, skipping group_b.
  vertical_tab_strip_controller()->ShiftGroupNext(group_a);

  // Verify the groups swapped positions: [Group B (0, 1)], [Group A (2, 3)]
  EXPECT_EQ(group_b, model->GetTabGroupForTab(0));
  EXPECT_EQ(group_b, model->GetTabGroupForTab(1));
  EXPECT_EQ(group_a, model->GetTabGroupForTab(2));
  EXPECT_EQ(group_a, model->GetTabGroupForTab(3));
}

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, ShiftGroupPrevious_AlreadyAtFirst) {
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

  // Attempt to shift the group previous when it is already at the first index.
  vertical_tab_strip_controller()->ShiftGroupPrevious(group);

  // Verify the group has not moved ([Grouped Tab 0, Grouped Tab 1], [Ungrouped
  // Tab 2]).
  EXPECT_EQ(group, model->GetTabGroupForTab(0));
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_FALSE(model->GetTabGroupForTab(2).has_value());
}

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, ShiftGroupNext_AlreadyAtLast) {
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

  // Attempt to shift the group next when it is already at the last index.
  vertical_tab_strip_controller()->ShiftGroupNext(group);

  // Verify the group has not moved ([Ungrouped Tab 0], [Grouped Tab 1, Grouped
  // Tab 2]).
  EXPECT_FALSE(model->GetTabGroupForTab(0).has_value());
  EXPECT_EQ(group, model->GetTabGroupForTab(1));
  EXPECT_EQ(group, model->GetTabGroupForTab(2));
}

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, FocusTraversalBetweenHeaderAndTabs) {
  TabStripModel* model = browser()->GetTabStripModel();
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});

  auto* group_node = root_node()->GetNodeForHandle(
      model->group_model()->GetTabGroup(group_id)->GetCollectionHandle());
  ASSERT_NE(group_node, nullptr);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_NE(group_view, nullptr);

  auto* header_view = group_view->group_header();
  ASSERT_NE(header_view, nullptr);

  auto* first_tab_node = group_node->children()[0].get();
  auto* first_tab_view = views::AsViewClass<TabView>(first_tab_node->view());
  ASSERT_NE(first_tab_view, nullptr);

  views::FocusSearch focus_search(group_view, /*cycle=*/false,
                                  /*accessibility_mode=*/true);
  views::FocusTraversable* dummy_focus_traversable = nullptr;
  views::View* dummy_focus_traversable_view = nullptr;

  views::View* next_from_header = focus_search.FindNextFocusableView(
      header_view, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
  EXPECT_EQ(first_tab_view, next_from_header);

  views::View* prev_from_tab = focus_search.FindNextFocusableView(
      first_tab_view, views::FocusSearch::SearchDirection::kBackwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
  EXPECT_EQ(header_view, prev_from_tab);
}

IN_PROC_BROWSER_TEST_P(TabGroupViewTest, FocusTraversalInFocusMode) {
  DisableFocusAnimations();
  TabStripModel* model = browser()->GetTabStripModel();
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = model->AddToNewGroup({0, 1});

  auto* group_node = root_node()->GetNodeForHandle(
      model->group_model()->GetTabGroup(group_id)->GetCollectionHandle());
  ASSERT_NE(group_node, nullptr);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_NE(group_view, nullptr);

  auto* header_view = group_view->group_header();
  ASSERT_NE(header_view, nullptr);

  auto* first_tab_node = group_node->children()[0].get();
  auto* first_tab_view = views::AsViewClass<TabView>(first_tab_node->view());
  ASSERT_NE(first_tab_view, nullptr);

  model->EnterFocusMode(group_id);
  RunScheduledLayouts();

  auto* unfocus_button = group_view->unfocus_header_for_testing();
  ASSERT_NE(unfocus_button, nullptr);
  EXPECT_TRUE(unfocus_button->GetVisible());

  views::FocusSearch focus_search(group_view, /*cycle=*/false,
                                  /*accessibility_mode=*/true);
  views::FocusTraversable* dummy_focus_traversable = nullptr;
  views::View* dummy_focus_traversable_view = nullptr;

  views::View* next_from_unfocus = focus_search.FindNextFocusableView(
      unfocus_button, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
  EXPECT_EQ(header_view, next_from_unfocus);

  views::View* next_from_header = focus_search.FindNextFocusableView(
      header_view, views::FocusSearch::SearchDirection::kForwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);

  if (GetParam() == TabStripOrientation::kVertical) {
    ASSERT_NE(header_view->editor_bubble_button(), nullptr);
    EXPECT_TRUE(header_view->editor_bubble_button()->GetVisible());
    EXPECT_EQ(header_view->editor_bubble_button(), next_from_header);

    views::View* next_from_editor = focus_search.FindNextFocusableView(
        header_view->editor_bubble_button(),
        views::FocusSearch::SearchDirection::kForwards,
        views::FocusSearch::TraversalDirection::kDown,
        views::FocusSearch::StartingViewPolicy::kSkipStartingView,
        views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
        &dummy_focus_traversable, &dummy_focus_traversable_view);
    EXPECT_EQ(first_tab_view, next_from_editor);

    views::View* prev_from_tab = focus_search.FindNextFocusableView(
        first_tab_view, views::FocusSearch::SearchDirection::kBackwards,
        views::FocusSearch::TraversalDirection::kDown,
        views::FocusSearch::StartingViewPolicy::kSkipStartingView,
        views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
        &dummy_focus_traversable, &dummy_focus_traversable_view);
    EXPECT_EQ(header_view->editor_bubble_button(), prev_from_tab);

    views::View* prev_from_editor = focus_search.FindNextFocusableView(
        header_view->editor_bubble_button(),
        views::FocusSearch::SearchDirection::kBackwards,
        views::FocusSearch::TraversalDirection::kDown,
        views::FocusSearch::StartingViewPolicy::kSkipStartingView,
        views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
        &dummy_focus_traversable, &dummy_focus_traversable_view);
    EXPECT_EQ(header_view, prev_from_editor);
  } else {
    EXPECT_EQ(first_tab_view, next_from_header);

    views::View* prev_from_tab = focus_search.FindNextFocusableView(
        first_tab_view, views::FocusSearch::SearchDirection::kBackwards,
        views::FocusSearch::TraversalDirection::kDown,
        views::FocusSearch::StartingViewPolicy::kSkipStartingView,
        views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
        &dummy_focus_traversable, &dummy_focus_traversable_view);
    EXPECT_EQ(header_view, prev_from_tab);
  }

  views::View* prev_from_header = focus_search.FindNextFocusableView(
      header_view, views::FocusSearch::SearchDirection::kBackwards,
      views::FocusSearch::TraversalDirection::kDown,
      views::FocusSearch::StartingViewPolicy::kSkipStartingView,
      views::FocusSearch::AnchoredDialogPolicy::kCanGoIntoAnchoredDialog,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
  EXPECT_EQ(unfocus_button, prev_from_header);

  model->ExitFocusMode();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabGroupViewTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });

class VerticalTabGroupViewBrowserTest : public TabGroupViewTestBase {};

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#define MAYBE_EditorBubbleOpensOnEditorBubbleButtonPress \
  EditorBubbleOpensOnEditorBubbleButtonPress
#else
#define MAYBE_EditorBubbleOpensOnEditorBubbleButtonPress \
  DISABLED_EditorBubbleOpensOnEditorBubbleButtonPress
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
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

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
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
  browser()->GetTabStripModel()->EnterFocusMode(group_id);
  RunScheduledLayouts();

  // In focus mode, group line should be hidden and tab should be aligned at x =
  // 0.
  EXPECT_FALSE(group_view->group_line()->GetVisible());
  EXPECT_EQ(tab->x(), 0);

  // Unfocus the group.
  browser()->GetTabStripModel()->ExitFocusMode();
  RunScheduledLayouts();

  // Group line should be restored and tab indented again.
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(tab->x(), TabGroupView::kTabLeadingPadding);
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       HeaderVisibilityInNonGroupFocusMode) {
  AppendTab();
  AppendTab();

  // Enter non-group focus mode, which creates an ephemeral tab group.
  browser()->GetTabStripModel()->EnterFocusMode({1});
  RunScheduledLayouts();

  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  TabGroupView* group_view =
      views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_TRUE(group_view);
  TabGroupHeaderView* group_header = group_view->group_header();
  ASSERT_TRUE(group_header);

  // In non-group focus mode, the collapse (caret) icon should be hidden and
  // the 3-dot editor bubble button should be visible.
  EXPECT_FALSE(group_header->collapse_icon_for_testing()->GetVisible());
  EXPECT_TRUE(group_header->editor_bubble_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       EditorBubbleButtonNotVisibleOnHoverWhenCollapsed) {
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();

  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  ASSERT_TRUE(group_node);
  TabGroupView* group_view =
      views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_TRUE(group_view);
  TabGroupHeaderView* group_header = group_view->group_header();
  ASSERT_TRUE(group_header);

  // Collapse vertical tabs.
  vertical_tab_strip_state_controller()->RequestCollapse(true);
  RunScheduledLayouts();
  ASSERT_TRUE(vertical_tab_strip_state_controller()->IsCollapsed());

  // Enter focus mode.
  browser()->GetTabStripModel()->EnterFocusMode(group_id);
  RunScheduledLayouts();
  EXPECT_TRUE(group_header->editor_bubble_button()->GetVisible());

  // Exit focus mode.
  browser()->GetTabStripModel()->ExitFocusMode();
  RunScheduledLayouts();

  // Hover over the tab group header and verify the 3-dot button is not visible.
  ui::test::EventGenerator event_generator(
      views::GetRootWindow(group_header->GetWidget()),
      browser()->GetWindow()->GetNativeWindow());
  event_generator.MoveMouseTo(group_header->GetBoundsInScreen().CenterPoint());
  ui::MouseEvent mouse_enter_event(ui::EventType::kMouseEntered, gfx::Point(),
                                   gfx::Point(), ui::EventTimeForNow(), 0, 0);
  group_header->OnMouseEntered(mouse_enter_event);

  EXPECT_FALSE(group_header->editor_bubble_button()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeEnterAndExitAnimation) {
  DisableFocusAnimations();
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();
  TabGroupView* const group_view = GetTabGroupView(group_id);
  ASSERT_NE(group_view, nullptr);
  const TabGroupUnfocusButton* const unfocus_button =
      group_view->unfocus_header();
  TabGroupLineView* const group_line = group_view->group_line();
  views::View* const grouped_tab_view =
      group_view->collection_node()->children()[0]->view();
  views::View* const unrelated_tab_view =
      unpinned_collection_node()->children()[0]->view();
  const int unrelated_tab_height = unrelated_tab_view->height();
  ASSERT_GT(unrelated_tab_height, 0);

  EXPECT_FALSE(unfocus_button->GetVisible());
  EXPECT_TRUE(group_line->GetVisible());
  EXPECT_EQ(grouped_tab_view->x(), TabGroupView::kTabLeadingPadding);
  EXPECT_LE(group_line->bounds().right(), grouped_tab_view->x());

  EnableFocusAnimations();
  TabStripModel* const model = browser()->GetTabStripModel();
  const BrowserAnimationController* const anim_controller =
      BrowserAnimationController::From(browser());

  // Enter: the focus motion drives the layouts directly, so no tab layout
  // animation runs alongside it. The Unfocus button grows in from zero height
  // while the group line stays to the left of the grouped tabs as it fades out,
  // and the unrelated tab shrinks away rather than disappearing at once.
  model->EnterFocusMode(group_id);
  RunScheduledLayouts();
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_FALSE(IsAnyTabLayoutAnimating());
  EXPECT_TRUE(group_line->GetVisible());
  EXPECT_LE(group_line->bounds().right(), grouped_tab_view->x());
  EXPECT_TRUE(unrelated_tab_view->GetVisible());

  AdvanceFocusMotion(base::Milliseconds(150));
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_TRUE(unfocus_button->GetVisible());
  EXPECT_GT(unfocus_button->height(), 0);
  EXPECT_LT(group_line->opacity(), 1.0f);
  EXPECT_TRUE(group_line->GetVisible());
  EXPECT_LE(group_line->bounds().right(), grouped_tab_view->x());
  EXPECT_LT(unrelated_tab_view->height(), unrelated_tab_height);
  EXPECT_FALSE(IsAnyTabLayoutAnimating());

  FinishFocusMotion();
  EXPECT_TRUE(unfocus_button->GetVisible());
  EXPECT_FALSE(group_line->GetVisible());
  EXPECT_EQ(grouped_tab_view->x(), 0);
  EXPECT_FALSE(unrelated_tab_view->GetVisible());

  // Exit: the Unfocus button stays visible while it shrinks out and the
  // unrelated tab grows back in.
  model->ExitFocusMode();
  RunScheduledLayouts();
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_FALSE(IsAnyTabLayoutAnimating());
  EXPECT_TRUE(unfocus_button->GetVisible());
  EXPECT_LE(group_line->bounds().right(), grouped_tab_view->x());

  AdvanceFocusMotion(base::Milliseconds(125));
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_TRUE(unrelated_tab_view->GetVisible());
  EXPECT_GT(unrelated_tab_view->height(), 0);
  EXPECT_LT(unrelated_tab_view->height(), unrelated_tab_height);
  EXPECT_LE(group_line->bounds().right(), grouped_tab_view->x());

  FinishFocusMotion();
  EXPECT_FALSE(unfocus_button->GetVisible());
  EXPECT_TRUE(group_line->GetVisible());
  EXPECT_EQ(grouped_tab_view->x(), TabGroupView::kTabLeadingPadding);
  EXPECT_LE(group_line->bounds().right(), grouped_tab_view->x());
  EXPECT_TRUE(unrelated_tab_view->GetVisible());
  EXPECT_EQ(unrelated_tab_view->height(), unrelated_tab_height);
}

IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeSwitchGroupsMidAnimation) {
  DisableFocusAnimations();
  AppendTab();
  AppendTab();
  AppendTab();
  TabStripModel* const model = browser()->GetTabStripModel();
  const tab_groups::TabGroupId group_a = model->AddToNewGroup({1});
  const tab_groups::TabGroupId group_b = model->AddToNewGroup({3});
  ActivateTab(model->GetTabAtIndex(2));

  TabGroupView* const group_view_a = GetTabGroupView(group_a);
  TabGroupView* const group_view_b = GetTabGroupView(group_b);
  ASSERT_NE(group_view_a, nullptr);
  ASSERT_NE(group_view_b, nullptr);
  views::View* const unrelated_tab_view =
      unpinned_collection_node()->children()[0]->view();

  EnableFocusAnimations();
  model->EnterFocusMode(group_a);
  AdvanceFocusMotion(base::Milliseconds(150));
  ASSERT_TRUE(group_view_a->unfocus_header()->GetVisible());
  ASSERT_GT(group_view_a->unfocus_header()->height(), 0);

  model->EnterFocusMode(group_b);
  RunScheduledLayouts();

  EXPECT_FALSE(unrelated_tab_view->GetVisible());
  EXPECT_TRUE(group_view_a->unfocus_header()->GetVisible());
  EXPECT_FALSE(group_view_a->group_line()->GetVisible());

  // The enter curve is front-loaded, so kFocusSwitchHandoffProgress (0.75) is
  // reached around 50 ms into the 300 ms motion. Before the handoff the
  // outgoing group holds the top of the container and slides out.
  AdvanceFocusMotion(base::Milliseconds(25));
  EXPECT_TRUE(group_view_a->GetVisible());
  EXPECT_FALSE(group_view_b->GetVisible());
  EXPECT_LT(group_view_a->GetFocusSwitchSlideOffset(), 0);
  EXPECT_FALSE(unrelated_tab_view->GetVisible());
  EXPECT_EQ(unrelated_tab_view->layer(), nullptr);
  ASSERT_NE(group_view_a->layer(), nullptr);
  EXPECT_GT(group_view_a->layer()->opacity(), 0.0f);
  EXPECT_LT(group_view_a->layer()->opacity(), 1.0f);
  EXPECT_EQ(group_view_b->layer(), nullptr);
  EXPECT_EQ(group_view_a->y(), 0);

  // After the handoff (100 ms in) the incoming group takes its place and
  // slides in.
  AdvanceFocusMotion(base::Milliseconds(75));
  EXPECT_FALSE(group_view_a->GetVisible());
  EXPECT_TRUE(group_view_b->GetVisible());
  EXPECT_GT(group_view_b->GetFocusSwitchSlideOffset(), 0);
  EXPECT_FALSE(unrelated_tab_view->GetVisible());
  EXPECT_EQ(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(group_view_a->layer(), nullptr);
  ASSERT_NE(group_view_b->layer(), nullptr);
  EXPECT_GT(group_view_b->layer()->opacity(), 0.0f);
  EXPECT_LT(group_view_b->layer()->opacity(), 1.0f);
  EXPECT_EQ(group_view_b->y(), 0);
  EXPECT_TRUE(group_view_b->unfocus_header()->GetVisible());
  EXPECT_FALSE(group_view_b->group_line()->GetVisible());

  FinishFocusMotion();

  EXPECT_FALSE(unrelated_tab_view->GetVisible());
  EXPECT_FALSE(group_view_a->GetVisible());
  EXPECT_FALSE(group_view_a->unfocus_header()->GetVisible());
  EXPECT_TRUE(group_view_b->GetVisible());
  EXPECT_TRUE(group_view_b->unfocus_header()->GetVisible());
  EXPECT_EQ(group_view_a->GetFocusSwitchSlideOffset(), 0);
  EXPECT_EQ(group_view_b->GetFocusSwitchSlideOffset(), 0);
  EXPECT_EQ(group_view_a->layer(), nullptr);
  EXPECT_EQ(group_view_b->layer(), nullptr);
}

// Exiting focus on an ephemeral (non-group focus) group defers dissolving the
// group in the model until the exit motion finishes, so the group view stays
// alive and shrinks both its unfocus button and header away while the
// unrelated tabs grow back.
IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeExitOfEphemeralGroup) {
  DisableFocusAnimations();
  AppendTab();
  AppendTab();
  TabStripModel* const model = browser()->GetTabStripModel();
  views::View* const unpinned_view = unpinned_collection_node()->view();
  views::View* const unrelated_tab_view =
      unpinned_collection_node()->children()[0]->view();
  const int unrelated_tab_height = unrelated_tab_view->height();
  ASSERT_GT(unrelated_tab_height, 0);
  const auto find_group_view = [&]() -> TabGroupView* {
    const auto it = std::ranges::find_if(
        unpinned_view->children(),
        [](views::View* v) { return views::IsViewClass<TabGroupView>(v); });
    return it == unpinned_view->children().end()
               ? nullptr
               : views::AsViewClass<TabGroupView>(*it);
  };

  EnableFocusAnimations();
  const BrowserAnimationController* const anim_controller =
      BrowserAnimationController::From(browser());
  const tab_groups::TabGroupId group_id = model->EnterFocusMode({1, 2});
  RunScheduledLayouts();
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  TabGroupView* const group_view = find_group_view();
  ASSERT_NE(group_view, nullptr);
  EXPECT_EQ(SkColorGetA(group_view->group_header()
                            ->title_label_for_testing()
                            ->GetEnabledColor()),
            0u);

  FinishFocusMotion();
  const int group_view_height = group_view->height();
  EXPECT_FALSE(unrelated_tab_view->GetVisible());
  EXPECT_EQ(SkColorGetA(group_view->group_header()
                            ->title_label_for_testing()
                            ->GetEnabledColor()),
            SK_AlphaOPAQUE);

  model->ExitFocusMode();
  RunScheduledLayouts();
  // The controller holds the group's removal lock while the exit animation
  // runs, so the group stays in the model, unfocused.
  EXPECT_TRUE(model->group_model()->ContainsTabGroup(group_id));
  EXPECT_EQ(model->GetFocusedGroup(), std::nullopt);
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_FALSE(IsAnyTabLayoutAnimating());
  EXPECT_EQ(find_group_view(), group_view);

  AdvanceFocusMotion(base::Milliseconds(125));
  EXPECT_TRUE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  ASSERT_EQ(find_group_view(), group_view);
  EXPECT_TRUE(group_view->GetVisible());
  EXPECT_GT(group_view->height(), 0);
  EXPECT_LT(group_view->height(), group_view_height);
  EXPECT_EQ(SkColorGetA(group_view->group_header()
                            ->title_label_for_testing()
                            ->GetEnabledColor()),
            0u);
  EXPECT_TRUE(unrelated_tab_view->GetVisible());

  FinishFocusMotion();
  // Dissolving the ephemeral group at the end of kExit must not kick off a
  // secondary layout animation.
  EXPECT_FALSE(IsAnyTabLayoutAnimating());
  EXPECT_FALSE(model->group_model()->ContainsTabGroup(group_id));
  EXPECT_EQ(find_group_view(), nullptr);
  EXPECT_EQ(unrelated_tab_view->height(), unrelated_tab_height);
  // The dissolved group's tabs were reparented into the container as the exit
  // motion ended; neither they nor the unrelated tab keep a fade layer.
  for (views::View* child : unpinned_view->children()) {
    EXPECT_EQ(child->layer(), nullptr);
  }
}

// With rich animations disabled (e.g. reduced motion), entering and exiting
// focus mode snap straight to their resting states without layering children.
IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeWithoutRichAnimations) {
  DisableFocusAnimations();
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();
  TabGroupView* const group_view = GetTabGroupView(group_id);
  ASSERT_NE(group_view, nullptr);
  views::View* const unpinned_view = unpinned_collection_node()->view();
  TabStripModel* const model = browser()->GetTabStripModel();

  const base::CallbackListSubscription sub =
      BrowserAnimationController::From(browser())->Subscribe(
          FocusModeAnimations::kFocusMode,
          base::BindRepeating(
              [](views::View* unpinned_view, const BrowserAnimationController*,
                 BrowserAnimationUpdate) {
                for (views::View* child : unpinned_view->children()) {
                  EXPECT_EQ(child->layer(), nullptr);
                }
              },
              unpinned_view));

  model->EnterFocusMode(group_id);
  RunScheduledLayouts();
  FinishFocusMotion();

  EXPECT_TRUE(group_view->unfocus_header()->GetVisible());
  EXPECT_FALSE(group_view->group_line()->GetVisible());
  for (views::View* child : unpinned_view->children()) {
    EXPECT_EQ(child->GetVisible(), child == group_view);
    EXPECT_EQ(child->layer(), nullptr);
  }

  model->ExitFocusMode();
  RunScheduledLayouts();
  FinishFocusMotion();

  EXPECT_FALSE(group_view->unfocus_header()->GetVisible());
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  for (views::View* child : unpinned_view->children()) {
    EXPECT_TRUE(child->GetVisible());
    EXPECT_EQ(child->layer(), nullptr);
  }
}

// Closing the focused group while its enter transition is still running tears
// the group view down cleanly and leaves no focus motion behind.
IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeGroupClosedMidEnterAnimation) {
  DisableFocusAnimations();
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();
  ASSERT_NE(GetTabGroupView(group_id), nullptr);
  EnableFocusAnimations();

  TabStripModel* const model = browser()->GetTabStripModel();
  model->EnterFocusMode(group_id);
  AdvanceFocusMotion(base::Milliseconds(150));
  ASSERT_TRUE(BrowserAnimationController::From(browser())->IsAnimating(
      FocusModeAnimations::kFocusMode));

  model->CloseAllTabsInGroup(group_id);
  RunScheduledLayouts();
  FinishFocusMotion();

  EXPECT_FALSE(model->group_model()->ContainsTabGroup(group_id));
  EXPECT_FALSE(model->GetFocusedGroup().has_value());
  EXPECT_EQ(unpinned_collection_node()->GetChildNodeOfType(
                TabCollectionNode::Type::GROUP),
            nullptr);
  for (views::View* child : unpinned_collection_node()->view()->children()) {
    EXPECT_TRUE(child->GetVisible());
    EXPECT_EQ(child->layer(), nullptr);
  }
}

// While a focus transition runs, the container's unrelated children paint to
// layers that fade with `FocusModeAnimations::kUnrelatedTabsOpacity`; the
// focused group never does. The layers are released once the motion ends.
IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeFadesUnrelatedChildren) {
  DisableFocusAnimations();
  tab_groups::TabGroupId group_id = CreateInactiveTabGroup();
  TabGroupView* const group_view = GetTabGroupView(group_id);
  ASSERT_NE(group_view, nullptr);
  views::View* const unrelated_tab_view =
      unpinned_collection_node()->children()[0]->view();
  ASSERT_EQ(unrelated_tab_view->layer(), nullptr);

  EnableFocusAnimations();
  auto animation_container = base::MakeRefCounted<gfx::AnimationContainer>();
  BrowserAnimationController* const anim_controller =
      BrowserAnimationController::From(browser());
  anim_controller->SetAnimationContainerForTesting(
      FocusModeAnimations::kFocusMode, animation_container.get());
  TabStripModel* const model = browser()->GetTabStripModel();

  // Enter: the unrelated tab is layered on the first progress step and fades
  // out over [0, 100] ms while the 300 ms enter motion shrinks it away.
  model->EnterFocusMode(group_id);
  gfx::AnimationContainerTestApi enter_test_api(animation_container.get());
  RunScheduledLayouts();
  EXPECT_EQ(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(group_view->layer(), nullptr);

  enter_test_api.IncrementTime(base::Milliseconds(50));
  RunScheduledLayouts();
  ASSERT_NE(unrelated_tab_view->layer(), nullptr);
  EXPECT_GT(unrelated_tab_view->layer()->opacity(), 0.0f);
  EXPECT_LT(unrelated_tab_view->layer()->opacity(), 1.0f);
  EXPECT_EQ(group_view->layer(), nullptr);

  enter_test_api.IncrementTime(base::Milliseconds(100));
  RunScheduledLayouts();
  ASSERT_NE(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(unrelated_tab_view->layer()->opacity(), 0.0f);
  EXPECT_EQ(group_view->layer(), nullptr);

  enter_test_api.IncrementTime(base::Milliseconds(200));
  RunScheduledLayouts();
  EXPECT_FALSE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_EQ(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(group_view->layer(), nullptr);

  // Exit: the unrelated tab is layered at opacity 0 while growing back, then
  // fades in over [167, 250] ms and releases its layer when the motion ends.
  model->ExitFocusMode();
  gfx::AnimationContainerTestApi exit_test_api(animation_container.get());
  RunScheduledLayouts();
  EXPECT_EQ(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(group_view->layer(), nullptr);

  exit_test_api.IncrementTime(base::Milliseconds(100));
  RunScheduledLayouts();
  ASSERT_NE(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(unrelated_tab_view->layer()->opacity(), 0.0f);
  EXPECT_TRUE(unrelated_tab_view->GetVisible());
  EXPECT_EQ(group_view->layer(), nullptr);

  exit_test_api.IncrementTime(base::Milliseconds(100));
  RunScheduledLayouts();
  ASSERT_NE(unrelated_tab_view->layer(), nullptr);
  EXPECT_GT(unrelated_tab_view->layer()->opacity(), 0.0f);
  EXPECT_LT(unrelated_tab_view->layer()->opacity(), 1.0f);
  EXPECT_EQ(group_view->layer(), nullptr);

  exit_test_api.IncrementTime(base::Milliseconds(100));
  RunScheduledLayouts();
  EXPECT_FALSE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_EQ(unrelated_tab_view->layer(), nullptr);
  EXPECT_EQ(group_view->layer(), nullptr);
  EXPECT_TRUE(unrelated_tab_view->GetVisible());

  anim_controller->SetAnimationContainerForTesting(
      FocusModeAnimations::kFocusMode, nullptr);
}

// A child that leaves the container mid-transition (here, an unrelated tab
// pulled into the focused group) gives up its fade layer rather than carrying
// it into its new parent.
IN_PROC_BROWSER_TEST_F(VerticalTabGroupViewBrowserTest,
                       FocusModeFadeReleasedWhenChildLeavesContainer) {
  DisableFocusAnimations();
  AppendTab();
  AppendTab();
  TabStripModel* const model = browser()->GetTabStripModel();
  const tab_groups::TabGroupId group_id = model->AddToNewGroup({2});
  ActivateTab(model->GetTabAtIndex(0));
  TabGroupView* const group_view = GetTabGroupView(group_id);
  ASSERT_NE(group_view, nullptr);
  views::View* const unpinned_view = unpinned_collection_node()->view();
  views::View* const tab_view_1 =
      unpinned_collection_node()->children()[1]->view();
  ASSERT_TRUE(views::IsViewClass<TabView>(tab_view_1));

  EnableFocusAnimations();
  auto animation_container = base::MakeRefCounted<gfx::AnimationContainer>();
  BrowserAnimationController* const anim_controller =
      BrowserAnimationController::From(browser());
  anim_controller->SetAnimationContainerForTesting(
      FocusModeAnimations::kFocusMode, animation_container.get());

  model->EnterFocusMode(group_id);
  gfx::AnimationContainerTestApi test_api(animation_container.get());
  test_api.IncrementTime(base::Milliseconds(50));
  RunScheduledLayouts();
  ASSERT_NE(tab_view_1->layer(), nullptr);
  EXPECT_LT(tab_view_1->layer()->opacity(), 1.0f);

  // Move tab 1 into the focused group while the enter motion is still running.
  model->AddToExistingGroup({1}, group_id);
  RunScheduledLayouts();
  EXPECT_TRUE(group_view->Contains(tab_view_1));
  EXPECT_EQ(tab_view_1->layer(), nullptr);

  test_api.IncrementTime(base::Milliseconds(300));
  RunScheduledLayouts();
  EXPECT_FALSE(anim_controller->IsAnimating(FocusModeAnimations::kFocusMode));
  EXPECT_EQ(tab_view_1->layer(), nullptr);
  for (views::View* child : unpinned_view->children()) {
    EXPECT_EQ(child->layer(), nullptr);
  }

  anim_controller->SetAnimationContainerForTesting(
      FocusModeAnimations::kFocusMode, nullptr);
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

  void ResetAnimationMode() { animation_mode_reset_.reset(); }

  TabStripView* tab_strip_view() {
    return views::AsViewClass<TabStripView>(
        views::AsViewClass<BaseTabStripRegionView>(
            BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view())
            ->GetTabStripView());
  }

  TabGroupView* SetUpTrailingGroup(int ungrouped_tabs,
                                   std::optional<int> window_width = {}) {
    while (GetTabStripModel()->count() < ungrouped_tabs + 2) {
      AppendTab();
    }
    GetTabStripModel()->ActivateTabAt(0);
    tab_groups::TabGroupId group_id =
        GetTabStripModel()->AddToNewGroup({ungrouped_tabs, ungrouped_tabs + 1});
    auto* widget = tab_strip_view()->GetWidget();
    if (window_width.has_value()) {
      gfx::Rect bounds = widget->GetWindowBoundsInScreen();
      bounds.set_width(*window_width);
      widget->SetBounds(bounds);
    }
    widget->LayoutRootViewIfNecessary();
    views::View* group_header = BrowserView::GetBrowserViewForBrowser(browser())
                                    ->tab_strip_view()
                                    ->GetTabGroupAnchorView(group_id);
    return group_header
               ? views::AsViewClass<TabGroupView>(group_header->parent())
               : nullptr;
  }

  void ToggleCollapse(TabGroupView* group_view,
                      bool expected_collapsed,
                      ToggleTabGroupCollapsedStateOrigin origin =
                          ToggleTabGroupCollapsedStateOrigin::kMouse) {
    group_view->ToggleCollapsedState(origin);
    EXPECT_EQ(group_view->IsCollapsed(), expected_collapsed);
    tab_strip_view()->GetWidget()->LayoutRootViewIfNecessary();
  }

  void ExpectTrailingPadding(TabGroupView* group_view) {
    const int expected_padding = TabGroupStyle::GetTrailingGroupHeaderPadding();
    EXPECT_EQ(tab_strip_view()->GetUnpinnedTabsContainer()->width() -
                  group_view->bounds().right(),
              expected_padding);
    EXPECT_EQ(BrowserElementsViews::From(browser())
                      ->GetViewAs<views::View>(kNewTabButtonElementId)
                      ->GetBoundsInScreen()
                      .x() -
                  group_view->group_header()->GetBoundsInScreen().right(),
              expected_padding);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;
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

  const views::SizeBounds original_available_size =
      tab_strip_view->GetAvailableSize(
          tab_strip_view->unpinned_tabs_scroll_view());
  ASSERT_TRUE(original_available_size.width().is_bounded());
  EXPECT_GT(original_available_size.width().value(), 0);

  const int original_group_target_width =
      GetTabStripViewTargetBounds(group_view).width();
  EXPECT_GT(original_group_target_width, 0);

  // Perform unconstrained layout measurement queries on TabStripView and
  // UnpinnedTabContainerView.
  tab_strip_view->GetPreferredSize(views::SizeBounds());
  unpinned_container->GetPreferredSize(views::SizeBounds());

  // Verify that unbounded size queries did not clear or overwrite the bounded
  // available space or group target width.
  const views::SizeBounds available_size = tab_strip_view->GetAvailableSize(
      tab_strip_view->unpinned_tabs_scroll_view());
  EXPECT_EQ(available_size.width(), original_available_size.width());

  EXPECT_EQ(GetTabStripViewTargetBounds(group_view).width(),
            original_group_target_width);
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
  const views::SizeBounds original_available_size =
      tab_strip_view->GetAvailableSize(
          tab_strip_view->unpinned_tabs_scroll_view());
  ASSERT_TRUE(original_available_size.width().is_bounded());
  const int original_group_target_width =
      GetTabStripViewTargetBounds(group_view).width();
  ASSERT_GT(original_available_size.width().value(), 0);
  ASSERT_GT(original_group_target_width, 0);

  // Perform zero-sized layout measurement queries on TabStripView and
  // UnpinnedTabContainerView (e.g. minimum size calculations).
  tab_strip_view->GetMinimumSize();
  tab_strip_view->GetPreferredSize(views::SizeBounds(0, 0));
  unpinned_container->GetMinimumSize();
  unpinned_container->GetPreferredSize(views::SizeBounds(0, 0));

  const views::SizeBounds available_size = tab_strip_view->GetAvailableSize(
      tab_strip_view->unpinned_tabs_scroll_view());
  EXPECT_EQ(available_size.width(), original_available_size.width());
  EXPECT_EQ(GetTabStripViewTargetBounds(group_view).width(),
            original_group_target_width);
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

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       HeaderBoundsAndSizingMatchesLegacy) {
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = GetTabStripModel()->AddToNewGroup({1, 2});

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

  auto* header_view = group_view->group_header();
  ASSERT_NE(header_view, nullptr);

  // Unnamed group header chip height and width should match legacy chip size
  // (20x20).
  EXPECT_EQ(header_view->bounds().height(), TabGroupStyle::GetEmptyChipSize());
  EXPECT_EQ(header_view->bounds().width(), TabGroupStyle::GetEmptyChipSize());
  // Header should be vertically centered.
  EXPECT_EQ(header_view->bounds().y(), TabGroupStyle::GetTitleChipOffset().y());

  // Set a title.
  GetTabStripModel()->ChangeTabGroupVisuals(
      group_id,
      tab_groups::TabGroupVisualData(u"Group Title",
                                     tab_groups::TabGroupColorId::kBlue),
      false);
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  // Named group header chip height should remain 20 DIPs, vertically centered,
  // and width should expand to fit the label.
  EXPECT_EQ(header_view->bounds().height(), TabGroupStyle::GetEmptyChipSize());
  EXPECT_GT(header_view->bounds().width(), TabGroupStyle::GetEmptyChipSize());
  EXPECT_EQ(header_view->bounds().y(), TabGroupStyle::GetTitleChipOffset().y());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       PaddingBetweenCollapsedHeadersMatchesLegacy) {
  // Tabs layout:
  // Tab 0: ungrouped (keeps active tab outside collapsed groups)
  // Tab 1, 2: Group 1
  // Tab 3, 4: Group 2
  AppendTab();
  AppendTab();
  AppendTab();
  AppendTab();
  GetTabStripModel()->ActivateTabAt(0);

  tab_groups::TabGroupId group_id1 = GetTabStripModel()->AddToNewGroup({1, 2});
  tab_groups::TabGroupId group_id2 = GetTabStripModel()->AddToNewGroup({3, 4});

  auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
      BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  ASSERT_NE(base_region_view, nullptr);

  auto* tab_strip_view =
      views::AsViewClass<TabStripView>(base_region_view->GetTabStripView());
  ASSERT_NE(tab_strip_view, nullptr);

  auto* group_node1 =
      root_node()->GetNodeForHandle(GetTabStripModel()
                                        ->group_model()
                                        ->GetTabGroup(group_id1)
                                        ->GetCollectionHandle());
  ASSERT_NE(group_node1, nullptr);
  auto* group_view1 = views::AsViewClass<TabGroupView>(group_node1->view());
  ASSERT_NE(group_view1, nullptr);

  auto* group_node2 =
      root_node()->GetNodeForHandle(GetTabStripModel()
                                        ->group_model()
                                        ->GetTabGroup(group_id2)
                                        ->GetCollectionHandle());
  ASSERT_NE(group_node2, nullptr);
  auto* group_view2 = views::AsViewClass<TabGroupView>(group_node2->view());
  ASSERT_NE(group_view2, nullptr);

  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  // Collapse both groups.
  group_view1->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  group_view2->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return group_view1->IsCollapsed() && group_view2->IsCollapsed();
  }));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  const int expected_padding =
      TabGroupStyle::GetPaddingBetweenCollapsedHeaders();
  EXPECT_EQ(expected_padding, 14);

  // Verify padding between group views.
  EXPECT_EQ(group_view2->bounds().x() - group_view1->bounds().right(),
            expected_padding);

  // Verify visual padding between header views in screen coordinates.
  auto* header_view1 = group_view1->group_header();
  auto* header_view2 = group_view2->group_header();
  ASSERT_NE(header_view1, nullptr);
  ASSERT_NE(header_view2, nullptr);
  EXPECT_EQ(header_view2->GetBoundsInScreen().x() -
                header_view1->GetBoundsInScreen().right(),
            expected_padding);

  // When group 2 is uncollapsed, its leading child is still header 2, so the
  // padding between header chips should remain 14 DIPs.
  group_view2->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !group_view2->IsCollapsed(); }));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(group_view2->bounds().x() - group_view1->bounds().right(),
            expected_padding);
  EXPECT_EQ(header_view2->GetBoundsInScreen().x() -
                header_view1->GetBoundsInScreen().right(),
            expected_padding);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       FirstGroupHeaderPaddingWithPinnedTab) {
  // Tabs layout:
  // Tab 0: Pinned tab
  // Tab 1, 2: Group 1
  AppendTab();
  AppendTab();
  GetTabStripModel()->SetTabPinned(0, true);
  tab_groups::TabGroupId group_id = GetTabStripModel()->AddToNewGroup({1, 2});

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

  // In the unpinned container, group view is the first child and starts at
  // TabGroupStyle::GetLeadingGroupHeaderPadding().
  EXPECT_EQ(group_view->bounds().x(),
            TabGroupStyle::GetLeadingGroupHeaderPadding());

  auto* pinned_node =
      root_node()->GetChildNodeOfType(TabCollectionNode::Type::PINNED);
  ASSERT_NE(pinned_node, nullptr);
  ASSERT_GE(pinned_node->children().size(), 1u);
  auto* pinned_tab_view =
      views::AsViewClass<TabView>(pinned_node->children()[0]->view());
  ASSERT_NE(pinned_tab_view, nullptr);

  auto* header_view = group_view->group_header();
  ASSERT_NE(header_view, nullptr);

  // Visually, the header overlaps the pinned tab by
  // TabGroupStyle::GetTabGroupOverlapAdjustment() (2 DIPs).
  const int header_overlap = TabGroupStyle::GetTabGroupOverlapAdjustment();
  EXPECT_EQ(pinned_tab_view->GetBoundsInScreen().right() -
                header_view->GetBoundsInScreen().x(),
            header_overlap);
}

IN_PROC_BROWSER_TEST_F(
    HorizontalTabGroupViewBrowserTest,
    GroupLineRemainsVisibleAndShrinksDuringCollapseAnimation) {
  ResetAnimationMode();
  auto animation_mode_reset = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
  gfx::ScopedAnimationDurationScaleMode duration_scale_mode(
      gfx::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  AppendTab();
  AppendTab();
  GetTabStripModel()->AddToNewGroup({1, 2});
  GetTabStripModel()->ActivateTabAt(0);

  auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
      BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  ASSERT_NE(base_region_view, nullptr);
  auto* controller = base_region_view->GetTabStripCollectionController();
  ASSERT_NE(controller, nullptr);
  auto* widget = base_region_view->GetWidget();

  controller->animation_coordinator().AnimationEnded(nullptr);
  widget->LayoutRootViewIfNecessary();

  auto* group_node = root_node()
                         ->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED)
                         ->GetChildNodeOfType(TabCollectionNode::Type::GROUP);
  ASSERT_NE(group_node, nullptr);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());
  ASSERT_NE(group_view, nullptr);
  auto* last_grouped_tab = group_node->children()[1]->view();

  const int expanded_width = group_view->width();
  const int header_width = group_view->group_header()->width();
  ASSERT_GT(expanded_width, header_width);
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(group_view->group_line()->width(), expanded_width);

  group_view->ToggleCollapsedState(ToggleTabGroupCollapsedStateOrigin::kMouse);
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(group_view->IsCollapsed());
  EXPECT_TRUE(TabCollectionAnimatingLayoutManager::IsViewAnimating(group_view));
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(group_view->group_line()->width(), group_view->width());

  gfx::SlideAnimation half_progress(nullptr);
  half_progress.Reset(0.5);
  controller->animation_coordinator().AnimationProgressed(&half_progress);
  widget->LayoutRootViewIfNecessary();
  EXPECT_LT(group_view->width(), expanded_width);
  EXPECT_GT(group_view->width(), header_width);
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_NEAR(group_view->group_line()->width(), group_view->width(), 1);
  EXPECT_LE(last_grouped_tab->bounds().right(), group_view->width() + 1);

  controller->animation_coordinator().AnimationEnded(nullptr);
  widget->LayoutRootViewIfNecessary();
  EXPECT_EQ(group_view->width(), header_width);
  EXPECT_FALSE(group_view->group_line()->GetVisible());
  EXPECT_FALSE(last_grouped_tab->GetVisible());

  // Expanding by activating a grouped tab starts that active tab at 0 width;
  // group_line must stay visible without painting an overline arch past
  // header_width.
  GetTabStripModel()->ActivateTabAt(1);
  widget->LayoutRootViewIfNecessary();
  EXPECT_FALSE(group_view->IsCollapsed());
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(group_view->group_line()->width(), group_view->width());
  gfx::Canvas canvas(gfx::Size(expanded_width, group_view->height()), 1.0f,
                     false);
  group_view->group_line()->OnPaint(&canvas);
  EXPECT_EQ(canvas.GetBitmap().getColor(header_width + 2, 1),
            SK_ColorTRANSPARENT);

  controller->animation_coordinator().AnimationProgressed(&half_progress);
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_NEAR(group_view->group_line()->width(), group_view->width(), 1);

  controller->animation_coordinator().AnimationEnded(nullptr);
  widget->LayoutRootViewIfNecessary();
  EXPECT_TRUE(group_view->group_line()->GetVisible());
  EXPECT_EQ(group_view->group_line()->width(), expanded_width);
}

IN_PROC_BROWSER_TEST_F(
    HorizontalTabGroupViewBrowserTest,
    CollapsedTrailingGroupHeaderHasPaddingBeforeNewTabButton) {
  auto* group_view = SetUpTrailingGroup(/*ungrouped_tabs=*/1);
  auto* unpinned_container = tab_strip_view()->GetUnpinnedTabsContainer();
  ASSERT_NE(group_view, nullptr);
  ASSERT_FALSE(group_view->IsCollapsed());
  EXPECT_EQ(unpinned_container->width(), group_view->bounds().right());

  ToggleCollapse(group_view, /*expected_collapsed=*/true);

  ExpectTrailingPadding(group_view);

  ToggleCollapse(group_view, /*expected_collapsed=*/false);

  EXPECT_EQ(unpinned_container->width(), group_view->bounds().right());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       CollapsingConstrainedGroupMaintainsPrecedingTabWidths) {
  auto* group_view =
      SetUpTrailingGroup(/*ungrouped_tabs=*/5, /*window_width=*/800);
  auto* unpinned_node =
      root_node()->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  ASSERT_NE(group_view, nullptr);
  auto* first_tab_view =
      views::AsViewClass<TabView>(unpinned_node->children().at(0)->view());
  EXPECT_LT(first_tab_view->width(),
            TabStyle::Get()->GetStandardWidth(/*is_split=*/false));
  ASSERT_FALSE(tab_strip_view()
                   ->unpinned_tabs_scroll_view()
                   ->GetDrawOverflowIndicator());
  const int tab_width_before = first_tab_view->width();
  const int group_x_before = group_view->bounds().x();

  ToggleCollapse(group_view, /*expected_collapsed=*/true);

  EXPECT_EQ(first_tab_view->width(), tab_width_before);
  EXPECT_EQ(group_view->bounds().x(), group_x_before);
  ExpectTrailingPadding(group_view);

  ToggleCollapse(group_view, /*expected_collapsed=*/false);
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/-1,
                   /*foreground=*/false);
  tab_strip_view()->GetWidget()->LayoutRootViewIfNecessary();
  const int non_trailing_tab_width = first_tab_view->width();
  const int non_trailing_group_x = group_view->bounds().x();

  ToggleCollapse(group_view, /*expected_collapsed=*/true);

  EXPECT_EQ(first_tab_view->width(), non_trailing_tab_width);
  EXPECT_EQ(group_view->bounds().x(), non_trailing_group_x);
}

IN_PROC_BROWSER_TEST_F(
    HorizontalTabGroupViewBrowserTest,
    ClosingTabsBeforeCollapsedTrailingGroupPreservesPadding) {
  auto* group_view =
      SetUpTrailingGroup(/*ungrouped_tabs=*/5, /*window_width=*/800);
  auto* unpinned_node =
      root_node()->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  ASSERT_NE(group_view, nullptr);
  ToggleCollapse(group_view, /*expected_collapsed=*/true,
                 ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  const std::vector<int> expected_widths = {
      unpinned_node->children().at(0)->view()->width(),
      unpinned_node->children().at(2)->view()->width(),
      unpinned_node->children().at(3)->view()->width(),
      unpinned_node->children().at(4)->view()->width(),
  };

  root_node()->GetController()->CloseTab(GetTabStripModel()->GetTabAtIndex(1),
                                         CloseTabSource::kFromMouse);
  tab_strip_view()->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_EQ(unpinned_node->children().size(), 5u);
  for (size_t i = 0; i < expected_widths.size(); ++i) {
    EXPECT_EQ(unpinned_node->children().at(i)->view()->width(),
              expected_widths[i]);
  }
  ExpectTrailingPadding(group_view);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabGroupViewBrowserTest,
                       TabAfterGroupIsTargetableInOverlapRegion) {
  // Tabs layout:
  // Tab 0, 1: Group (with Tab 1 active so the group has highest Z-order)
  // Tab 2: Ungrouped tab immediately after the group
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id = GetTabStripModel()->AddToNewGroup({0, 1});
  GetTabStripModel()->ActivateTabAt(1);

  auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
      BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  auto* tab_strip_view =
      views::AsViewClass<TabStripView>(base_region_view->GetTabStripView());
  auto* unpinned_container = tab_strip_view->GetUnpinnedTabsContainer();

  auto* group_node = root_node()->GetNodeForHandle(GetTabStripModel()
                                                       ->group_model()
                                                       ->GetTabGroup(group_id)
                                                       ->GetCollectionHandle());
  ASSERT_EQ(group_node->children().size(), 2u);
  auto* group_view = views::AsViewClass<TabGroupView>(group_node->view());

  auto* unpinned_node =
      root_node()->GetChildNodeOfType(TabCollectionNode::Type::UNPINNED);
  ASSERT_EQ(unpinned_node->children().size(), 2u);
  auto* ungrouped_tab_view =
      views::AsViewClass<TabView>(unpinned_node->children()[1]->view());
  auto* last_grouped_tab_view =
      views::AsViewClass<TabView>(group_node->children()[1]->view());

  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  gfx::RectF last_grouped_tab_hit_bounds = gfx::SkRectToRectF(
      last_grouped_tab_view->tab_styling()
          ->GetPath(TabStyle::PathType::kHitTest, /*scale=*/1.0f,
                    {.render_units = TabStyle::RenderUnits::kDips})
          .getBounds());
  views::View::ConvertRectToTarget(last_grouped_tab_view, unpinned_container,
                                   &last_grouped_tab_hit_bounds);
  const int hit_threshold_x =
      gfx::ToEnclosingRect(last_grouped_tab_hit_bounds).right();

  // A point 1 DIP past `last_grouped_tab_view`'s right hit-test edge still
  // lies inside `group_view->bounds()` and inside `ungrouped_tab_view`'s
  // hit-test mask, so it should target `ungrouped_tab_view`.
  const gfx::Point point_in_trailing_overlap(hit_threshold_x + 1,
                                             group_view->height() / 2);
  ASSERT_TRUE(group_view->bounds().Contains(point_in_trailing_overlap));
  EXPECT_EQ(
      unpinned_container->GetEventHandlerForPoint(point_in_trailing_overlap),
      ungrouped_tab_view);

  // A point 1 DIP inside `last_grouped_tab_view`'s right hit-test edge should
  // still target `last_grouped_tab_view`.
  const gfx::Point point_in_last_grouped_tab(hit_threshold_x - 1,
                                             group_view->height() / 2);
  EXPECT_EQ(
      unpinned_container->GetEventHandlerForPoint(point_in_last_grouped_tab),
      last_grouped_tab_view);
}

// TODO(crbug.com/490428062): Create Tests to Verify Focus Order of Tab Group
// Header w/ Editor Bubble Button.
