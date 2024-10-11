// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/public/cpp/system_tray_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_menu_model_adapter.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/speech_monitor.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/app_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/browser_shortcut_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_test_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_app_window_icon_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/app_constants/constants.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/test/service_worker_registration_waiter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/widget_animation_waiter.h"
#include "url/gurl.h"

namespace {

using ::ash::AccessibilityManager;
using ::content::WebContents;
using ::extensions::AppWindow;
using ::extensions::Extension;
using ::web_app::WebAppProvider;

ash::ShelfAction SelectItem(
    const ash::ShelfID& id,
    ui::EventType event_type = ui::EventType::kMousePressed,
    int64_t display_id = display::kInvalidDisplayId,
    ash::ShelfLaunchSource source = ash::LAUNCH_FROM_UNKNOWN) {
  return SelectShelfItem(id, event_type, display_id, source);
}

// Find the browser that associated with |app_name|.
Browser* FindBrowserForApp(const std::string& app_name) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    std::string browser_app_name =
        web_app::GetAppIdFromApplicationName(browser->app_name());
    if (browser_app_name == app_name)
      return browser;
  }
  return nullptr;
}

// Close |app_browser| and wait until it's closed.
void CloseAppBrowserWindow(Browser* app_browser) {
  app_browser->window()->Close();
  ui_test_utils::WaitForBrowserToClose(app_browser);
}

// Close browsers from context menu
void CloseBrowserWindow(Browser* browser,
                        ShelfContextMenu* menu,
                        int close_command) {
  // Note that event_flag is never used inside function ExecuteCommand.
  menu->ExecuteCommand(close_command, ui::EF_NONE);
  ui_test_utils::WaitForBrowserToClose(browser);
}

int64_t GetDisplayIdForBrowserWindow(BrowserWindow* window) {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window->GetNativeWindow())
      .id();
}

void ExecuteScriptInChromeVox(Browser* browser, const std::string& script) {
  std::string execute_script = R"JS((async function() {
      )JS" + script + R"JS(
      window.domAutomationController.send('done');
  })())JS";

  extensions::browsertest_util::ExecuteScriptInBackgroundPageDeprecated(
      browser->profile(), extension_misc::kChromeVoxExtensionId,
      execute_script);
}

void ExtendHotseat(Browser* browser) {
  ash::RootWindowController* const controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::HotseatState::kHidden,
            controller->shelf()->shelf_layout_manager()->hotseat_state());

  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  aura::Window* const browser_window =
      browser_view->GetWidget()->GetNativeWindow();

  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(browser_window)
                                       .bounds();
  const gfx::Point start_point = gfx::Point(
      display_bounds.width() / 4,
      display_bounds.bottom() - ash::ShelfConfig::Get()->shelf_size() / 2);
  // Swipe up for a small distance to bring up the hotseat.
  gfx::Point end_point(start_point.x(), start_point.y() - 80);

  ash::ShelfView* shelf_view = controller->shelf()->GetShelfViewForTesting();

  // Observe hotseat animation before animation starts. Because
  // views::WidgetAnimationWaiter only reacts to completion of the animation
  // whose animation scheduling is recorded in views::WidgetAnimationWaiter.
  views::WidgetAnimationWaiter waiter(shelf_view->GetWidget());

  ui::test::EventGenerator event_generator(controller->GetRootWindow());
  event_generator.GestureScrollSequence(start_point, end_point,
                                        base::Milliseconds(500), 4);

  // Wait until hotseat bounds animation completes.
  waiter.WaitForAnimation();

  EXPECT_EQ(ash::HotseatState::kExtended,
            controller->shelf()->shelf_layout_manager()->hotseat_state());
}

ash::ShelfID CreateAppShortcutItem(const ash::ShelfID& shelf_id) {
  auto* controller = ChromeShelfController::instance();

  return controller->CreateAppItem(
      std::make_unique<AppShortcutShelfItemController>(shelf_id),
      ash::STATUS_CLOSED, /*pinned=*/true, /*title=*/std::u16string());
}

// A class that waits for the child removal to occur on a parent view.
class ChildRemovalWaiter : public views::ViewObserver {
 public:
  explicit ChildRemovalWaiter(views::View* parent_view)
      : parent_view_(parent_view) {
    parent_view_->AddObserver(this);
  }
  ChildRemovalWaiter(const ChildRemovalWaiter&) = delete;
  ChildRemovalWaiter& operator=(const ChildRemovalWaiter&) = delete;
  ~ChildRemovalWaiter() override { parent_view_->RemoveObserver(this); }

  void WaitForChildRemoval() { run_loop_.Run(); }

  // views::ViewObserver:
  void OnChildViewRemoved(views::View* observed_view,
                          views::View* child) override {
    run_loop_.Quit();
  }

 private:
  const raw_ptr<views::View> parent_view_;
  base::RunLoop run_loop_;
};

}  // namespace

class ShelfPlatformAppBrowserTest : public extensions::PlatformAppBrowserTest {
 protected:
  ShelfPlatformAppBrowserTest() = default;
  ShelfPlatformAppBrowserTest(const ShelfPlatformAppBrowserTest&) = delete;
  ShelfPlatformAppBrowserTest& operator=(const ShelfPlatformAppBrowserTest&) =
      delete;
  ~ShelfPlatformAppBrowserTest() override = default;

  void SetUpOnMainThread() override {
    controller_ = ChromeShelfController::instance();
    ASSERT_TRUE(controller_);
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    app_service_test_.SetUp(browser()->profile());
  }

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  // Returns the last item in the shelf.
  const ash::ShelfItem& GetLastShelfItem() {
    return shelf_model()->items()[shelf_model()->item_count() - 1];
  }

  ash::ShelfItemDelegate* GetShelfItemDelegate(const ash::ShelfID& id) {
    return shelf_model()->GetShelfItemDelegate(id);
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  raw_ptr<ChromeShelfController, DanglingUntriaged> controller_ = nullptr;

 private:
  apps::AppServiceTest app_service_test_;
};

class ShelfAppBrowserTest : public extensions::ExtensionBrowserTest {
 protected:
  ShelfAppBrowserTest() = default;
  ShelfAppBrowserTest(const ShelfAppBrowserTest&) = delete;
  ShelfAppBrowserTest& operator=(const ShelfAppBrowserTest&) = delete;
  ~ShelfAppBrowserTest() override {}

  ash::ShelfModel* shelf_model() { return controller_->shelf_model(); }

  void SetUpOnMainThread() override {
    controller_ = ChromeShelfController::instance();
    ASSERT_TRUE(controller_);
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
  }

  size_t BrowserShortcutMenuItemCount(bool show_all_tabs) {
    ash::ShelfItemDelegate* item_controller =
        controller_->GetBrowserShortcutShelfItemControllerForTesting();
    return item_controller
        ->GetAppMenuItems(show_all_tabs ? ui::EF_SHIFT_DOWN : 0,
                          base::NullCallback())
        .size();
  }

  const Extension* LoadAndLaunchExtension(const char* name,
                                          int32_t event_flags) {
    EXPECT_TRUE(LoadExtension(test_data_dir_.AppendASCII(name)));

    const Extension* extension =
        extension_registry()->enabled_extensions().GetByID(
            last_loaded_extension_id());
    EXPECT_TRUE(extension);

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    proxy->Launch(extension->id(), event_flags, apps::LaunchSource::kFromTest,
                  std::make_unique<apps::WindowInfo>(
                      display::Screen::GetScreen()->GetPrimaryDisplay().id()));
    return extension;
  }

  ash::ShelfID CreateShortcut(const char* name) {
    LoadExtension(test_data_dir_.AppendASCII(name));

    // First get app_id.
    const Extension* extension =
        extension_registry()->enabled_extensions().GetByID(
            last_loaded_extension_id());
    const std::string app_id = extension->id();

    // Then create a shortcut.
    int item_count = shelf_model()->item_count();
    ash::ShelfID shortcut_id = CreateAppShortcutItem(ash::ShelfID(app_id));
    controller_->SyncPinPosition(shortcut_id);
    EXPECT_EQ(++item_count, shelf_model()->item_count());
    const ash::ShelfItem& item = *shelf_model()->ItemByID(shortcut_id);
    EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
    return item.id;
  }

  // Get the index of an item which has the given type.
  int GetIndexOfShelfItemType(ash::ShelfItemType type) const {
    return controller_->shelf_model()->GetItemIndexForType(type);
  }

  // Creates a context menu for the existing browser shortcut item.
  std::unique_ptr<ShelfContextMenu> CreateBrowserItemContextMenu() {
    int index = shelf_model()->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
    DCHECK_GE(index, 0);
    ash::ShelfItem item = shelf_model()->items()[index];
    int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
    return ShelfContextMenu::Create(controller_, &item, display_id);
  }

  bool IsItemPresentInMenu(ShelfContextMenu* shelf_context_menu,
                           int command_id) {
    base::RunLoop run_loop;
    std::unique_ptr<ui::SimpleMenuModel> menu;
    shelf_context_menu->GetMenuModel(base::BindLambdaForTesting(
        [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
          menu = std::move(created_menu);
          run_loop.Quit();
        }));
    run_loop.Run();
    ui::MenuModel* menu_ptr = menu.get();
    size_t index = 0;
    return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &menu_ptr,
                                                       &index);
  }

  // Launch the app.
  void LaunchApp(const ash::ShelfID& id,
                 ash::ShelfLaunchSource source,
                 int event_flags,
                 int64_t display_id) {
    controller_->LaunchApp(ash::ShelfID(last_loaded_extension_id()),
                           ash::LAUNCH_FROM_UNKNOWN, 0,
                           display::kInvalidDisplayId);
  }

  // Select the app.
  void SelectApp(const std::string& app_id, ash::ShelfLaunchSource source) {
    ash::ShelfID shelf_id(app_id);
    ash::ShelfModel* model = controller_->shelf_model();
    ash::ShelfItemDelegate* delegate = model->GetShelfItemDelegate(shelf_id);
    ASSERT_TRUE(delegate);
    delegate->ItemSelected(/*event=*/nullptr, display::kInvalidDisplayId,
                           ash::LAUNCH_FROM_UNKNOWN,
                           /*callback=*/base::DoNothing(),
                           /*filter_predicate=*/base::NullCallback());
  }

  raw_ptr<ChromeShelfController, DanglingUntriaged> controller_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ShelfAppBrowserTestNoDefaultBrowser : public ShelfAppBrowserTest {
 protected:
  ShelfAppBrowserTestNoDefaultBrowser() {}
  ShelfAppBrowserTestNoDefaultBrowser(
      const ShelfAppBrowserTestNoDefaultBrowser&) = delete;
  ShelfAppBrowserTestNoDefaultBrowser& operator=(
      const ShelfAppBrowserTestNoDefaultBrowser&) = delete;
  ~ShelfAppBrowserTestNoDefaultBrowser() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShelfAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kNoStartupWindow);
  }
};

class ShelfWebAppBrowserTest : public ShelfAppBrowserTest {
 protected:
  ShelfWebAppBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~ShelfWebAppBrowserTest() override = default;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  GURL GetSecureAppURL() {
    return https_server()->GetURL("app.com", "/ssl/google.html");
  }

  webapps::AppId InstallWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

  // ShelfAppBrowserTest:
  void SetUp() override {
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ShelfAppBrowserTest::SetUp();
  }
  void SetUpInProcessBrowserTestFixture() override {
    ShelfAppBrowserTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }
  void TearDownInProcessBrowserTestFixture() override {
    ShelfAppBrowserTest::TearDownInProcessBrowserTestFixture();
    cert_verifier_.TearDownInProcessBrowserTestFixture();
  }
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ShelfAppBrowserTest::SetUpCommandLine(command_line);
    cert_verifier_.SetUpCommandLine(command_line);
  }
  void SetUpOnMainThread() override {
    ShelfAppBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server()->Start());
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    web_app::test::WaitUntilReady(
        web_app::WebAppProvider::GetForTest(browser()->profile()));
  }

 private:
  net::EmbeddedTestServer https_server_;
  content::ContentMockCertVerifier cert_verifier_;
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
};

// Test that we can launch a platform app and get a running item.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, LaunchUnpinned) {
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item = GetLastShelfItem();
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);
  CloseAppWindow(window);
  --item_count;
  EXPECT_EQ(item_count, shelf_model()->item_count());
}

// Test that we can launch a platform app that already has a shortcut.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, LaunchPinned) {
  int item_count = shelf_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::ShelfID shortcut_id = CreateAppShortcutItem(ash::ShelfID(app_id));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  ash::ShelfItem item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Open a window. Confirm the item is now running.
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  window->GetBaseWindow()->Activate();
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

IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, PinRunning) {
  // Run.
  int item_count = shelf_model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastShelfItem();
  ash::ShelfID id = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Create a shortcut. The app item should be after it.
  ash::ShelfID foo_id =
      CreateAppShortcutItem(ash::ShelfID(extension_misc::kYoutubeAppId));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(foo_id),
            shelf_model()->ItemIndexByID(id));

  // Pin the app. The item should remain.
  controller_->shelf_model()->PinExistingItemWithID(extension->id());
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = *shelf_model()->ItemByID(id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item2.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item2.status);

  // New shortcuts should come after the item.
  ash::ShelfID bar_id =
      CreateAppShortcutItem(ash::ShelfID(extension_misc::kGoogleDocsAppId));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(id),
            shelf_model()->ItemIndexByID(bar_id));

  // Then close it, make sure the item remains.
  CloseAppWindow(window);
  ASSERT_EQ(item_count, shelf_model()->item_count());
}

IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, UnpinRunning) {
  int item_count = shelf_model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ash::ShelfID shortcut_id = CreateAppShortcutItem(ash::ShelfID(app_id));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  ash::ShelfItem item = *shelf_model()->ItemByID(shortcut_id);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item.type);
  EXPECT_EQ(ash::STATUS_CLOSED, item.status);

  // Create a second shortcut. This will be needed to force the first one to
  // move once it gets unpinned.
  ash::ShelfID foo_id =
      CreateAppShortcutItem(ash::ShelfID(extension_misc::kYoutubeAppId));
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  EXPECT_LT(shelf_model()->ItemIndexByID(shortcut_id),
            shelf_model()->ItemIndexByID(foo_id));

  // Open a window. Confirm the item is now running.
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  window->GetBaseWindow()->Activate();
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
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, MultipleWindows) {
  int item_count = shelf_model()->item_count();

  // Run the application; a shelf item should be added with one app menu item.
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_EQ(item_count + 1, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastShelfItem();
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

IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, MultipleApps) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastShelfItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Then run second app.
  const Extension* extension2 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  AppWindow* window2 = CreateAppWindow(browser()->profile(), extension2);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = GetLastShelfItem();
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
// TODO(crbug.com/331536126): This test is flaky.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, DISABLED_WindowActivation) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item1 = GetLastShelfItem();
  ash::ShelfID item_id1 = item1.id;
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Then run second app.
  const Extension* extension2 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  AppWindow* window2 = CreateAppWindow(browser()->profile(), extension2);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item2 = GetLastShelfItem();
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
  window1b->GetBaseWindow()->Activate();
  EXPECT_FALSE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window2->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1b->GetBaseWindow()->IsActive());

  // Key events selecting app1's shelf item will cycle through its windows.
  SelectItem(item_id1, ui::EventType::kKeyReleased);
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_FALSE(window1b->GetBaseWindow()->IsActive());
  SelectItem(item_id1, ui::EventType::kKeyReleased);
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

IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, MultipleBrowsers) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  Browser* const browser1 = chrome::FindLastActive();
  ASSERT_TRUE(browser1);

  Browser* const browser2 = CreateBrowser(profile());
  ASSERT_TRUE(browser2);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_NE(browser1->window(), browser2->window());
  EXPECT_TRUE(browser2->window()->IsActive());

  const Extension* app = LoadAndLaunchPlatformApp("launch", "Launched");
  ui::BaseWindow* const app_window =
      CreateAppWindow(browser()->profile(), app)->GetBaseWindow();

  const ash::ShelfItem item = GetLastShelfItem();
  EXPECT_EQ(app->id(), item.id.app_id);
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  EXPECT_TRUE(app_window->IsActive());
  EXPECT_FALSE(browser2->window()->IsActive());

  SelectItem(ash::ShelfID(app_constants::kChromeAppId));

  EXPECT_FALSE(app_window->IsActive());
  EXPECT_TRUE(browser2->window()->IsActive());
}

// Confirm the minimizing click behavior for apps.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest,
                       PackagedAppClickBehaviorInMinimizeMode) {
  // Launch one platform app and create a window for it.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window1 = CreateAppWindow(browser()->profile(), extension1);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());

  // Confirm that a controller item was created and is the correct state.
  const ash::ShelfItem& item = GetLastShelfItem();
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
  window1->GetBaseWindow()->Maximize();
  window1->GetBaseWindow()->Minimize();
  SelectItem(item.id);
  EXPECT_TRUE(window1->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(window1->GetBaseWindow()->IsActive());
  EXPECT_TRUE(window1->GetBaseWindow()->IsMaximized());
  window1->GetBaseWindow()->Restore();
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
  EXPECT_EQ(item.id, GetLastShelfItem().id);
  EXPECT_EQ(GetShelfItemDelegate(item.id),
            GetShelfItemDelegate(GetLastShelfItem().id));

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

IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, BrowserActivation) {
  int item_count = shelf_model()->item_count();

  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  CreateAppWindow(browser()->profile(), extension1);
  ++item_count;
  ASSERT_EQ(item_count, shelf_model()->item_count());
  const ash::ShelfItem& item = GetLastShelfItem();
  ash::ShelfID item_id1 = item.id;
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  browser()->window()->Activate();
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(item_id1)->status);
}

// Test that opening an app sets the correct icon
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, SetIcon) {
  TestAppWindowIconObserver test_observer(browser()->profile());

  int base_shelf_item_count = shelf_model()->item_count();
  ExtensionTestMessageListener ready_listener("ready",
                                              ReplyBehavior::kWillReply);
  const Extension* extension = LoadAndLaunchPlatformApp("app_icon", "Launched");
  ASSERT_TRUE(extension);

  gfx::ImageSkia image_skia;
  int32_t size_hint_in_dip = 48;
  image_skia =
      app_service_test().LoadAppIconBlocking(extension->id(), size_hint_in_dip);

  // Create non-shelf window.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createNonShelfWindow");
  ready_listener.Reset();
  // Default app icon + extension icon updates + AppServiceProxy load icon
  // updates.
  test_observer.WaitForIconUpdates(3);
  EXPECT_TRUE(app_service_test().AreIconImageEqual(
      image_skia, test_observer.last_app_icon()));

  // Create shelf window.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createShelfWindow");
  ready_listener.Reset();
  // Default app icon + extension icon updates + AppServiceProxy load icon
  // updates.
  test_observer.WaitForIconUpdates(3);
  EXPECT_TRUE(app_service_test().AreIconImageEqual(
      image_skia, test_observer.last_app_icon()));

  // Set shelf window icon.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("setShelfWindowIcon");
  ready_listener.Reset();
  // Custom icon update.
  test_observer.WaitForIconUpdate();
  EXPECT_FALSE(app_service_test().AreIconImageEqual(
      image_skia, test_observer.last_app_icon()));
  gfx::ImageSkia custome_icon = test_observer.last_app_icon();

  // Create shelf window with custom icon on init.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("createShelfWindowWithCustomIcon");
  ready_listener.Reset();
  int update_number;
  // Default app icon + extension icon + AppServiceProxy load icon + custom
  // icon updates. Ensure the custom icon is set as the window's icon.
  test_observer.WaitForIconUpdates(custome_icon);
  EXPECT_TRUE(app_service_test().AreIconImageEqual(
      custome_icon, test_observer.last_app_icon()));
  update_number = test_observer.icon_updates();

  const gfx::ImageSkia app_item_custom_image = test_observer.last_app_icon();

  const int shelf_item_count = shelf_model()->item_count();
  ASSERT_EQ(base_shelf_item_count + 3, shelf_item_count);

  const ash::ShelfItem& app_item =
      shelf_model()->items()[base_shelf_item_count];
  const ash::ShelfItem& app_custom_icon_item =
      shelf_model()->items()[base_shelf_item_count + 1];

  // Icons for Apps are set by the AppWindowShelfController, so
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
  // Note, images are no longer available in ChromeShelfController. They are
  // are passed directly to the ShelfController.
  EXPECT_EQ(extension_misc::EXTENSION_ICON_LARGE,
            app_item_custom_image.height());

  // No more icon updates.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(app_service_test().AreIconImageEqual(
      custome_icon, test_observer.last_app_icon()));
  EXPECT_EQ(update_number, test_observer.icon_updates());

  // Exit.
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());
  ready_listener.Reply("exit");
  ready_listener.Reset();
}

// Test that app window has shelf ID and app ID properties set.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, AppIDWindowProperties) {
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow* window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(window);

  const gfx::NativeWindow native_window = window->GetNativeWindow();
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(native_window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(extension->id(), shelf_id.app_id);
  std::string* app_id = native_window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(app_id);
  EXPECT_EQ(shelf_id.app_id, *app_id);

  CloseAppWindow(window);
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
      .UpdateDisplay("0+0-800x700,801+0-800x700");
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
  SelectItem(shortcut_id, ui::EventType::kMousePressed, displays[1].id());
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
      .UpdateDisplay("800x700,801+0-800x700");
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
  EXPECT_EQ(browser_list->size(), 1U);
  EXPECT_EQ(displays[0].id(), GetDisplayIdForBrowserWindow(browser0->window()));
  EXPECT_EQ(browser0->tab_strip_model()->count(), 1);

  // Launches an app from the shelf of display 0 and expects a new browser with
  // one tab is opened in display 0.
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  SelectItem(shortcut_id, ui::EventType::kMousePressed, displays[1].id());
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
  LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  true /* prefer_containner */));
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

// Verifies that native browser window properties are properly set when showing
// an unpinned hosted app web contents.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AppIDForUnpinnedHostedApp) {
  const extensions::Extension* extension = LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  true /* prefer_containner */));

  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  ash::ShelfID browser_id = shelf_model()->items()[browser_index].id;

  // If the app is not pinned, and thus does not have an associated shelf item,
  // the shelf ID should be set to the browser ID,
  const gfx::NativeWindow native_window =
      browser()->window()->GetNativeWindow();
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(native_window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(browser_id, shelf_id);
  // The app ID should have the actual extension ID.
  std::string* app_id = native_window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(app_id);
  EXPECT_EQ(extension->id(), *app_id);
}

// Verifies that native browser window properties are properly set when showing
// a pinned hosted app web contents.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AppIDForPinnedHostedApp) {
  // Load and pin a hosted app.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("app1/"));
  ASSERT_TRUE(extension);
  PinAppWithIDToShelf(extension->id());

  // Navigate to the app's launch URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(extension)));

  // When an app shportcut exists, the window shelf ID should point to the app
  // shortcut.
  const gfx::NativeWindow native_window =
      browser()->window()->GetNativeWindow();
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(native_window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(extension->id(), shelf_id.app_id);
  std::string* app_id = native_window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(app_id);
  EXPECT_EQ(extension->id(), *app_id);
}

// Verifies that native browser window properties are properly set when showing
// an unpinned web app.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, AppIDForUnpinnedWebApp) {
  // Load and navigate to a web app.
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId web_app_id = InstallWebApp(app_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  ash::ShelfID browser_id = shelf_model()->items()[browser_index].id;

  // If the app is not pinned, and thus does not have an associated shelf item,
  // the shelf ID should be set to the browser ID,
  const gfx::NativeWindow native_window =
      browser()->window()->GetNativeWindow();
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(native_window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(browser_id, shelf_id);
  // The app ID should have the actual web app ID.
  std::string* app_id = native_window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(app_id);
  EXPECT_EQ(web_app_id, *app_id);
}

// Verifies that native browser window properties are properly set when showing
// a pinned web app.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, AppIDForPinnedWebApp) {
  // Load and navigate to a web app.
  const GURL app_url = GetSecureAppURL();
  const webapps::AppId web_app_id = InstallWebApp(app_url);

  PinAppWithIDToShelf(web_app_id);

  // Navigate to the app's launch URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  // When an app shportcut exists, the window shelf ID should point to the app
  // shortcut.
  const gfx::NativeWindow native_window =
      browser()->window()->GetNativeWindow();
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(native_window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(web_app_id, shelf_id.app_id);
  std::string* app_id = native_window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(app_id);
  EXPECT_EQ(web_app_id, *app_id);
}

// Verifies that native browser window properties are properly set when showing
// a PWA tab.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, AppIDForPWA) {
  // Start server and open test page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  web_app::ServiceWorkerRegistrationWaiter registration_waiter(profile(), url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  registration_waiter.AwaitRegistration();

  // Install PWA.
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  web_app::WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);
  const webapps::AppId app_id = install_observer.Wait();
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  // Find the native window for the app.
  gfx::NativeWindow native_window = gfx::NativeWindow();
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->app_controller() &&
        browser->app_controller()->app_id() == app_id) {
      native_window = browser->window()->GetNativeWindow();
      break;
    }
  }
  ASSERT_TRUE(native_window);

  // The native window shelf ID and app ID should match the web app ID.
  ash::ShelfID shelf_id =
      ash::ShelfID::Deserialize(native_window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(app_id, shelf_id.app_id);
  std::string* window_app_id = native_window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(window_app_id);
  EXPECT_EQ(app_id, *window_app_id);
}

// Launches an app in the background and then tries to open it. This is test for
// a crash we had.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchInBackground) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_BACKGROUND_TAB,
                                  true /* prefer_containner */));
  EXPECT_EQ(++tab_count, tab_strip->count());
  controller_->LaunchApp(ash::ShelfID(last_loaded_extension_id()),
                         ash::LAUNCH_FROM_UNKNOWN, 0,
                         display::kInvalidDisplayId);
}

// Confirm that clicking a icon for an app running in one of 2 maximized windows
// activates the right window.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchMaximized) {
  browser()->window()->Maximize();
  // Load about:blank in a new window.
  Browser* browser2 = CreateBrowser(browser()->profile());
  EXPECT_NE(browser(), browser2);
  TabStripModel* tab_strip = browser2->tab_strip_model();
  int tab_count = tab_strip->count();
  browser2->window()->Maximize();

  ash::ShelfID shortcut_id = CreateShortcut("app1");
  SelectItem(shortcut_id);
  EXPECT_EQ(++tab_count, tab_strip->count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  // Activate the first browser window.
  browser()->window()->Activate();
  EXPECT_FALSE(browser2->window()->IsActive());

  // Selecting the shortcut activates the second window.
  SelectItem(shortcut_id);
  EXPECT_TRUE(browser2->window()->IsActive());
}

// Launching the same app multiple times should launch a copy for each call.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, LaunchApp) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count = tab_strip->count();
  ash::ShelfID id(LoadExtension(test_data_dir_.AppendASCII("app1"))->id());
  LaunchApp(id, ash::LAUNCH_FROM_UNKNOWN, 0, display::kInvalidDisplayId);
  EXPECT_EQ(++tab_count, tab_strip->count());
  LaunchApp(id, ash::LAUNCH_FROM_UNKNOWN, 0, display::kInvalidDisplayId);
  EXPECT_EQ(++tab_count, tab_strip->count());
}

// The Browsertest verifying FilesManager's features.
class FilesManagerExtensionTest : public ShelfPlatformAppBrowserTest {
 public:
  void SetUp() override { ShelfPlatformAppBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    ShelfPlatformAppBrowserTest::SetUpOnMainThread();
    CHECK(profile());

    file_manager::test::AddDefaultComponentExtensionsOnMainThread(profile());
    ash::SystemWebAppManager::GetForTest(profile())
        ->InstallSystemAppsForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that FilesManager's first shelf context menu item is "New window"
// (see https://crbug.com/1102781).
IN_PROC_BROWSER_TEST_F(FilesManagerExtensionTest, VerifyFirstItem) {
  const std::string top_level_item_label("New window");

  auto shelf_id =
      CreateAppShortcutItem(ash::ShelfID(file_manager::kFileManagerSwaAppId));
  const ash::ShelfItem* item = shelf_model()->ItemByID(shelf_id);
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  auto menu = ShelfContextMenu::Create(controller_, item, display_id);

  // Fetch |extension|'s shelf context menu model and verify that the top level
  // menu item should be the first one.
  base::RunLoop run_loop;
  menu->GetMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::SimpleMenuModel> menu_model) {
        EXPECT_EQ(base::ASCIIToUTF16(top_level_item_label),
                  menu_model->GetLabelAt(0));
        run_loop.Quit();
      }));

  run_loop.Run();
}

// Launching an app from the shelf when not in Demo Mode should not record app
// launch stat.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, NoDemoModeAppLaunchSourceReported) {
  EXPECT_FALSE(ash::DemoSession::IsDeviceInDemoMode());

  base::HistogramTester histogram_tester;

  // Should see 0 apps launched from the Shelf in the histogram at first.
  histogram_tester.ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  ash::ShelfID id(LoadExtension(test_data_dir_.AppendASCII("app1"))->id());
  controller_->LaunchApp(id, ash::LAUNCH_FROM_SHELF, 0,
                         display::kInvalidDisplayId);

  // Should still see 0 apps launched from the Shelf in the histogram.
  histogram_tester.ExpectTotalCount("DemoMode.AppLaunchSource", 0);
}

// Launching an app from the shelf in Demo Mode should record app
// launch stat.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, DemoModeAppLaunchSourceReported) {
  // Set Demo mode
  ash::test::LockDemoDeviceInstallAttributes();
  EXPECT_TRUE(ash::DemoSession::IsDeviceInDemoMode());

  base::HistogramTester histogram_tester;

  // Should see 0 apps launched from the Shelf in the histogram at first.
  histogram_tester.ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  ash::ShelfID id(LoadExtension(test_data_dir_.AppendASCII("app1"))->id());
  controller_->LaunchApp(id, ash::LAUNCH_FROM_SHELF, 0,
                         display::kInvalidDisplayId);

  // Should see 1 app launched from the shelf in the histogram.
  histogram_tester.ExpectUniqueSample(
      "DemoMode.AppLaunchSource", ash::DemoSession::AppLaunchSource::kShelf, 1);
}

// Confirm that a page can be navigated from and to while maintaining the
// correct running state.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, Navigation) {
  ash::ShelfID shortcut_id = CreateShortcut("app1");
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
  SelectItem(shortcut_id);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);

  // Navigate away.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("http://www.example.com/path0/bar.html")));
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);

  // Navigate back.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("http://www.example.com/path1/foo.html")));
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
  std::unique_ptr<tabs::TabModel> detached_tab =
      tab_strip_model1->DetachTabAtForInsertion(1);
  tab_strip_model2->InsertDetachedTabAt(1, std::move(detached_tab),
                                        AddTabTypes::ADD_ACTIVE);
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
  SetRefocusURL(shortcut_id, GURL("http://www.example.com/path1/*"));

  // Create new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://www.example2.com/path2/bar.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
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

// Check that the shelf item activation state for a V1 application stays closed
// even after an asynchronous browser event comes in after the tab got
// destroyed.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AsyncActivationStateCheck) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::ShelfID shortcut_id = CreateShortcut("app1");
  SetRefocusURL(shortcut_id, GURL("http://www.example.com/path1/*"));

  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);

  // Create new tab which would be the running app.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://www.example.com/path1/bar.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(shortcut_id)->status);
  // To address the issue of crbug.com/174050, the tab we are about to close
  // has to be active.
  tab_strip->ActivateTabAt(1);
  EXPECT_EQ(1, tab_strip->active_index());

  // Close the web contents.
  tab_strip->CloseWebContentsAt(1, TabCloseTypes::CLOSE_NONE);
  // The status should now be set to closed.
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(shortcut_id)->status);
}

// Test that the App window could restore to its previous window state from
// before it was closed.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AppWindowRestoreBehaviorTest) {
  // Open an App, maximized its window, and close it.
  const Extension* extension = LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                  false /* prefer_containner */));
  Browser* app_browser = FindBrowserForApp(extension->id());
  ASSERT_TRUE(app_browser);
  BrowserWindow* window = app_browser->window();
  EXPECT_FALSE(window->IsMaximized());
  window->Maximize();
  EXPECT_TRUE(window->IsMaximized());
  CloseAppBrowserWindow(app_browser);

  // Reopen the App. It should start maximized. Un-maximize it and close it.
  extension = LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                  false /* prefer_containner */));
  app_browser = FindBrowserForApp(extension->id());
  ASSERT_TRUE(app_browser);
  window = app_browser->window();
  EXPECT_TRUE(window->IsMaximized());

  window->Restore();
  EXPECT_FALSE(window->IsMaximized());
  app_browser->window()->Close();
  CloseAppBrowserWindow(app_browser);

  // Reopen the App. It should start un-maximized.
  extension = LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                  false /* prefer_containner */));
  app_browser = FindBrowserForApp(extension->id());
  ASSERT_TRUE(app_browser);
  window = app_browser->window();
  EXPECT_FALSE(window->IsMaximized());
}

// Checks that a windowed application does not add an item to the browser list.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       WindowedAppDoesNotAddToBrowser) {
  // Get the number of items in the browser menu.
  size_t items = BrowserShortcutMenuItemCount(false);
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, items);
  EXPECT_EQ(0u, running_browser);

  const Extension* extension = LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                  false /* prefer_containner */));
  ASSERT_TRUE(extension);

  // No new browser should get detected, even though one more is running.
  EXPECT_EQ(0u, BrowserShortcutMenuItemCount(false));
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->Launch(extension->id(),
                apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    true /* prefer_containner */),
                apps::LaunchSource::kFromTest,
                std::make_unique<apps::WindowInfo>(
                    display::Screen::GetScreen()->GetPrimaryDisplay().id()));

  // A new browser should get detected and one more should be running.
  EXPECT_EQ(BrowserShortcutMenuItemCount(false), 1u);
  EXPECT_EQ(++running_browser, chrome::GetTotalBrowserCount());
}

// Checks the functionality to enumerate all browsers vs. all tabs.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       EnumerateAllBrowsersAndTabs) {
  // Create at least one browser.
  LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  true /* prefer_containner */));
  size_t browsers = BrowserShortcutMenuItemCount(false);
  size_t tabs = BrowserShortcutMenuItemCount(true);

  // Create a second browser.
  const Extension* extension =
      extension_registry()->enabled_extensions().GetByID(
          last_loaded_extension_id());
  EXPECT_TRUE(extension);
  apps::AppServiceProxyFactory::GetForProfile(profile())->LaunchAppWithParams(
      apps::AppLaunchParams(
          extension->id(), apps::LaunchContainer::kLaunchContainerTab,
          WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromTest));

  EXPECT_EQ(++browsers, BrowserShortcutMenuItemCount(false));
  EXPECT_EQ(++tabs, BrowserShortcutMenuItemCount(true));

  // Create only a tab.
  LoadAndLaunchExtension(
      "app1", apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                  true /* prefer_containner */));

  EXPECT_EQ(browsers, BrowserShortcutMenuItemCount(false));
  EXPECT_EQ(++tabs, BrowserShortcutMenuItemCount(true));
}

// Check that the keyboard activation of a shelf item tabs properly through
// the items at hand.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, AltNumberTabsTabbing) {
  TabStripModel* tab_strip = browser()->tab_strip_model();

  ash::ShelfID shortcut_id = CreateShortcut("app");
  SetRefocusURL(shortcut_id, GURL("http://www.example.com/path/*"));
  std::string url = "http://www.example.com/path/bla";

  // Create an application handled browser tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  content::WebContents* content1 = tab_strip->GetActiveWebContents();

  // Create some other browser tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://www.test.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* content1a = tab_strip->GetActiveWebContents();

  // Make sure that the active tab is now our handled tab.
  EXPECT_NE(content1a, content1);

  // The active tab should still be the unnamed tab. Then we switch and reach
  // the first app and stay there.
  EXPECT_EQ(content1a, tab_strip->GetActiveWebContents());
  SelectItem(shortcut_id, ui::EventType::kKeyReleased);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());
  SelectItem(shortcut_id, ui::EventType::kKeyReleased);
  EXPECT_EQ(content1, tab_strip->GetActiveWebContents());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  content::WebContents* content2 = tab_strip->GetActiveWebContents();

  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
  SelectItem(shortcut_id, ui::EventType::kKeyReleased);
  EXPECT_EQ(content1, browser()->tab_strip_model()->GetActiveWebContents());
  SelectItem(shortcut_id, ui::EventType::kKeyReleased);
  EXPECT_EQ(content2, browser()->tab_strip_model()->GetActiveWebContents());
}

// Check that the keyboard activation of a shelf item tabs properly through
// the items at hand.
// TODO(crbug.com/331536126): This test is flaky.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest,
                       DISABLED_AltNumberAppsTabbing) {
  // First run app.
  const Extension* extension1 = LoadAndLaunchPlatformApp("launch", "Launched");
  ui::BaseWindow* window1 =
      CreateAppWindow(browser()->profile(), extension1)->GetBaseWindow();

  const ash::ShelfItem item = GetLastShelfItem();
  EXPECT_EQ(ash::TYPE_APP, item.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item.status);

  const Extension* extension2 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  ui::BaseWindow* window2 =
      CreateAppWindow(browser()->profile(), extension2)->GetBaseWindow();

  // By now the browser should be active. Issue Alt keystrokes several times to
  // see that we stay on that application.
  EXPECT_TRUE(window2->IsActive());
  SelectItem(item.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(window1->IsActive());
  SelectItem(item.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(window1->IsActive());

  ui::BaseWindow* window1a =
      CreateAppWindow(browser()->profile(), extension1)->GetBaseWindow();

  EXPECT_TRUE(window1a->IsActive());
  EXPECT_FALSE(window1->IsActive());
  SelectItem(item.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(window1->IsActive());
  SelectItem(item.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(window1a->IsActive());
}

// Check that the keyboard activation of a shelf item tabs even if the app is
// not currently activated.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest,
                       AltNumberAppsTabbingFromOtherApp) {
  // Create one app with two windows.
  const Extension* app1_extension1 =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ui::BaseWindow* app1_window1 =
      CreateAppWindow(browser()->profile(), app1_extension1)->GetBaseWindow();
  ui::BaseWindow* app1_window2 =
      CreateAppWindow(browser()->profile(), app1_extension1)->GetBaseWindow();
  const ash::ShelfItem item1 = GetLastShelfItem();
  EXPECT_EQ(ash::TYPE_APP, item1.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item1.status);

  // Create another app with two windows.
  const Extension* app2_extension1 =
      LoadAndLaunchPlatformApp("launch_2", "Launched");
  ui::BaseWindow* app2_window1 =
      CreateAppWindow(browser()->profile(), app2_extension1)->GetBaseWindow();
  ui::BaseWindow* app2_window2 =
      CreateAppWindow(browser()->profile(), app2_extension1)->GetBaseWindow();
  const ash::ShelfItem item2 = GetLastShelfItem();
  EXPECT_EQ(ash::TYPE_APP, item2.type);
  EXPECT_EQ(ash::STATUS_RUNNING, item2.status);

  // Last created window should be active. Hitting the app shortcut should go to
  // the first window of the app.
  ASSERT_TRUE(app2_window2->IsActive());
  SelectItem(item2.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(app2_window1->IsActive());

  // Hitting the other app's shortcut should jump and focus the other app's
  // windows.
  SelectItem(item1.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(app1_window2->IsActive());
  SelectItem(item1.id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(app1_window1->IsActive());
}

// Test that we get correct shelf presence with hidden app windows.
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, HiddenAppWindows) {
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
IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest, WindowAttentionStatus) {
  const Extension* extension = LoadAndLaunchPlatformApp("launch", "Launched");
  AppWindow::CreateParams params;
  params.focused = false;
  AppWindow* window =
      CreateAppWindowFromParams(browser()->profile(), extension, params);
  EXPECT_TRUE(window->GetNativeWindow()->IsVisible());
  // The window should not be active by default.
  EXPECT_FALSE(window->GetBaseWindow()->IsActive());
  // Confirm that a shelf item was created and is the correct state.
  const ash::ShelfItem& item = GetLastShelfItem();
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

IN_PROC_BROWSER_TEST_F(ShelfPlatformAppBrowserTest,
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
  const ash::ShelfID browser_id = shelf_model()->items()[0].id;
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  // A second activation should not create a new instance.
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  Browser* browser1 = chrome::FindLastActive();
  EXPECT_TRUE(browser1);
  Browser* browser2 = CreateBrowser(profile());

  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_NE(browser1->window(), browser2->window());
  EXPECT_TRUE(browser2->window()->IsActive());

  // Activate multiple times the switcher to see that the windows get activated.
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(browser1->window()->IsActive());
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(browser2->window()->IsActive());

  // Create a third browser - make sure that we do not toggle simply between
  // two windows.
  Browser* browser3 = CreateBrowser(profile());

  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());
  EXPECT_NE(browser1->window(), browser3->window());
  EXPECT_NE(browser2->window(), browser3->window());
  EXPECT_TRUE(browser3->window()->IsActive());

  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(browser1->window()->IsActive());
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(browser2->window()->IsActive());
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(browser3->window()->IsActive());
  SelectItem(browser_id, ui::EventType::kKeyReleased);
  EXPECT_TRUE(browser1->window()->IsActive());

  // Create another app and make sure that none of our browsers is active.
  LoadAndLaunchExtension("app1",
                         apps::GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                                             false /* prefer_containner */));
  EXPECT_FALSE(browser1->window()->IsActive());
  EXPECT_FALSE(browser2->window()->IsActive());

  // After activation our browser should be active again.
  SelectItem(browser_id, ui::EventType::kKeyReleased);
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
  params.initial_show_state = ui::mojom::WindowShowState::kInactive;
  Browser* browser2 = Browser::Create(params);
  SetRefocusURL(shortcut_id, GURL("http://www.example.com/path/*"));
  std::string url = "http://www.example.com/path/bla";
  ui_test_utils::NavigateToURLWithDisposition(
      browser2, GURL(url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Remember the number of tabs for each browser.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  int tab_count1 = tab_strip->count();
  TabStripModel* tab_strip2 = browser2->tab_strip_model();
  int tab_count2 = tab_strip2->count();

  // Check that we have two browsers and the inactive browser remained inactive.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(chrome::FindLastActive(), browser());
  EXPECT_TRUE(browser()->window()->IsActive());
  // Check that the MRU browser list contains both the original browser and
  // |browser2|.
  BrowserList* browser_list = BrowserList::GetInstance();
  BrowserList::const_reverse_iterator it =
      browser_list->begin_browsers_ordered_by_activation();
  EXPECT_EQ(*it, browser());
  ++it;
  EXPECT_EQ(*it, browser2);

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

// TODO(crbug.com/759779, crbug.com/819386): add back |ClickItem|.

// Check browser shortcut item functionality.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       BrowserShortcutShelfItemController) {
  ash::ShelfItemDelegate* item_controller =
      controller_->GetBrowserShortcutShelfItemControllerForTesting();
  ASSERT_TRUE(item_controller);
  const ash::ShelfID browser_id = item_controller->shelf_id();
  EXPECT_EQ(app_constants::kChromeAppId, browser_id.app_id);

  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());

  // Get the number of browsers.
  size_t running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(0u, running_browser);
  EXPECT_FALSE(controller_->IsOpen(browser_id));
  // No launch time recorded for Chrome yet.
  EXPECT_EQ(base::Time(),
            prefs->GetLastLaunchTime(app_constants::kChromeAppId));

  // Activate. This creates new browser
  base::Time time_before_launch = base::Time::Now();
  SelectItem(browser_id, ui::EventType::kUnknown);
  base::Time time_after_launch = base::Time::Now();
  // New Window is created.
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(controller_->IsOpen(browser_id));
  // Valid launch time should be recorded for Chrome.
  const base::Time time_launch =
      prefs->GetLastLaunchTime(app_constants::kChromeAppId);
  EXPECT_LE(time_before_launch, time_launch);
  EXPECT_GE(time_after_launch, time_launch);

  // Minimize Window.
  Browser* browser = chrome::FindLastActive();
  ASSERT_TRUE(browser);
  browser->window()->Minimize();
  EXPECT_TRUE(browser->window()->IsMinimized());

  // Activate again. This doesn't create new browser, it activates the window.
  SelectItem(browser_id, ui::EventType::kUnknown);
  running_browser = chrome::GetTotalBrowserCount();
  EXPECT_EQ(1u, running_browser);
  EXPECT_TRUE(controller_->IsOpen(browser_id));
  EXPECT_FALSE(browser->window()->IsMinimized());
  // Re-activation should not upate the recorded launch time.
  EXPECT_GE(time_launch, prefs->GetLastLaunchTime(app_constants::kChromeAppId));
}

// Check that browser launch time is recorded when the browser is started
// by means other than BrowserShortcutShelfItemController.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestNoDefaultBrowser,
                       BrowserLaunchTimeRecorded) {
  extensions::ExtensionPrefs* prefs =
      extensions::ExtensionPrefs::Get(profile());

  EXPECT_EQ(0u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(base::Time(),
            prefs->GetLastLaunchTime(app_constants::kChromeAppId));

  base::Time time_before_launch = base::Time::Now();
  // Load about:blank in a new window.
  CreateBrowser(profile());
  base::Time time_after_launch = base::Time::Now();
  const base::Time time_launch =
      prefs->GetLastLaunchTime(app_constants::kChromeAppId);
  EXPECT_LE(time_before_launch, time_launch);
  EXPECT_GE(time_after_launch, time_launch);
}

// Verifies that closing a launched (unpinned) system app through the shelf app
// icon's context menu works as expected.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, CloseSystemAppByShelfContextMenu) {
  // Prepare for launching the setting app.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();

  // Record the default shelf item count.
  const int default_item_count = shelf_model()->item_count();

  // Verify that the setting app does not exist in the shelf model by default.
  ash::RootWindowController* controller = ash::RootWindowController::ForWindow(
      browser()->window()->GetNativeWindow());
  ash::ShelfView* shelf_view = controller->shelf()->GetShelfViewForTesting();
  ash::ShelfModel* model = shelf_view->model();
  EXPECT_EQ(-1, model->ItemIndexByAppID(web_app::kOsSettingsAppId));

  // Open the system tray then click at the quick setting button.
  std::unique_ptr<ash::SystemTrayTestApi> tray_test_api =
      ash::SystemTrayTestApi::Create();
  tray_test_api->ShowBubble();

  ui_test_utils::BrowserChangeObserver browser_opened(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  tray_test_api->ClickBubbleView(ash::VIEW_ID_QS_SETTINGS_BUTTON);
  browser_opened.Wait();

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_EQ(web_app::kOsSettingsAppId,
            ash::ShelfID::Deserialize(
                app_browser->window()->GetNativeWindow()->GetProperty(
                    ash::kShelfIDKey))
                .app_id);

  // Wait until the web contents finish loading.
  EXPECT_TRUE(
      WaitForLoadStop(app_browser->tab_strip_model()->GetActiveWebContents()));

  // Wait until the shelf app icon addition animation finishes.
  ash::ShelfViewTestAPI shelf_test_api(shelf_view);
  shelf_test_api.RunMessageLoopUntilAnimationsDone();

  // Verify that the shelf item count increases by one.
  EXPECT_EQ(default_item_count + 1, model->item_count());

  // Verify that the shelf item of the setting app is placed at the end of the
  // shelf model.
  const int setting_app_item_index =
      model->ItemIndexByAppID(web_app::kOsSettingsAppId);
  EXPECT_EQ(model->item_count() - 1, setting_app_item_index);

  // Get the setting app's shelf app button.
  const ash::ShelfID setting_app_id = model->items()[setting_app_item_index].id;
  views::View* setting_app_icon = shelf_view->GetShelfAppButton(setting_app_id);

  ash::ShelfViewTestAPI test_api(shelf_view);
  base::RunLoop run_loop;
  test_api.SetShelfContextMenuCallback(run_loop.QuitClosure());

  // Right mouse click at `setting_app_icon`.
  ui::test::EventGenerator event_generator(controller->GetRootWindow());
  event_generator.MoveMouseTo(
      setting_app_icon->GetBoundsInScreen().CenterPoint());
  event_generator.PressRightButton();

  // Wait until the context menu shows.
  run_loop.Run();

  ash::ShelfMenuModelAdapter* shelf_menu_model_adapter =
      shelf_view->shelf_menu_model_adapter_for_testing();
  ASSERT_TRUE(shelf_menu_model_adapter->IsShowingMenu());

  // Click at the menu item whose command is ash::MENU_CLOSE.
  event_generator.MoveMouseTo(shelf_menu_model_adapter->root_for_testing()
                                  ->GetMenuItemByID(ash::MENU_CLOSE)
                                  ->GetBoundsInScreen()
                                  .CenterPoint());
  event_generator.ClickLeftButton();

  ChildRemovalWaiter removal_waiter(shelf_view);
  removal_waiter.WaitForChildRemoval();
  shelf_test_api.RunMessageLoopUntilAnimationsDone();

  // Verify that the shelf item count decreases.
  EXPECT_EQ(default_item_count, model->item_count());
}

// Check that the window's ShelfID property matches that of the active tab.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, MatchingShelfIDAndActiveTab) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(1, shelf_model()->item_count());

  aura::Window* window = browser()->window()->GetNativeWindow();

  int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  ash::ShelfID browser_id = shelf_model()->items()[browser_index].id;
  ash::ShelfID id =
      ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(browser_id, id);
  std::string* window_app_id = window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(window_app_id);
  EXPECT_EQ(browser_id.app_id, *window_app_id);

  ash::ShelfID app_id = CreateShortcut("app1");
  EXPECT_EQ(2, shelf_model()->item_count());

  // Create and activate a new tab for "app1" and expect an application ShelfID.
  SelectItem(app_id);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(app_id, id);

  window_app_id = window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(window_app_id);
  EXPECT_EQ(app_id.app_id, *window_app_id);

  // Activate the tab at index 0 (NTP) and expect a browser ShelfID.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  id = ash::ShelfID::Deserialize(window->GetProperty(ash::kShelfIDKey));
  EXPECT_EQ(browser_id, id);

  window_app_id = window->GetProperty(ash::kAppIDKey);
  ASSERT_TRUE(window_app_id);
  EXPECT_EQ(browser_id.app_id, *window_app_id);
}

// Check that a windowed V1 application can navigate away from its domain, but
// still gets detected properly.
// Disabled due to https://crbug.com/838743.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, DISABLED_V1AppNavigation) {
  // We assume that the web store is always there (which it apparently is).
  PinAppWithIDToShelf(extensions::kWebStoreAppId);
  const ash::ShelfID id(extensions::kWebStoreAppId);
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(id)->status);

  // Create a windowed application.
  apps::AppServiceProxyFactory::GetForProfile(profile())->Launch(
      extensions::kWebStoreAppId,
      apps::GetEventFlags(WindowOpenDisposition::NEW_FOREGROUND_TAB,
                          true /* prefer_containner */),
      apps::LaunchSource::kFromTest,
      std::make_unique<apps::WindowInfo>(
          display::Screen::GetScreen()->GetPrimaryDisplay().id()));
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(id)->status);

  // Find the browser which holds our app.
  Browser* app_browser = nullptr;
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser->is_type_app()) {
      app_browser = browser;
      break;
    }
  }
  ASSERT_TRUE(app_browser);

  // After navigating away in the app, we should still be active.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      app_browser, GURL("http://www.foo.com/bar.html")));
  // Make sure the navigation was entirely performed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_model()->ItemByID(id)->status);
  app_browser->tab_strip_model()->CloseWebContentsAt(0,
                                                     TabCloseTypes::CLOSE_NONE);
  // Make sure that the app is really gone.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(ash::STATUS_CLOSED, shelf_model()->ItemByID(id)->status);
}

// Ensure opening settings and task manager windows create new shelf items.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, SettingsAndTaskManagerWindows) {
  // Install the Settings App.
  ash::SystemWebAppManager::GetForTest(browser()->profile())
      ->InstallSystemAppsForTesting();
  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  // Get the number of items in the shelf and browser menu.
  int item_count = shelf_model()->item_count();
  ASSERT_GE(item_count, 0);
  size_t browser_count = BrowserShortcutMenuItemCount(false);

  base::RunLoop run_loop;
  // Open a settings window. Number of browser items should remain unchanged,
  // number of shelf items should increase.
  settings_manager->ShowChromePageForProfile(
      browser()->profile(), chrome::GetOSSettingsUrl(std::string()),
      display::kInvalidDisplayId,
      base::BindOnce([](apps::LaunchResult&& result) {
        EXPECT_EQ(apps::State::kSuccess, result.state);
      }).Then(run_loop.QuitClosure()));
  // Spin a run loop to sync Ash's ShelfModel change for the settings window.
  run_loop.Run();
  Browser* settings_browser =
      settings_manager->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  EXPECT_EQ(browser_count, BrowserShortcutMenuItemCount(false));
  EXPECT_EQ(item_count + 1, shelf_model()->item_count());

  aura::Window* settings_window = settings_browser->window()->GetNativeWindow();
  ASSERT_TRUE(settings_window->GetProperty(ash::kAppIDKey));
  EXPECT_TRUE(crx_file::id_util::IdIsValid(
      *settings_window->GetProperty(ash::kAppIDKey)));

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

// Check that tabbed hosted and web apps have correct shelf presence.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, TabbedHostedAndWebApps) {
  // Load and pin a hosted app.
  const Extension* hosted_app =
      LoadExtension(test_data_dir_.AppendASCII("app1/"));
  ASSERT_TRUE(hosted_app);
  PinAppWithIDToShelf(hosted_app->id());
  const ash::ShelfID hosted_app_shelf_id(hosted_app->id());

  // Load and pin a web app.
  const GURL web_app_url = GetSecureAppURL();
  const webapps::AppId web_app_id = InstallWebApp(web_app_url);
  PinAppWithIDToShelf(web_app_id);
  const ash::ShelfID web_app_shelf_id(web_app_id);

  // The apps should be closed.
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(web_app_shelf_id)->status);

  // Navigate to the app's launch URLs in two tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(hosted_app)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), web_app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB, 0);

  // The apps should now be running.
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(web_app_shelf_id)->status);

  // Now use the shelf controller to activate the apps.
  SelectApp(hosted_app->id(), ash::LAUNCH_FROM_APP_LIST);
  SelectApp(web_app_id, ash::LAUNCH_FROM_APP_LIST);

  // There should be no new browsers or tabs as both apps were already open.
  EXPECT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
}

// Check that windowed hosted and web apps have correct shelf presence.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, WindowedHostedAndWebApps) {
  // Load and pin a hosted app.
  const Extension* hosted_app =
      LoadExtension(test_data_dir_.AppendASCII("app1/"));
  ASSERT_TRUE(hosted_app);
  PinAppWithIDToShelf(hosted_app->id());
  const ash::ShelfID hosted_app_shelf_id(hosted_app->id());

  // Load and pin a web app.
  const GURL web_app_url = GetSecureAppURL();
  const webapps::AppId web_app_id = InstallWebApp(web_app_url);
  PinAppWithIDToShelf(web_app_id);
  const ash::ShelfID web_app_shelf_id(web_app_id);

  // Set both apps to open in windows.
  extensions::SetLaunchType(browser()->profile(), hosted_app->id(),
                            extensions::LAUNCH_TYPE_WINDOW);
  WebAppProvider* provider = WebAppProvider::GetForTest(browser()->profile());
  DCHECK(provider);
  provider->sync_bridge_unsafe().SetAppUserDisplayModeForTesting(
      web_app_id, web_app::mojom::UserDisplayMode::kStandalone);

  // The apps should be closed.
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(web_app_shelf_id)->status);

  // Navigate to the app's launch URLs in two tabs.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), extensions::AppLaunchInfo::GetLaunchWebURL(hosted_app)));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), web_app_url, WindowOpenDisposition::NEW_FOREGROUND_TAB, 0);

  // The apps should still be closed.
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_CLOSED,
            shelf_model()->ItemByID(web_app_shelf_id)->status);

  // Now use the shelf controller to activate the apps.

  ui_test_utils::BrowserChangeObserver browser_opened1(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  SelectApp(hosted_app->id(), ash::LAUNCH_FROM_APP_LIST);
  browser_opened1.Wait();

  ui_test_utils::BrowserChangeObserver browser_opened2(
      nullptr, ui_test_utils::BrowserChangeObserver::ChangeType::kAdded);
  SelectApp(web_app_id, ash::LAUNCH_FROM_APP_LIST);
  browser_opened2.Wait();

  // There should be two new browsers.
  EXPECT_EQ(3u, chrome::GetBrowserCount(browser()->profile()));

  // The apps should now be running.
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(hosted_app_shelf_id)->status);
  EXPECT_EQ(ash::STATUS_RUNNING,
            shelf_model()->ItemByID(web_app_shelf_id)->status);
}

// Windowed progressive web apps should have shelf activity indicator showing
// after install.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest,
                       WindowedPwasHaveActivityIndicatorSet) {
  // Start server and open test page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  web_app::ServiceWorkerRegistrationWaiter registration_waiter(profile(), url);
  ASSERT_TRUE(AddTabAtIndex(1, url, ui::PAGE_TRANSITION_LINK));
  registration_waiter.AwaitRegistration();
  // Install PWA.
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(true);
  web_app::WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  chrome::ExecuteCommand(browser(), IDC_INSTALL_PWA);
  const webapps::AppId app_id = install_observer.Wait();
  web_app::SetAutoAcceptPWAInstallConfirmationForTesting(false);

  ash::ShelfID shelf_id(app_id);
  EXPECT_FALSE(ChromeShelfController::instance()->IsPinned(shelf_id));
  EXPECT_EQ(
      shelf_id,
      ChromeShelfController::instance()->shelf_model()->active_shelf_id());
}

// Windowed shortcut apps should have shelf activity indicator showing after
// install.
IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest,
                       WindowedShortcutAppsHaveActivityIndicatorSet) {
  // Start server and open test page.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(AddTabAtIndex(
      1,
      GURL(embedded_test_server()->GetURL("/banners/manifest_test_page.html")),
      ui::PAGE_TRANSITION_LINK));
  // Install shortcut app.
  web_app::SetAutoAcceptWebAppDialogForTesting(true, true);
  web_app::WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();
  chrome::ExecuteCommand(browser(), IDC_CREATE_SHORTCUT);
  const webapps::AppId app_id = install_observer.Wait();
  web_app::SetAutoAcceptWebAppDialogForTesting(false, false);

  ash::ShelfID shelf_id(app_id);
  EXPECT_FALSE(ChromeShelfController::instance()->IsPinned(shelf_id));
  EXPECT_EQ(
      shelf_id,
      ChromeShelfController::instance()->shelf_model()->active_shelf_id());
}

IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, WebAppPolicy) {
  // Install web app from policy.
  GURL app_url = https_server()->GetURL("/web_apps/basic.html");
  web_app::ExternalInstallOptions options =
      web_app::CreateInstallOptions(app_url);
  options.install_source = web_app::ExternalInstallSource::kExternalPolicy;
  web_app::ExternallyManagedAppManager::InstallResult result =
      web_app::ExternallyManagedAppManagerInstall(browser()->profile(),
                                                  options);

  // Set policy to pin the web app.
  base::Value::Dict entry;
  entry.Set(ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, app_url.spec());
  base::Value::List policy_value;
  policy_value.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kPolicyPinnedLauncherApps,
                                 std::move(policy_value));

  // Check web app is pinned and fixed.
  ASSERT_EQ(shelf_model()->item_count(), 2);
  EXPECT_EQ(shelf_model()->items()[0].type, ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_EQ(shelf_model()->items()[1].type, ash::TYPE_PINNED_APP);
  ASSERT_TRUE(result.app_id.has_value());
  EXPECT_EQ(shelf_model()->items()[1].id.app_id, result.app_id.value());
  EXPECT_EQ(shelf_model()->items()[1].title, u"Basic web app");
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(result.app_id.value(), profile()));
}

IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, WebAppPolicyUpdate) {
  // Install web app.
  GURL app_url = GURL("https://example.org/");
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(app_url);
  web_app_info->scope = app_url;
  web_app_info->title = u"Example";
  webapps::AppId app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));

  // Set policy to pin the web app.
  base::Value::Dict entry;
  entry.Set(ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, app_url.spec());
  base::Value::List policy_value;
  policy_value.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kPolicyPinnedLauncherApps,
                                 std::move(policy_value));

  // Check web app is not pinned.
  EXPECT_EQ(shelf_model()->item_count(), 1);
  EXPECT_EQ(shelf_model()->items()[0].type, ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(app_id, profile()));

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForTest(profile());
  web_app::test::AddInstallUrlData(
      profile()->GetPrefs(), &provider->sync_bridge_unsafe(), app_id, app_url,
      web_app::ExternalInstallSource::kExternalPolicy);
  provider->install_manager().NotifyWebAppInstalledWithOsHooks(app_id);

  // Check web app is pinned and fixed.
  EXPECT_EQ(shelf_model()->item_count(), 2);
  EXPECT_EQ(shelf_model()->items()[0].type, ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_EQ(shelf_model()->items()[1].type, ash::TYPE_PINNED_APP);
  EXPECT_EQ(shelf_model()->items()[1].id.app_id, app_id);
  EXPECT_EQ(shelf_model()->items()[1].title, u"Example");
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));
}

IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, WebAppPolicyNonExistentApp) {
  // Don't install the web app.
  GURL app_url = GURL("https://example.org/");
  webapps::AppId app_id =
      web_app::GenerateAppId(/*manifest_id=*/std::nullopt, app_url);

  // Set policy to pin the non existent web app.
  base::Value::Dict entry;
  entry.Set(ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, app_url.spec());
  base::Value::List policy_value;
  policy_value.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kPolicyPinnedLauncherApps,
                                 std::move(policy_value));

  // Check web app policy is ignored.
  EXPECT_EQ(shelf_model()->item_count(), 1);
  EXPECT_EQ(shelf_model()->items()[0].type, ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(app_id, profile()));
}

IN_PROC_BROWSER_TEST_F(ShelfWebAppBrowserTest, WebAppInstallForceList) {
  constexpr char kAppUrl[] = "https://example.site";
  base::RunLoop run_loop;
  web_app::WebAppProvider::GetForTest(browser()->profile())
      ->policy_manager()
      .SetOnAppsSynchronizedCompletedCallbackForTesting(run_loop.QuitClosure());
  web_app::WebAppTestInstallWithOsHooksObserver install_observer(profile());
  install_observer.BeginListening();

  {
    base::Value::Dict entry;
    entry.Set(ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, kAppUrl);
    base::Value::List policy_value;
    policy_value.Append(std::move(entry));
    profile()->GetPrefs()->SetList(prefs::kPolicyPinnedLauncherApps,
                                   std::move(policy_value));
  }
  {
    base::Value::Dict item;
    item.Set(web_app::kUrlKey, kAppUrl);
    base::Value::List list;
    list.Append(std::move(item));
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   std::move(list));
  }

  const webapps::AppId app_id = install_observer.Wait();
  run_loop.Run();

  // Check web app is pinned and fixed.
  ASSERT_EQ(shelf_model()->item_count(), 2);
  EXPECT_EQ(shelf_model()->items()[0].type, ash::TYPE_BROWSER_SHORTCUT);
  EXPECT_EQ(shelf_model()->items()[1].type, ash::TYPE_PINNED_APP);
  EXPECT_EQ(shelf_model()->items()[1].id.app_id, app_id);
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));
}

// Test that "Close" is shown in the context menu when there are opened browsers
// windows.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest,
                       ShelfContextMenuVerifyCloseItemAppearance) {
  // Open a context menu for the existing browser window.
  std::unique_ptr<ShelfContextMenu> menu1 = CreateBrowserItemContextMenu();
  // Check if "Close" is added to in the context menu.
  ASSERT_TRUE(IsItemPresentInMenu(menu1.get(), ash::MENU_CLOSE));

  // Close all windows via the menu item.
  CloseBrowserWindow(browser(), menu1.get(), ash::MENU_CLOSE);
  EXPECT_EQ(0u, BrowserList::GetInstance()->size());

  // Check if "Close" is removed from the context menu.
  std::unique_ptr<ShelfContextMenu> menu2 = CreateBrowserItemContextMenu();
  ASSERT_FALSE(IsItemPresentInMenu(menu2.get(), ash::MENU_CLOSE));
}

// Chrome's ShelfModel should have the browser item and delegate.
IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTest, ShelfModelInitialization) {
  EXPECT_EQ(1, shelf_model()->item_count());
  EXPECT_EQ(app_constants::kChromeAppId, shelf_model()->items()[0].id.app_id);
  EXPECT_TRUE(
      shelf_model()->GetShelfItemDelegate(shelf_model()->items()[0].id));
}

class HotseatShelfAppBrowserTest : public ShelfAppBrowserTest {
 public:
  HotseatShelfAppBrowserTest() = default;
  HotseatShelfAppBrowserTest(const HotseatShelfAppBrowserTest&) = delete;
  HotseatShelfAppBrowserTest& operator=(const HotseatShelfAppBrowserTest&) =
      delete;
  ~HotseatShelfAppBrowserTest() override = default;

  // ShelfAppBrowserTest:
  void SetUp() override {
    // Disable contextual nudges to prevent in-app to home nudge from being
    // announced in the ChromeVox test.
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kHideShelfControlsInTabletMode);
    ShelfAppBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that hotseat should be hidden after launching the browser from
// a context menu (https://crbug.com/1072043).
IN_PROC_BROWSER_TEST_F(HotseatShelfAppBrowserTest, LaunchAppFromContextMenu) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  ash::RootWindowController* controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ash::ShelfView* shelf_view = controller->shelf()->GetShelfViewForTesting();

  ash::ShelfModel* model = shelf_view->model();
  EXPECT_EQ(1, model->item_count());

  ExtendHotseat(browser());

  const ash::ShelfID browser_icon_id = model->items()[0].id;
  views::View* browser_icon = shelf_view->GetShelfAppButton(browser_icon_id);

  ash::ShelfViewTestAPI test_api(shelf_view);
  base::RunLoop run_loop;
  test_api.SetShelfContextMenuCallback(run_loop.QuitClosure());

  ui::test::EventGenerator event_generator(controller->GetRootWindow());
  event_generator.MoveMouseTo(browser_icon->GetBoundsInScreen().CenterPoint());
  event_generator.PressRightButton();

  // Wait until the context menu shows.
  run_loop.Run();

  ash::ShelfMenuModelAdapter* shelf_menu_model_adapter =
      shelf_view->shelf_menu_model_adapter_for_testing();
  ASSERT_TRUE(shelf_menu_model_adapter->IsShowingMenu());

  // Click at the menu item whose command is ash::MENU_NEW_WINDOW.
  event_generator.MoveMouseTo(
      shelf_menu_model_adapter->root_for_testing()
          ->GetMenuItemByID(ash::APP_CONTEXT_MENU_NEW_WINDOW)
          ->GetBoundsInScreen()
          .CenterPoint());
  event_generator.ClickLeftButton();

  // Verify that hotseat is hidden.
  EXPECT_EQ(ash::HotseatState::kHidden,
            controller->shelf()->shelf_layout_manager()->hotseat_state());
}

// Tests that launching and switching apps by tapping shelf buttons hides the
// hotseat.
IN_PROC_BROWSER_TEST_F(HotseatShelfAppBrowserTest,
                       TappingAppIconsHidesHotseat) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Create two apps, then extend the hotseat.
  ash::ShelfID shortcut_id_1 = CreateShortcut("app1");
  ash::ShelfID shortcut_id_2 = CreateShortcut("app2");
  ExtendHotseat(browser());

  // Launch app1, the hotseat should hide.
  ash::RootWindowController* controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ash::ShelfView* shelf_view = controller->shelf()->GetShelfViewForTesting();
  views::View* button_1 = shelf_view->GetShelfAppButton(shortcut_id_1);
  ui::test::EventGenerator event_generator(controller->GetRootWindow());
  event_generator.GestureTapAt(button_1->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(ash::HotseatState::kHidden,
            controller->shelf()->shelf_layout_manager()->hotseat_state());

  // Show the hotseat again, and launch app2. The hotseat should hide again.
  ExtendHotseat(browser());
  views::View* button_2 = shelf_view->GetShelfAppButton(shortcut_id_2);
  event_generator.GestureTapAt(button_2->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(ash::HotseatState::kHidden,
            controller->shelf()->shelf_layout_manager()->hotseat_state());

  // Extend the hotseat and test that switching back to app1 results in a hidden
  // hotseat.
  ExtendHotseat(browser());
  event_generator.GestureTapAt(button_1->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(ash::HotseatState::kHidden,
            controller->shelf()->shelf_layout_manager()->hotseat_state());
}

// Verify that the in-app shelf should be shown when the app icon receives
// the accessibility focus.
IN_PROC_BROWSER_TEST_F(HotseatShelfAppBrowserTest, EnableChromeVox) {
  ash::TabletModeControllerTestApi().EnterTabletMode();
  ash::test::SpeechMonitor speech_monitor;

  // Enable ChromeVox.
  ASSERT_FALSE(AccessibilityManager::Get()->IsSpokenFeedbackEnabled());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // Wait for ChromeVox to start reading anything.
  speech_monitor.ExpectSpeechPattern("*");
  speech_monitor.Call([this]() {
    // Disable earcons (https://crbug.com/396507).
    const std::string script(R"JS(
        let module = await import('/chromevox/background/chromevox.js');
        module.ChromeVox.earcons.playEarcon = function() {};
        module = await import('/chromevox/background/chromevox_state.js');
        let ChromeVoxState = module.ChromeVoxState;
        module = await import('/chromevox/background/chromevox_range.js');
        let ChromeVoxRange = module.ChromeVoxRange;

        await ChromeVoxState.ready();

        // Wait for ChromeVox to have a current range before the test starts
        // traversal through shelf to ensure that the browser does not show
        // mid shelf traversal, and causes the a11y focus to unexpectedly
        // switch to the omnibox mid test.
        if (!ChromeVoxRange.current) {
          await new Promise(resolve => {
              new (class {
                  constructor() {
                    ChromeVoxRange.addObserver(this);
                  }
                  onCurrentRangeChanged(newRange) {
                    if (newRange) {
                        ChromeVoxRange.removeObserver(this);
                        resolve();
                    }
                  }
              })();
          });
        }
    )JS");
    ExecuteScriptInChromeVox(browser(), script);
  });

  ash::RootWindowController* controller =
      ash::Shell::GetRootWindowControllerWithDisplayId(
          display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ui::test::EventGenerator event_generator(controller->GetRootWindow());
  auto* generator_ptr = &event_generator;

  base::SimpleTestTickClock clock;
  auto* clock_ptr = &clock;
  ui::SetEventTickClockForTesting(clock_ptr);

  views::View* home_button = ash::ShelfTestApi().GetHomeButton();
  speech_monitor.Call([clock_ptr, generator_ptr, home_button]() {
    // Hover touch over the come button.
    const gfx::Point home_button_center =
        home_button->GetBoundsInScreen().CenterPoint();

    ui::TouchEvent touch_press(
        ui::EventType::kTouchPressed, home_button_center,
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_press);

    clock_ptr->Advance(base::Seconds(1));

    ui::TouchEvent touch_move(
        ui::EventType::kTouchMoved, home_button_center, base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch, 0));
    generator_ptr->Dispatch(&touch_move);
  });

  speech_monitor.ExpectSpeech("Launcher");
  speech_monitor.ExpectSpeech("Button");
  speech_monitor.ExpectSpeech("Shelf");
  speech_monitor.ExpectSpeech("Tool bar");
  speech_monitor.ExpectSpeech(", window");

  speech_monitor.Call([controller]() {
    // Hotseat is expected to be hidden (by default).
    ASSERT_EQ(ash::HotseatState::kHidden,
              controller->shelf()->shelf_layout_manager()->hotseat_state());
  });

  speech_monitor.Call([generator_ptr]() {
    // Press the search + right. Expects that the browser icon receives the
    // accessibility focus and the hotseat switches to kExtended state.
    generator_ptr->PressKeyAndModifierKeys(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
  });

  const int browser_index =
      ash::ShelfModel::Get()->GetItemIndexForType(ash::TYPE_BROWSER_SHORTCUT);
  speech_monitor.ExpectSpeech(
      base::UTF16ToASCII(ash::ShelfModel::Get()->items()[browser_index].title));
  speech_monitor.Replay();

  EXPECT_EQ(ash::HotseatState::kExtended,
            controller->shelf()->shelf_layout_manager()->hotseat_state());

  // Click on the home button. Expects that the hotseat is shown in
  // kShownHomeLauncher state. Note that the home button should be shown in
  // tablet mode with spoken feedback enabled.
  event_generator.MoveMouseTo(home_button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  EXPECT_EQ(ash::HotseatState::kShownHomeLauncher,
            controller->shelf()->shelf_layout_manager()->hotseat_state());
}

using ShelfAppBrowserTestWithDesks = ShelfAppBrowserTest;

IN_PROC_BROWSER_TEST_F(ShelfAppBrowserTestWithDesks, MultipleDesks) {
  auto* desks_controller = ash::DesksController::Get();
  desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);

  // Tests starts with an existing browser on desk_1.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Activate desk_2 and click on the browser's icon on the shelf while being on
  // that desk. This should not switch back to desk_1, but rather create a new
  // browser window.
  ASSERT_EQ(2u, desks_controller->desks().size());
  auto* desk_2 = desks_controller->desks()[1].get();
  ash::ActivateDesk(desk_2);

  const int browser_index = GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
  ash::ShelfID browser_id = shelf_model()->items()[browser_index].id;

  SelectItem(browser_id);
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_FALSE(desks_controller->AreDesksBeingModified());
  EXPECT_TRUE(desk_2->is_active());

  // The shelf context menu should show 2 items for both browsers. No new items
  // should be created and existing window should not be minimized.
  EXPECT_EQ(ash::ShelfAction::SHELF_ACTION_NONE,
            SelectItem(browser_id, ui::EventType::kMousePressed,
                       display::kInvalidDisplayId, ash::LAUNCH_FROM_SHELF));
  EXPECT_EQ(
      2u, controller_
              ->GetAppMenuItemsForTesting(shelf_model()->items()[browser_index])
              .size());
}

class PerDeskShelfAppBrowserTest : public ShelfAppBrowserTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  PerDeskShelfAppBrowserTest() = default;
  PerDeskShelfAppBrowserTest(const PerDeskShelfAppBrowserTest&) = delete;
  PerDeskShelfAppBrowserTest& operator=(const PerDeskShelfAppBrowserTest&) =
      delete;
  ~PerDeskShelfAppBrowserTest() override = default;

  ash::ShelfView* shelf_view() { return shelf_view_; }

  // ShelfAppBrowserTest:
  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(ash::features::kPerDeskShelf);
    } else {
      scoped_feature_list_.InitAndDisableFeature(ash::features::kPerDeskShelf);
    }

    ShelfAppBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ShelfAppBrowserTest::SetUpOnMainThread();
    shelf_view_ = ash::Shell::GetPrimaryRootWindowController()
                      ->shelf()
                      ->GetShelfViewForTesting();
    // Start tests with 2 desks.
    ash::DesksController::Get()->NewDesk(
        ash::DesksCreationRemovalSource::kButton);
    ash::ShelfViewTestAPI test_api(shelf_view());
    test_api.SetShelfContextMenuCallback(base::BindRepeating(
        &PerDeskShelfAppBrowserTest::OnAppMenuShown, base::Unretained(this)));
  }

  void CreateTestBrowser() {
    Browser* new_browser = CreateBrowser(browser()->profile());
    new_browser->window()->Show();
    new_browser->window()->Activate();
  }

  ash::ShelfID GetBrowserId() const {
    const int browser_index =
        GetIndexOfShelfItemType(ash::TYPE_BROWSER_SHORTCUT);
    return controller_->shelf_model()->items()[browser_index].id;
  }

  ash::ShelfMenuModelAdapter* ClickBrowserShelfButtonAndGetMenu() {
    views::View* browser_icon = shelf_view()->GetShelfAppButton(GetBrowserId());
    run_loop_ = std::make_unique<base::RunLoop>();
    ui::test::EventGenerator event_generator(
        ash::Shell::GetPrimaryRootWindow());
    event_generator.MoveMouseTo(
        browser_icon->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
    run_loop_->Run();
    ash::ShelfMenuModelAdapter* shelf_menu_model_adapter =
        shelf_view()->shelf_menu_model_adapter_for_testing();
    EXPECT_TRUE(shelf_menu_model_adapter);
    EXPECT_TRUE(shelf_menu_model_adapter->IsShowingMenu());
    return shelf_menu_model_adapter;
  }

 private:
  void OnAppMenuShown() {
    if (run_loop_)
      std::move(run_loop_)->Quit();
  }

  raw_ptr<ash::ShelfView, DanglingUntriaged> shelf_view_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PerDeskShelfAppBrowserTest, AppMenus) {
  // On desk_1, create 3 browsers. Note that the test starts with a default
  // browser.
  CreateTestBrowser();
  CreateTestBrowser();
  EXPECT_EQ(3u, chrome::GetTotalBrowserCount());

  // Switch to desk_2, and create 2 more browsers.
  auto* desks_controller = ash::DesksController::Get();
  auto* desk_2 = desks_controller->desks()[1].get();
  ash::ActivateDesk(desk_2);
  CreateTestBrowser();
  CreateTestBrowser();
  EXPECT_EQ(5u, chrome::GetTotalBrowserCount());

  // Click on the Browser icon on the shelf and expect the app items menu will
  // show, and the number of items in the menu will depend on whether the
  // per-desk shelf feature is enabled or not.
  auto* model_adapter = ClickBrowserShelfButtonAndGetMenu();
  const bool is_per_desk_shelf_enabled = GetParam();
  constexpr size_t kTitleAndSeparatorCount = 2;
  if (is_per_desk_shelf_enabled) {
    EXPECT_EQ(2 + kTitleAndSeparatorCount,
              model_adapter->model()->GetItemCount());
  } else {
    EXPECT_EQ(5 + kTitleAndSeparatorCount,
              model_adapter->model()->GetItemCount());
  }

  // Switch to desk_1, and verify the app items count again.
  auto* desk_1 = desks_controller->desks()[0].get();
  ash::ActivateDesk(desk_1);
  model_adapter = ClickBrowserShelfButtonAndGetMenu();
  if (is_per_desk_shelf_enabled) {
    EXPECT_EQ(3 + kTitleAndSeparatorCount,
              model_adapter->model()->GetItemCount());
  } else {
    EXPECT_EQ(5 + kTitleAndSeparatorCount,
              model_adapter->model()->GetItemCount());
  }
}

// The Browsertest verifying Files System Web App features.
class FilesSystemWebAppPinnedTest : public ShelfPlatformAppBrowserTest {
 public:
  void SetUp() override { ShelfPlatformAppBrowserTest::SetUp(); }
};

IN_PROC_BROWSER_TEST_F(FilesSystemWebAppPinnedTest, EnterpriseMigration) {
  // Setup: the customer pins Files Chrome App (ID:hhaomji...).
  base::Value::Dict entry;
  entry.Set(ChromeShelfPrefs::kPinnedAppsPrefAppIDKey,
            file_manager::kFileManagerAppId);
  base::Value::List policy_value;
  policy_value.Append(std::move(entry));
  profile()->GetPrefs()->SetList(prefs::kPolicyPinnedLauncherApps,
                                 std::move(policy_value));

  // Ensure shelf is updated.
  ash::SystemWebAppManager::Get(browser()->profile())
      ->InstallSystemAppsForTesting();
  web_app::WebAppProvider::GetForTest(browser()->profile())
      ->install_manager()
      .NotifyWebAppInstalledWithOsHooks(file_manager::kFileManagerSwaAppId);

  // Expected results: Files SWA App (ID:fkiggjm...) is force-pinned.
  ash::ShelfID swa_shelf_id(file_manager::kFileManagerSwaAppId);
  EXPECT_TRUE(ChromeShelfController::instance()->IsPinned(swa_shelf_id));
  ash::ShelfID extension_shelf_id(file_manager::kFileManagerAppId);
  EXPECT_FALSE(ChromeShelfController::instance()->IsPinned(extension_shelf_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(file_manager::kFileManagerSwaAppId, profile()));
}

INSTANTIATE_TEST_SUITE_P(All, PerDeskShelfAppBrowserTest, ::testing::Bool());
