// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/native_widget_types.h"

using ApplicationLaunchBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(ApplicationLaunchBrowserTest, CreateWindowInDisplay) {
  display::Screen* screen = display::Screen::GetScreen();
  // Create 2 displays.
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display::test::DisplayManagerTestApi display_manager_test(display_manager);
  display_manager_test.UpdateDisplay("800x750,801+0-800x750");
  int64_t display1 = screen->GetPrimaryDisplay().id();
  int64_t display2 = display_manager_test.GetSecondaryDisplay().id();
  EXPECT_EQ(2, screen->GetNumDisplays());

  // Primary display should hold browser() and be the default for new windows.
  gfx::NativeWindow window = browser()->window()->GetNativeWindow();
  EXPECT_EQ(display1, screen->GetDisplayNearestWindow(window).id());
  EXPECT_EQ(display1, screen->GetDisplayForNewWindows().id());

  // Create browser2 on display 2.
  apps::AppLaunchParams params("app_id",
                               apps::LaunchContainer::kLaunchContainerWindow,
                               WindowOpenDisposition::NEW_WINDOW,
                               apps::LaunchSource::kFromAppListGrid, display2);
  Browser* browser2 =
      CreateApplicationWindow(browser()->profile(), params, GURL());
  gfx::NativeWindow window2 = browser2->window()->GetNativeWindow();
  EXPECT_EQ(display2, screen->GetDisplayNearestWindow(window2).id());
}
