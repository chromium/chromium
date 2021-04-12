// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_toggle_menu.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace arc {
namespace {

constexpr char kTestAppId[] = "123";

class TestArcResizeLockPrefDelegate : public ArcResizeLockPrefDelegate {
 public:
  ~TestArcResizeLockPrefDelegate() override = default;

  // ArcResizeLockPrefDelegate:
  bool GetResizeLockNeedsConfirmation(const std::string& app_id) override {
    return false;
  }
  void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                      bool is_needed) override {}
};

}  // namespace

class ResizeToggleMenuTest : public views::ViewsTestBase {
 public:
  // Overridden from test::Test.
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    widget_->GetNativeWindow()->SetProperty(ash::kAppIDKey,
                                            std::string(kTestAppId));
    resize_toggle_menu_ =
        std::make_unique<ResizeToggleMenu>(widget_.get(), &pref_delegate);
  }
  void TearDown() override {
    widget_->CloseNow();
    views::ViewsTestBase::TearDown();
  }

  bool IsMenuRunning() {
    return resize_toggle_menu_->menu_runner_->IsRunning();
  }

  // Re-show the menu. This might close the running menu if any.
  void ReshowMenu() {
    resize_toggle_menu_.reset();
    resize_toggle_menu_ =
        std::make_unique<ResizeToggleMenu>(widget_.get(), &pref_delegate);
  }

  bool IsOnlyOneItemSelected(ResizeToggleMenu::CommandId target_command_id) {
    if (!resize_toggle_menu_->root_view_->GetMenuItemByID(target_command_id)) {
      // The target item is NOT in the menu.
      return false;
    }
    for (int id = ResizeToggleMenu::CommandId::kResizePhone;
         id <= ResizeToggleMenu::CommandId::kMaxValue; ++id) {
      const auto* menu_item_view =
          resize_toggle_menu_->root_view_->GetMenuItemByID(id);
      const bool is_selected = menu_item_view->IsSelected();
      if (id == target_command_id && !is_selected) {
        // The target item is NOT selected.
        return false;
      }
      if (id != target_command_id && is_selected) {
        // The NON-target item is wrongly selected.
        return false;
      }
    }
    return true;
  }

  ResizeToggleMenu* resize_toggle_menu() { return resize_toggle_menu_.get(); }
  views::Widget* widget() { return widget_.get(); }

 private:
  TestArcResizeLockPrefDelegate pref_delegate;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;
};

TEST_F(ResizeToggleMenuTest, ConstructDestruct) {
  EXPECT_TRUE(IsMenuRunning());
}

TEST_F(ResizeToggleMenuTest, TestResizePhone) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());
  widget()->Maximize();
  EXPECT_TRUE(widget()->IsMaximized());

  // Test that resize command is properly handled.
  resize_toggle_menu()->ExecuteCommand(
      ResizeToggleMenu::CommandId::kResizePhone, 0);
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  // Test that the item is selected after the resize.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_TRUE(IsOnlyOneItemSelected(ResizeToggleMenu::CommandId::kResizePhone));
}

TEST_F(ResizeToggleMenuTest, TestResizeTablet) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());
  widget()->Maximize();
  EXPECT_TRUE(widget()->IsMaximized());

  // Test that resize command is properly handled.
  resize_toggle_menu()->ExecuteCommand(
      ResizeToggleMenu::CommandId::kResizeTablet, 0);
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  // Test that the item is selected after the resize.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_TRUE(
      IsOnlyOneItemSelected(ResizeToggleMenu::CommandId::kResizeTablet));
}

TEST_F(ResizeToggleMenuTest, TestResizeDesktop) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_FALSE(widget()->IsMaximized());

  // Test that resize command is properly handled.
  resize_toggle_menu()->ExecuteCommand(
      ResizeToggleMenu::CommandId::kResizeDesktop, 0);
  EXPECT_TRUE(widget()->IsMaximized());

  // Test that the item is selected after the resize.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_TRUE(
      IsOnlyOneItemSelected(ResizeToggleMenu::CommandId::kResizeDesktop));
}

}  // namespace arc
