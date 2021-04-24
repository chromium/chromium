// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include <set>
#include <string>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace arc {
namespace {

class FakeArcResizeLockManager : public ArcResizeLockManager {
 public:
  FakeArcResizeLockManager() : ArcResizeLockManager(nullptr, nullptr) {}

  bool IsResizeLockEnabled(aura::Window* window) const {
    return base::Contains(resize_lock_enabled_windows_, window);
  }

  // ArcResizeLockManager:
  void EnableResizeLock(aura::Window* window) override {
    DCHECK(!base::Contains(resize_lock_enabled_windows_, window));
    resize_lock_enabled_windows_.push_back(window);
  }
  void DisableResizeLock(aura::Window* window) override {
    DCHECK(base::Contains(resize_lock_enabled_windows_, window));
    base::Erase(resize_lock_enabled_windows_, window);
  }

 private:
  std::vector<aura::Window*> resize_lock_enabled_windows_;
};

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kNonInterestedPropKey, false)

}  // namespace

class ArcResizeLockManagerTest : public views::ViewsTestBase {
 public:
  aura::Window* CreateFakeWindow(bool is_arc) {
    aura::Window* window =
        new aura::Window(nullptr, aura::client::WINDOW_TYPE_NORMAL);
    if (is_arc) {
      window->SetProperty(aura::client::kAppType,
                          static_cast<int>(ash::AppType::ARC_APP));
    }
    window->Init(ui::LAYER_TEXTURED);
    window->Show();
    return window;
  }

  bool IsResizeLockEnabled(aura::Window* window) const {
    return fake_arc_resize_lock_manager_.IsResizeLockEnabled(window);
  }

  bool IsToggleMenuShown() {
    return !!fake_arc_resize_lock_manager_.resize_toggle_menu_;
  }

  bool OnResizeButtonPressed(views::Widget* widget) {
    return fake_arc_resize_lock_manager_.OnResizeButtonPressed(widget);
  }

 private:
  FakeArcResizeLockManager fake_arc_resize_lock_manager_;
};

TEST_F(ArcResizeLockManagerTest, ConstructDestruct) {}

// Tests that resize lock state is properly sync'ed with the window property.
TEST_F(ArcResizeLockManagerTest, TestArcWindowPropertyChange) {
  auto* arc_window = CreateFakeWindow(true);

  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test EnableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockKey, true);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockKey, true);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));

  // Test DisableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockKey, false);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockKey, false);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the NON-interested property change.
  arc_window->SetProperty(kNonInterestedPropKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
}

// Test that resize lock will NOT be enabled for non ARC windows.
TEST_F(ArcResizeLockManagerTest, TestNonArcWindowPropertyChange) {
  auto* non_arc_window = CreateFakeWindow(false);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
  non_arc_window->SetProperty(ash::kArcResizeLockKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
  non_arc_window->SetProperty(ash::kArcResizeLockKey, false);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
}

// Test that size button callback changes nothing for fullscreen.
TEST_F(ArcResizeLockManagerTest, TestSizeButtonOnFullscreenWidget) {
  auto widget = CreateTestWidget();
  widget->SetFullscreen(true);
  EXPECT_TRUE(widget->IsFullscreen());
  EXPECT_FALSE(OnResizeButtonPressed(widget.get()));
  EXPECT_TRUE(widget->IsFullscreen());
}

// Test that size button callback changes nothing for maximized.
TEST_F(ArcResizeLockManagerTest, TestSizeButtonOnMaximizedWidget) {
  auto widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_TRUE(widget->IsMaximized());
  EXPECT_FALSE(OnResizeButtonPressed(widget.get()));
  EXPECT_TRUE(widget->IsMaximized());
}

// Test that size button callback works properly for freeform.
TEST_F(ArcResizeLockManagerTest, TestSizeButtonOnFreeformWidget) {
  auto widget = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);

  // Test the toggle menu is shown and the default maximize button
  // behavior is cancelled.
  EXPECT_FALSE(widget->IsMaximized());
  EXPECT_FALSE(IsToggleMenuShown());
  EXPECT_TRUE(OnResizeButtonPressed(widget.get()));
  EXPECT_FALSE(widget->IsMaximized());
  EXPECT_TRUE(IsToggleMenuShown());
}

}  // namespace arc
