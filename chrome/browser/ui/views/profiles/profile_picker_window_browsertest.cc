// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_picker_test_base.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

class ProfilePickerWindowBrowserTest : public ProfilePickerTestBase {
 public:
  void SetUpOnMainThread() override {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kOnStartup));
    EXPECT_TRUE(ProfilePicker::IsOpen());
    WaitForPickerWidgetCreated();
  }

  views::NonClientFrameView* frame_view() {
    return widget()->non_client_view()->frame_view();
  }

  chromeos::HeaderView* header_view() {
    auto* frame_view_base =
        static_cast<chromeos::NonClientFrameViewBase*>(frame_view());
    return frame_view_base->GetHeaderView();
  }
};

// Based on the existing Ash test of the same name in:
//   //ash/frame/non_client_frame_view_ash_unittest.cc
//
// Regression test for:
//   - https://crbug.com/839955
//   - https://crbug.com/1385921
IN_PROC_BROWSER_TEST_F(ProfilePickerWindowBrowserTest,
                       ActiveStateOfButtonMatchesWidget) {
  // Wait for the profile picker widget to activate.
  base::RunLoop run_loop;
  auto subscription =
      widget()->RegisterPaintAsActiveChangedCallback(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(widget()->IsActive());

  chromeos::FrameCaptionButtonContainerView::TestApi test_api(
      header_view()->caption_button_container());

  EXPECT_TRUE(frame_view()->ShouldPaintAsActive());
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
  EXPECT_FALSE(frame_view()->ShouldPaintAsActive());
  EXPECT_FALSE(test_api.size_button()->GetPaintAsActive());
}
