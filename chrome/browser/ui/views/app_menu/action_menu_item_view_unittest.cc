// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_menu_item_view.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/menu/menu_item_view.h"

class ActionMenuItemViewTest : public ChromeViewsTestBase {
 public:
  ActionMenuItemViewTest() = default;
  ~ActionMenuItemViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    root_menu_ = std::make_unique<views::MenuItemView>();
  }

  void TearDown() override {
    root_menu_.reset();
    action_item_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<actions::ActionItem> action_item_;
  std::unique_ptr<views::MenuItemView> root_menu_;
};

TEST_F(ActionMenuItemViewTest, InitialAttributes) {
  action_item_ = actions::ActionItem::Builder()
                     .SetText(u"Test Title")
                     .SetEnabled(false)
                     .SetVisible(true)
                     .Build();

  auto* item_view = root_menu_->AddChildView(
      std::make_unique<ActionMenuItemView>(root_menu_.get(), action_item_.get(),
                                           views::MenuItemView::Type::kNormal));

  EXPECT_EQ(item_view->title(), u"Test Title");
  EXPECT_FALSE(item_view->GetEnabled());
  EXPECT_TRUE(item_view->GetVisible());
}

TEST_F(ActionMenuItemViewTest, DynamicUpdates) {
  action_item_ = actions::ActionItem::Builder()
                     .SetText(u"Initial Title")
                     .SetEnabled(true)
                     .SetVisible(true)
                     .Build();

  auto* item_view = root_menu_->AddChildView(
      std::make_unique<ActionMenuItemView>(root_menu_.get(), action_item_.get(),
                                           views::MenuItemView::Type::kNormal));

  EXPECT_EQ(item_view->title(), u"Initial Title");
  EXPECT_TRUE(item_view->GetEnabled());

  action_item_->SetText(u"Updated Title");
  action_item_->SetEnabled(false);

  EXPECT_EQ(item_view->title(), u"Updated Title");
  EXPECT_FALSE(item_view->GetEnabled());
}
