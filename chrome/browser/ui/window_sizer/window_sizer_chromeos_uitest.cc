// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

// Get the bounds of the browser shortcut item in screen space.
gfx::Rect GetChromeIconBoundsInScreen(aura::Window* root) {
  ash::ShelfView* shelf_view =
      ash::Shelf::ForWindow(root)->GetShelfViewForTesting();
  const views::ViewModel* view_model = shelf_view->view_model_for_test();
  EXPECT_EQ(1u, view_model->view_size());
  gfx::Rect bounds = view_model->view_at(0)->GetBoundsInScreen();
  return bounds;
}

// Ensure animations progress to give the shelf button a non-empty size.
void EnsureShelfInitialization() {
  aura::Window* root = ash::Shell::GetPrimaryRootWindow();
  ash::ShelfView* shelf_view =
      ash::Shelf::ForWindow(root)->GetShelfViewForTesting();
  ash::ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
  ASSERT_GT(GetChromeIconBoundsInScreen(root).height(), 0);
}

// Launch a new browser window by left-clicking the browser shortcut item.
void OpenBrowserUsingShelfOnRootWindow(aura::Window* root) {
  ui::test::EventGenerator generator(root);
  gfx::Point center = GetChromeIconBoundsInScreen(root).CenterPoint();
  generator.MoveMouseTo(center);
  generator.ClickLeftButton();
}

class WindowSizerTest : public InProcessBrowserTest {
 public:
  WindowSizerTest() = default;
  WindowSizerTest(const WindowSizerTest&) = delete;
  WindowSizerTest& operator=(const WindowSizerTest&) = delete;
  ~WindowSizerTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "800x600,801+0-800x600");
  }

  gfx::ScopedAnimationDurationScaleMode zero_duration_{
      gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION};
};

// TODO(crbug.com/40113148): Test is flaky on sanitizers.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_OpenBrowserUsingShelfItem DISABLED_OpenBrowserUsingShelfItem
#else
#define MAYBE_OpenBrowserUsingShelfItem OpenBrowserUsingShelfItem
#endif
IN_PROC_BROWSER_TEST_F(WindowSizerTest, MAYBE_OpenBrowserUsingShelfItem) {
  // Don't shutdown when closing the last browser window.
  ScopedKeepAlive test_keep_alive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                                  KeepAliveRestartOption::DISABLED);
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  EnsureShelfInitialization();

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  // Close the browser window so that clicking the icon creates a new window.
  CloseBrowserSynchronously(
      GetLastActiveBrowserWindowInterfaceWithAnyProfile());
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());

  auto browser_created_observer =
      std::make_optional<ui_test_utils::BrowserCreatedObserver>();
  OpenBrowserUsingShelfOnRootWindow(root_windows[1]);
  BrowserWindowInterface* new_browser = browser_created_observer->Wait();

  // A new browser window should be opened on the 2nd display.
  display::Screen* screen = display::Screen::Get();
  std::pair<display::Display, display::Display> displays =
      ui_test_utils::GetDisplays(screen);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(
      displays.second.id(),
      screen
          ->GetDisplayNearestWindow(new_browser->GetWindow()->GetNativeWindow())
          .id());
  EXPECT_EQ(root_windows[1], ash::Shell::GetRootWindowForNewWindows());

  // Close the browser window so that clicking the icon creates a new window.
  CloseBrowserSynchronously(new_browser);
  new_browser = nullptr;
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());

  browser_created_observer.emplace();
  OpenBrowserUsingShelfOnRootWindow(root_windows[0]);
  new_browser = browser_created_observer->Wait();

  // A new browser window should be opened on the 1st display.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(
      displays.first.id(),
      screen
          ->GetDisplayNearestWindow(new_browser->GetWindow()->GetNativeWindow())
          .id());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
}

}  // namespace
