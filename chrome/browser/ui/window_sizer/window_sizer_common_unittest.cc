// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_common_unittest.h"

#include <stddef.h>

#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/display/screen.h"
#include "ui/display/screen_base.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace {

class TestScreen : public display::ScreenBase {
 public:
  TestScreen() : previous_screen_(display::Screen::GetScreen()) {
    display::Screen::SetScreenInstance(this);
  }

  TestScreen(const TestScreen&) = delete;
  TestScreen& operator=(const TestScreen&) = delete;

  ~TestScreen() override {
    display::Screen::SetScreenInstance(previous_screen_);
  }

  void AddDisplay(const gfx::Rect& bounds,
                  const gfx::Rect& work_area) {
    const int num_displays = GetNumDisplays();
    display::Display display(num_displays, bounds);
    display.set_work_area(work_area);
    ProcessDisplayChanged(display, num_displays == 0);
  }

 private:
  raw_ptr<display::Screen> previous_screen_;
};

}  // namespace

TestStateProvider::TestStateProvider()
    : has_persistent_data_(false),
      persistent_show_state_(ui::mojom::WindowShowState::kDefault),
      has_last_active_data_(false),
      last_active_show_state_(ui::mojom::WindowShowState::kDefault) {}

void TestStateProvider::SetPersistentState(
    const gfx::Rect& bounds,
    const gfx::Rect& work_area,
    ui::mojom::WindowShowState show_state) {
  persistent_bounds_ = bounds;
  persistent_work_area_ = work_area;
  persistent_show_state_ = show_state;
  has_persistent_data_ = true;
}

void TestStateProvider::SetLastActiveState(
    const gfx::Rect& bounds,
    ui::mojom::WindowShowState show_state) {
  last_active_bounds_ = bounds;
  last_active_show_state_ = show_state;
  has_last_active_data_ = true;
}

bool TestStateProvider::GetPersistentState(
    gfx::Rect* bounds,
    gfx::Rect* saved_work_area,
    ui::mojom::WindowShowState* show_state) const {
  DCHECK(show_state);
  *bounds = persistent_bounds_;
  *saved_work_area = persistent_work_area_;
  if (*show_state == ui::mojom::WindowShowState::kDefault) {
    *show_state = persistent_show_state_;
  }
  return has_persistent_data_;
}

bool TestStateProvider::GetLastActiveWindowState(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  DCHECK(show_state);
  *bounds = last_active_bounds_;
  if (*show_state == ui::mojom::WindowShowState::kDefault) {
    *show_state = last_active_show_state_;
  }
  return has_last_active_data_;
}

WindowSizerTestUtil& WindowSizerTestUtil::WithMonitorBounds(
    const gfx::Rect& monitor1_bounds,
    const gfx::Rect& monitor2_bounds) {
  monitor1_bounds_ = monitor1_bounds;
  monitor2_bounds_ = monitor2_bounds;
  return *this;
}

WindowSizerTestUtil& WindowSizerTestUtil::WithMonitorWorkArea(
    const gfx::Rect& monitor1_work_area) {
  monitor1_work_area_ = monitor1_work_area;
  return *this;
}

WindowSizerTestUtil& WindowSizerTestUtil::WithLastActiveBounds(
    const gfx::Rect& bounds) {
  last_active_bounds_ = bounds;
  return *this;
}

WindowSizerTestUtil& WindowSizerTestUtil::WithPersistedBounds(
    const gfx::Rect& bounds) {
  persisted_bounds_ = bounds;
  return *this;
}

WindowSizerTestUtil& WindowSizerTestUtil::WithPersistedWorkArea(
    const gfx::Rect& work_area) {
  persisted_work_area_ = work_area;
  return *this;
}

WindowSizerTestUtil& WindowSizerTestUtil::WithSpecifiedBounds(
    const gfx::Rect& bounds) {
  specified_bounds_ = bounds;
  return *this;
}

gfx::Rect WindowSizerTestUtil::GetWindowBounds() {
  DCHECK(!monitor1_bounds_.IsEmpty());
  TestScreen test_screen;
  test_screen.AddDisplay(monitor1_bounds_, monitor1_work_area_.IsEmpty()
                                               ? monitor1_bounds_
                                               : monitor1_work_area_);
  if (!monitor2_bounds_.IsEmpty())
    test_screen.AddDisplay(monitor2_bounds_, monitor2_bounds_);

  auto provider = std::make_unique<TestStateProvider>();
  if (!persisted_bounds_.IsEmpty() || !persisted_work_area_.IsEmpty())
    provider->SetPersistentState(persisted_bounds_, persisted_work_area_,
                                 ui::mojom::WindowShowState::kDefault);
  if (!last_active_bounds_.IsEmpty())
    provider->SetLastActiveState(last_active_bounds_,
                                 ui::mojom::WindowShowState::kDefault);

  ui::mojom::WindowShowState ignored;
  gfx::Rect out_bounds;
  WindowSizer::GetBrowserWindowBoundsAndShowState(
      std::move(provider), specified_bounds_, /*browser=*/nullptr, &out_bounds,
      &ignored);
  return out_bounds;
}

#if !BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Passing null for the browser parameter of GetWindowBounds makes the test skip
// all Ash-specific logic, so there's no point running this on Chrome OS.
TEST(WindowSizerTestCommon,
     PersistedWindowOffscreenWithNonAggressiveRepositioning) {
  { // off the left but the minimum visibility condition is barely satisfied
    // without relocaiton.
    gfx::Rect initial_bounds(-470, 50, 500, 400);

    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the left and the minimum visibility condition is satisfied by
    // relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-471, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the top
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, -370, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // off the right but the minimum visibility condition is barely satisified
    // without relocation.
    gfx::Rect initial_bounds(994, 50, 500, 400);
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the right and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(995, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottom but the minimum visibility condition is barely satisified
    // without relocation.
    gfx::Rect initial_bounds(50, 738, 500, 400);
    gfx::Rect window_bounds = WindowSizerTestUtil()
                                  .WithMonitorBounds(p1024x768)
                                  .WithPersistedBounds(initial_bounds)
                                  .GetWindowBounds();
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the bottom and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, 739, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 738 /* not 739 */, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the topleft
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-471, -371, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */, 0, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the topright and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(995, -371, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 0, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottomleft and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-471, 739, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottomright and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(995, 739, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994 /* not 995 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off left
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-700, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(-470 /* not -700 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off left (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(-700, 50, 500, 400))
            .WithPersistedWorkArea(left_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ("0,50 500x400", window_bounds.ToString());
  }

  { // entirely off top
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, -500, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // entirely off top (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, -500, 500, 400))
            .WithPersistedWorkArea(top_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // entirely off right
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(1200, 50, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(994 /* not 1200 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off right (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(1200, 50, 500, 400))
            .WithPersistedWorkArea(right_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ("524,50 500x400", window_bounds.ToString());
  }

  { // entirely off bottom
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, 800, 500, 400))
            .GetWindowBounds();
    EXPECT_EQ(gfx::Rect(50, 738 /* not 800 */, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off bottom (monitor was detached since last run)
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithPersistedBounds(gfx::Rect(50, 800, 500, 400))
            .WithPersistedWorkArea(bottom_s1024x768)
            .GetWindowBounds();
    EXPECT_EQ("50,368 500x400", window_bounds.ToString());
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST(WindowSizerTestCommon, AdjustFitSize) {
  { // Check that the window gets resized to the screen.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithSpecifiedBounds(gfx::Rect(-10, -10, 1024 + 20, 768 + 20))
            .GetWindowBounds();
    EXPECT_EQ("0,0 1024x768", window_bounds.ToString());
  }

  { // Check that a window which hangs out of the screen get moved back in.
    gfx::Rect window_bounds =
        WindowSizerTestUtil()
            .WithMonitorBounds(p1024x768)
            .WithSpecifiedBounds(gfx::Rect(1020, 700, 100, 100))
            .GetWindowBounds();
    EXPECT_EQ("924,668 100x100", window_bounds.ToString());
  }
}

#endif  // !BUILDFLAG(IS_MAC)
