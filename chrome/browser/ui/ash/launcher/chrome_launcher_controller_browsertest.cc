// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_test_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_app_window_icon_observer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"

using ash::Shelf;
using extensions::AppWindow;
using extensions::Extension;
using content::WebContents;

namespace {

ash::ShelfAction SelectItem(const ash::ShelfID& id,
                            ui::EventType event_type = ui::ET_MOUSE_PRESSED,
                            int64_t display_id = display::kInvalidDisplayId) {
  ash::ShelfAction action = SelectShelfItem(id, event_type, display_id);
  // Wait for window manager to stabilize.
  aura::test::WaitForAllChangesToComplete();
  return action;
}

class TestEvent : public ui::Event {
 public:
  explicit TestEvent(ui::EventType type)
      : ui::Event(type, base::TimeTicks(), 0) {}
  ~TestEvent() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestEvent);
};

// Find the browser that associated with |app_name|.
Browser* FindBrowserForApp(const std::string& app_name) {
  for (auto* browser : *BrowserList::GetInstance()) {
    std::string browser_app_name =
        web_app::GetAppIdFromApplicationName(browser->app_name());
    if (browser_app_name == app_name)
      return browser;
  }
  return nullptr;
}

// Close |app_browser| and wait until it's closed.
void CloseAppBrowserWindow(Browser* app_browser) {
  content::WindowedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(app_browser));
  app_browser->window()->Close();
  close_observer.Wait();
}

// Close browsers from context menu
void CloseBrowserWindow(Browser* browser,
                        LauncherContextMenu* menu,
                        int close_command) {
  content::WindowedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, content::Source<Browser>(browser));
  // Note that event_flag is never used inside function ExecuteCommand.
  menu->ExecuteCommand(close_command, ui::EventFlags::EF_NONE);
  close_observer.Wait();
}

// Activates a window. Waits until ash and ui services are aware of the change.
void ActivateWindow(ui::BaseWindow* window) {
  window->Activate();
  aura::test::WaitForAllChangesToComplete();
}

// Maximizes a window. Waits until ash and ui services are aware of the change.
void MaximizeWindow(ui::BaseWindow* window) {
  window->Maximize();
  aura::test::WaitForAllChangesToComplete();
}

// Minimizes a window. Waits until ash and ui services are aware of the change.
void MinimizeWindow(ui::BaseWindow* window) {
  window->Minimize();
  aura::test::WaitForAllChangesToComplete();
}

// Restores a window. Waits until ash and ui services are aware of the change.
void RestoreWindow(ui::BaseWindow* window) {
  window->Restore();
  aura::test::WaitForAllChangesToComplete();
}

// Returns the shelf view for the primary display.
// TODO(jamescook): Convert users of this function to the mojo shelf test API.
ash::ShelfView* GetPrimaryShelfView() {
  return ash::Shelf::ForWindow(ash::Shell::GetPrimaryRootWindow())
      ->GetShelfViewForTesting();
}

int64_t GetDisplayIdForBrowserWindow(BrowserWindow* window) {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window->GetNativeWindow())
      .id();
}

}  // namespace

class LauncherPlatformAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  LauncherPlatformAppBrowserTest() : controller_(nullptr) {}

  ~LauncherPlatformAppBrowserTest() override {}

  void SetUpOnMainThread() override {
    // Ensure ash starts the session and creates the shelf and controller.
    SessionControllerClient::FlushForTesting();

    controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(controller_);
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
  }

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  ash::ShelfID CreateAppShortcutLauncherItem(const ash::ShelfID& shelf_id) {
    return controller_->CreateAppShortcutLauncherItem(
        shelf_id, shelf_model()->item_count());
  }

  // Returns the last item in the shelf.
  const ash::ShelfItem& GetLastLauncherItem() {
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  ash::ShelfItemDelegate* GetShelfItemDelegate(const ash::ShelfID& id) {
    return shelf_model()->GetShelfItemDelegate(id);
  }

  ChromeLauncherController* controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherPlatformAppBrowserTest);
};

enum RipOffCommand {
  // Drag the item off the shelf and let the mouse go.
  RIP_OFF_ITEM,
  // Drag the item off the shelf, move the mouse back and then let go.
  RIP_OFF_ITEM_AND_RETURN,
  // Drag the item off the shelf and then issue a cancel command.
  RIP_OFF_ITEM_AND_CANCEL,
  // Drag the item off the shelf and do not release the mouse.
  RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE,
};

class ShelfAppBrowserTest : public extensions::ExtensionBrowserTest {
 protected:
  ShelfAppBrowserTest() {}

  ~ShelfAppBrowserTest() override {}

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  void SetUpOnMainThread() override {
    // Ensure ash starts the session and creates the shelf and controller.
    SessionControllerClient::FlushForTesting();

    controller_ = ChromeLauncherController::instance();
    ASSERT_TRUE(controller_);
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
  }

  size_t NumberOfDetectedLauncherBrowsers(bool show_all_tabs) {
    ash::ShelfItemDelegate* item_controller =
        controller_->GetBrowserShortcutLauncherItemController();
    return item_controller
        ->GetAppMenuItems(show_all_tabs ? ui::EF_SHIFT_DOWN : 0)
        .size();
  }

  const Extension* LoadAndLaunchExtension(const char* name,
                                          extensions::LaunchContainer container,
                                          WindowOpenDisposition disposition) {
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    const Extension* extension =
        service->GetExtensionById(last_loaded_extension_id(), false);
    EXPECT_TRUE(extension);

    OpenApplication(AppLaunchParams(profile(), extension, container,
                                    disposition, extensions::SOURCE_TEST));
    return extension;
  }

  ash::ShelfID CreateShortcut(const char* name) {
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(profile())->extension_service();
    LoadExtension(test_data_dir_.AppendASCII(name));

    // First get app_id.
    const Extension* extension =
        service->GetExtensionById(last_loaded_extension_id(), false);
    const std::string app_id = extension->id();

    // Then create a shortcut.
    int item_count = shelf_model()->item_count();
    ash::ShelfID shortcut_id = controller_->CreateAppShortcutLauncherItem(
        ash::ShelfID(app_id), item_count);
    controller_->SyncPinPosition(shortcut_id);
    EXPECT_EQ(++item_count, shelf_model()->item_count());
    const ash::ShelfItem& item = *shelf_model()->ItemByID(shortcut_id);
    EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
    return item.id;
  }

  ash::ShelfID PinFakeApp(const std::string& app_id) {
    return controller_->CreateAppShortcutLauncherItem(
        ash::ShelfID(app_id), shelf_model()->item_count());
  }

  // Get the index of an item which has the given type.
  int GetIndexOfShelfItemType(ash::ShelfItemType type) {
    return shelf_model()->GetItemIndexForType(type);
  }

  // Try to rip off |item_index|.
  void RipOffItemIndex(int index,
                       ui::test::EventGenerator* generator,
                       ash::ShelfViewTestAPI* test,
                       RipOffCommand command) {
    ash::ShelfButton* button = test->GetButton(index);
    gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
    gfx::Point rip_off_point(start_point.x(), 0);
    generator->MoveMouseTo(start_point.x(), start_point.y());
    base::RunLoop().RunUntilIdle();
    generator->PressLeftButton();
    base::RunLoop().RunUntilIdle();
    generator->MoveMouseTo(rip_off_point.x(), rip_off_point.y());
    base::RunLoop().RunUntilIdle();
    test->RunMessageLoopUntilAnimationsDone();
    if (command == RIP_OFF_ITEM_AND_RETURN) {
      generator->MoveMouseTo(start_point.x(), start_point.y());
      base::RunLoop().RunUntilIdle();
      test->RunMessageLoopUntilAnimationsDone();
    } else if (command == RIP_OFF_ITEM_AND_CANCEL) {
      // This triggers an internal cancel. Using VKEY_ESCAPE was too unreliable.
      button->OnMouseCaptureLost();
    }
    if (command != RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE) {
      generator->ReleaseLeftButton();
      base::RunLoop().RunUntilIdle();
      test->RunMessageLoopUntilAnimationsDone();
    }
  }

  // Creates a context menu for the existing browser shortcut item.
  std::unique_ptr<LauncherContextMenu> CreateBrowserItemContextMenu() {
    int index = shelf_model()->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
    DCHECK_GE(index, 0);
    ash::ShelfItem item = shelf_model()->items()[index];
    int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
    return LauncherContextMenu::Create(controller_, &item, display_id);
  }

  bool IsItemPresentInMenu(LauncherContextMenu* launcher_context_menu,
                           int command_id) {
    base::RunLoop run_loop;
    std::unique_ptr<ui::MenuModel> menu;
    launcher_context_menu->GetMenuModel(base::BindLambdaForTesting(
        [&](std::unique_ptr<ui::MenuModel> created_menu) {
          menu = std::move(created_menu);
          run_loop.Quit();
        }));
    run_loop.Run();
    ui::MenuModel* menu_ptr = menu.get();
    int index = 0;
    return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &menu_ptr,
                                                       &index);
  }

  ChromeLauncherController* controller_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfAppBrowserTest);
};

class ShelfAppBrowserTestNoDefaultBrowser : public ShelfAppBrowserTest {
 protected:
  ShelfAppBrowserTestNoDefaultBrowser() {}
  ~ShelfAppBrowserTestNoDefaultBrowser() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShelfAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfAppBrowserTestNoDefaultBrowser);
};

// Test that we can launch a platform app and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchUnpinned) {
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item = GetLastLauncherItem();
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  CloseAppWindow(window);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Test that we can launch a platform app that already has a shortcut.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchPinned) {
  int item_count = shelf_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::ShelfID shortcut_id =
      CreateAppShortcutLauncherItem(ash::ShelfID(app_id));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  ash::ShelfItem item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Open a window. Confirm the item is now running.
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ActivateWindow(window->GetBaseWindow());
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  // Then close it, make sure there's still an item.
  CloseAppWindow(window);
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, PinRunning) {
  // Run.
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID id = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Create a shortcut. The app item should be after it.
  ash::ShelfID foo_id = CreateAppShortcutLauncherItem(
      ash::ShelfID(extension_misc::kYoutubeAppId));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(foo_id),
            shelf_model()->ItemIndexByID(id));

  // Pin the app. The item should remain.
  controller_->PinAppWithID(extension->id());
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = *shelf_model()->ItemByID(id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item2.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item2.status);

  // New shortcuts should come after the item.
  ash::ShelfID bar_id = CreateAppShortcutLauncherItem(
      ash::ShelfID(extension_misc::kGoogleDocAppId));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(id),
            shelf_model()->ItemIndexByID(bar_id));

  // Then close it, make sure the item remains.
  CloseAppWindow(window);
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, UnpinRunning) {
  int item_count = shelf_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::ShelfID shortcut_id =
      CreateAppShortcutLauncherItem(ash::ShelfID(app_id));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  ash::ShelfItem item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Create a second shortcut. This will be needed to force the first one to
  // move once it gets unpinned.
  ash::ShelfID foo_id = CreateAppShortcutLauncherItem(
      ash::ShelfID(extension_misc::kYoutubeAppId));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(shortcut_id),
            shelf_model()->ItemIndexByID(foo_id));

  // Open a window. Confirm the item is now running.
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ActivateWindow(window->GetBaseWindow());
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  // Unpin the app. The item should remain.
  controller_->UnpinAppWithID(app_id);
  ASSERT_EQ(item_count, shelf_model()->item_count());
  item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  // The item should have moved after the other shortcuts.
  EXPECT_GT(shelf_model()->ItemIndexByID(shortcut_id),
            shelf_model()->ItemIndexByID(foo_id));

  // Then close it, make sure the item's gone.
  CloseAppWindow(window);
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

// Test that we can launch a platform app with more than one window.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleWindows) {
  int item_count = shelf_model()->item_count();

  // Run the application; a shelf item should be added with one app menu item.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_EQ(item_count + 1, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);
  EXPECT_EQ(1u, controller_->GetAppMenuItemsForTesting(item1).size());

  // Add a second window; confirm the shelf item stays; check the app menu.
  AppWindow* window2 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_EQ(item_count + 1, shelf_model()->item_count());
  const ash::ShelfItem& item2 = *shelf_model()->ItemByID(item_id);
  EXPECT_EQ(ash::STATUS_RUNNING, item2.status);
  EXPECT_EQ(2u, controller_->GetAppMenuItemsForTesting(item2).size());

  // Close the second window; confirm the shelf item stays; check the app menu.
  CloseAppWindow(window2);
  ASSERT_EQ(item_count + 1, shelf_model()->item_count());
  const ash::ShelfItem& item3 = *shelf_model()->ItemByID(item_id);
  EXPECT_EQ(ash::STATUS_RUNNING, item3.status);
  EXPECT_EQ(1u, controller_->GetAppMenuItemsForTesting(item3).size());

  // Close the first window; the shelf item should be removed.
  CloseAppWindow(window1);
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleApps) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Then run second app.
  const Extension* extension2 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  AppWindow* window2 = CreateAppWindow(browser()->profile(), extension2);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = GetLastLauncherItem();
  ash::ShelfID item_id2 = item2.id;
  EXPECT_EQ(ash::TYPE_APP, item2.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item2.status);

  EXPECT_NE(item_id1, item_id2);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);

  // Close second app.
  CloseAppWindow(window2);
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  // First app should still be running.
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);

  // Close first app.
  CloseAppWindow(window1);
  --item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

// Confirm that app windows can be reactivated by clicking their icons and that
// the correct activation order is maintained.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, WindowActivation) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastLauncherItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Then run second app.
  const Extension* extension2 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  AppWindow* window2 = CreateAppWindow(browser()->profile(), extension2);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = GetLastLauncherItem();
  ash::ShelfID item_id2 = item2.id;
  EXPECT_EQ(ash::TYPE_APP, item2.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item2.status);

  EXPECT_NE(item_id1, item_id2);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);

  // Activate first one.
  SelectItem(item_id1);
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window2->GetBaseWindow()->IsActive());

  // Activate second one.
  SelectItem(item_id2);
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window2->GetBaseWindow()->IsActive());

  // Add window for app1. This will activate it.
  AppWindow* window1b = CreateAppWindow(browser()->profile(), extension1);
  ActivateWindow(window1b->GetBaseWindow());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window2->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1b->GetBaseWindow()->IsActive());

  // Key events selecting app1's shelf item will cycle through its windows.
  SelectItem(item_id1, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1b->GetBaseWindow()->IsActive());
  SelectItem(item_id1, ui::ET_KEY_RELEASED);
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1b->GetBaseWindow()->IsActive());

  // Activate the second app again
  SelectItem(item_id2);
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window2->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1b->GetBaseWindow()->IsActive());

  // Activate the first app again
  SelectItem(item_id1);
  EXPECT_TRUE(window1b->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window2->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());

  // Close second app.
  CloseAppWindow(window2);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
  // First app is still running.
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);

  // Close first app.
  CloseAppWindow(window1b);
  CloseAppWindow(window1);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Confirm the minimizing click behavior for apps.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest,
                       PackagedAppClickBehaviorInMinimizeMode) {
  // Launch one platform app and create a window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());

  // Confirm that a controller item was created and is the correct state.
  const ash::ShelfItem& item = GetLastLauncherItem();
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  // Since it is already active, clicking it should minimize.
  SelectItem(item.id);
  EXPECT_FALSE(window1->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMinimized());
  // Clicking the item again should activate the window again.
  SelectItem(item.id);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  // Maximizing a window should preserve state after minimize + click.
  MaximizeWindow(window1->GetBaseWindow());
  MinimizeWindow(window1->GetBaseWindow());
  SelectItem(item.id);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMaximized());
  RestoreWindow(window1->GetBaseWindow());
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1->GetBaseWindow()->IsMaximized());

  // Creating a second window of the same type should change the behavior so
  // that a click on the shelf item does not change the activation state.
  AppWindow* window1a = CreateAppWindow(browser()->profile(), extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1a->GetBaseWindow()->IsActive());

  // Ensure the same shelf item and delegate are used for |window1a|.
  EXPECT_EQ(item.id, GetLastLauncherItem().id);
  EXPECT_EQ(GetShelfItemDelegate(item.id),
            GetShelfItemDelegate(GetLastLauncherItem().id));

  // The first click does nothing.
  SelectItem(item.id);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1a->GetBaseWindow()->IsActive());
  // The second neither.
  SelectItem(item.id);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1a->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1a->GetBaseWindow()->IsActive());
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, BrowserActivation) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  CreateAppWindow(browser()->profile(), extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item = GetLastLauncherItem();
  ash::ShelfID item_id1 = item.id;
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  ActivateWindow(browser()->window());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);
}

// Test that opening an app sets the correct icon
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, SetIcon) {
  TestAppWindowIconObserver test_observer(browser()->profile());

  int base_shelf_item_count = shelf_model()->item_count();
  ExtensionTestMessageListener ready_listener("ready", true);
  LoadAndLaunchPlatformApp("app_icon", "Launched");

  // Create non-shelf window.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createNonShelfWindow");
  ready_listener.Reset();
  // Default app icon + extension icon updates.
  test_observer.WaitForIconUpdates(2);

  // Create shelf window.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createShelfWindow");
  ready_listener.Reset();
  // Default app icon + extension icon updates.
  test_observer.WaitForIconUpdates(2);

  // Set shelf window icon.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("setShelfWindowIcon");
  ready_listener.Reset();
  // Custom icon update.
  test_observer.WaitForIconUpdate();

  // Create shelf window with custom icon on init.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createShelfWindowWithCustomIcon");
  ready_listener.Reset();
  // Default app icon + extension icon + custom icon updates.
  test_observer.WaitForIconUpdates(3);
  const gfx::ImageSkia app_item_custom_image = test_observer.last_app_icon();

  const int shelf_item_count = shelf_model()->item_count();
  ASSERT_EQ(base_shelf_item_count + 3, shelf_item_count);

  const ash::ShelfItem& app_item =
      shelf_model()->items()[base_shelf_item_count];
  const ash::ShelfItem& app_custom_icon_item =
      shelf_model()->items()[base_shelf_item_count + 1];

  // Icons for Apps are set by the AppWindowLauncherController, so
  // image_set_by_controller() should be set.
  const ash::ShelfItemDelegate* app_item_delegate =
      GetShelfItemDelegate(app_item.id);
  ASSERT_TRUE(app_item_delegate);
  EXPECT_TRUE(app_item_delegate->image_set_by_controller());

  const ash::ShelfItemDelegate* app_custom_icon_item_delegate =
      GetShelfItemDelegate(app_custom_icon_item.id);
  ASSERT_TRUE(app_custom_icon_item_delegate);
  EXPECT_TRUE(app_custom_icon_item_delegate->image_set_by_controller());

  // Ensure icon height is correct (see test.js in app_icon/ test directory)
  // Note, images are no longer available in ChromeLauncherController. They are
  // are passed directly to the ShelfController.
  EXPECT_EQ(extension_misc::EXTENSION_ICON_LARGE,
            app_item_custom_image.height());

  // No more icon updates.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(8, test_observer.icon_updates());

  // Exit.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("exit");
  ready_listener.Reset();
}

// Test that we can launch an app with a shortcut.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchPinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
  SelectItem(shortcut_id);
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);
  WebContents* tab = tab_strip->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
  EXPECT_EQ(--tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
}

// Tests behavior of launching app from shelf in the first display while the
// second display has the focus. Initially, Browsers exists in the first
// display.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchAppFromDisplayWithoutFocus0) {
  // Updates the display configuration to add a secondary display.
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("0+0-800x800,801+0-800x800");
  display::Displays displays = display_manager->active_display_list();
  aura::Window::Windows roots = ash::Shell::GetAllRootWindows();
  EXPECT_EQ(displays.size(), 2U);
  EXPECT_EQ(roots.size(), 2U);
  EXPECT_EQ(
      displays[0].id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(roots[0]).id());
  EXPECT_EQ(
      displays[1].id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(roots[1]).id());

  // Ensures that display 0 has one browser with focus and display 1 has two
  // browsers. Each browser only has one tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* browser0 = browser();
  Browser* browser1 = CreateBrowser(browser()->profile());
  Browser* browser2 = CreateBrowser(browser()->profile());
  browser0->window()->SetBounds(displays[0].work_area());
  browser1->window()->SetBounds(displays[1].work_area());
  browser2->window()->SetBounds(displays[1].work_area());
  // Ensures browser 2 is above browser 1 in display 1.
  browser_list->SetLastActive(browser2);
  browser_list->SetLastActive(browser0);
  aura::test::WaitForAllChangesToComplete();
  EXPECT_EQ(browser_list->size(), 3U);
  EXPECT_EQ(displays[0].id(), GetDisplayIdForBrowserWindow(browser0->window()));
  EXPECT_EQ(displays[1].id(), GetDisplayIdForBrowserWindow(browser1->window()));
  EXPECT_EQ(displays[1].id(), GetDisplayIdForBrowserWindow(browser2->window()));
  EXPECT_EQ(browser0->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser1->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser2->tab_strip_model()->count(), 1);

  // Launches an app from the shelf of display 0 and expects a new tab is opened
  // in the uppermost browser in display 0.
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  SelectItem(shortcut_id, ui::ET_MOUSE_PRESSED, displays[1].id());
  EXPECT_EQ(browser0->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser1->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser2->tab_strip_model()->count(), 2);
}

// Tests behavior of launching app from shelf in the first display while the
// second display has the focus. Initially, No browser exists in the first
// display.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchAppFromDisplayWithoutFocus1) {
  // Updates the display configuration to add a secondary display.
  display::DisplayManager* display_manager =
      ash::Shell::Get()->display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .UpdateDisplay("800x800,801+0-800x800");
  display::Displays displays = display_manager->active_display_list();
  aura::Window::Windows roots = ash::Shell::GetAllRootWindows();
  EXPECT_EQ(displays.size(), 2U);
  EXPECT_EQ(roots.size(), 2U);
  EXPECT_EQ(
      displays[0].id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(roots[0]).id());
  EXPECT_EQ(
      displays[1].id(),
      display::Screen::GetScreen()->GetDisplayNearestWindow(roots[1]).id());

  // Ensures that display 0 has one browser with focus and display 1 has no
  // browser. The browser only has one tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  Browser* browser0 = browser();
  browser0->window()->SetBounds(displays[0].work_area());
  aura::test::WaitForAllChangesToComplete();
  EXPECT_EQ(browser_list->size(), 1U);
  EXPECT_EQ(displays[0].id(), GetDisplayIdForBrowserWindow(browser0->window()));
  EXPECT_EQ(browser0->tab_strip_model()->count(), 1);

  // Launches an app from the shelf of display 0 and expects a new browser with
  // one tab is opened in display 0.
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  SelectItem(shortcut_id, ui::ET_MOUSE_PRESSED, displays[1].id());
  Browser* browser1 = browser_list->GetLastActive();
  EXPECT_EQ(browser_list->size(), 2U);
  EXPECT_NE(browser1, browser0);
  EXPECT_EQ(browser0->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser1->tab_strip_model()->count(), 1);
}

// Launch the app first and then create the shortcut.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchUnpinned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);
  WebContents* tab = tab_strip->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();
  EXPECT_EQ(--tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
}

// Launches an app in the background and then tries to open it. This is test for
// a crash we had.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchInBackground) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_EQ(++tab_count, tab_strip->count());
  controller_->LaunchApp(ash::ShelfID(last_loaded_extension_id()),
                         ash::LAUNCH_FROM_UNKNOWN, 0,
                         display::kInvalidDisplayId);
}

// Confirm that clicking a icon for an app running in one of 2 maximized windows
// activates the right window.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchMaximized) {
  MaximizeWindow(browser()->window());
  content::WindowedNotificationObserver open_observer(
      chrome::NOTIFICATION_BROWSER_OPENED,
      content::NotificationService::AllSources());
  chrome::NewEmptyWindow(browser()->profile());
  open_observer.Wait();
  Browser* browser2 = content::Source<Browser>(open_observer.source()).ptr();
  TabStripModel* tab_strip = browser2->tab_strip_model();
  int tab_count = tab_strip->count();
  MaximizeWindow(browser2->window());

  ash::ShelfID shortcut_id = CreateShortcut("app1");
  SelectItem(shortcut_id);
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  // Activate the first browser window.
  ActivateWindow(browser()->window());
  EXPECT_FALSE(browser2->window()->IsActive());

  // Selecting the shortcut activates the second window.
  SelectItem(shortcut_id);
  EXPECT_TRUE(browser2->window()->IsActive());
}

// Activating the same app multiple times should launch only a single copy.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ActivateApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1"));

  controller_->ActivateApp(extension->id(), ash::LAUNCH_FROM_UNKNOWN, 0,
                           display::kInvalidDisplayId);
  EXPECT_EQ(++tab_count, tab_strip->count());
  controller_->ActivateApp(extension->id(), ash::LAUNCH_FROM_UNKNOWN, 0,
                           display::kInvalidDisplayId);
  EXPECT_EQ(tab_count, tab_strip->count());
}

// Launching the same app multiple times should launch a copy for each call.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID id(LoadExtension(test_data_dir_.AppendASCII("app1"))->id());
  controller_->LaunchApp(id, ash::LAUNCH_FROM_UNKNOWN, 0,
                         display::kInvalidDisplayId);
  EXPECT_EQ(++tab_count, tab_strip->count());
  controller_->LaunchApp(id, ash::LAUNCH_FROM_UNKNOWN, 0,
                         display::kInvalidDisplayId);
  EXPECT_EQ(++tab_count, tab_strip->count());
}

// Confirm that a page can be navigated from and to while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, Navigation) {
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
  SelectItem(shortcut_id);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  // Navigate away.
  ui_test_utils::NavigateToURL(browser(),
                               GURL("http://www.example.com/path0/bar.html"));
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);

  // Navigate back.
  ui_test_utils::NavigateToURL(browser(),
                               GURL("http://www.example.com/path1/foo.html"));
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);
}

// Confirm that a tab can be moved between browsers while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, TabDragAndDrop) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  TabStripModel* tab_strip_model1 = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model1->count());
  const int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_GE(browser_index, 0);

  // Create a shortcut for app1.
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);

  // Activate app1 and check its item status.
  SelectItem(shortcut_id);
  EXPECT_EQ(2, tab_strip_model1->count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  // Create a new browser with blank tab.
  Browser* browser2 = CreateBrowser(profile());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model2->count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  // Detach a tab at index 1 (app1) from |tab_strip_model1| and insert it as an
  // active tab at index 1 to |tab_strip_model2|.
  std::unique_ptr<content::WebContents> detached_tab =
      tab_strip_model1->DetachWebContentsAt(1);
  tab_strip_model2->InsertWebContentsAt(1, std::move(detached_tab),
                                        TabStripModel::ADD_ACTIVE);
  EXPECT_EQ(1, tab_strip_model1->count());
  EXPECT_EQ(2, tab_strip_model2->count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->items()[browser_index].status);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  tab_strip_model1->CloseAllTabs();
  tab_strip_model2->CloseAllTabs();
}

IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, RefocusFilterLaunch) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path1/*"));

  // Create new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://www.example2.com/path2/bar.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* first_tab = tab_strip->GetActiveWebContents();
  // Confirm app is not active.
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);

  // Activating app should launch new tab, because second tab isn't
  // in its refocus url path.
  SelectItem(shortcut_id);
  EXPECT_EQ(++tab_count, tab_strip->count());
  WebContents* second_tab = tab_strip->GetActiveWebContents();
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);
  EXPECT_NE(first_tab, second_tab);
  EXPECT_EQ(tab_strip->GetActiveWebContents(), second_tab);
}

// Check that the launcher activation state for a V1 application stays closed
// even after an asynchronous browser event comes in after the tab got
// destroyed.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AsyncActivationStateCheck) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::ShelfID shortcut_id = CreateShortcut("app1");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path1/*"));

  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);

  // Create new tab which would be the running app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://www.example.com/path1/bar.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);
  // To address the issue of crbug.com/174050, the tab we are about to close
  // has to be active.
  tab_strip->ActivateTabAt(1, false);
  EXPECT_EQ(1, tab_strip->active_index());

  // Close the web contents.
  tab_strip->CloseWebContentsAt(1, TabStripModel::CLOSE_NONE);
  // The status should now be set to closed.
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
}

// Test that the App window could restore to its previous window state from
// before it was closed.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AppWindowRestoreBehaviorTest) {
  // Open an App, maximized its window, and close it.
  const Extension* extension =
      LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_WINDOW,
                             WindowOpenDisposition::NEW_WINDOW);
  Browser* app_browser = FindBrowserForApp(extension->id());
  ASSERT_TRUE(app_browser);
  BrowserWindow* window = app_browser->window();
  EXPECT_FALSE(window->IsMaximized());
  MaximizeWindow(window);
  EXPECT_TRUE(window->IsMaximized());
  CloseAppBrowserWindow(app_browser);

  // Reopen the App. It should start maximized. Un-maximize it and close it.
  extension =
      LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_WINDOW,
                             WindowOpenDisposition::NEW_WINDOW);
  app_browser = FindBrowserForApp(extension->id());
  ASSERT_TRUE(app_browser);
  window = app_browser->window();
  EXPECT_TRUE(window->IsMaximized());

  RestoreWindow(window);
  EXPECT_FALSE(window->IsMaximized());
  app_browser->window()->Close();
  CloseAppBrowserWindow(app_browser);

  // Reopen the App. It should start un-maximized.
  extension =
      LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_WINDOW,
                             WindowOpenDisposition::NEW_WINDOW);
  app_browser = FindBrowserForApp(extension->id());
  ASSERT_TRUE(app_browser);
  window = app_browser->window();
  EXPECT_FALSE(window->IsMaximized());
}

// Checks that a windowed application does not add an item to the browser list.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       WindowedAppDoesNotAddToBrowser) {
  // Get the number of items in the browser menu.
  size_t items = NumberOfDetectedLauncherBrowsers(false);
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, items);
  EXPECT_EQ(0u, running_browser);

  const Extension* extension =
      LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_WINDOW,
                             WindowOpenDisposition::NEW_WINDOW);

  // No new browser should get detected, even though one more is running.
  EXPECT_EQ(0u, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());

  OpenApplication(AppLaunchParams(
      profile(), extension, extensions::LAUNCH_CONTAINER_TAB,
      WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));

  // A new browser should get detected and one more should be running.
  EXPECT_EQ(NumberOfDetectedLauncherBrowsers(false), 1u);
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());
}

// Checks the functionality to enumerate all browsers vs. all tabs.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       EnumerateAllBrowsersAndTabs) {
  // Create at least one browser.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         WindowOpenDisposition::NEW_WINDOW);
  size_t browsers = NumberOfDetectedLauncherBrowsers(false);
  size_t tabs = NumberOfDetectedLauncherBrowsers(true);

  // Create a second browser.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         WindowOpenDisposition::NEW_WINDOW);

  EXPECT_EQ(++browsers, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++tabs, NumberOfDetectedLauncherBrowsers(true));

  // Create only a tab.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         WindowOpenDisposition::NEW_FOREGROUND_TAB);

  EXPECT_EQ(browsers, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(++tabs, NumberOfDetectedLauncherBrowsers(true));
}

// Check that the keyboard activation of a launcher item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AltNumberTabsTabbing) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::ShelfID shortcut_id = CreateShortcut("app");
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path/*"));
  std::string url = "http://www.example.com/path/bla";

  // Create an application handled browser tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  content::WebContents* content1 = tab_strip->GetActiveWebContents();

  // Create some other browser tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://www.test.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* content1a = tab_strip->GetActiveWebContents();

  // Make sure that the active tab is now our handled tab.
  EXPECT_NE(content1a, content1);

  // The active tab should still be the unnamed tab. Then we switch and reach
  // the first app and stay there.
  EXPECT_EQ(content1a, tab_strip->GetActiveWebContents());
  SelectItem(shortcut_id, ui::ET_KEY_RELEASED);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());
  SelectItem(shortcut_id, ui::ET_KEY_RELEASED);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* content2 = tab_strip->GetActiveWebContents();

  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
  SelectItem(shortcut_id, ui::ET_KEY_RELEASED);
  EXPECT_EQ(content1, browser()->tab_strip_model()->GetActiveWebContents());
  SelectItem(shortcut_id, ui::ET_KEY_RELEASED);
  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
}

// Check that the keyboard activation of a launcher item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, AltNumberAppsTabbing) {
  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  ui::BaseWindow* window1 =
      CreateAppWindow(browser()->profile(), extension1)->GetBaseWindow();

  const ash::ShelfItem item = GetLastLauncherItem();
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  const Extension* extension2 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  ui::BaseWindow* window2 =
      CreateAppWindow(browser()->profile(), extension2)->GetBaseWindow();

  // By now the browser should be active. Issue Alt keystrokes several times to
  // see that we stay on that application.
  EXPECT_TRUE(window2->IsActive());
  SelectItem(item.id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(window1->IsActive());
  SelectItem(item.id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(window1->IsActive());

  ui::BaseWindow* window1a =
      CreateAppWindow(browser()->profile(), extension1)->GetBaseWindow();

  EXPECT_TRUE(window1a->IsActive());
  EXPECT_FALSE(window1->IsActive());
  SelectItem(item.id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(window1->IsActive());
  SelectItem(item.id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(window1a->IsActive());
}

// Test that we get correct shelf presence with hidden app windows.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, HiddenAppWindows) {
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;

  // Create a hidden window.
  params.hidden = true;
  AppWindow* window_1 =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Create a visible window.
  params.hidden = false;
  AppWindow* window_2 =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  ++item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Minimize the visible window.
  window_2->Minimize();
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Hide the visible window.
  window_2->Hide();
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Show the originally hidden window.
  window_1->Show(AppWindow::SHOW_ACTIVE);
  ++item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Close the originally hidden window.
  CloseAppWindow(window_1);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Test attention states of windows.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, WindowAttentionStatus) {
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;
  params.focused = false;
  AppWindow* window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_TRUE(window->GetNativeWindow()->IsVisible());
  // The window should not be active by default.
  EXPECT_FALSE(window->GetBaseWindow()->IsActive());
  // Confirm that a shelf item was created and is the correct state.
  const ash::ShelfItem& item = GetLastLauncherItem();
  EXPECT_TRUE(GetShelfItemDelegate(item.id));
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  // App windows should go to attention state.
  window->GetNativeWindow()->SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ATTENTION, item.status);

  // Click the item and confirm that the window is activated.
  EXPECT_EQ(ash::SHELF_ACTION_WINDOW_ACTIVATED, SelectItem(item.id));
  EXPECT_TRUE(window->GetBaseWindow()->IsActive());

  // Active windows don't show attention.
  window->GetNativeWindow()->SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
}

IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest,
                       ShowInShelfWindowsWithWindowKeySet) {
  // Add a window with shelf True, close it
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;

  params.show_in_shelf = true;
  params.window_key = "window1";
  AppWindow* window1 =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  // There should be only 1 item added to the shelf.
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  CloseAppWindow(window1);
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Add a window with false, following one with true
  item_count = shelf_model()->item_count();
  extension = LoadAndLaunchPlatformApp("launch", "Launched");

  params.show_in_shelf = false;
  params.window_key = "window1";
  window1 = CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  params.show_in_shelf = true;
  params.window_key = "window2";
  AppWindow* window2 =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  // There should be 2 items added to the shelf: although window1 has
  // show_in_shelf set to false, it's the first window created so its icon must
  // show up in shelf.
  EXPECT_EQ(item_count + 2, shelf_model()->item_count());
  CloseAppWindow(window1);
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  CloseAppWindow(window2);
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Open just one window with false
  item_count = shelf_model()->item_count();
  extension = LoadAndLaunchPlatformApp("launch", "Launched");

  params.show_in_shelf = false;
  params.window_key = "window1";
  window1 = CreateAppWindowFromParams(browser()->profile(), extension, params);
  // There should be 1 item added to the shelf: although show_in_shelf is false,
  // this is the first window created.
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  CloseAppWindow(window1);
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Add a window with true, following one with false
  item_count = shelf_model()->item_count();
  extension = LoadAndLaunchPlatformApp("launch", "Launched");

  params.show_in_shelf = true;
  params.window_key = "window1";
  window1 = CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());  // main window
  params.show_in_shelf = false;
  params.window_key = "window2";
  window2 = CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count + 2, shelf_model()->item_count());
  CloseAppWindow(window1);
  // There should be 1 item added to the shelf as the second window
  // is set to show_in_shelf false
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  CloseAppWindow(window2);
  EXPECT_EQ(item_count, shelf_model()->item_count());

  // Test closing windows in different order
  item_count = shelf_model()->item_count();
  extension = LoadAndLaunchPlatformApp("launch", "Launched");

  params.show_in_shelf = false;
  params.window_key = "window1";
  window1 = CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  params.show_in_shelf = false;
  params.window_key = "window2";
  window2 = CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  params.show_in_shelf = true;
  params.window_key = "window3";
  AppWindow* window3 =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_EQ(item_count + 2, shelf_model()->item_count());
  params.show_in_shelf = true;
  params.window_key = "window4";
  AppWindow* window4 =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  // There should be 3 items added to the shelf.
  EXPECT_EQ(item_count + 3, shelf_model()->item_count());
  // Any window close order should be valid
  CloseAppWindow(window4);
  // Closed window4 that was shown in shelf. item_count would decrease
  EXPECT_EQ(item_count + 2, shelf_model()->item_count());
  CloseAppWindow(window1);
  // Closed window1 which was grouped together with window2 so item_count
  // would not decrease
  EXPECT_EQ(item_count + 2, shelf_model()->item_count());
  CloseAppWindow(window3);
  // Closed window3 that was shown in shelf. item_count would decrease
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());
  CloseAppWindow(window2);
  // Closed window2 - there is no other window in that group and item_count
  // would decrease
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Checks that the browser Alt "tabbing" is properly done.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       AltNumberBrowserTabbing) {
  // Get the number of items in the browser menu.
  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  // The first activation should create a browser at index 2 (App List @ 0 and
  // back button @ 1).
  const ash::ShelfID browser_id = shelf_model()->items()[2].id;
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  // A second activation should not create a new instance.
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  Browser* browser1 = chrome::FindLastActive();
  EXPECT_TRUE(browser1);
  Browser* browser2 = CreateBrowser(profile());

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_NE(browser1->window(), browser2->window());
  EXPECT_TRUE(browser2->window()->IsActive());

  // Activate multiple times the switcher to see that the windows get activated.
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser1->window()->IsActive());
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser2->window()->IsActive());

  // Create a third browser - make sure that we do not toggle simply between
  // two windows.
  Browser* browser3 = CreateBrowser(profile());

  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_NE(browser1->window(), browser3->window());
  EXPECT_NE(browser2->window(), browser3->window());
  EXPECT_TRUE(browser3->window()->IsActive());

  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser1->window()->IsActive());
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser2->window()->IsActive());
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser3->window()->IsActive());
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser1->window()->IsActive());

  // Create another app and make sure that none of our browsers is active.
  LoadAndLaunchExtension("app1", extensions::LAUNCH_CONTAINER_TAB,
                         WindowOpenDisposition::NEW_WINDOW);
  EXPECT_FALSE(browser1->window()->IsActive());
  EXPECT_FALSE(browser2->window()->IsActive());

  // After activation our browser should be active again.
  SelectItem(browser_id, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(browser1->window()->IsActive());
}

// Checks that after a session restore, we do not start applications on an
// activation.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ActivateAfterSessionRestore) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Create a known application.
  ash::ShelfID shortcut_id = CreateShortcut("app1");

  // Create a new browser - without activating it - and load an "app" into it.
  Browser::CreateParams params = Browser::CreateParams(profile(), true);
  params.initial_show_state = ui::SHOW_STATE_INACTIVE;
  Browser* browser2 = new Browser(params);
  controller_->SetRefocusURLPatternForTest(
      shortcut_id, GURL("http://www.example.com/path/*"));
  std::string url = "http://www.example.com/path/bla";
  ui_test_utils::NavigateToURLWithDisposition(
      browser2, GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Remember the number of tabs for each browser.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count1 = tab_strip->count();
  TabStripModel* tab_strip2 = browser2->tab_strip_model();
  int tab_count2 = tab_strip2->count();

  // Check that we have two browsers and the inactive browser remained inactive.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(chrome::FindLastActive(), browser());
  EXPECT_TRUE(browser()->window()->IsActive());
  // Check that the LRU browser list does only contain the original browser.
  BrowserList* browser_list = BrowserList::GetInstance();
  BrowserList::const_reverse_iterator it = browser_list->begin_last_active();
  EXPECT_EQ(*it, browser());
  ++it;
  EXPECT_EQ(it, browser_list->end_last_active());

  // Now request to either activate an existing app or create a new one.
  SelectItem(shortcut_id);

  // Check that we have set focus on the existing application and nothing new
  // was created.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(tab_count1, tab_strip->count());
  EXPECT_EQ(tab_count2, tab_strip2->count());
  EXPECT_EQ(chrome::FindLastActive(), browser2);
  EXPECT_TRUE(browser2->window()->IsActive());
}

// TODO(crbug.com/759779, crbug.com/819386): add back |DISABLED_DragAndDrop|.
// TODO(crbug.com/759779, crbug.com/819386): add back
// |MultiDisplayBasicDragAndDrop|.

// Do tests for removal of items from the shelf by dragging.
// Disabled due to flake: http://crbug.com/448482
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, DISABLED_DragOffShelf) {
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  ash::ShelfViewTestAPI test(GetPrimaryShelfView());
  test.SetAnimationDuration(1);  // Speed up animations for test.
  // Create a known application and check that we have 3 items in the shelf.
  CreateShortcut("app1");
  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(3, shelf_model()->item_count());

  // Test #1: Ripping out the browser item should not change anything.
  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_LE(0, browser_index);
  RipOffItemIndex(browser_index, &generator, &test, RIP_OFF_ITEM);
  // => It should not have been removed and the location should be unchanged.
  EXPECT_EQ(3, shelf_model()->item_count());
  EXPECT_EQ(browser_index, GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT));
  // Make sure that the hide state has been unset after the snap back animation
  // finished.
  ash::ShelfButton* button = test.GetButton(browser_index);
  EXPECT_FALSE(button->state() & ash::ShelfButton::STATE_HIDDEN);

  // Test #2: Ripping out the application and canceling the operation should
  // not change anything.
  int app_index = GetIndexOfShelfItemType(ash::TYPE_PINNED_APP);
  EXPECT_LE(0, app_index);
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM_AND_CANCEL);
  // => It should not have been removed and the location should be unchanged.
  ASSERT_EQ(3, shelf_model()->item_count());
  EXPECT_EQ(app_index, GetIndexOfShelfItemType(ash::TYPE_PINNED_APP));

  // Test #3: Ripping out the application and moving it back in should not
  // change anything.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM_AND_RETURN);
  // => It should not have been removed and the location should be unchanged.
  ASSERT_EQ(3, shelf_model()->item_count());
  // Through the operation the index might have changed.
  app_index = GetIndexOfShelfItemType(ash::TYPE_PINNED_APP);

  // Test #4: Ripping out the application should remove the item.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  // => It should not have been removed and the location should be unchanged.
  EXPECT_EQ(2, shelf_model()->item_count());
  EXPECT_EQ(-1, GetIndexOfShelfItemType(ash::TYPE_PINNED_APP));

  // Test #5: Uninstalling an application while it is being ripped off should
  // not crash.
  CreateShortcut("app2");
  test.RunMessageLoopUntilAnimationsDone();
  int app2_index = GetIndexOfShelfItemType(ash::TYPE_PINNED_APP);
  EXPECT_EQ(3, shelf_model()->item_count());  // And it remains that way.
  RipOffItemIndex(app2_index, &generator, &test,
                  RIP_OFF_ITEM_AND_DONT_RELEASE_MOUSE);
  controller_->UnpinAppWithID("app2");
  test.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(2, shelf_model()->item_count());  // The item should now be gone.
  generator.ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, shelf_model()->item_count());  // And it remains that way.
  EXPECT_EQ(-1, GetIndexOfShelfItemType(ash::TYPE_PINNED_APP));

  // Test #6: Ripping out the application when the overflow button exists.
  // After ripping out, overflow button should be removed.
  int items_added = 0;
  EXPECT_FALSE(test.IsOverflowButtonVisible());

  // Create fake app shortcuts until overflow button is created.
  while (!test.IsOverflowButtonVisible()) {
    std::string fake_app_id = base::StringPrintf("fake_app_%d", items_added);
    PinFakeApp(fake_app_id);
    test.RunMessageLoopUntilAnimationsDone();

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }
  // Make one more item after creating a overflow button.
  std::string fake_app_id = base::StringPrintf("fake_app_%d", items_added);
  PinFakeApp(fake_app_id);
  test.RunMessageLoopUntilAnimationsDone();

  int total_count = shelf_model()->item_count();
  app_index = GetIndexOfShelfItemType(ash::TYPE_PINNED_APP);
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  // When an item is ripped off from the shelf that has overflow button
  // (see crbug.com/3050787), it was hidden accidentally and was then
  // suppressing any further events. If handled correctly the operation will
  // however correctly done and the item will get removed (as well as the
  // overflow button).
  EXPECT_EQ(total_count - 1, shelf_model()->item_count());
  EXPECT_TRUE(test.IsOverflowButtonVisible());

  // Rip off again and the overflow button should has disappeared.
  RipOffItemIndex(app_index, &generator, &test, RIP_OFF_ITEM);
  EXPECT_EQ(total_count - 2, shelf_model()->item_count());
  EXPECT_FALSE(test.IsOverflowButtonVisible());
}

// TODO(crbug.com/759779, crbug.com/819386): add back |ClickItem|.

// Check browser shortcut item functionality.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       BrowserShortcutLauncherItemController) {
  ash::ShelfItemDelegate* item_controller =
      controller_->GetBrowserShortcutLauncherItemController();
  EXPECT_TRUE(item_controller);
  const ash::ShelfID browser_id = item_controller->shelf_id();
  EXPECT_EQ(extension_misc::kChromeAppId, browser_id.app_id);

  // Get the number of browsers.
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, running_browser);
  EXPECT_FALSE(controller_->IsOpen(browser_id));

  // Activate. This creates new browser
  SelectItem(browser_id, ui::ET_UNKNOWN);
  // New Window is created.
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(controller_->IsOpen(browser_id));

  // Minimize Window.
  Browser* browser = chrome::FindLastActive();
  ASSERT_TRUE(browser);
  MinimizeWindow(browser->window());
  EXPECT_TRUE(browser->window()->IsMinimized());

  // Activate again. This doesn't create new browser, it activates the window.
  SelectItem(browser_id, ui::ET_UNKNOWN);
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(controller_->IsOpen(browser_id));
  EXPECT_FALSE(browser->window()->IsMinimized());
}

// Check that the window's ShelfID property matches that of the active tab.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, MatchingShelfIDandActiveTab) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(3, shelf_model()->item_count());

  aura::Window* window = browser()->window()->GetNativeWindow();

  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  ash::ShelfID browser_id = shelf_model()->items()[browser_index].id;
  ash::ShelfID id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(browser_id, id);

  ash::ShelfID app_id = CreateShortcut("app1");
  EXPECT_EQ(4, shelf_model()->item_count());

  // Create and activate a new tab for "app1" and expect an application ShelfID.
  SelectItem(app_id);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(app_id, id);

  // Activate the tab at index 0 (NTP) and expect a browser ShelfID.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(browser_id, id);
}

// Check that a windowed V1 application can navigate away from its domain, but
// still gets detected properly.
// Disabled due to https://crbug.com/838743.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, DISABLED_V1AppNavigation) {
  // We assume that the web store is always there (which it apparently is).
  controller_->PinAppWithID(extensions::kWebStoreAppId);
  const ash::ShelfID id(extensions::kWebStoreAppId);
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(id)->status);

  // Create a windowed application.
  AppLaunchParams params = CreateAppLaunchParamsUserContainer(
      profile(), GetExtensionForAppID(extensions::kWebStoreAppId, profile()),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, extensions::SOURCE_TEST);
  params.container = extensions::LAUNCH_CONTAINER_WINDOW;
  OpenApplication(params);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(id)->status);

  // Find the browser which holds our app.
  Browser* app_browser = NULL;
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active() && !app_browser; ++it) {
    if ((*it)->is_app()) {
      app_browser = *it;
      break;
    }
  }
  ASSERT_TRUE(app_browser);

  // After navigating away in the app, we should still be active.
  ui_test_utils::NavigateToURL(app_browser,
                               GURL("http://www.foo.com/bar.html"));
  // Make sure the navigation was entirely performed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(id)->status);
  app_browser->tab_strip_model()->CloseWebContentsAt(0,
                                                     TabStripModel::CLOSE_NONE);
  // Make sure that the app is really gone.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(id)->status);
}

// Ensure opening settings and task manager windows create new shelf items.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, SettingsAndTaskManagerWindows) {
  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  // Get the number of items in the shelf and browser menu.
  int item_count = shelf_model()->item_count();
  // At least App List and back button should exist.
  ASSERT_GE(item_count, 2);
  size_t browser_count = NumberOfDetectedLauncherBrowsers(false);

  // Open a settings window. Number of browser items should remain unchanged,
  // number of shelf items should increase.
  settings_manager->ShowChromePageForProfile(
      browser()->profile(), chrome::GetSettingsUrl(std::string()));
  // Spin a run loop to sync Ash's ShelfModel change for the settings window.
  base::RunLoop().RunUntilIdle();
  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  EXPECT_EQ(browser_count, NumberOfDetectedLauncherBrowsers(false));
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());

  chrome::ShowTaskManager(browser());
  // Spin a run loop to sync Ash's ShelfModel change for the task manager.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(item_count + 2, shelf_model()->item_count());

  // Validates that all items have valid app id.
  for (const auto& item : shelf_model()->items())
    EXPECT_TRUE(crx_file::id_util::IdIsValid(item.id.app_id));

  // TODO(stevenjb): Test multiprofile on Chrome OS when test support is addded.
  // crbug.com/230464.
}

// Check that tabbed hosted and bookmark apps have correct shelf presence.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, TabbedHostedAndBookmarkApps) {
  // Load and pin a hosted app.
  const Extension* hosted_app =
      LoadExtension(test_data_dir_.AppendASCII("app1/"));
  ASSERT_TRUE(hosted_app);
  controller_->PinAppWithID(hosted_app->id());
  const ash::ShelfID hosted_app_shelf_id(hosted_app->id());

  // Load and pin a bookmark app.
  const Extension* bookmark_app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("app2/"), 1, extensions::Manifest::INTERNAL,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(bookmark_app);
  controller_->PinAppWithID(bookmark_app->id());
  const ash::ShelfID bookmark_app_shelf_id(bookmark_app->id());

  // The apps should be closed.
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(bookmark_app_shelf_id)->status);

  // Navigate to the app's launch URLs in two tabs.
  ui_test_utils::NavigateToURL(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(hosted_app));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(bookmark_app),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, 0);

  // The apps should now be running.
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(bookmark_app_shelf_id)->status);

  // Now use the launcher controller to activate the apps.
  controller_->ActivateApp(hosted_app->id(), ash::LAUNCH_FROM_APP_LIST, 0,
                           display::kInvalidDisplayId);
  controller_->ActivateApp(bookmark_app->id(), ash::LAUNCH_FROM_APP_LIST, 0,
                           display::kInvalidDisplayId);

  // There should be no new browsers or tabs as both apps were already open.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// Check that windowed hosted and bookmark apps have correct shelf presence.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, WindowedHostedAndBookmarkApps) {
  // Load and pin a hosted app.
  const Extension* hosted_app =
      LoadExtension(test_data_dir_.AppendASCII("app1/"));
  ASSERT_TRUE(hosted_app);
  controller_->PinAppWithID(hosted_app->id());
  const ash::ShelfID hosted_app_shelf_id(hosted_app->id());

  // Load and pin a bookmark app.
  const Extension* bookmark_app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("app2/"), 1, extensions::Manifest::INTERNAL,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(bookmark_app);
  controller_->PinAppWithID(bookmark_app->id());
  const ash::ShelfID bookmark_app_shelf_id(bookmark_app->id());

  // Set both apps to open in windows.
  extensions::SetLaunchType(browser()->profile(), hosted_app->id(),
                            extensions::LAUNCH_TYPE_WINDOW);
  extensions::SetLaunchType(browser()->profile(), bookmark_app->id(),
                            extensions::LAUNCH_TYPE_WINDOW);

  // The apps should be closed.
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(bookmark_app_shelf_id)->status);

  // Navigate to the app's launch URLs in two tabs.
  ui_test_utils::NavigateToURL(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(hosted_app));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(bookmark_app),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, 0);

  // The apps should still be closed.
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(bookmark_app_shelf_id)->status);

  // Now use the launcher controller to activate the apps.
  controller_->ActivateApp(hosted_app->id(), ash::LAUNCH_FROM_APP_LIST, 0,
                           display::kInvalidDisplayId);
  controller_->ActivateApp(bookmark_app->id(), ash::LAUNCH_FROM_APP_LIST, 0,
                           display::kInvalidDisplayId);

  // There should be two new browsers.
  EXPECT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));

  // The apps should now be running.
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(bookmark_app_shelf_id)->status);
}

// Test that "Close" is shown in the context menu when there are opened browsers
// windows.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest,
                       LauncherContextMenuVerifyCloseItemAppearance) {
  // Open a context menu for the existing browser window.
  std::unique_ptr<LauncherContextMenu> menu1 = CreateBrowserItemContextMenu();
  // Check if "Close" is added to in the context menu.
  ASSERT_TRUE(IsItemPresentInMenu(menu1.get(), ash::MENU_CLOSE));

  // Close all windows via the menu item.
  CloseBrowserWindow(browser(), menu1.get(), ash::MENU_CLOSE);
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  // Check if "Close" is removed from the context menu.
  std::unique_ptr<LauncherContextMenu> menu2 = CreateBrowserItemContextMenu();
  ASSERT_FALSE(IsItemPresentInMenu(menu2.get(), ash::MENU_CLOSE));
}

// Chrome's ShelfModel should have back button, AppList and browser items and
// delegates.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ShelfModelInitialization) {
  EXPECT_EQ(3, shelf_model()->item_count());
  EXPECT_EQ(ash::kBackButtonId, shelf_model()->items()[0].id.app_id);
  EXPECT_EQ(ash::kAppListId, shelf_model()->items()[1].id.app_id);
  EXPECT_TRUE(
      shelf_model()->GetShelfItemDelegate(shelf_model()->items()[1].id));
  EXPECT_EQ(extension_misc::kChromeAppId, shelf_model()->items()[2].id.app_id);
  EXPECT_TRUE(
      shelf_model()->GetShelfItemDelegate(shelf_model()->items()[2].id));
}
