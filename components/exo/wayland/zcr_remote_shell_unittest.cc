// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell.h"

#include "components/exo/test/exo_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"

namespace exo {

using ZcrRemoteShellTest = test::ExoTestBase;

TEST_F(ZcrRemoteShellTest, GetWorkAreaInsetsInPixel) {
  UpdateDisplay("3000x2000*2.25,1920x1080");

  auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const float device_scale_factor = display.device_scale_factor();
  EXPECT_EQ(display::kDsf_2_252, device_scale_factor);
  gfx::Insets insets = wayland::GetWorkAreaInsetsInPixel(
      display, device_scale_factor, display.GetSizeInPixel(),
      display.work_area());
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 108, 0).ToString(), insets.ToString());

  auto secondary_display = GetSecondaryDisplay();
  gfx::Size secondary_size(secondary_display.size());
  gfx::Size secondary_size_in_pixel =
      gfx::ScaleToRoundedSize(secondary_size, device_scale_factor);
  gfx::Insets secondary_insets = wayland::GetWorkAreaInsetsInPixel(
      secondary_display, device_scale_factor, secondary_size_in_pixel,
      secondary_display.work_area());
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 108, 0).ToString(),
            secondary_insets.ToString());

  // Stable Insets
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  widget->SetFullscreen(true);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_EQ(display.bounds(), display.work_area());
  gfx::Insets stable_insets = wayland::GetWorkAreaInsetsInPixel(
      display, device_scale_factor, display.GetSizeInPixel(),
      wayland::GetStableWorkArea(display));
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 108, 0).ToString(),
            stable_insets.ToString());
  gfx::Insets secondary_stable_insets = wayland::GetWorkAreaInsetsInPixel(
      secondary_display, device_scale_factor, secondary_size_in_pixel,
      wayland::GetStableWorkArea(secondary_display));
  EXPECT_EQ(gfx::Insets::TLBR(0, 0, 108, 0).ToString(),
            secondary_stable_insets.ToString());
}

}  // namespace exo
