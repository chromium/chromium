// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_chip_view.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/actions/actions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/view_utils.h"

namespace {

class ActionAppMenuChipViewTest : public ActionAppMenuTestBase {
 public:
  ActionAppMenuChipViewTest() = default;
  ~ActionAppMenuChipViewTest() override = default;
};

TEST_F(ActionAppMenuChipViewTest, AttachTo_AddsChipAndUpdatesAccessibleName) {
  views::TestMenuItemView root_item;
  views::MenuItemView* item = root_item.AppendMenuItem(1, u"Profile Title");

  ActionAppMenuChipView::AttachTo(item, u"Signed in");

  // Expect two child views: ActionAppMenuChipView and edge spacing view.
  ASSERT_EQ(item->children().size(), 2u);
  auto* chip_view =
      views::AsViewClass<ActionAppMenuChipView>(item->children()[0]);
  ASSERT_NE(chip_view, nullptr);
  EXPECT_EQ(chip_view->chip_label_for_testing()->GetText(), u"Signed in");

  // Verify accessible name includes both item title and chip text.
  ui::AXNodeData data;
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  const std::u16string expected_accessible_name =
      views::MenuItemView::GetAccessibleNameForMenuItem(
          u"Profile Title", u"Signed in", std::nullopt);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            expected_accessible_name);
}

TEST_F(ActionAppMenuChipViewTest, AttachTo_DrivenByChipTextProperty) {
  auto item = ActionAppMenuManager::CreateIndirectActionItem(
      kActionNewTab, ActionAppMenuManager::DisplayType::kRow,
      /*container_color=*/std::nullopt,
      /*text_override=*/u"Profile Name",
      /*icon_override=*/std::nullopt,
      /*chip_text=*/u"Signed in");
  ASSERT_NE(item, nullptr);

  std::u16string* chip_text_prop =
      item->GetProperty(ActionAppMenuManager::kChipTextKey);
  ASSERT_NE(chip_text_prop, nullptr);
  EXPECT_EQ(*chip_text_prop, u"Signed in");
}

}  // namespace
