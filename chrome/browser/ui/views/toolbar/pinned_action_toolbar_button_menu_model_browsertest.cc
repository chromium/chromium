// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/pinned_action_toolbar_button_menu_model.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"
#include "ui/base/models/menu_model.h"
#include "ui/menus/simple_menu_model.h"

class PinnedActionToolbarButtonMenuModelBrowserTest
    : public InProcessBrowserTest {
 public:
  PinnedActionToolbarButtonMenuModelBrowserTest() = default;

  void SetUpOnMainThread() override {
    action_item()->SetProperty(
        actions::kActionItemPinnableKey,
        std::underlying_type_t<actions::ActionPinnableState>(
            actions::ActionPinnableState::kPinnable));
  }

  actions::ActionItem* action_item() {
    return actions::ActionManager::Get().FindAction(
        actions::kActionCut, browser()->browser_actions()->root_action_item());
  }
};

IN_PROC_BROWSER_TEST_F(PinnedActionToolbarButtonMenuModelBrowserTest,
                       DefaultItemsExist) {
  PinnedActionToolbarButtonMenuModel menu_model(browser(),
                                                *action_item()->GetActionId());
  // There should be three items in the menu, two of which are visible.
  EXPECT_EQ(3u, menu_model.GetItemCount());
  int visible_count = 0;
  for (size_t index = 0; index < menu_model.GetItemCount(); index++) {
    if (menu_model.IsVisibleAt(index)) {
      visible_count++;
    }
  }
  EXPECT_EQ(2, visible_count);
}

IN_PROC_BROWSER_TEST_F(PinnedActionToolbarButtonMenuModelBrowserTest,
                       DefaultItemSettings) {
  // Verify all default aspects of menu items.
  PinnedActionToolbarButtonMenuModel menu_model(browser(),
                                                *action_item()->GetActionId());
  ui::Accelerator menu_accelerator;
  for (size_t index = 0; index < menu_model.GetItemCount(); index++) {
    EXPECT_FALSE(menu_model.IsItemDynamicAt(index));
    EXPECT_FALSE(menu_model.GetAcceleratorAt(index, &menu_accelerator));
    EXPECT_FALSE(menu_model.GetGroupIdAt(index));
    EXPECT_EQ(menu_model.GetButtonMenuItemAt(index), nullptr);
    EXPECT_EQ(menu_model.GetSubmenuModelAt(index), nullptr);
  }
}

IN_PROC_BROWSER_TEST_F(PinnedActionToolbarButtonMenuModelBrowserTest,
                       ChildActionsAddedAsContextMenuItems) {
  const int test_child_action_id1 = 3;
  const int test_child_action_id2 = 4;
  const std::u16string test_child_string1 = u"test_child_string1";
  const std::u16string test_child_string2 = u"test_child_string2";
  const auto test_child_icon1 = ui::ImageModel::FromVectorIcon(
      vector_icons::kBackArrowIcon, ui::kColorSysPrimary,
      ui::SimpleMenuModel::kDefaultIconSize);
  // Add two child actions
  action_item()->AddChild(actions::ActionItem::Builder()
                              .SetActionId(test_child_action_id1)
                              .SetText(test_child_string1)
                              .SetImage(test_child_icon1)
                              .Build());
  action_item()->AddChild(actions::ActionItem::Builder()
                              .SetActionId(test_child_action_id2)
                              .SetText(test_child_string2)
                              .Build());
  PinnedActionToolbarButtonMenuModel menu_model(browser(),
                                                *action_item()->GetActionId());
  // There should be 6 items in the menu, the two child actions, a separator,
  // and the three default actions.
  EXPECT_EQ(6u, menu_model.GetItemCount());
  EXPECT_EQ(test_child_string1, menu_model.GetLabelAt(0));
  EXPECT_EQ(test_child_icon1, menu_model.GetIconAt(0));
  EXPECT_EQ(test_child_string2, menu_model.GetLabelAt(1));
  EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR, menu_model.GetTypeAt(2));
  EXPECT_EQ(ui::NORMAL_SEPARATOR, menu_model.GetSeparatorTypeAt(2));
  EXPECT_EQ(std::u16string(), menu_model.GetLabelAt(2));
  EXPECT_TRUE(menu_model.IsEnabledAt(2));
  EXPECT_TRUE(menu_model.IsVisibleAt(2));
}
