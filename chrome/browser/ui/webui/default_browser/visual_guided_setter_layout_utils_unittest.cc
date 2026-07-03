// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_layout_utils.h"

#include <windows.h>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace visual_guided_setter {

namespace {

// Test helper class to decouple the fake HWND from the running environment's
// window manager state.
class TestScreenWin : public display::win::ScreenWin {
 public:
  TestScreenWin() = default;

  TestScreenWin(const TestScreenWin&) = delete;
  TestScreenWin& operator=(const TestScreenWin&) = delete;

  ~TestScreenWin() override = default;

  HWND GetRootWindow(HWND hwnd) const override {
    if (hwnd == reinterpret_cast<HWND>(1)) {
      return ::GetDesktopWindow();
    }
    return display::win::ScreenWin::GetRootWindow(hwnd);
  }
};

class VisualGuidedSetterLayoutUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    old_screen_ = display::Screen::SetScreenInstance(&test_screen_);
  }

  void TearDown() override { display::Screen::SetScreenInstance(old_screen_); }

  base::test::TaskEnvironment task_environment_;
  TestScreenWin test_screen_;
  raw_ptr<display::Screen> old_screen_ = nullptr;
  HWND fake_hwnd() const { return reinterpret_cast<HWND>(1); }
};

}  // namespace

TEST_F(VisualGuidedSetterLayoutUtilsTest, IsAnchorLargeEnoughForDocking) {
  gfx::Rect too_narrow(0, 0, 319, 200);
  EXPECT_FALSE(IsAnchorLargeEnoughForDocking(too_narrow));

  gfx::Rect too_short(0, 0, 400, 159);
  EXPECT_FALSE(IsAnchorLargeEnoughForDocking(too_short));

  gfx::Rect exact_min(0, 0, 320, 160);
  EXPECT_TRUE(IsAnchorLargeEnoughForDocking(exact_min));
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, ComputeDockedSettingsRect_BasicMath) {
  // Anchor width: 600, height: 400 (in DIPs).
  gfx::Rect anchor_dip(400, 300, 600, 400);
  gfx::Rect work_area_px(0, 0, 1920, 1080);

  gfx::Rect result = ComputeDockedSettingsRectFromAnchor(
      fake_hwnd(), anchor_dip, work_area_px);

  // Math check: (400 + 61) = 461. (1000 - 61) = 939.
  EXPECT_EQ(result.x(), 461);
  EXPECT_EQ(result.right(), 939);
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, ComputeDockedSettingsRectClamped) {
  // Test clamping left.
  gfx::Rect anchor_dip(-50, 300, 250, 400);  // left inset: -50+61 = 11.
  gfx::Rect work_area(50, 0, 1870, 1080);
  gfx::Rect result =
      ComputeDockedSettingsRectFromAnchor(fake_hwnd(), anchor_dip, work_area);
  EXPECT_EQ(result.x(), 50);

  // Test clamping right.
  anchor_dip = gfx::Rect(1700, 300, 250, 400);  // right inset: 1950-61 = 1889.
  work_area = gfx::Rect(0, 0, 1800, 1080);
  result =
      ComputeDockedSettingsRectFromAnchor(fake_hwnd(), anchor_dip, work_area);
  EXPECT_EQ(result.right(), 1800);
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, ComputeArrowStartAndEndPoints) {
  gfx::Rect anchor(100, 200, 200, 200);
  gfx::Point start = ComputeArrowStartPointFromAnchor(anchor);
  EXPECT_EQ(start.x(), 300);
  EXPECT_EQ(start.y(), 300);

  gfx::Rect target(200, 300, 200, 200);
  gfx::Point end = ComputeArrowEndPoint(target);
  EXPECT_EQ(end.x(), 400);
  EXPECT_EQ(end.y(), 400);
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, IsDpiCompatibleForDocking) {
  // Get the primary display's bounds natively in physical pixels.
  gfx::Rect primary_bounds_dip =
      display::Screen::Get()->GetPrimaryDisplay().bounds();
  gfx::Rect physical_bounds =
      test_screen_.DIPToScreenRect(fake_hwnd(), primary_bounds_dip);

  // The fake window should be compatible with its own primary bounds.
  EXPECT_TRUE(IsDpiCompatibleForDocking(fake_hwnd(), physical_bounds));
}

}  // namespace visual_guided_setter
