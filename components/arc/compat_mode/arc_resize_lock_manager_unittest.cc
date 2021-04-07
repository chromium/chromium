// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_resize_lock_manager.h"

#include <set>
#include <string>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "components/exo/test/exo_test_base_views.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

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

class TestArcResizeLockPrefDelegate : public ArcResizeLockPrefDelegate {
 public:
  ~TestArcResizeLockPrefDelegate() override = default;

  // ArcResizeLockPrefDelegate:
  bool GetResizeLockNeedsConfirmation(const std::string& app_id) override {
    return base::Contains(confirmation_needed_app_ids_, app_id);
  }
  void SetResizeLockNeedsConfirmation(const std::string& app_id,
                                      bool is_needed) override {
    if (GetResizeLockNeedsConfirmation(app_id) == is_needed)
      return;

    if (is_needed)
      confirmation_needed_app_ids_.push_back(app_id);
    else
      base::Erase(confirmation_needed_app_ids_, app_id);
  }

 private:
  std::vector<std::string> confirmation_needed_app_ids_;
};

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kNonInterestedPropKey, false)

}  // namespace

class ArcResizeLockManagerTest : public exo::test::ExoTestBaseViews {
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

  void AcceptChildDialog(views::Widget* parent_widget) {
    auto* dialog_widget = GetChildDialogWidget(parent_widget);
    views::test::WidgetDestroyedWaiter waiter(dialog_widget);
    DialogDelegateFor(dialog_widget)->AcceptDialog();
    waiter.Wait();
  }

  void CancelChildDialog(views::Widget* parent_widget) {
    auto* dialog_widget = GetChildDialogWidget(parent_widget);
    views::test::WidgetDestroyedWaiter waiter(dialog_widget);
    DialogDelegateFor(dialog_widget)->CancelDialog();
    waiter.Wait();
  }

  FakeArcResizeLockManager* fake_arc_resize_lock_manager() {
    return &fake_arc_resize_lock_manager_;
  }

  bool OnResizeButtonPressed(views::Widget* widget) {
    return fake_arc_resize_lock_manager()->OnResizeButtonPressed(widget);
  }

 private:
  views::DialogDelegate* DialogDelegateFor(views::Widget* widget) {
    auto* delegate = widget->widget_delegate()->AsDialogDelegate();
    return delegate;
  }

  views::Widget* GetChildDialogWidget(views::Widget* widget) {
    std::set<views::Widget*> child_widgets;
    views::Widget::GetAllOwnedWidgets(widget->GetNativeView(), &child_widgets);
    DCHECK_EQ(1u, child_widgets.size());
    return *child_widgets.begin();
  }

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

// Test that size button callback works properly with needs-confirmation app.
TEST_F(ArcResizeLockManagerTest, TestSizeButtonNeedsConfirmation) {
  const std::string test_app_id = "123";
  TestArcResizeLockPrefDelegate pref_delegate;
  auto widget = CreateTestWidget();
  widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, test_app_id);
  fake_arc_resize_lock_manager()->SetPrefDelegate(&pref_delegate);

  pref_delegate.SetResizeLockNeedsConfirmation(test_app_id, true);

  // Test the widget will be maximized if accepted.
  EXPECT_TRUE(OnResizeButtonPressed(widget.get()));
  EXPECT_FALSE(widget->IsMaximized());
  AcceptChildDialog(widget.get());
  EXPECT_TRUE(widget->IsMaximized());
  widget->Restore();

  // Test the widget will NOT be maximized if cancelled.
  EXPECT_TRUE(OnResizeButtonPressed(widget.get()));
  EXPECT_FALSE(widget->IsMaximized());
  CancelChildDialog(widget.get());
  EXPECT_FALSE(widget->IsMaximized());
}

// Test that size button callback works properly with no-needs-confirmation app.
TEST_F(ArcResizeLockManagerTest, TestSizeButtonNoNeedsConfirmation) {
  const std::string test_app_id = "123";
  TestArcResizeLockPrefDelegate pref_delegate;
  auto widget = CreateTestWidget();
  widget->GetNativeWindow()->SetProperty(ash::kAppIDKey, test_app_id);
  fake_arc_resize_lock_manager()->SetPrefDelegate(&pref_delegate);

  pref_delegate.SetResizeLockNeedsConfirmation(test_app_id, false);

  // Test the widget will be maximized immediately.
  EXPECT_FALSE(widget->IsMaximized());
  EXPECT_TRUE(OnResizeButtonPressed(widget.get()));
  EXPECT_TRUE(widget->IsMaximized());
}

}  // namespace arc
