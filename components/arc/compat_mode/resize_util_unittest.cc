// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_util.h"

#include <memory>
#include <set>
#include <string>

#include "ash/public/cpp/window_properties.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/stl_util.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "components/exo/test/exo_test_base_views.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

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
      return mojom::ArcResizeLockState::UNDEFINED;

    return it->second;
  }
  void SetResizeLockState(const std::string& app_id,
                          mojom::ArcResizeLockState state) override {
    resize_lock_states[app_id] = state;
  }
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
  base::flat_map<std::string, mojom::ArcResizeLockState> resize_lock_states;
};

}  // namespace

class ResizeUtilTest : public exo::test::ExoTestBaseViews {
 public:
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
  TestArcResizeLockPrefDelegate pref_delegate_;
  std::unique_ptr<views::Widget> widget_;
};

// Test that resize phone works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeLockToPhone) {
  widget()->Maximize();

  // Test the widget is NOT resized immediately if the confirmation dialog is
  // needed.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeLockToPhoneWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());

  // Test the widget is resized without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_TRUE(widget()->IsMaximized());
  ResizeLockToPhoneWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_LT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
}

// Test that resize tablet works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestResizeLockToTablet) {
  widget()->Maximize();

  // Test the widget is NOT resized immediately if the confirmation dialog is
  // needed.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  ResizeLockToTabletWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_TRUE(widget()->IsMaximized());

  // Test the widget is resized without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EXPECT_TRUE(widget()->IsMaximized());
  ResizeLockToTabletWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_GT(widget()->GetWindowBoundsInScreen().width(),
            widget()->GetWindowBoundsInScreen().height());
}

// Test that enabling resizing works properly in both needs-confirmation and no
// needs-conirmation case.
TEST_F(ResizeUtilTest, TestEnableResizing) {
  // Test the state is NOT changed immediately if the confirmation dialog is
  // needed.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, true);
  EnableResizingWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_NE(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::OFF);

  // Test the state is changed without confirmation.
  pref_delegate()->SetResizeLockNeedsConfirmation(kTestAppId, false);
  EnableResizingWithConfirmationIfNeeded(widget(), pref_delegate());
  EXPECT_EQ(pref_delegate()->GetResizeLockState(kTestAppId),
            mojom::ArcResizeLockState::OFF);
}

}  // namespace arc
