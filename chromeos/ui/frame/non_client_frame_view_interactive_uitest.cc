// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/chromeos/test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

class NonClientFrameViewTest : public ChromeOSBrowserUITest {
 public:
  void SetUpOnMainThread() override {
    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    widget_.Init(std::move(params));
    widget_.Show();
  }

 protected:
  views::Widget* widget() { return &widget_; }

  aura::Window* window() { return widget_.GetNativeWindow(); }

  views::NonClientFrameView* frame_view() {
    return widget_.non_client_view()->frame_view();
  }

  chromeos::HeaderView* header_view() {
    return static_cast<chromeos::NonClientFrameViewBase*>(frame_view())
        ->GetHeaderView();
  }

 private:
  views::Widget widget_;
};

// Based on the existing Ash test of the same name in:
//   //ash/frame/non_client_frame_view_ash_unittest.cc
//
// Regression test for:
//   - https://crbug.com/839955
//   - https://crbug.com/1385921
IN_PROC_BROWSER_TEST_F(NonClientFrameViewTest,
                       ActiveStateOfButtonMatchesWidget) {
  // Wait for the widget to activate.
  ASSERT_TRUE(base::test::RunUntil([&]() { return widget()->IsActive(); }));

  chromeos::FrameCaptionButtonContainerView::TestApi test_api(
      header_view()->caption_button_container());

  EXPECT_TRUE(frame_view()->ShouldPaintAsActive());
  EXPECT_TRUE(test_api.size_button()->GetPaintAsActive());

  widget()->Deactivate();
  ASSERT_TRUE(base::test::RunUntil([&]() { return !widget()->IsActive(); }));

  EXPECT_FALSE(frame_view()->ShouldPaintAsActive());
  EXPECT_FALSE(test_api.size_button()->GetPaintAsActive());
}

// Regression test for https://crbug.com/40223676
IN_PROC_BROWSER_TEST_F(NonClientFrameViewTest,
                       TabletModeTitlebarHideForMaximizedWindow) {
  // TODO(https://crbug.com/325001477) Lacros updates inactive windows' state
  // late on tablet state change, so we have to wait for activation before
  // entering tablet mode so that there isn't a delay.
  ASSERT_TRUE(base::test::RunUntil([&]() { return widget()->IsActive(); }));

  EXPECT_EQ(chromeos::WindowStateType::kNormal,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));

  EnterTabletMode();

  EXPECT_EQ(chromeos::WindowStateType::kMaximized,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(0, window()->GetProperty(aura::client::kTopViewInset));

  ExitTabletMode();

  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));
}

IN_PROC_BROWSER_TEST_F(NonClientFrameViewTest,
                       TabletModeTitlebarHideForSnappedWindow) {
  if (!IsSnapWindowSupported()) {
    GTEST_SKIP() << "Ash is too old.";
  }
  // TODO(https://crbug.com/325001477) Lacros updates inactive windows' state
  // late on tablet state change, so we have to wait for activation before
  // entering tablet mode so that there isn't a delay.
  ASSERT_TRUE(base::test::RunUntil([&]() { return widget()->IsActive(); }));

  SnapWindow(window(), crosapi::mojom::SnapPosition::kPrimary);

  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));

  EnterTabletMode();

  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(0, window()->GetProperty(aura::client::kTopViewInset));

  ExitTabletMode();

  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));
}

IN_PROC_BROWSER_TEST_F(NonClientFrameViewTest,
                       TabletModeTitlebarShowForFloatedWindow) {
  // TODO(https://crbug.com/325001477) Lacros updates inactive windows' state
  // late on tablet state change, so we have to wait for activation before
  // entering tablet mode so that there isn't a delay.
  ASSERT_TRUE(base::test::RunUntil([&]() { return widget()->IsActive(); }));

  {
    auto waiter = ui_test_utils::CreateAsyncWidgetRequestWaiter(*browser());
    chromeos::FloatControllerBase::Get()->SetFloat(
        window(), chromeos::FloatStartLocation::kBottomRight);
    waiter.Wait();
  }

  EXPECT_EQ(chromeos::WindowStateType::kFloated,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));

  EnterTabletMode();

  EXPECT_EQ(chromeos::WindowStateType::kFloated,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));

  ExitTabletMode();

  EXPECT_EQ(chromeos::WindowStateType::kFloated,
            window()->GetProperty(chromeos::kWindowStateTypeKey));
  EXPECT_EQ(header_view()->GetPreferredHeight(),
            window()->GetProperty(aura::client::kTopViewInset));
}
