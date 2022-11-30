// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fullscreen_control/fullscreen_control_view.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

TEST(FullscreenControlView, CheckExitButtonFocusBehavior) {
  FullscreenControlView control_view{views::Button::PressedCallback()};
  EXPECT_EQ(
      views::View::FocusBehavior::ACCESSIBLE_ONLY,
      control_view.exit_fullscreen_button_for_testing()->GetFocusBehavior());
}
