// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_shortcut_launcher_item_controller.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/events/event_constants.h"

// Unit tests for the left click menu and interaction with the menu items. There
// are integration tests in ./chrome_launcher_controller_browsertest.cc which
// covers different cases in AppShortcutLauncherItemController::ItemSelected().
//
// We do tests on the terminal system web app because it uses tabbed window,
// which allows us to test app menu with either windows or tabs (shift click).
class AppShortcutLauncherItemControllerBrowserTest
    : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(controller_);
  }

  void InstallApp() {
    web_app::WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();

    app_id_ = *web_app::GetAppIdForSystemWebApp(
        browser()->profile(), web_app::SystemAppType::TERMINAL);
    app_shelf_id_ = ash::ShelfID(app_id_);
    controller_->PinAppWithID(app_id_);
  }

  Browser* LaunchApp() {
    Browser* app_browser = web_app::LaunchSystemWebApp(
        browser()->profile(), web_app::SystemAppType::TERMINAL,
        GURL("chrome-untrusted://terminal/html/terminal.html"));
    DCHECK(app_browser);
    return app_browser;
  }

  ash::ShelfItemDelegate* GetShelfItemDelegate() {
    return controller_->shelf_model()->GetShelfItemDelegate(app_shelf_id_);
  }

  ash::ShelfItemDelegate::AppMenuItems GetAppMenuItems(int event_flags) {
    return GetShelfItemDelegate()->GetAppMenuItems(event_flags,
                                                   base::NullCallback());
  }

  ChromeLauncherController* controller_;

  web_app::AppId app_id_;
  ash::ShelfID app_shelf_id_;
};

// Test interacting with the app menu without shift key down: the app menu has
// one entry per browser window.
IN_PROC_BROWSER_TEST_F(AppShortcutLauncherItemControllerBrowserTest,
                       AppMenuByWindows) {
  InstallApp();

  EXPECT_EQ(0u, GetAppMenuItems(0).size());

  // Launch an app window.
  Browser* app_browser0 = LaunchApp();
  EXPECT_EQ(1u, GetAppMenuItems(0).size());

  // Launch two new app windows.
  Browser* app_browser1 = LaunchApp();
  LaunchApp();
  EXPECT_EQ(3u, GetAppMenuItems(0).size());

  // Open a new tab in an existing app browser. There are still 3 window items.
  AddBlankTabAndShow(app_browser1);
  EXPECT_EQ(3u, GetAppMenuItems(0).size());

  // Clicking the first item in the menu should activate the first app window.
  EXPECT_FALSE(app_browser0->window()->IsActive());
  GetAppMenuItems(0);
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/0, ui::EF_NONE,
                                         display::kInvalidDisplayId);
  EXPECT_TRUE(app_browser0->window()->IsActive());

  // Clicking on a closed item should not crash.
  GetAppMenuItems(0);
  CloseBrowserSynchronously(app_browser0);
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/0, ui::EF_NONE,
                                         display::kInvalidDisplayId);

  // Shift-clicking on an item should close it.
  EXPECT_EQ(2u, GetAppMenuItems(0).size());
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/0, ui::EF_SHIFT_DOWN,
                                         display::kInvalidDisplayId);
  EXPECT_EQ(1u, GetAppMenuItems(0).size());
}

// Test interacting with the app menu with shift key down: the app menu has
// one entry per tabs.
IN_PROC_BROWSER_TEST_F(AppShortcutLauncherItemControllerBrowserTest,
                       AppMenuByTabs) {
  InstallApp();

  EXPECT_EQ(0u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Launch an app window.
  Browser* app_browser0 = LaunchApp();
  EXPECT_EQ(1u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Launch a new app window.
  Browser* app_browser1 = LaunchApp();
  EXPECT_EQ(2u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Open a new app tab in an existing app browser. There are 3 tab items.
  chrome::NewTab(app_browser1);
  EXPECT_EQ(3u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Clicking the second item in the menu should activate the first tab in the
  // second window.
  app_browser1->tab_strip_model()->ActivateTabAt(1);
  app_browser1->window()->Minimize();
  GetAppMenuItems(ui::EF_SHIFT_DOWN);
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/1, ui::EF_NONE,
                                         display::kInvalidDisplayId);
  EXPECT_TRUE(app_browser1->window()->IsActive());
  EXPECT_TRUE(app_browser1->tab_strip_model()->active_index() == 0);

  // Clicking on a closed item should not crash.
  GetAppMenuItems(ui::EF_SHIFT_DOWN);
  CloseBrowserSynchronously(app_browser0);
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/0, ui::EF_NONE,
                                         display::kInvalidDisplayId);

  // Shift-clicking on a item should close it.
  EXPECT_EQ(2u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/0, ui::EF_SHIFT_DOWN,
                                         display::kInvalidDisplayId);
  EXPECT_EQ(1u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());
}
