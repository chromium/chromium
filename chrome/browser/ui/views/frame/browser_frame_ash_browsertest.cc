// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_ash.h"

#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {

class BrowserTestParam : public InProcessBrowserTest,
                         public testing::WithParamInterface<bool> {
 public:
  BrowserTestParam() = default;

  BrowserTestParam(const BrowserTestParam&) = delete;
  BrowserTestParam& operator=(const BrowserTestParam&) = delete;

  ~BrowserTestParam() override = default;

  bool CreateV1App() { return GetParam(); }
};

}  // namespace

// Test that in Chrome OS, for app browser (v1app) windows, the auto management
// logic is disabled while for tabbed browser windows, it is enabled.
IN_PROC_BROWSER_TEST_P(BrowserTestParam,
                       TabbedOrAppBrowserWindowAutoManagementTest) {
  // Default |browser()| is not used by this test.
  browser()->window()->Close();

  // Open a new browser window (app or tabbed depending on a parameter).
  bool is_test_app = CreateV1App();
  Browser::CreateParams params =
      is_test_app ? Browser::CreateParams::CreateForApp(
                        "test_browser_app", true /* trusted_source */,
                        gfx::Rect(), browser()->profile(), true)
                  : Browser::CreateParams(browser()->profile(), true);
  gfx::Rect original_bounds(gfx::Rect(150, 250, 510, 150));
  params.initial_show_state = ui::mojom::WindowShowState::kNormal;
  params.initial_bounds = original_bounds;
  Browser* browser = Browser::Create(params);
  browser->window()->Show();

  // The bounds passed via |initial_bounds| should be respected regardless of
  // the window type.
  EXPECT_EQ(original_bounds, browser->window()->GetBounds());

  // Close the browser and re-create the browser window with the same app name.
  // Don't provide initial bounds. The bounds should have been saved, but for
  // tabbed windows, the position should be auto-managed.
  browser->window()->Close();
  params.initial_bounds = gfx::Rect();
  browser = Browser::Create(params);
  browser->window()->Show();

  // For tabbed browser window, it will be centered to work area by auto window
  // management logic; for app browser window, it will remain the given bounds.
  gfx::Rect expectation = original_bounds;
  if (!is_test_app) {
    expectation =
        display::Screen::GetScreen()
            ->GetDisplayNearestPoint(browser->window()->GetBounds().origin())
            .work_area();
    expectation.ClampToCenteredSize(original_bounds.size());
    expectation.set_y(original_bounds.y());
  }

  EXPECT_EQ(expectation, browser->window()->GetBounds())
      << (is_test_app ? "for app window" : "for tabbed browser window");
}

using BrowserFrameAshTest = InProcessBrowserTest;

// Tests that the correct bounds are being saved when a snapped window is
// closed.
IN_PROC_BROWSER_TEST_F(BrowserFrameAshTest, SnappedWindowSaveBounds) {
  auto* profile = browser()->profile();

  // Get the params using the same profile.
  Browser* browser = CreateBrowser(profile);
  aura::Window* window = browser->window()->GetNativeWindow();
  const gfx::Rect restored_bounds(600, 600);
  window->SetBounds(restored_bounds);

  // Snap the window to the left.
  const ash::WindowSnapWMEvent left_snap_event(ash::WM_EVENT_SNAP_PRIMARY);
  ash::WindowState::Get(window)->OnWMEvent(&left_snap_event);
  const gfx::Size snapped_size = window->GetBoundsInScreen().size();

  browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(browser);

  // Recreate the browser window. Test that the bounds are the same as the
  // snapped size (position has been shifted by the ash auto window positioner).
  Browser* new_browser = CreateBrowser(profile);
  new_browser->window()->Show();
  aura::Window* new_window = new_browser->window()->GetNativeWindow();
  EXPECT_EQ(snapped_size, new_window->GetBoundsInScreen().size());
}

INSTANTIATE_TEST_SUITE_P(BrowserTestTabbedOrApp,
                         BrowserTestParam,
                         testing::Bool());
