// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

using ProfilePickerWindowBrowserTest = ProfilePickerTestBase;

// Based on the existing Ash test of the same name in:
//   //ash/frame/non_client_frame_view_ash_unittest.cc
//
// Regression test for:
//   - https://crbug.com/839955
//   - https://crbug.com/1385921
IN_PROC_BROWSER_TEST_F(ProfilePickerWindowBrowserTest,
                       ActiveStateOfButtonMatchesWidget) {
  ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
      ProfilePicker::EntryPoint::kOnStartup));
  EXPECT_TRUE(ProfilePicker::IsOpen());
  WaitForPickerWidgetCreated();

  views::NonClientView* non_client_view = widget()->non_client_view();
  ASSERT_NE(non_client_view, nullptr);
  views::NonClientFrameView* non_client_frame_view =
      non_client_view->frame_view();
  ASSERT_NE(non_client_frame_view, nullptr);
  // We make the assumption that this test is running under ChromeOS so that we
  // can use the ChromeOS-specific subclass chromeos::NonClientFrameViewBase.
  chromeos::NonClientFrameViewBase* non_client_frame_view_base =
      static_cast<chromeos::NonClientFrameViewBase*>(non_client_frame_view);
  chromeos::FrameCaptionButtonContainerView::TestApi test_api(
      non_client_frame_view_base->GetHeaderView()->caption_button_container());

  // Wait for the profile picker widget to activate.
  base::RunLoop run_loop;
  auto subscription =
      widget()->RegisterPaintAsActiveChangedCallback(run_loop.QuitClosure());
  if (widget()->ShouldPaintAsActive()) {
    run_loop.Quit();
  }
  run_loop.Run();

  EXPECT_TRUE(widget()->IsActive());
  EXPECT_TRUE(non_client_frame_view->ShouldPaintAsActive());
  EXPECT_TRUE(test_api.size_button()->GetPaintAsActive());

  // Activate a different widget so the original one loses activation.
  auto widget2 = std::make_unique<views::Widget>();
  views::Widget::InitParams params;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  widget2->Init(std::move(params));

  // Wait for the other widget to activate.
  base::RunLoop run_loop2;
  auto subscription2 =
      widget2->RegisterPaintAsActiveChangedCallback(run_loop2.QuitClosure());
  widget2->Show();
  run_loop2.Run();

  EXPECT_FALSE(widget()->IsActive());
  EXPECT_FALSE(non_client_frame_view->ShouldPaintAsActive());
  EXPECT_FALSE(test_api.size_button()->GetPaintAsActive());
}
