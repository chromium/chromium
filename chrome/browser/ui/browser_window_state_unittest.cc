// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window_state.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace chrome {

namespace internal {

namespace {

constexpr int kDefaultWidth = 1920;
constexpr int kDefaultHeight = 1200;
constexpr int kDefaultOffsetX = 0;
constexpr int kDefaultOffsetY = 0;
constexpr ui::mojom::WindowShowState kDefaultShowState =
    ui::mojom::WindowShowState::kMaximized;

class BrowserWindowStateTest : public testing::Test {
 public:
  BrowserWindowStateTest()
      : bounds_(gfx::Point(kDefaultOffsetX, kDefaultOffsetY),
                gfx::Size(kDefaultWidth, kDefaultHeight)),
        show_state_(kDefaultShowState),
        command_line_(base::CommandLine::NO_PROGRAM) {}

  gfx::Rect bounds_;
  ui::mojom::WindowShowState show_state_;
  base::CommandLine command_line_;
};

}  // namespace

TEST_F(BrowserWindowStateTest, NoCommandLineLeavesParamsIntact) {
  UpdateWindowBoundsAndShowStateFromCommandLine(command_line_, &bounds_,
                                                &show_state_);
  EXPECT_EQ(bounds_.x(), kDefaultOffsetX);
  EXPECT_EQ(bounds_.y(), kDefaultOffsetY);
  EXPECT_EQ(bounds_.width(), kDefaultWidth);
  EXPECT_EQ(bounds_.height(), kDefaultHeight);
  EXPECT_EQ(show_state_, kDefaultShowState);
}

TEST_F(BrowserWindowStateTest, InvalidCommandLineLeavesParamsIntact) {
  command_line_.AppendSwitchASCII(switches::kWindowSize, "0,abc");
  command_line_.AppendSwitchASCII(switches::kWindowPosition, "invalid");

  UpdateWindowBoundsAndShowStateFromCommandLine(command_line_, &bounds_,
                                                &show_state_);
  EXPECT_EQ(bounds_.x(), kDefaultOffsetX);
  EXPECT_EQ(bounds_.y(), kDefaultOffsetY);
  EXPECT_EQ(bounds_.width(), kDefaultWidth);
  EXPECT_EQ(bounds_.height(), kDefaultHeight);
  EXPECT_EQ(show_state_, kDefaultShowState);
}

TEST_F(BrowserWindowStateTest, WindowSizeOverridesShowState) {
  command_line_.AppendSwitchASCII(switches::kWindowSize, "100,200");

  UpdateWindowBoundsAndShowStateFromCommandLine(command_line_, &bounds_,
                                                &show_state_);
  EXPECT_EQ(bounds_.x(), kDefaultOffsetX);
  EXPECT_EQ(bounds_.y(), kDefaultOffsetY);
  EXPECT_EQ(bounds_.width(), 100);
  EXPECT_EQ(bounds_.height(), 200);
  EXPECT_EQ(show_state_, ui::mojom::WindowShowState::kNormal);
}

TEST_F(BrowserWindowStateTest, WindowPositionOverridesShowState) {
  command_line_.AppendSwitchASCII(switches::kWindowPosition, "100,200");

  UpdateWindowBoundsAndShowStateFromCommandLine(command_line_, &bounds_,
                                                &show_state_);
  EXPECT_EQ(bounds_.x(), 100);
  EXPECT_EQ(bounds_.y(), 200);
  EXPECT_EQ(bounds_.width(), kDefaultWidth);
  EXPECT_EQ(bounds_.height(), kDefaultHeight);
  EXPECT_EQ(show_state_, ui::mojom::WindowShowState::kNormal);
}

}  // namespace internal

}  // namespace chrome
