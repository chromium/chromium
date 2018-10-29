// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/view.h"
#include "ui/views/view_model.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace {

// Get the bounds of the browser shortcut item in the root window space.
gfx::Rect GetChromeIconBoundsForRootWindow(aura::Window* root) {
  ash::ShelfView* shelf_view =
      ash::Shelf::ForWindow(root)->GetShelfViewForTesting();
  const views::ViewModel* view_model = shelf_view->view_model_for_test();
  EXPECT_EQ(3, view_model->view_size());
  gfx::Rect bounds = view_model->view_at(2)->GetBoundsInScreen();
  wm::ConvertRectFromScreen(root, &bounds);
  return bounds;
}

// Ensure animations progress to give the shelf button a non-empty size.
void EnsureShelfInitialization() {
  aura::Window* root = ash::Shell::GetPrimaryRootWindow();
  ash::ShelfView* shelf_view =
      ash::Shelf::ForWindow(root)->GetShelfViewForTesting();
  ash::ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
  ASSERT_GT(GetChromeIconBoundsForRootWindow(root).height(), 0);
}

// Launch a new browser window by left-clicking the browser shortcut item.
void OpenBrowserUsingShelfOnRootWindow(aura::Window* root) {
  ui::test::EventGenerator generator(root);
  gfx::Point center = GetChromeIconBoundsForRootWindow(root).CenterPoint();
  generator.MoveMouseTo(center);
  generator.ClickLeftButton();
  // Ash notifies Chrome that the browser shortcut item was selected.
  ash::Shell::Get()->shelf_controller()->FlushForTesting();
  // Chrome replies to Ash that a new window was opened.
  ChromeLauncherController::instance()->FlushForTesting();
}

// Launch a new browser window by clicking the "New window" context menu item.
void OpenBrowserUsingContextMenuOnRootWindow(aura::Window* root) {
  ui::test::EventGenerator generator(root);
  gfx::Point chrome_icon = GetChromeIconBoundsForRootWindow(root).CenterPoint();
  generator.MoveMouseTo(chrome_icon);
  generator.PressRightButton();

  // Ash notifies Chrome that the browser shortcut item was right-clicked.
  ash::Shell::Get()->shelf_controller()->FlushForTesting();
  // Chrome replies to Ash with the context menu items to display.
  ChromeLauncherController::instance()->FlushForTesting();

  // Move the cursor up to the "New window" menu option - assumes menu content.
  const int offset =
      // Top half of the button we just clicked on.
      ash::ShelfConstants::button_size() / 2 +
      // Space between shelf top and menu bottom. Here we get this menu with
      // a right-click but long-pressing yields the same result. All menus
      // here use a touchable layout.
      views::MenuConfig::instance().touchable_anchor_offset +
      // 2 menu items we don't want, and go over part of the one we want.
      2.2 * views::MenuConfig::instance().touchable_menu_height;
  generator.MoveMouseBy(0, -offset);
  generator.ReleaseRightButton();

  // Ash notifies Chrome's ShelfItemDelegate that the menu item was selected.
  ash::Shell::Get()->shelf_controller()->FlushForTesting();
}

class WindowSizerTest : public InProcessBrowserTest {
 public:
  WindowSizerTest() {}
  ~WindowSizerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make screens sufficiently wide to host 2 browsers side by side.
    command_line->AppendSwitchASCII("ash-host-window-bounds",
                                    "600x600,601+0-600x600");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowSizerTest);
};

IN_PROC_BROWSER_TEST_F(WindowSizerTest, OpenBrowserUsingShelfItem) {
  // Don't shutdown when closing the last browser window.
  ScopedKeepAlive test_keep_alive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                                  KeepAliveRestartOption::DISABLED);
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  BrowserList* browser_list = BrowserList::GetInstance();
  EnsureShelfInitialization();

  EXPECT_EQ(1u, browser_list->size());
  // Close the browser window so that clicking the icon creates a new window.
  CloseBrowserSynchronously(browser_list->get(0));
  EXPECT_EQ(0u, browser_list->size());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());

  OpenBrowserUsingShelfOnRootWindow(root_windows[1]);

  // A new browser window should be opened on the 2nd display.
  display::Screen* screen = display::Screen::GetScreen();
  std::pair<display::Display, display::Display> displays =
      ui_test_utils::GetDisplays(screen);
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(displays.second.id(),
            screen
                ->GetDisplayNearestWindow(
                    browser_list->get(0)->window()->GetNativeWindow())
                .id());
  EXPECT_EQ(root_windows[1], ash::Shell::GetRootWindowForNewWindows());

  // Close the browser window so that clicking the icon creates a new window.
  CloseBrowserSynchronously(browser_list->get(0));
  EXPECT_EQ(0u, browser_list->size());

  OpenBrowserUsingShelfOnRootWindow(root_windows[0]);

  // A new browser window should be opened on the 1st display.
  EXPECT_EQ(1u, browser_list->size());
  EXPECT_EQ(displays.first.id(),
            screen
                ->GetDisplayNearestWindow(
                    browser_list->get(0)->window()->GetNativeWindow())
                .id());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
}

IN_PROC_BROWSER_TEST_F(WindowSizerTest, OpenBrowserUsingContextMenu) {
  // Don't shutdown when closing the last browser window.
  ScopedKeepAlive test_keep_alive(KeepAliveOrigin::BROWSER_PROCESS_CHROMEOS,
                                  KeepAliveRestartOption::DISABLED);
  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();
  BrowserList* browser_list = BrowserList::GetInstance();
  EnsureShelfInitialization();

  views::MenuController::TurnOffMenuSelectionHoldForTest();

  ASSERT_EQ(1u, browser_list->size());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
  CloseBrowserSynchronously(browser_list->get(0));

  OpenBrowserUsingContextMenuOnRootWindow(root_windows[1]);

  // A new browser window should be opened on the 2nd display.
  display::Screen* screen = display::Screen::GetScreen();
  std::pair<display::Display, display::Display> displays =
      ui_test_utils::GetDisplays(screen);
  ASSERT_EQ(1u, browser_list->size());
  EXPECT_EQ(displays.second.id(),
            screen
                ->GetDisplayNearestWindow(
                    browser_list->get(0)->window()->GetNativeWindow())
                .id());
  EXPECT_EQ(root_windows[1], ash::Shell::GetRootWindowForNewWindows());

  CloseBrowserSynchronously(browser_list->get(0));
  OpenBrowserUsingContextMenuOnRootWindow(root_windows[0]);

  // A new browser window should be opened on the 1st display.
  ASSERT_EQ(1u, browser_list->size());
  EXPECT_EQ(displays.first.id(),
            screen
                ->GetDisplayNearestWindow(
                    browser_list->get(0)->window()->GetNativeWindow())
                .id());
  EXPECT_EQ(root_windows[0], ash::Shell::GetRootWindowForNewWindows());
}

}  // namespace
