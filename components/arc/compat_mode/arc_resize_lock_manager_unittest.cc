// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include <set>
#include <string>

#include "ash/constants/app_types.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/window_properties.h"
#include "base/containers/contains.h"
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

 private:
  FakeArcResizeLockManager fake_arc_resize_lock_manager_;
};

TEST_F(ArcResizeLockManagerTest, ConstructDestruct) {}

// Tests that resize lock state is properly sync'ed with the window property.
TEST_F(ArcResizeLockManagerTest, TestArcWindowPropertyChange) {
  auto* arc_window = CreateFakeWindow(true);

  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // App id needs to be set to toogle resize lock state.
  arc_window->SetProperty(ash::kAppIDKey, new std::string("app-id"));
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test EnableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));

  // Test DisableResizeLock will be called by the property change.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test if enabling/disabling |FULLY_LOCKED| toggles the resize lock state
  // properly.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::FULLY_LOCKED);
  EXPECT_TRUE(IsResizeLockEnabled(arc_window));
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the property overwrite with the same value.
  arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                          ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));

  // Test nothing will be called by the NON-interested property change.
  arc_window->SetProperty(kNonInterestedPropKey, true);
  EXPECT_FALSE(IsResizeLockEnabled(arc_window));
}

// Test that resize lock will NOT be enabled for non ARC windows.
TEST_F(ArcResizeLockManagerTest, TestNonArcWindowPropertyChange) {
  auto* non_arc_window = CreateFakeWindow(false);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
  non_arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                              ash::ArcResizeLockType::RESIZE_LIMITED);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
  non_arc_window->SetProperty(ash::kArcResizeLockTypeKey,
                              ash::ArcResizeLockType::RESIZABLE);
  EXPECT_FALSE(IsResizeLockEnabled(non_arc_window));
}

}  // namespace arc
