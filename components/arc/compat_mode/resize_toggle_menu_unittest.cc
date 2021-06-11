// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_toggle_menu.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/containers/flat_map.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace arc {
namespace {

constexpr char kTestAppId[] = "123";

class TestArcResizeLockPrefDelegate : public ArcResizeLockPrefDelegate {
 public:
  ~TestArcResizeLockPrefDelegate() override = default;

  // ArcResizeLockPrefDelegate:
  mojom::ArcResizeLockState GetResizeLockState(
      const std::string& app_id) const override {
    auto it = resize_lock_states.find(app_id);
    if (it == resize_lock_states.end())
      return mojom::ArcResizeLockState::ON;

    return it->second;
  }
  void SetResizeLockState(const std::string& app_id,
                          mojom::ArcResizeLockState state) override {
    resize_lock_states[app_id] = state;
  }
  bool GetResizeLockNeedsConfirmation(const std::string& app_id) override {
    return false;
  }
  void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                      bool is_needed) override {}

 private:
  base::flat_map<std::string, mojom::ArcResizeLockState> resize_lock_states;
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
    widget_->Show();
    resize_toggle_menu_ =
        std::make_unique<ResizeToggleMenu>(widget_.get(), &pref_delegate_);
  }
  void TearDown() override {
    widget_->CloseNow();
    views::ViewsTestBase::TearDown();
  }

  bool IsMenuRunning() {
    return resize_toggle_menu_->bubble_widget_->IsVisible();
  }

  // Re-show the menu. This might close the running menu if any.
  void ReshowMenu() {
    resize_toggle_menu_.reset();
    resize_toggle_menu_ =
        std::make_unique<ResizeToggleMenu>(widget_.get(), &pref_delegate_);
  }

  bool IsCommandButtonDisabled(ResizeCompatMode command_id) {
    return GetButtonByCommandId(command_id)->GetState() ==
           views::Button::ButtonState::STATE_DISABLED;
  }

  void ClickButton(ResizeCompatMode command_id) {
    const auto* button = GetButtonByCommandId(command_id);
    ui::test::EventGenerator event_generator(GetRootWindow(widget_.get()));
    event_generator.MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }

  TestArcResizeLockPrefDelegate* pref_delegate() { return &pref_delegate_; }
  views::Widget* widget() { return widget_.get(); }

 private:
  views::Button* GetButtonByCommandId(ResizeCompatMode command_id) {
    switch (command_id) {
      case ResizeCompatMode::kPhone:
        return resize_toggle_menu_->phone_button_;
      case ResizeCompatMode::kTablet:
        return resize_toggle_menu_->tablet_button_;
      case ResizeCompatMode::kResizable:
        return resize_toggle_menu_->resizable_button_;
    }
  }

  TestArcResizeLockPrefDelegate pref_delegate_;
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
  ClickButton(ResizeCompatMode::kPhone);
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  // Test that the selected item is changed dynamically after the resize.
  EXPECT_TRUE(IsCommandButtonDisabled(ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kResizable));

  // Test that the item is selected after re-showing.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_TRUE(IsCommandButtonDisabled(ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kResizable));
}

TEST_F(ResizeToggleMenuTest, TestResizeTablet) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());
  widget()->Maximize();
  EXPECT_TRUE(widget()->IsMaximized());

  // Test that resize command is properly handled.
  ClickButton(ResizeCompatMode::kTablet);
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  // Test that the selected item is changed dynamically after the resize.
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kPhone));
  EXPECT_TRUE(IsCommandButtonDisabled(ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kResizable));

  // Test that the item is selected after re-showing.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kPhone));
  EXPECT_TRUE(IsCommandButtonDisabled(ResizeCompatMode::kTablet));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kResizable));
}

TEST_F(ResizeToggleMenuTest, TestResizable) {
  // Verify pre-conditions.
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_EQ(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::ON);

  // Test that resize command is properly handled.
  ClickButton(ResizeCompatMode::kResizable);
  EXPECT_EQ(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::OFF);

  // Test that the item is selected after the resize.
  ReshowMenu();
  EXPECT_TRUE(IsMenuRunning());
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kPhone));
  EXPECT_FALSE(IsCommandButtonDisabled(ResizeCompatMode::kTablet));
  EXPECT_TRUE(IsCommandButtonDisabled(ResizeCompatMode::kResizable));
}

}  // namespace arc
