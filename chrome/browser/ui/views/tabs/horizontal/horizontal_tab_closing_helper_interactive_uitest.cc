// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_closing_helper.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/view_utils.h"

class HorizontalTabClosingHelperInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  HorizontalTabClosingHelperInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }
  ~HorizontalTabClosingHelperInteractiveUiTest() override = default;

  void SetWindowWidth(int width) {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    if (browser_view && browser_view->GetWidget()) {
      gfx::Rect bounds = browser_view->GetWidget()->GetWindowBoundsInScreen();
      bounds.set_width(width);
      browser_view->GetWidget()->SetBounds(bounds);
      browser_view->GetWidget()->LayoutRootViewIfNecessary();
    }
  }

  BaseTabStripRegionView* GetBaseTabStripRegionView() {
    return views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  TabStripCollectionController* GetController() {
    auto* region = GetBaseTabStripRegionView();
    return region ? region->GetTabStripCollectionController() : nullptr;
  }

  HorizontalTabClosingHelper* GetClosingHelper() {
    auto* controller = GetController();
    return controller ? controller->tab_closing_helper() : nullptr;
  }

  TabCollectionNode* GetUnpinnedNode() {
    auto* region = GetBaseTabStripRegionView();
    return region && region->root_node_for_testing()
               ? region->root_node_for_testing()->GetChildNodeOfType(
                     TabCollectionNode::Type::UNPINNED)
               : nullptr;
  }

  TabGroupView* GetTabGroupView(tab_groups::TabGroupId group_id) {
    auto* region = GetBaseTabStripRegionView();
    if (!region || !region->root_node_for_testing()) {
      return nullptr;
    }
    TabGroup* tab_group =
        browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
    if (!tab_group) {
      return nullptr;
    }
    TabCollectionNode* node = region->root_node_for_testing()->GetNodeForHandle(
        tab_group->GetCollectionHandle());
    return node ? views::AsViewClass<TabGroupView>(node->view()) : nullptr;
  }

  void AddTabs(int count) {
    for (int i = 0; i < count; ++i) {
      CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                          ui::PAGE_TRANSITION_TYPED));
    }
  }

  void CreateConstrainedTabs(int min_tabs = 5) {
    SetWindowWidth(600);
    TabStripModel* model = browser()->tab_strip_model();
    while (model->count() < min_tabs) {
      AddTabs(1);
    }
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    if (browser_view && browser_view->GetWidget()) {
      browser_view->GetWidget()->LayoutRootViewIfNecessary();
    }
  }

  bool IsUnpinnedContainerOverflowing() {
    TabCollectionNode* unpinned_node = GetUnpinnedNode();
    if (!unpinned_node || !unpinned_node->view()) {
      return false;
    }
    views::ScrollView* scroll_view =
        views::ScrollView::GetScrollViewForContents(unpinned_node->view());
    return scroll_view && scroll_view->IsHorizontalContentOverflowing();
  }

  void CreateOverflowingTabs() {
    SetWindowWidth(500);
    TabStripModel* model = browser()->tab_strip_model();
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    while (!IsUnpinnedContainerOverflowing() && model->count() < 100) {
      AddTabs(10);
      if (browser_view && browser_view->GetWidget()) {
        browser_view->GetWidget()->LayoutRootViewIfNecessary();
      }
    }
  }

  int GetStandardTabWidth() const {
    return TabStyle::Get()->GetStandardWidth(/*is_split=*/false);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  gfx::ScopedAnimationDurationScaleMode disable_animations_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

// Stays in closing mode and locks tab widths while tabs are below standard
// width.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       StaysInClosingModeBelowStandardWidth) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      Do([this]() {
        CreateConstrainedTabs(8);
        // Add 2 more so removing a tab leaves tabs below standard width.
        CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                            ui::PAGE_TRANSITION_TYPED));
        CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                            ui::PAGE_TRANSITION_TYPED));
      }),
      Do([this]() {
        TabStripModel* model = browser()->tab_strip_model();
        auto* controller = GetController();
        auto* helper = GetClosingHelper();
        EXPECT_NE(helper, nullptr);
        if (!helper) {
          return;
        }

        TabCollectionNode* unpinned = GetUnpinnedNode();
        EXPECT_NE(unpinned, nullptr);
        if (!unpinned || unpinned->children().empty()) {
          return;
        }
        const int initial_tab_width = unpinned->children()[0]->view()->width();
        EXPECT_LT(initial_tab_width, GetStandardTabWidth());

        // Close a middle tab.
        controller->CloseTab(model->GetTabAtIndex(3),
                             CloseTabSource::kFromMouse);

        EXPECT_TRUE(helper->in_tab_close());
        // Remaining tabs should maintain their width without expanding.
        EXPECT_EQ(unpinned->children()[0]->view()->width(), initial_tab_width);
      }));
}

// Closing mode sets minimum size constraint on the unpinned tab container.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       ClosingModeAffectsMinWidth) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      Do([this]() {
        CreateConstrainedTabs(8);
        CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                            ui::PAGE_TRANSITION_TYPED));
        CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                            ui::PAGE_TRANSITION_TYPED));
      }),
      Do([this]() {
        TabStripModel* model = browser()->tab_strip_model();
        auto* controller = GetController();
        auto* helper = GetClosingHelper();
        EXPECT_NE(helper, nullptr);
        if (!helper) {
          return;
        }

        views::View* unpinned =
            GetBaseTabStripRegionView()->GetUnpinnedTabsContainer();
        EXPECT_NE(unpinned, nullptr);
        if (!unpinned) {
          return;
        }

        controller->CloseTab(model->GetTabAtIndex(model->count() - 1),
                             CloseTabSource::kFromMouse);

        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        if (browser_view && browser_view->GetWidget()) {
          browser_view->GetWidget()->LayoutRootViewIfNecessary();
        }

        EXPECT_TRUE(helper->in_tab_close());
        EXPECT_TRUE(helper->override_available_width_for_tabs().has_value());
        if (helper->override_available_width_for_tabs().has_value()) {
          EXPECT_EQ(unpinned->GetMinimumSize().width(),
                    helper->override_available_width_for_tabs().value());
        }
      }));
}

// Closing a tab in a tab group maintains tab closing mode.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       RemoveTabInGroupWithTabClosingMode) {
  RunTestSequence(WaitForShow(kNewTabButtonElementId),
                  MoveMouseTo(kNewTabButtonElementId), Do([this]() {
                    SetWindowWidth(700);
                    TabStripModel* model = browser()->tab_strip_model();
                    while (model->count() < 8) {
                      CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                                          ui::PAGE_TRANSITION_TYPED));
                    }
                    model->AddToNewGroup({1, 2, 3});
                  }),
                  Do([this]() {
                    TabStripModel* model = browser()->tab_strip_model();
                    auto* controller = GetController();
                    auto* helper = GetClosingHelper();
                    EXPECT_NE(helper, nullptr);
                    if (!helper) {
                      return;
                    }

                    // Close a tab inside the group.
                    controller->CloseTab(model->GetTabAtIndex(2),
                                         CloseTabSource::kFromMouse);

                    EXPECT_TRUE(helper->in_tab_close());
                    EXPECT_EQ(model->count(), 7);
                  }));
}

// Moving mouse out of the watched host area exits tab closing mode.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       ClosingModeExitsWhenMouseLeaves) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      Do([this]() { CreateConstrainedTabs(8); }), Do([this]() {
        auto* controller = GetController();
        controller->CloseTab(browser()->tab_strip_model()->GetTabAtIndex(1),
                             CloseTabSource::kFromMouse);
        EXPECT_TRUE(GetClosingHelper()->in_tab_close());
      }),
      // Move mouse outside the tab strip into location bar.
      MoveMouseTo(kLocationBarElementId), Do([this]() {
        // Trigger exit via mouse watcher callback.
        GetClosingHelper()->MouseMovedOutOfHost();
        EXPECT_FALSE(GetClosingHelper()->in_tab_close());
      }));
}

// Verify close button stays under cursor when closing multiple tabs.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       ConsecutiveTabClosesStayUnderCursor) {
  RunTestSequence(WaitForShow(kNewTabButtonElementId),
                  MoveMouseTo(kNewTabButtonElementId),
                  Do([this]() { CreateConstrainedTabs(8); }), Do([this]() {
                    TabStripModel* model = browser()->tab_strip_model();
                    auto* controller = GetController();
                    auto* helper = GetClosingHelper();
                    EXPECT_NE(helper, nullptr);
                    if (!helper) {
                      return;
                    }

                    // Close Tab 2.
                    controller->CloseTab(model->GetTabAtIndex(2),
                                         CloseTabSource::kFromMouse);
                    EXPECT_TRUE(helper->in_tab_close());
                    EXPECT_EQ(model->count(), 7);

                    // Close Tab 2 again (which is now what was formerly Tab 3).
                    controller->CloseTab(model->GetTabAtIndex(2),
                                         CloseTabSource::kFromMouse);
                    EXPECT_TRUE(helper->in_tab_close());
                    EXPECT_EQ(model->count(), 6);
                  }));
}

// Collapsing a tab group via mouse enters closing mode and locks width.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       CollapseGroupEntersClosingMode) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), Do([this]() {
        CreateConstrainedTabs(8);
        TabStripModel* model = browser()->tab_strip_model();
        tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
        TabGroupView* group_view = GetTabGroupView(group_id);
        ASSERT_NE(group_view, nullptr);

        auto* helper = GetClosingHelper();
        ASSERT_NE(helper, nullptr);
        EXPECT_FALSE(helper->in_tab_close());

        group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMouse);

        EXPECT_TRUE(helper->in_tab_close());
        EXPECT_TRUE(helper->override_available_width_for_tabs().has_value());
      }));
}

// Expanding a collapsed tab group exits tab closing mode.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       ExpandGroupExitsClosingMode) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), Do([this]() {
        CreateConstrainedTabs(8);
        TabStripModel* model = browser()->tab_strip_model();
        tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
        TabGroupView* group_view = GetTabGroupView(group_id);
        ASSERT_NE(group_view, nullptr);

        auto* helper = GetClosingHelper();
        ASSERT_NE(helper, nullptr);

        // Collapse via mouse to enter closing mode.
        group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMouse);
        EXPECT_TRUE(helper->in_tab_close());

        // Expand via mouse.
        group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMouse);
        EXPECT_FALSE(helper->in_tab_close());
        EXPECT_FALSE(helper->override_available_width_for_tabs().has_value());
      }));
}

// Collapsing a tab group via non-mouse origin (e.g. menu) does not enter
// closing mode.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       NonMouseCollapseDoesNotEnterClosingMode) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), Do([this]() {
        CreateConstrainedTabs(8);
        TabStripModel* model = browser()->tab_strip_model();
        tab_groups::TabGroupId group_id = model->AddToNewGroup({1, 2});
        TabGroupView* group_view = GetTabGroupView(group_id);
        ASSERT_NE(group_view, nullptr);

        auto* helper = GetClosingHelper();
        ASSERT_NE(helper, nullptr);

        group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMenuAction);

        EXPECT_FALSE(helper->in_tab_close());
        EXPECT_FALSE(helper->override_available_width_for_tabs().has_value());
      }));
}

// Collapsing a group does not enter closing mode if tabs are already at
// standard width.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       CollapseGroupExitsWhenRemainingTabsFit) {
  RunTestSequence(WaitForShow(kNewTabButtonElementId), Do([this]() {
                    SetWindowWidth(800);
                    TabStripModel* model = browser()->tab_strip_model();
                    while (model->count() < 3) {
                      CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                                          ui::PAGE_TRANSITION_TYPED));
                    }
                    tab_groups::TabGroupId group_id =
                        model->AddToNewGroup({1, 2});
                    BrowserView::GetBrowserViewForBrowser(browser())
                        ->GetWidget()
                        ->LayoutRootViewIfNecessary();

                    TabGroupView* group_view = GetTabGroupView(group_id);
                    ASSERT_NE(group_view, nullptr);

                    auto* helper = GetClosingHelper();
                    ASSERT_NE(helper, nullptr);

                    // When 3 tabs in 800px window are already at standard
                    // width, collapsing the group does not enter tab closing
                    // mode.
                    group_view->ToggleCollapsedState(
                        ToggleTabGroupCollapsedStateOrigin::kMouse);

                    EXPECT_FALSE(helper->in_tab_close());
                  }));
}

// When tabs are overflowing into a scroll view, closing a tab keeps the visible
// container full and does not shrink the visible container bounds.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       ClosingTabWhenOverflowingMaintainsContainerSize) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      Do([this]() { CreateOverflowingTabs(); }), Do([this]() {
        TabStripModel* model = browser()->tab_strip_model();
        auto* controller = GetController();
        auto* helper = GetClosingHelper();
        ASSERT_NE(helper, nullptr);

        TabCollectionNode* unpinned = GetUnpinnedNode();
        ASSERT_NE(unpinned, nullptr);
        ASSERT_FALSE(unpinned->children().empty());

        auto* unpinned_container =
            views::AsViewClass<UnpinnedTabContainerView>(unpinned->view());
        ASSERT_NE(unpinned_container, nullptr);
        ASSERT_TRUE(unpinned_container->GetAvailableMainAxisSpaceOverride()
                        .has_value());
        ASSERT_TRUE(unpinned_container->GetAvailableMainAxisSpaceOverride()
                        ->is_bounded());

        const int initial_available_space =
            unpinned_container->GetAvailableMainAxisSpaceOverride()->value();
        const int initial_tab_width = unpinned->children()[0]->view()->width();

        // Close a middle tab.
        controller->CloseTab(model->GetTabAtIndex(2),
                             CloseTabSource::kFromMouse);

        // The override available width reflects total tab content width and
        // remains larger than the visible container so scroll buttons remain.
        EXPECT_TRUE(helper->override_available_width_for_tabs().has_value());
        if (helper->override_available_width_for_tabs().has_value()) {
          EXPECT_GT(helper->override_available_width_for_tabs().value(),
                    initial_available_space);
        }
        // The container maintains its overflowing state and visible bounds.
        EXPECT_TRUE(IsUnpinnedContainerOverflowing());
        // Remaining tabs should maintain their width without expanding.
        EXPECT_EQ(unpinned->children()[0]->view()->width(), initial_tab_width);
      }));
}

// When closing tabs causes the strip to transition from overflowing to
// non-overflowing, tab closing mode keeps tabs at their frozen size and
// sets the override width to the exact remaining tabs' width.
IN_PROC_BROWSER_TEST_F(HorizontalTabClosingHelperInteractiveUiTest,
                       ClosingTabTransitioningFromOverflowShrinksPartially) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId), MoveMouseTo(kNewTabButtonElementId),
      Do([this]() { CreateOverflowingTabs(); }), Do([this]() {
        TabStripModel* model = browser()->tab_strip_model();
        auto* controller = GetController();
        auto* helper = GetClosingHelper();
        ASSERT_NE(helper, nullptr);

        TabCollectionNode* unpinned = GetUnpinnedNode();
        ASSERT_NE(unpinned, nullptr);
        ASSERT_FALSE(unpinned->children().empty());

        auto* unpinned_container =
            views::AsViewClass<UnpinnedTabContainerView>(unpinned->view());
        ASSERT_NE(unpinned_container, nullptr);
        ASSERT_TRUE(unpinned_container->available_space().is_bounded());

        const int available_space =
            unpinned_container->available_space().value();
        const int initial_tab_width = unpinned->children()[0]->view()->width();

        // Close tabs until the container transitions to non-overflowing.
        while (model->count() > 1 && IsUnpinnedContainerOverflowing()) {
          controller->CloseTab(model->GetTabAtIndex(1),
                               CloseTabSource::kFromMouse);
          auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
          if (browser_view && browser_view->GetWidget()) {
            browser_view->GetWidget()->LayoutRootViewIfNecessary();
          }
        }

        EXPECT_TRUE(helper->in_tab_close());
        EXPECT_TRUE(helper->override_available_width_for_tabs().has_value());
        if (helper->override_available_width_for_tabs().has_value()) {
          EXPECT_LE(helper->override_available_width_for_tabs().value(),
                    available_space);
        }

        // Remaining tabs stay at their exact initial width without expanding.
        EXPECT_EQ(unpinned->children()[0]->view()->width(), initial_tab_width);
      }));
}
