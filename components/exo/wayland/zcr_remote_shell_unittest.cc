// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell.h"

#include "components/exo/test/exo_test_base.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"

namespace exo {

using ZcrRemoteShellTest = test::ExoTestBase;

TEST_F(ZcrRemoteShellTest, GetWorkAreaInsetsInClientPixel) {
  UpdateDisplay("3000x2000*2.25,1920x1080");

  auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const float device_scale_factor = display.device_scale_factor();
  EXPECT_EQ(2.25f, device_scale_factor);
  gfx::Insets insets = wayland::GetWorkAreaInsetsInClientPixel(
      display, device_scale_factor, display.GetSizeInPixel(),
      display.work_area());
  EXPECT_EQ(gfx::Insets(0, 0, 110, 0).ToString(), insets.ToString());

  auto secondary_display = GetSecondaryDisplay();
  gfx::Size secondary_size(secondary_display.size());
  gfx::Size secondary_size_in_client_pixel =
      gfx::ScaleToRoundedSize(secondary_size, device_scale_factor);
  gfx::Insets secondary_insets = wayland::GetWorkAreaInsetsInClientPixel(
      secondary_display, device_scale_factor, secondary_size_in_client_pixel,
      secondary_display.work_area());
  EXPECT_EQ(gfx::Insets(0, 0, 108, 0).ToString(), secondary_insets.ToString());

  // Stable Insets
  auto widget = CreateTestWidget();
  widget->SetFullscreen(true);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_EQ(display.bounds(), display.work_area());
  gfx::Insets stable_insets = wayland::GetWorkAreaInsetsInClientPixel(
      display, device_scale_factor, display.GetSizeInPixel(),
      wayland::GetStableWorkArea(display));
  EXPECT_EQ(gfx::Insets(0, 0, 110, 0).ToString(), stable_insets.ToString());
  gfx::Insets secondary_stable_insets = wayland::GetWorkAreaInsetsInClientPixel(
      secondary_display, device_scale_factor, secondary_size_in_client_pixel,
      wayland::GetStableWorkArea(secondary_display));
  EXPECT_EQ(gfx::Insets(0, 0, 108, 0).ToString(),
            secondary_stable_insets.ToString());
}

}  // namespace exo
