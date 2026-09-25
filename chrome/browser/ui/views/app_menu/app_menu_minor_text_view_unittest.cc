// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_minor_text_view.h"

#include <string>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/views/app_menu/action_app_menu_test_base.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/view_utils.h"

namespace {

class AppMenuMinorTextViewTest : public ActionAppMenuTestBase {
 public:
  AppMenuMinorTextViewTest() = default;
  ~AppMenuMinorTextViewTest() override = default;
};

TEST_F(AppMenuMinorTextViewTest, AttachTo_AddsMinorTextViewAndUpdatesA11yName) {
  views::TestMenuItemView root_item;
  views::MenuItemView* item =
      root_item.AppendMenuItem(1, u"Relaunch to update");

  AppMenuMinorTextView::AttachTo(item, u"Your tabs will reopen");

  ASSERT_EQ(item->children().size(), 1u);
  auto* minor_text_view =
      views::AsViewClass<AppMenuMinorTextView>(item->children()[0]);
  ASSERT_NE(minor_text_view, nullptr);
  EXPECT_EQ(minor_text_view->label_for_testing()->GetText(),
            u"Your tabs will reopen");

  ui::AXNodeData data;
  item->GetViewAccessibility().GetAccessibleNodeData(&data);
  const std::u16string expected_accessible_name =
      views::MenuItemView::GetAccessibleNameForMenuItem(
          u"Relaunch to update", u"Your tabs will reopen", std::nullopt);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            expected_accessible_name);
}

TEST_F(AppMenuMinorTextViewTest, AttachTo_DrivenByMinorTextProperty) {
  auto item = AppMenuActionItem::CreateIndirect(
      kActionUpgradeDialog,
      BrowserActions::From(&mock_window_interface_)->root_action_item(),
      {
          .minor_text = u"Your tabs will reopen",
      });
  ASSERT_NE(item, nullptr);

  std::u16string* minor_text_prop =
      item->GetProperty(AppMenuActionItem::kMinorTextKey);
  ASSERT_NE(minor_text_prop, nullptr);
  EXPECT_EQ(*minor_text_prop, u"Your tabs will reopen");
}

}  // namespace
