// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/recent_activity_bubble_dialog_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/test/tab_strip_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/collaboration/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/interaction/polling_view_observer.h"

// The Param value sets tab_groups::kLeftClickOpensTabGroupBubble.
class TabGroupHeaderInteractiveUiTest
    : public TabStripInteractiveTestMixin<InteractiveBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  TabGroupHeaderInteractiveUiTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          /* enabled_features =*/{tab_groups::kLeftClickOpensTabGroupBubble},
          /* disabled_features =*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /* enabled_features =*/{},
          /* disabled_features =*/{tab_groups::kLeftClickOpensTabGroupBubble});
    }
  }

  ~TabGroupHeaderInteractiveUiTest() override = default;

  tabs::TabInterface* CreateTab() {
    auto index = browser()->tab_strip_model()->count();
    CHECK(AddTabAtIndex(index, GURL(chrome::kChromeUINewTabPageURL),
                        ui::PAGE_TRANSITION_TYPED));
    auto* tab = browser()->tab_strip_model()->GetTabAtIndex(index);
    CHECK(tab);
    return tab;
  }

  const tab_groups::TabGroupId CreateTabGroup(
      std::vector<tabs::TabInterface*> tabs) {
    std::vector<int> tab_indices = {};
    for (auto* tab : tabs) {
      tab_indices.emplace_back(
          browser()->tab_strip_model()->GetIndexOfTab(tab));
    }
    return browser()->tab_strip_model()->AddToNewGroup(tab_indices);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Disable these tests on windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Collapse DISABLED_Collapse
#else
#define MAYBE_Collapse Collapse
#endif
using TabGroupCollapsedObserver =
    views::test::PollingViewPropertyObserver<bool, TabGroupHeader>;
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(TabGroupCollapsedObserver,
                                    kTabGroupCollapsedState);
IN_PROC_BROWSER_TEST_P(TabGroupHeaderInteractiveUiTest, MAYBE_Collapse) {
  CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action =
      base::FeatureList::IsEnabled(tab_groups::kLeftClickOpensTabGroupBubble)
          ? ui_controls::MouseButton::RIGHT
          : ui_controls::MouseButton::LEFT;

  RunTestSequence(
      WaitForShow(kTabGroupHeaderElementId), FinishTabstripAnimations(),
      PollViewProperty(kTabGroupCollapsedState, kTabGroupHeaderElementId,
                       &TabGroupHeader::is_collapsed_for_testing),
      MoveMouseTo(kTabGroupHeaderElementId), ClickMouse(action),
      WaitForState(kTabGroupCollapsedState, true));
}

IN_PROC_BROWSER_TEST_P(TabGroupHeaderInteractiveUiTest, OpenEditorBubble) {
  CreateTabGroup({CreateTab()});

  ui_controls::MouseButton action =
      base::FeatureList::IsEnabled(tab_groups::kLeftClickOpensTabGroupBubble)
          ? ui_controls::MouseButton::LEFT
          : ui_controls::MouseButton::RIGHT;

  RunTestSequence(WaitForShow(kTabGroupHeaderElementId),
                  FinishTabstripAnimations(),
                  MoveMouseTo(kTabGroupHeaderElementId), ClickMouse(action),
                  WaitForShow(kTabGroupEditorBubbleId));
}

INSTANTIATE_TEST_SUITE_P(TabGroupFeature,
                         TabGroupHeaderInteractiveUiTest,
                         ::testing::Bool());
