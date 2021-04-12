// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_util.h"

#include <memory>
#include <set>
#include <string>

#include "ash/public/cpp/window_properties.h"
#include "base/stl_util.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "components/exo/test/exo_test_base_views.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace arc {
namespace {

constexpr char kTestAppId[] = "123";

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

}  // namespace

class ResizeUtilTest : public exo::test::ExoTestBaseViews {
 public:
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

  // Overridden from test::Test.
  void SetUp() override {
    exo::test::ExoTestBaseViews::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::TYPE_WINDOW);
    widget_->GetNativeWindow()->SetProperty(ash::kAppIDKey,
                                            std::string(kTestAppId));
  }

  TestArcResizeLockPrefDelegate* pref_delegate() { return &pref_delegate_; }
  views::Widget* widget() { return widget_.get(); }

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

  TestArcResizeLockPrefDelegate pref_delegate_;
  std::unique_ptr<views::Widget> widget_;
};

// Test that resize phone works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeToPhone) {
  widget()->Maximize();

  // Test the widget is resized if accepted the confirmation dialog.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeToPhoneWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());
  AcceptChildDialog(widget());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  widget()->Maximize();

  // Test the widget is NOT resized if cancelled the confirmation dialog.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeToPhoneWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());
  CancelChildDialog(widget());
  EXPECT_TRUE(widget()->IsMaximized());

  widget()->Maximize();

  // Test the widget is resized without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_TRUE(widget()->IsMaximized());
  ResizeToPhoneWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
}

// Test that resize tablet works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeToTablet) {
  widget()->Maximize();

  // Test the widget is resized if accepted the confirmation dialog.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeToTabletWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());
  AcceptChildDialog(widget());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());

  widget()->Maximize();

  // Test the widget is NOT resized if cancelled the confirmation dialog.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeToTabletWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());
  CancelChildDialog(widget());
  EXPECT_TRUE(widget()->IsMaximized());

  widget()->Maximize();

  // Test the widget is resized without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_TRUE(widget()->IsMaximized());
  ResizeToTabletWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
}

// Test that resize desktop works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeToDesktop) {
  widget()->Restore();

  // Test the widget is resized if accepted the confirmation dialog.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeToDesktopWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_FALSE(widget()->IsMaximized());
  AcceptChildDialog(widget());
  EXPECT_TRUE(widget()->IsMaximized());

  widget()->Restore();

  // Test the widget is NOT resized if cancelled the confirmation dialog.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeToDesktopWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_FALSE(widget()->IsMaximized());
  CancelChildDialog(widget());
  EXPECT_FALSE(widget()->IsMaximized());

  widget()->Restore();

  // Test the widget is resized without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_FALSE(widget()->IsMaximized());
  ResizeToDesktopWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());
}

}  // namespace arc
