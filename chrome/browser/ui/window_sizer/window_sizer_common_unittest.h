// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
#define CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_

#include "chrome/browser/ui/window_sizer/window_sizer.h"
#include "chrome/test/base/test_browser_window.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/gfx/geometry/rect.h"

// Some standard primary monitor sizes (no task bar).
static const gfx::Rect p1024x768(0, 0, 1024, 768);
static const gfx::Rect p1280x1024(0, 0, 1280, 1024);
static const gfx::Rect p1600x1200(0, 0, 1600, 1200);
static const gfx::Rect p1680x1050(0, 0, 1680, 1050);
static const gfx::Rect p1920x1200(0, 0, 1920, 1200);

// Represents a 1024x768 monitor that is the secondary monitor, arranged to
// the immediate left of the primary 1024x768 monitor.
static const gfx::Rect left_s1024x768(-1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is the secondary monitor, arranged to
// the immediate right of the primary 1024x768 monitor.
static const gfx::Rect right_s1024x768(1024, 0, 1024, 768);

// Represents a 1024x768 monitor that is the secondary monitor, arranged to
// the immediate top of the primary 1024x768 monitor.
static const gfx::Rect top_s1024x768(0, -768, 1024, 768);

// Represents a 1024x768 monitor that is the secondary monitor, arranged to
// the immediate bottom of the primary 1024x768 monitor.
static const gfx::Rect bottom_s1024x768(0, 768, 1024, 768);

// Represents a 1600x1200 monitor that is the secondary monitor, arranged to
// the immediate bottom of the primary 1600x1200 monitor.
static const gfx::Rect bottom_s1600x1200(0, 1200, 1600, 1200);

// The work area for 1024x768 monitors with different taskbar orientations.
static const gfx::Rect taskbar_bottom_work_area(0, 0, 1024, 734);
static const gfx::Rect taskbar_top_work_area(0, 34, 1024, 734);
static const gfx::Rect taskbar_left_work_area(107, 0, 917, 768);
static const gfx::Rect taskbar_right_work_area(0, 0, 917, 768);

// Testing implementation of WindowSizer::StateProvider that we use to fake
// persistent storage and existing windows.
class TestStateProvider : public WindowSizer::StateProvider {
 public:
  TestStateProvider();

  TestStateProvider(const TestStateProvider&) = delete;
  TestStateProvider& operator=(const TestStateProvider&) = delete;

  ~TestStateProvider() override {}

  void SetPersistentState(const gfx::Rect& bounds,
                          const gfx::Rect& work_area,
                          ui::mojom::WindowShowState show_state);
  void SetLastActiveState(const gfx::Rect& bounds,
                          ui::mojom::WindowShowState show_state);

  // Overridden from WindowSizer::StateProvider:
  bool GetPersistentState(
      gfx::Rect* bounds,
      gfx::Rect* saved_work_area,
      ui::mojom::WindowShowState* show_state) const override;
  bool GetLastActiveWindowState(
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;

 private:
  gfx::Rect persistent_bounds_;
  gfx::Rect persistent_work_area_;
  bool has_persistent_data_;
  ui::mojom::WindowShowState persistent_show_state_;

  gfx::Rect last_active_bounds_;
  bool has_last_active_data_;
  ui::mojom::WindowShowState last_active_show_state_;
};

// Builder class for setting up window sizer test state with a single statement.
class WindowSizerTestUtil {
 public:
  WindowSizerTestUtil() = default;
  WindowSizerTestUtil(const WindowSizerTestUtil&) = delete;
  WindowSizerTestUtil& operator=(const WindowSizerTestUtil&) = delete;

  // Set up monitor bounds. Tests have to always call this method with bounds
  // for at least one monitor.
  WindowSizerTestUtil& WithMonitorBounds(const gfx::Rect& monitor1_bounds,
                                         const gfx::Rect& monitor2_bounds = {});

  // Override the monitor work area. By default work area will be equal to the
  // monitor bounds.
  WindowSizerTestUtil& WithMonitorWorkArea(const gfx::Rect& monitor1_work_area);

  WindowSizerTestUtil& WithLastActiveBounds(const gfx::Rect& bounds);
  WindowSizerTestUtil& WithPersistedBounds(const gfx::Rect& bounds);
  WindowSizerTestUtil& WithPersistedWorkArea(const gfx::Rect& work_area);

  WindowSizerTestUtil& WithSpecifiedBounds(const gfx::Rect& bounds);

  gfx::Rect GetWindowBounds();

 private:
  gfx::Rect monitor1_bounds_;
  gfx::Rect monitor1_work_area_;
  gfx::Rect monitor2_bounds_;
  gfx::Rect last_active_bounds_;
  gfx::Rect persisted_bounds_;
  gfx::Rect persisted_work_area_;
  gfx::Rect specified_bounds_;
};

#endif  // CHROME_BROWSER_UI_WINDOW_SIZER_WINDOW_SIZER_COMMON_UNITTEST_H_
