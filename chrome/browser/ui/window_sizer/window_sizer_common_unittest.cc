// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer_common_unittest.h"

#include <stddef.h>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
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
  ~TestScreen() override {
    display::Screen::SetScreenInstance(previous_screen_);
  }

  void AddDisplay(const gfx::Rect& bounds,
                  const gfx::Rect& work_area) {
    display::Display display(display_list().displays().size(), bounds);
    display.set_work_area(work_area);
    display_list().AddDisplay(display,
                              display_list().displays().empty()
                                  ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
  }

 private:
  display::Screen* previous_screen_;

  DISALLOW_COPY_AND_ASSIGN(TestScreen);
};

}  // namespace

TestStateProvider::TestStateProvider()
    : has_persistent_data_(false),
      persistent_show_state_(ui::SHOW_STATE_DEFAULT),
      has_last_active_data_(false),
      last_active_show_state_(ui::SHOW_STATE_DEFAULT) {}

void TestStateProvider::SetPersistentState(const gfx::Rect& bounds,
                                           const gfx::Rect& work_area,
                                           ui::WindowShowState show_state) {
  persistent_bounds_ = bounds;
  persistent_work_area_ = work_area;
  persistent_show_state_ = show_state;
  has_persistent_data_ = true;
}

void TestStateProvider::SetLastActiveState(const gfx::Rect& bounds,
                                           ui::WindowShowState show_state) {
  last_active_bounds_ = bounds;
  last_active_show_state_ = show_state;
  has_last_active_data_ = true;
}

bool TestStateProvider::GetPersistentState(
    gfx::Rect* bounds,
    gfx::Rect* saved_work_area,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  *bounds = persistent_bounds_;
  *saved_work_area = persistent_work_area_;
  if (*show_state == ui::SHOW_STATE_DEFAULT)
    *show_state = persistent_show_state_;
  return has_persistent_data_;
}

bool TestStateProvider::GetLastActiveWindowState(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  *bounds = last_active_bounds_;
  if (*show_state == ui::SHOW_STATE_DEFAULT)
    *show_state = last_active_show_state_;
  return has_last_active_data_;
}

// static
void WindowSizerTestUtil::GetWindowBounds(const gfx::Rect& monitor1_bounds,
                                          const gfx::Rect& monitor1_work_area,
                                          const gfx::Rect& monitor2_bounds,
                                          const gfx::Rect& bounds,
                                          const gfx::Rect& work_area,
                                          Source source,
                                          const Browser* browser,
                                          const gfx::Rect& passed_in,
                                          gfx::Rect* out_bounds) {
  TestScreen test_screen;
  test_screen.AddDisplay(monitor1_bounds, monitor1_work_area);
  if (!monitor2_bounds.IsEmpty())
    test_screen.AddDisplay(monitor2_bounds, monitor2_bounds);

  auto provider = std::make_unique<TestStateProvider>();
  if (source == PERSISTED || source == BOTH)
    provider->SetPersistentState(bounds, work_area, ui::SHOW_STATE_DEFAULT);
  if (source == LAST_ACTIVE || source == BOTH)
    provider->SetLastActiveState(bounds, ui::SHOW_STATE_DEFAULT);

  ui::WindowShowState ignored;
  WindowSizer sizer(std::move(provider), browser);
  sizer.DetermineWindowBoundsAndShowState(passed_in, out_bounds, &ignored);
}

#if !defined(OS_MACOSX)

#if !defined(OS_CHROMEOS)
// Passing null for the browser parameter of GetWindowBounds makes the test skip
// all Ash-specific logic, so there's no point running this on Chrome OS.
TEST(WindowSizerTestCommon,
     PersistedWindowOffscreenWithNonAggressiveRepositioning) {
  { // off the left but the minimum visibility condition is barely satisfied
    // without relocaiton.
    gfx::Rect initial_bounds(-470, 50, 500, 400);

    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                                         initial_bounds, gfx::Rect(), PERSISTED,
                                         NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the left and the minimum visibility condition is satisfied by
    // relocation.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(-471, 50, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the top
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(50, -370, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // off the right but the minimum visibility condition is barely satisified
    // without relocation.
    gfx::Rect initial_bounds(994, 50, 500, 400);

    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                                         initial_bounds, gfx::Rect(), PERSISTED,
                                         NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the right and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(995, 50, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottom but the minimum visibility condition is barely satisified
    // without relocation.
    gfx::Rect initial_bounds(50, 738, 500, 400);

    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(p1024x768, p1024x768, gfx::Rect(),
                                         initial_bounds, gfx::Rect(), PERSISTED,
                                         NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(initial_bounds.ToString(), window_bounds.ToString());
  }

  { // off the bottom and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(50, 739, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(50, 738 /* not 739 */, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the topleft
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(-471, -371, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */, 0, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the topright and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(995, -371, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */, 0, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottomleft and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(-471, 739, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -471 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // off the bottomright and the minimum visibility condition is satisified by
    // relocation.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(995, 739, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 995 */,
                        738 /* not 739 */,
                        500,
                        400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off left
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(-700, 50, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(-470 /* not -700 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off left (monitor was detached since last run)
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(-700, 50, 500, 400),
        left_s1024x768, PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("0,50 500x400", window_bounds.ToString());
  }

  { // entirely off top
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(50, -500, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // entirely off top (monitor was detached since last run)
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(50, -500, 500, 400),
        top_s1024x768, PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,0 500x400", window_bounds.ToString());
  }

  { // entirely off right
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(1200, 50, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(994 /* not 1200 */, 50, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off right (monitor was detached since last run)
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(1200, 50, 500, 400),
        right_s1024x768, PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("524,50 500x400", window_bounds.ToString());
  }

  { // entirely off bottom
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(50, 800, 500, 400),
        gfx::Rect(), PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ(gfx::Rect(50, 738 /* not 800 */, 500, 400).ToString(),
              window_bounds.ToString());
  }

  { // entirely off bottom (monitor was detached since last run)
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(50, 800, 500, 400),
        bottom_s1024x768, PERSISTED, NULL, gfx::Rect(), &window_bounds);
    EXPECT_EQ("50,368 500x400", window_bounds.ToString());
  }
}
#endif  // !defined(OS_CHROMEOS)

// Test that the window is sized appropriately for the first run experience
// where the default window bounds calculation is invoked.
TEST(WindowSizerTestCommon, AdjustFitSize) {
  { // Check that the window gets resized to the screen.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(), gfx::Rect(), DEFAULT,
        NULL, gfx::Rect(-10, -10, 1024 + 20, 768 + 20), &window_bounds);
    EXPECT_EQ("0,0 1024x768", window_bounds.ToString());
  }

  { // Check that a window which hangs out of the screen get moved back in.
    gfx::Rect window_bounds;
    WindowSizerTestUtil::GetWindowBounds(
        p1024x768, p1024x768, gfx::Rect(), gfx::Rect(), gfx::Rect(), DEFAULT,
        NULL, gfx::Rect(1020, 700, 100, 100), &window_bounds);
    EXPECT_EQ("924,668 100x100", window_bounds.ToString());
  }
}

#endif // defined(OS_MACOSX)
