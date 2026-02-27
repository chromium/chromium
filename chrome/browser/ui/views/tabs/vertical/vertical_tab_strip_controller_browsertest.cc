// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_controller.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab/tab_context_menu_controller.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"

namespace {

class VerticalTabStripControllerBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    return {{features::kTabGroupsCollapseFreezing, {}},
            {tabs::kVerticalTabs, {}}};
  }

  bool CheckMenuHasStringId(int message_id) {
    ui::SimpleMenuModel* menu_model = vertical_tab_strip_controller()
                                          ->GetTabContextMenuController()
                                          ->GetMenuModel();
    for (size_t i = 0; i < menu_model->GetItemCount(); i++) {
      if (l10n_util::GetStringUTF16(message_id) == menu_model->GetLabelAt(i)) {
        return true;
      }
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       VerifyTabContextMenuText) {
  // Get the first tab's node.
  TabCollectionNode* first_tab_node =
      unpinned_collection_node()->children()[0].get();
  ASSERT_TRUE(first_tab_node);

  // Open Tab Context Menu manually.
  vertical_tab_strip_controller()->ShowContextMenuForNode(
      first_tab_node, first_tab_node->view(), gfx::Point(),
      ui::mojom::MenuSourceType::kMouse);

  // Verify "New Tab Below" text is present.
  EXPECT_TRUE(CheckMenuHasStringId(IDS_TAB_CXMENU_NEWTABBELOW));
  // Verify "Close Tabs Below" text is present.
  EXPECT_TRUE(CheckMenuHasStringId(IDS_TAB_CXMENU_CLOSETABSBELOW));

  // Close menu to avoid the test hanging.
  vertical_tab_strip_controller()->GetTabContextMenuController()->CloseMenu();
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripControllerBrowserTest,
                       TabGroupCollapseFreezing) {
  // Create two tabs and add them to a group.
  AppendTab();
  AppendTab();
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  TabCollectionNode* group_node =
      unpinned_collection_node()->GetChildNodeOfType(
          TabCollectionNode::Type::GROUP);
  VerticalTabView* tab_view0 = views::AsViewClass<VerticalTabView>(
      group_node->children()[0].get()->view());
  VerticalTabView* tab_view1 = views::AsViewClass<VerticalTabView>(
      group_node->children()[1].get()->view());

  // Collapse the group.
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);

  // Verify freezing votes.
  EXPECT_TRUE(tab_view0->HasFreezingVote());
  EXPECT_TRUE(tab_view1->HasFreezingVote());

  // Expand the group.
  vertical_tab_strip_controller()->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);

  // Verify freezing votes are released.
  EXPECT_FALSE(tab_view0->HasFreezingVote());
  EXPECT_FALSE(tab_view1->HasFreezingVote());
}

}  // namespace
