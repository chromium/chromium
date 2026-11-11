// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_header.h"

#include "base/test/bind.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_attention_indicator.h"
#include "chrome/browser/ui/tabs/tab_group_features.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_header_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_group_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/polling_view_observer.h"

class TabGroupHeaderInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  TabGroupHeaderInteractiveUiTest() = default;

  ~TabGroupHeaderInteractiveUiTest() override = default;

  tabs::TabInterface* CreateTab() {
    auto index = browser()->GetTabStripModel()->count();
    CHECK(AddTabAtIndex(index, chrome::ChromeUINewTabPageURLAsGURL(),
                        ui::PAGE_TRANSITION_TYPED));
    auto* tab = browser()->GetTabStripModel()->GetTabAtIndex(index);
    CHECK(tab);
    return tab;
  }

  const tab_groups::TabGroupId CreateTabGroup(
      std::vector<tabs::TabInterface*> tabs) {
    std::vector<int> tab_indices = {};
    for (auto* tab : tabs) {
      tab_indices.emplace_back(
          browser()->GetTabStripModel()->GetIndexOfTab(tab));
    }
    return browser()->GetTabStripModel()->AddToNewGroup(tab_indices);
  }

  static bool IsHeaderCollapsed(const views::View* view) {
    if (const auto* header = views::AsViewClass<TabGroupHeader>(view)) {
      return header->is_collapsed_for_testing();
    }
    if (const auto* header = views::AsViewClass<TabGroupHeaderView>(view)) {
      return header->is_collapsed_for_testing();
    }
    return false;
  }

  void ToggleTabGroupCollapsedState(tab_groups::TabGroupId group_id) {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    if (auto* tab_strip = browser_view->horizontal_tab_strip_for_testing()) {
      tab_strip->ToggleTabGroupCollapsedState(
          group_id, ToggleTabGroupCollapsedStateOrigin::kMouse);
    } else {
      views::View* anchor =
          browser_view->tab_strip_view()->GetTabGroupAnchorView(group_id);
      auto* tab_group_view = views::AsViewClass<TabGroupView>(anchor);
      if (!tab_group_view && anchor && anchor->parent()) {
        tab_group_view = views::AsViewClass<TabGroupView>(anchor->parent());
      }
      if (tab_group_view) {
        tab_group_view->ToggleCollapsedState(
            ToggleTabGroupCollapsedStateOrigin::kMouse);
      }
    }
  }
};

// Disable these tests on windows.
// TODO(crbug.com/547718513): Re-enable
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_Collapse DISABLED_Collapse
#else
#define MAYBE_Collapse Collapse
#endif

// TODO(crbug.com/547718513): Re-enable
#if BUILDFLAG(IS_MAC)
#define MAYBE_OpenEditorBubble DISABLED_OpenEditorBubble
#define MAYBE_AttentionIndicator DISABLED_AttentionIndicator
#define MAYBE_DragCollapsedGroup DISABLED_DragCollapsedGroup
#else
#define MAYBE_OpenEditorBubble OpenEditorBubble
#define MAYBE_AttentionIndicator AttentionIndicator
#define MAYBE_DragCollapsedGroup DragCollapsedGroup
#endif

using TabGroupCollapsedObserver =
    views::test::PollingViewObserver<bool, views::View>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabGroupCollapsedObserver,
                                    kTabGroupCollapsedState);
IN_PROC_BROWSER_TEST_F(TabGroupHeaderInteractiveUiTest, MAYBE_Collapse) {
  CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action = ui_controls::MouseButton::LEFT;

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      PollView(kTabGroupCollapsedState, kTabGroupHeaderElementId,
               [](const views::View* view) { return IsHeaderCollapsed(view); }),
      MoveMouseTo(kTabGroupHeaderElementId), ClickMouse(action),
      WaitForState(kTabGroupCollapsedState, true));
}

IN_PROC_BROWSER_TEST_F(TabGroupHeaderInteractiveUiTest,
                       MAYBE_OpenEditorBubble) {
  CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action = ui_controls::MouseButton::RIGHT;

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(),
                  MoveMouseTo(kTabGroupHeaderElementId), ClickMouse(action),
                  WaitForShow(kTabGroupEditorBubbleId));
}

class TabGroupHeaderAttentionIndicatorInteractiveUiTest
    : public TabGroupHeaderInteractiveUiTest {
 public:
  TabGroupHeaderAttentionIndicatorInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        data_sharing::features::kDataSharingFeature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabGroupHeaderAttentionIndicatorInteractiveUiTest,
                       MAYBE_AttentionIndicator) {
  tab_groups::TabGroupId group_id = CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action = ui_controls::MouseButton::LEFT;

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      PollView(kTabGroupCollapsedState, kTabGroupHeaderElementId,
               [](const views::View* view) { return IsHeaderCollapsed(view); }),
      // Click the group to collapse it.
      MoveMouseTo(kTabGroupHeaderElementId), ClickMouse(action),
      WaitForState(kTabGroupCollapsedState, true), Do([&]() {
        // Set the attention indicator to true.
        browser()
            ->GetTabStripModel()
            ->group_model()
            ->GetTabGroup(group_id)
            ->GetTabGroupFeatures()
            ->attention_indicator()
            ->SetHasAttention(true);
      }),
      WaitForShow(kTabGroupHeaderAttentionIndicatorElementId));
}

IN_PROC_BROWSER_TEST_F(TabGroupHeaderInteractiveUiTest,
                       MAYBE_DragCollapsedGroup) {
  tab_groups::TabGroupId group_id = CreateTabGroup({CreateTab()});

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      // Collapse the group
      Do([&]() { ToggleTabGroupCollapsedState(group_id); }),
      FinishTabstripAnimations(),
      // Verify it is collapsed
      CheckView(
          kTabGroupHeaderElementId,
          [](views::View* view) { return IsHeaderCollapsed(view); }, true),
      // Drag the group header. We drag it a bit to the right.
      MoveMouseTo(kTabGroupHeaderElementId),
      DragMouseTo(kTabGroupHeaderElementId,
                  base::BindLambdaForTesting([](ui::TrackedElement* el) {
                    return el->AsA<views::TrackedElementViews>()
                               ->view()
                               ->GetBoundsInScreen()
                               .CenterPoint() +
                           gfx::Vector2d(50, 0);
                  })),
      // Verify it is still collapsed
      CheckView(
          kTabGroupHeaderElementId,
          [](views::View* view) { return IsHeaderCollapsed(view); }, true));
}
