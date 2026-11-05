// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_layout_utils.h"

#include <windows.h>

#include <array>
#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/icu_test_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/win/screen_win.h"
#include "ui/display/win/test/scoped_screen_win.h"
#include "ui/display/win/uwp_text_scale_factor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace visual_guided_setter {

namespace {

// An offset from the top of the Settings window down to the row, as the row
// locator would report it.
constexpr int kMeasuredOffsetPx = 164;

// The narrowest anchor that can host a docked Settings window:
// kNoWrapClientWidthDip (410) + kHorizontalFrameDip (16) + two
// kHorizontalInsetDip (61) margins. Kept as a literal so a change to any of
// those constants has to be acknowledged here.
constexpr int kMinAnchorWidthDip = 548;
constexpr int kMinAnchorHeightDip = 160;

// The client width at which the Settings navigation pane expands, and the band
// around it where the flow declines to dock.
constexpr int kPaneCollapseClientWidthDip = 800;
constexpr int kBreakpointMarginDip = 20;

// 'visual_guided_setter_layout_utils.cc' generates its layout table from fitted
// formulas rather than writing these down, so this table is what holds those
// formulas to what was measured.
struct MeasuredLayout {
  float text_scale;
  int wrap_client_width_dip;
};

constexpr auto kMeasuredLayouts = std::to_array<MeasuredLayout>({
    {1.00f, 376},
    {1.25f, 404},
    {1.50f, 432},
    {1.58f, 441},
    {1.75f, 460},
    {2.00f, 488},
});

// Stands in for the Windows "Make text bigger" accessibility setting so the
// expectations below do not depend on how the developer's machine is
// configured. Without this, a machine running at 175% text scale reads
// different rows out of the layout table than the bots do.
class TestUwpTextScaleFactor : public display::win::UwpTextScaleFactor {
 public:
  explicit TestUwpTextScaleFactor(float scale) : scale_(scale) {}

  float GetTextScaleFactor() const override { return scale_; }

 private:
  const float scale_;
};

// A synthetic 1920x1200 screen at scale factor 1.0, so that DIPs and physical
// pixels are the same number and the arithmetic under test can be checked
// against literals.
class TestScreenWin : public display::win::test::ScopedScreenWin {
 public:
  TestScreenWin() = default;

  TestScreenWin(const TestScreenWin&) = delete;
  TestScreenWin& operator=(const TestScreenWin&) = delete;

  ~TestScreenWin() override = default;

  // Decouples the fake HWND from the running environment's window manager
  // state.
  HWND GetRootWindow(HWND hwnd) const override {
    if (hwnd == reinterpret_cast<HWND>(1)) {
      return ::GetDesktopWindow();
    }
    return display::win::test::ScopedScreenWin::GetRootWindow(hwnd);
  }
};

}  // namespace

class VisualGuidedSetterLayoutUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    text_scale_factor_ =
        std::make_unique<TestUwpTextScaleFactor>(GetTestTextScale());
    display::win::UwpTextScaleFactor::SetImplementationForTesting(
        text_scale_factor_.get());
    screen_ = std::make_unique<TestScreenWin>();
    old_screen_ = display::Screen::SetScreenInstance(screen_.get());
  }

  void TearDown() override {
    display::Screen::SetScreenInstance(old_screen_);
    screen_.reset();
    display::win::UwpTextScaleFactor::SetImplementationForTesting(nullptr);
    text_scale_factor_.reset();
  }

  // The Windows text scale the test runs at. Overridden by the large-text
  // fixture below.
  virtual float GetTestTextScale() const { return 1.0f; }

  // Moves the Windows text scale mid-test, for the sweep over the measured
  // scales below.
  void SetTextScale(float scale) {
    display::win::UwpTextScaleFactor::SetImplementationForTesting(nullptr);
    text_scale_factor_ = std::make_unique<TestUwpTextScaleFactor>(scale);
    display::win::UwpTextScaleFactor::SetImplementationForTesting(
        text_scale_factor_.get());
  }

  TestScreenWin* screen() { return screen_.get(); }
  HWND fake_hwnd() const { return reinterpret_cast<HWND>(1); }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestUwpTextScaleFactor> text_scale_factor_;
  std::unique_ptr<TestScreenWin> screen_;
  raw_ptr<display::Screen> old_screen_ = nullptr;
};

TEST_F(VisualGuidedSetterLayoutUtilsTest, IsAnchorLargeEnoughForDocking) {
  gfx::Rect too_narrow(0, 0, kMinAnchorWidthDip - 1, 200);
  EXPECT_FALSE(IsAnchorLargeEnoughForDocking(too_narrow));

  gfx::Rect too_short(0, 0, kMinAnchorWidthDip, kMinAnchorHeightDip - 1);
  EXPECT_FALSE(IsAnchorLargeEnoughForDocking(too_short));

  gfx::Rect exact_min(0, 0, kMinAnchorWidthDip, kMinAnchorHeightDip);
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

  gfx::Rect target(200, 300, 200, 400);
  gfx::Point end = ComputeArrowEndPoint(target, kMeasuredOffsetPx);
  EXPECT_EQ(end.x(), 400);
  EXPECT_EQ(end.y(), 300 + kMeasuredOffsetPx);
}

// A measurement is taken from one layout and applied to whatever window is on
// screen now. Those can disagree -- the window shrank between the two -- so
// the arrow must still end inside the window.
TEST_F(VisualGuidedSetterLayoutUtilsTest, ArrowEndPointStaysInsideTheWindow) {
  gfx::Rect short_target(200, 300, 200, 40);
  gfx::Point end = ComputeArrowEndPoint(short_target, 5000);
  EXPECT_LE(end.y(), short_target.bottom());
  EXPECT_GE(end.y(), short_target.y());
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, IsDpiCompatibleForDocking) {
  // Get the primary display's bounds natively in physical pixels.
  gfx::Rect primary_bounds_dip =
      display::Screen::Get()->GetPrimaryDisplay().bounds();
  gfx::Rect physical_bounds =
      screen()->DIPToScreenRect(fake_hwnd(), primary_bounds_dip);

  // The fake window should be compatible with its own primary bounds.
  EXPECT_TRUE(IsDpiCompatibleForDocking(fake_hwnd(), physical_bounds));
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, FittedLayoutMatchesMeasuredLayout) {
  // Any height will do; only the width decides whether the layout is stable.
  constexpr int kTallEnoughDip = 400;

  for (const MeasuredLayout& measured : kMeasuredLayouts) {
    SCOPED_TRACE(testing::Message() << "text scale " << measured.text_scale);
    SetTextScale(measured.text_scale);

    // Below the wrap width the button stacks under the subtitle, and where it
    // lands then is not modeled, so docking is declined instead of guessed at.
    EXPECT_FALSE(IsSettingsLayoutStableForDocking(
        fake_hwnd(),
        gfx::Rect(0, 0, measured.wrap_client_width_dip - 1, kTallEnoughDip)));
    EXPECT_TRUE(IsSettingsLayoutStableForDocking(
        fake_hwnd(),
        gfx::Rect(0, 0, measured.wrap_client_width_dip, kTallEnoughDip)));
  }
}

TEST_F(VisualGuidedSetterLayoutUtilsTest, LayoutIsUnstableOnThePaneBreakpoint) {
  auto stable = [&](int client_width_dip) {
    return IsSettingsLayoutStableForDocking(
        fake_hwnd(), gfx::Rect(0, 0, client_width_dip, 400));
  };

  EXPECT_TRUE(stable(kPaneCollapseClientWidthDip - kBreakpointMarginDip - 1));
  EXPECT_FALSE(stable(kPaneCollapseClientWidthDip - kBreakpointMarginDip));
  EXPECT_FALSE(stable(kPaneCollapseClientWidthDip));
  EXPECT_FALSE(stable(kPaneCollapseClientWidthDip + kBreakpointMarginDip));
  EXPECT_TRUE(stable(kPaneCollapseClientWidthDip + kBreakpointMarginDip + 1));
}

// The Settings page lays itself out against the Windows text scale.
class VisualGuidedSetterLayoutUtilsLargeTextTest
    : public VisualGuidedSetterLayoutUtilsTest {
 protected:
  float GetTestTextScale() const override { return 1.75f; }
};

TEST_F(VisualGuidedSetterLayoutUtilsLargeTextTest,
       MinimumAnchorWidthDoesNotFollowText) {
  EXPECT_FALSE(IsAnchorLargeEnoughForDocking(
      gfx::Rect(0, 0, kMinAnchorWidthDip - 1, kMinAnchorHeightDip)));
  EXPECT_TRUE(IsAnchorLargeEnoughForDocking(
      gfx::Rect(0, 0, kMinAnchorWidthDip, kMinAnchorHeightDip)));
}

// The arrow runs between the stage and the instructions, which swap sides with
// the UI direction, so both of its endpoints move to the opposite edge.
class VisualGuidedSetterLayoutUtilsRTLTest
    : public VisualGuidedSetterLayoutUtilsTest {
 private:
  base::test::ScopedRestoreICUDefaultLocale locale_{"ar"};
};

TEST_F(VisualGuidedSetterLayoutUtilsRTLTest, ArrowPointsTheOtherWay) {
  ASSERT_TRUE(base::i18n::IsRTL());

  gfx::Rect anchor(100, 200, 200, 200);
  gfx::Point start = ComputeArrowStartPointFromAnchor(anchor);
  EXPECT_EQ(start.x(), 100);
  EXPECT_EQ(start.y(), 300);

  gfx::Rect target(200, 300, 200, 2000);
  gfx::Point end = ComputeArrowEndPoint(target, kMeasuredOffsetPx);
  EXPECT_EQ(end.x(), 200);
  EXPECT_EQ(end.y(), 300 + kMeasuredOffsetPx);
}

}  // namespace visual_guided_setter
