// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/event_constants.h"

namespace {
class Waiter : public BrowserListObserver {
 public:
  static Browser* WaitForNewBrowser() {
    base::RunLoop loop;
    Waiter waiter(loop.QuitClosure());
    loop.Run();
    return waiter.browser_;
  }

 private:
  explicit Waiter(base::OnceClosure callback) : callback_{std::move(callback)} {
    BrowserList::AddObserver(this);
  }

  ~Waiter() override { BrowserList::RemoveObserver(this); }

  void OnBrowserAdded(Browser* browser) override {
    browser_ = browser;
    std::move(callback_).Run();
  }

  base::OnceClosure callback_;
  raw_ptr<Browser> browser_ = nullptr;
};
}  // namespace

// Unit tests for the left click menu and interaction with the menu items. There
// are integration tests in ./chrome_shelf_controller_browsertest.cc which
// covers different cases in AppShortcutShelfItemController::ItemSelected().
//
// We do tests on the terminal system web app because it uses tabbed window,
// which allows us to test app menu with either windows or tabs (shift click).
class AppShortcutShelfItemControllerBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    controller_ = ChromeShelfController::instance();
    ASSERT_TRUE(controller_);
  }

  void InstallApp() {
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();

    app_id_ = *ash::GetAppIdForSystemWebApp(browser()->profile(),
                                            ash::SystemWebAppType::TERMINAL);
    app_shelf_id_ = ash::ShelfID(app_id_);
    PinAppWithIDToShelf(app_id_);
  }

  Browser* LaunchApp() {
    guest_os::LaunchTerminal(browser()->profile(), display::kInvalidDisplayId,
                             crostini::DefaultContainerId());
    return Waiter::WaitForNewBrowser();
  }

  ash::ShelfItemDelegate* GetShelfItemDelegate() {
    return controller_->shelf_model()->GetShelfItemDelegate(app_shelf_id_);
  }

  ash::ShelfItemDelegate::AppMenuItems GetAppMenuItems(int event_flags) {
    return GetShelfItemDelegate()->GetAppMenuItems(event_flags,
                                                   base::NullCallback());
  }

  raw_ptr<ChromeShelfController, DanglingUntriaged> controller_;

  webapps::AppId app_id_;
  ash::ShelfID app_shelf_id_;
};

// Test interacting with the app menu without shift key down: the app menu has
// one entry per browser window.
IN_PROC_BROWSER_TEST_F(AppShortcutShelfItemControllerBrowserTest,
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
IN_PROC_BROWSER_TEST_F(AppShortcutShelfItemControllerBrowserTest,
                       AppMenuByTabs) {
  InstallApp();

  EXPECT_EQ(0u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Launch an app window. Terminal includes pinned home tab, and Linux tab.
  Browser* app_browser0 = LaunchApp();
  EXPECT_EQ(2u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Launch a new app window.
  Browser* app_browser1 = LaunchApp();
  EXPECT_EQ(4u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Open a new app tab in an existing app browser.
  chrome::NewTab(app_browser1);
  EXPECT_EQ(5u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());

  // Clicking the third item in the menu should activate the first tab in the
  // second window.
  app_browser1->tab_strip_model()->ActivateTabAt(1);
  app_browser1->window()->Minimize();
  GetAppMenuItems(ui::EF_SHIFT_DOWN);
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/2, ui::EF_NONE,
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
  EXPECT_EQ(3u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());
  GetShelfItemDelegate()->ExecuteCommand(/*from_context_menu=*/false,
                                         /*command_id=*/0, ui::EF_SHIFT_DOWN,
                                         display::kInvalidDisplayId);
  EXPECT_EQ(2u, GetAppMenuItems(ui::EF_SHIFT_DOWN).size());
}
