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
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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

  TabStrip* GetTabStrip() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->horizontal_tab_strip_for_testing();
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

DEFINE_LOCAL_POLLING_VIEW_PROPERTY_STATE_IDENTIFIER(TabGroupHeader,
                                                    is_collapsed_for_testing,
                                                    kTabGroupCollapsedState);
IN_PROC_BROWSER_TEST_F(TabGroupHeaderInteractiveUiTest, MAYBE_Collapse) {
  CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action = ui_controls::MouseButton::LEFT;

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      PollViewProperty(kTabGroupCollapsedState, kTabGroupHeaderElementId),
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

IN_PROC_BROWSER_TEST_F(TabGroupHeaderInteractiveUiTest,
                       MAYBE_AttentionIndicator) {
  tab_groups::TabGroupId group_id = CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action = ui_controls::MouseButton::LEFT;

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      PollViewProperty(kTabGroupCollapsedState, kTabGroupHeaderElementId),
      // Click the group to collapse it.
      MoveMouseTo(kTabGroupHeaderElementId), ClickMouse(action), Do([&]() {
        // Set the attention indicator to true.
        browser()
            ->GetTabStripModel()
            ->group_model()
            ->GetTabGroup(group_id)
            ->GetTabGroupFeatures()
            ->attention_indicator()
            ->SetHasAttention(true);
      }),
      Do([&]() {
        EXPECT_TRUE(GetTabStrip()
                        ->group_header(group_id)
                        ->ShouldShowAttentionIndicator());
      }));
}

IN_PROC_BROWSER_TEST_F(TabGroupHeaderInteractiveUiTest,
                       MAYBE_DragCollapsedGroup) {
  tab_groups::TabGroupId group_id = CreateTabGroup({CreateTab()});

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      // Collapse the group
      Do([&]() {
        GetTabStrip()->ToggleTabGroupCollapsedState(
            group_id, ToggleTabGroupCollapsedStateOrigin::kMouse);
      }),
      FinishTabstripAnimations(),
      // Verify it is collapsed
      CheckViewProperty(kTabGroupHeaderElementId,
                        &TabGroupHeader::is_collapsed_for_testing, true),
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
      CheckViewProperty(kTabGroupHeaderElementId,
                        &TabGroupHeader::is_collapsed_for_testing, true));
}
