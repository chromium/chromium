// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/ui/chromeos/test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using NonClientFrameViewTest = ChromeOSBrowserUITest;

// Regression test for https://crbug.com/40223676
IN_PROC_BROWSER_TEST_F(NonClientFrameViewTest,
                       TabletModeTitlebarHideForMaximizedWindow) {
  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  widget->Init(std::move(params));
  widget->Show();
  // TODO(https://crbug.com/325001477) Lacros updates inactive windows' state
  // late on tablet state change, so we have to wait for activation before
  // entering tablet mode so that there isn't a delay.
  ASSERT_TRUE(base::test::RunUntil([&]() { return widget->IsActive(); }));

  chromeos::HeaderView* header_view =
      static_cast<chromeos::NonClientFrameViewBase*>(
          widget->non_client_view()->frame_view())
          ->GetHeaderView();

  int expected_height = header_view->GetPreferredHeight();
  EXPECT_EQ(expected_height, widget->GetNativeWindow()->GetProperty(
                                 aura::client::kTopViewInset));

  EnterTabletMode();

  chromeos::WindowStateType window_state_type =
      widget->GetNativeWindow()->GetProperty(chromeos::kWindowStateTypeKey);
  EXPECT_EQ(chromeos::WindowStateType::kMaximized, window_state_type);
  EXPECT_EQ(
      0, widget->GetNativeWindow()->GetProperty(aura::client::kTopViewInset));

  ExitTabletMode();

  EXPECT_EQ(expected_height, widget->GetNativeWindow()->GetProperty(
                                 aura::client::kTopViewInset));
}
