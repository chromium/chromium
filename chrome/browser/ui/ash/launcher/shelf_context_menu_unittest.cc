// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/shelf_context_menu.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_model.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_command_line.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/arc/icon_decode_request.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_shelf_utils.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "chrome/browser/ui/ash/launcher/browser_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/extension_shelf_context_menu.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface_util.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

using crostini::CrostiniTestHelper;

namespace {

ash::ShelfItemDelegate::AppMenuItems GetAppMenuItems(
    ash::ShelfItemDelegate* delegate,
    int event_flags) {
  return delegate->GetAppMenuItems(event_flags, base::NullCallback());
}

bool IsItemPresentInMenu(ui::MenuModel* menu, int command_id) {
  ui::MenuModel* model = menu;
  int index = 0;
  return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                     &index);
}

bool IsItemEnabledInMenu(ui::MenuModel* menu, int command_id) {
  ui::MenuModel* model = menu;
  int index = 0;
  return ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model,
                                                     &index) &&
         menu->IsEnabledAt(index);
}

std::string GetAppNameInShelfGroup(uint32_t task_id) {
  return base::StringPrintf("AppInShelfGroup%d", task_id);
}

class ShelfContextMenuTest : public ChromeAshTestBase {
 protected:
  ShelfContextMenuTest() = default;
  ~ShelfContextMenuTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();

    ChromeAshTestBase::SetUp();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableDefaultApps);

    profile_ = std::make_unique<TestingProfile>();

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    crostini_helper_ = std::make_unique<CrostiniTestHelper>(profile());
    crostini_helper_->ReInitializeAppServiceIntegration();

    app_service_test_.SetUp(profile());
    arc_test_.SetUp(profile());

    session_manager_ = std::make_unique<session_manager::SessionManager>();
    model_ = std::make_unique<ash::ShelfModel>();
    launcher_controller_ =
        std::make_unique<ChromeLauncherController>(profile(), model_.get());
    launcher_controller_->SetProfileForTest(profile());
    launcher_controller_->SetLauncherControllerHelperForTest(
        std::make_unique<LauncherControllerHelper>(profile()));
    launcher_controller_->Init();

    // Disable safe icon decoding to ensure ArcAppShortcutRequests returns in
    // the test environment.
    ArcAppIcon::DisableSafeDecodingForTesting();
    arc::IconDecodeRequest::DisableSafeDecodingForTesting();
  }

  std::unique_ptr<ShelfContextMenu> CreateShelfContextMenu(
      ash::ShelfItemType shelf_item_type,
      int64_t display_id) {
    ash::ShelfItem item;
    item.id = ash::ShelfID("idmockidmockidmockidmockidmockid");
    item.type = shelf_item_type;
    return ShelfContextMenu::Create(controller(), &item, display_id);
  }

  // Creates app window and set optional ARC application id.
  views::Widget* CreateArcWindow(const std::string& window_app_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* widget = new views::Widget();
    params.context = GetContext();
    widget->Init(std::move(params));
    widget->Show();
    widget->Activate();
    exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
    return widget;
  }

  std::unique_ptr<ui::MenuModel> GetMenuModel(
      ShelfContextMenu* shelf_context_menu) {
    base::RunLoop run_loop;
    std::unique_ptr<ui::MenuModel> menu;
    shelf_context_menu->GetMenuModel(base::BindLambdaForTesting(
        [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
          menu = std::move(created_menu);
          run_loop.Quit();
        }));
    run_loop.Run();
    return menu;
  }

  std::unique_ptr<ui::MenuModel> GetContextMenu(
      ash::ShelfItemDelegate* item_delegate,
      int64_t display_id) {
    base::RunLoop run_loop;
    std::unique_ptr<ui::MenuModel> menu;
    item_delegate->GetContextMenu(
        display_id, base::BindLambdaForTesting(
                        [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
                          menu = std::move(created_menu);
                          run_loop.Quit();
                        }));
    run_loop.Run();
    return menu;
  }

  void TearDown() override {
    launcher_controller_.reset();

    arc_test_.TearDown();

    crostini_helper_.reset();
    profile_.reset();

    ChromeAshTestBase::TearDown();

    chromeos::DBusThreadManager::Shutdown();
  }

  ArcAppTest& arc_test() { return arc_test_; }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  TestingProfile* profile() { return profile_.get(); }

  CrostiniTestHelper* crostini_helper() { return crostini_helper_.get(); }

  ChromeLauncherController* controller() { return launcher_controller_.get(); }

  ash::ShelfModel* model() { return model_.get(); }

  void SendRefreshAppList(const std::vector<arc::mojom::AppInfo>& apps) {
    arc_test_.app_instance()->SendRefreshAppList(apps);
    app_service_test_.FlushMojoCalls();
  }

  void LaunchApp(const std::string& app_id,
                 const arc::mojom::AppInfo& info,
                 int32_t task_id) {
    arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                   arc::UserInteractionType::NOT_USER_INITIATED);

    // AppService checks the task id to decide whether the app is running, so
    // create the task id to simulate the running app.
    arc_test_.app_instance()->SendTaskCreated(task_id, info, std::string());
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniTestHelper> crostini_helper_;
  ArcAppTest arc_test_;
  apps::AppServiceTest app_service_test_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<ash::ShelfModel> model_;
  std::unique_ptr<ChromeLauncherController> launcher_controller_;
  extensions::ExtensionService* extension_service_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ShelfContextMenuTest);
};

// Verifies that "New Incognito window" menu item in the launcher context
// menu is disabled when Incognito mode is switched off (by a policy).
TEST_F(ShelfContextMenuTest,
       NewIncognitoWindowMenuIsDisabledWhenIncognitoModeOff) {
  const int64_t display_id = GetPrimaryDisplay().id();
  // Initially, "New Incognito window" should be enabled.
  std::unique_ptr<ShelfContextMenu> shelf_context_menu =
      CreateShelfContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  std::unique_ptr<ui::MenuModel> menu = GetMenuModel(shelf_context_menu.get());
  ASSERT_TRUE(IsItemPresentInMenu(menu.get(), ash::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(
      shelf_context_menu->IsCommandIdEnabled(ash::MENU_NEW_INCOGNITO_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  shelf_context_menu =
      CreateShelfContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  menu = GetMenuModel(shelf_context_menu.get());
  // The item should be disabled, and therefore not added to the menu.
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_NEW_INCOGNITO_WINDOW));
  EXPECT_FALSE(
      shelf_context_menu->IsCommandIdEnabled(ash::MENU_NEW_INCOGNITO_WINDOW));
}

// Verifies that "New window" menu item in the launcher context
// menu is disabled when Incognito mode is forced (by a policy).
TEST_F(ShelfContextMenuTest, NewWindowMenuIsDisabledWhenIncognitoModeForced) {
  const int64_t display_id = GetPrimaryDisplay().id();
  // Initially, "New window" should be enabled.
  std::unique_ptr<ShelfContextMenu> shelf_context_menu =
      CreateShelfContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  std::unique_ptr<ui::MenuModel> menu = GetMenuModel(shelf_context_menu.get());
  ASSERT_TRUE(IsItemPresentInMenu(menu.get(), ash::MENU_NEW_WINDOW));
  EXPECT_TRUE(shelf_context_menu->IsCommandIdEnabled(ash::MENU_NEW_WINDOW));

  // Disable Incognito mode.
  IncognitoModePrefs::SetAvailability(profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  shelf_context_menu =
      CreateShelfContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  menu = GetMenuModel(shelf_context_menu.get());
  ASSERT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_NEW_WINDOW));
  EXPECT_FALSE(shelf_context_menu->IsCommandIdEnabled(ash::MENU_NEW_WINDOW));
}

// Verifies that "Close" is not shown in context menu if no browser window is
// opened.
TEST_F(ShelfContextMenuTest, DesktopShellShelfContextMenuVerifyCloseItem) {
  const int64_t display_id = GetPrimaryDisplay().id();
  std::unique_ptr<ShelfContextMenu> shelf_context_menu =
      CreateShelfContextMenu(ash::TYPE_BROWSER_SHORTCUT, display_id);
  std::unique_ptr<ui::MenuModel> menu = GetMenuModel(shelf_context_menu.get());
  ASSERT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_CLOSE));
}

// Verifies context menu and app menu items for ARC app.
// The 0th item is sticky but not the following.
TEST_F(ShelfContextMenuTest, ArcLauncherMenusCheck) {
  arc_test().app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>(arc_test().fake_apps().begin(),
                                       arc_test().fake_apps().begin() + 1));
  app_service_test().WaitForAppService();
  const std::string app_id = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);
  const std::string app_name = arc_test().fake_apps()[0].name;

  controller()->PinAppWithID(app_id);

  const ash::ShelfID shelf_id(app_id);
  const ash::ShelfItem* item = controller()->GetItem(shelf_id);
  ASSERT_TRUE(item);
  EXPECT_EQ(base::UTF8ToUTF16(app_name), item->title);
  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_TRUE(GetAppMenuItems(item_delegate, 0 /* event_flags */).empty());

  const int64_t display_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, display_id);
  ASSERT_TRUE(menu);

  // ARC app is pinned but not running.
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_CLOSE));

  // ARC app is running.
  std::string window_app_id1("org.chromium.arc.1");
  CreateArcWindow(window_app_id1);
  arc_test().app_instance()->SendTaskCreated(1, arc_test().fake_apps()[0],
                                             std::string());
  app_service_test().WaitForAppService();

  item_delegate = model()->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  auto menu_list = GetAppMenuItems(item_delegate, 0 /* event_flags */);
  ASSERT_EQ(1U, menu_list.size());
  EXPECT_EQ(base::UTF8ToUTF16(app_name), menu_list[0].title);

  menu = GetContextMenu(item_delegate, display_id);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_CLOSE));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));

  // ARC non-launchable app is running.
  const std::string app_id2 = ArcAppTest::GetAppId(arc_test().fake_apps()[1]);
  const std::string app_name2 = arc_test().fake_apps()[1].name;
  std::string window_app_id2("org.chromium.arc.2");
  CreateArcWindow(window_app_id2);
  arc_test().app_instance()->SendTaskCreated(2, arc_test().fake_apps()[1],
                                             std::string());
  app_service_test().WaitForAppService();
  const ash::ShelfID shelf_id2(app_id2);
  const ash::ShelfItem* item2 = controller()->GetItem(shelf_id2);
  ASSERT_TRUE(item2);
  EXPECT_EQ(base::UTF8ToUTF16(app_name2), item2->title);
  ash::ShelfItemDelegate* item_delegate2 =
      model()->GetShelfItemDelegate(shelf_id2);
  ASSERT_TRUE(item_delegate2);

  menu_list = GetAppMenuItems(item_delegate2, 0 /* event_flags */);
  ASSERT_EQ(1U, menu_list.size());
  EXPECT_EQ(base::UTF8ToUTF16(app_name2), menu_list[0].title);

  menu = GetContextMenu(item_delegate2, display_id);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_OPEN_NEW));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_CLOSE));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));

  // Shelf group context menu.
  std::vector<arc::mojom::ShortcutInfo> shortcuts = arc_test().fake_shortcuts();
  shortcuts[0].intent_uri +=
      ";S.org.chromium.arc.shelf_group_id=arc_test_shelf_group;end";
  arc_test().app_instance()->SendInstallShortcuts(shortcuts);
  app_service_test().WaitForAppService();
  const std::string app_id3 =
      arc::ArcAppShelfId("arc_test_shelf_group",
                         ArcAppTest::GetAppId(arc_test().fake_apps()[2]))
          .ToString();

  constexpr int apps_to_test_in_shelf_group = 2;
  const std::string app_name3 = arc_test().fake_apps()[2].name;
  for (uint32_t i = 0; i < apps_to_test_in_shelf_group; ++i) {
    const uint32_t task_id = 3 + i;
    std::string window_app_id3 =
        base::StringPrintf("org.chromium.arc.%d", task_id);
    CreateArcWindow(window_app_id3);
    arc_test().app_instance()->SendTaskCreated(
        task_id, arc_test().fake_apps()[2], shortcuts[0].intent_uri);
    // Set custom name.
    arc_test().app_instance()->SendTaskDescription(
        task_id, GetAppNameInShelfGroup(task_id),
        std::string() /* icon_png_data_as_string */);
    app_service_test().WaitForAppService();
    const ash::ShelfID shelf_id3(app_id3);
    const ash::ShelfItem* item3 = controller()->GetItem(shelf_id3);
    ASSERT_TRUE(item3);

    // Validate item label is correct
    EXPECT_EQ(base::UTF8ToUTF16(app_name3), item3->title);

    ash::ShelfItemDelegate* item_delegate3 =
        model()->GetShelfItemDelegate(shelf_id3);
    ASSERT_TRUE(item_delegate3);

    menu = GetContextMenu(item_delegate3, display_id);
    ASSERT_TRUE(menu);

    EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_OPEN_NEW));
    EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_PIN));
    EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_CLOSE));
    EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
    EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));

    menu_list = GetAppMenuItems(item_delegate3, 0 /* event_flags */);
    ASSERT_EQ(i + 1, menu_list.size());

    // Ensure custom names are set in the app menu items. Note, they are
    // in reverse order, based on activation order.
    for (uint32_t j = 0; j <= i; ++j) {
      EXPECT_EQ(base::UTF8ToUTF16(GetAppNameInShelfGroup(3 + j)),
                menu_list[i - j].title);
    }
  }
}

TEST_F(ShelfContextMenuTest, ArcLauncherSuspendAppMenu) {
  arc::mojom::AppInfo app = arc_test().fake_apps()[0];
  app.suspended = true;
  SendRefreshAppList({app});
  const std::string app_id = ArcAppTest::GetAppId(app);

  controller()->PinAppWithID(app_id);

  const ash::ShelfID shelf_id(app_id);
  const ash::ShelfItem* item = controller()->GetItem(shelf_id);
  ASSERT_TRUE(item);
  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_TRUE(GetAppMenuItems(item_delegate, 0 /* event_flags */).empty());

  const int64_t display_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, display_id);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_PIN));
  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_CLOSE));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
}

TEST_F(ShelfContextMenuTest, ArcDeferredShelfContextMenuItemCheck) {
  SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      arc_test().fake_apps().begin(), arc_test().fake_apps().begin() + 2));
  const std::string app_id1 = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);
  const std::string app_id2 = ArcAppTest::GetAppId(arc_test().fake_apps()[1]);

  controller()->PinAppWithID(app_id1);

  arc_test().StopArcInstance();

  const ash::ShelfID shelf_id1(app_id1);
  const ash::ShelfID shelf_id2(app_id2);

  EXPECT_TRUE(controller()->GetItem(shelf_id1));
  EXPECT_FALSE(controller()->GetItem(shelf_id2));

  LaunchApp(app_id1, arc_test().fake_apps()[0], 1);
  LaunchApp(app_id2, arc_test().fake_apps()[1], 2);

  EXPECT_TRUE(controller()->GetItem(shelf_id1));
  EXPECT_TRUE(controller()->GetItem(shelf_id2));

  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(shelf_id1);
  ASSERT_TRUE(item_delegate);
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, 0 /* display_id */);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_CLOSE));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));

  item_delegate = model()->GetShelfItemDelegate(shelf_id2);
  ASSERT_TRUE(item_delegate);
  menu = GetContextMenu(item_delegate, 0 /* display_id */);
  ASSERT_TRUE(menu);

  EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_OPEN_NEW));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_PIN));
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_CLOSE));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
}

TEST_F(ShelfContextMenuTest, CommandIdsMatchEnumsForHistograms) {
  // Tests that CommandId enums are not changed as the values are used in
  // histograms.
  EXPECT_EQ(0, ash::MENU_OPEN_NEW);
  EXPECT_EQ(1, ash::MENU_CLOSE);
  EXPECT_EQ(2, ash::MENU_PIN);
  EXPECT_EQ(3, ash::LAUNCH_TYPE_PINNED_TAB);
  EXPECT_EQ(4, ash::LAUNCH_TYPE_REGULAR_TAB);
  EXPECT_EQ(5, ash::LAUNCH_TYPE_FULLSCREEN);
  EXPECT_EQ(6, ash::LAUNCH_TYPE_WINDOW);
  EXPECT_EQ(7, ash::MENU_NEW_WINDOW);
  EXPECT_EQ(8, ash::MENU_NEW_INCOGNITO_WINDOW);
  EXPECT_EQ(9, ash::NOTIFICATION_CONTAINER);
}

TEST_F(ShelfContextMenuTest, ArcContextMenuOptions) {
  // Tests that there are the right number of ARC app context menu options. If
  // you're adding a context menu option ensure that you have added the enum to
  // tools/metrics/histograms/enums.xml and that you haven't modified the order
  // of the existing enums.
  SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      arc_test().fake_apps().begin(), arc_test().fake_apps().begin() + 1));
  const std::string app_id = ArcAppTest::GetAppId(arc_test().fake_apps()[0]);
  const ash::ShelfID shelf_id(app_id);

  controller()->PinAppWithID(app_id);
  const ash::ShelfItem* item = controller()->GetItem(shelf_id);
  ASSERT_TRUE(item);
  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, primary_id);

  // Test that there are 9 items in an ARC app context menu.
  EXPECT_EQ(9, menu->GetItemCount());
}

// Tests that the context menu of internal app  is correct.
TEST_F(ShelfContextMenuTest, InternalAppShelfContextMenu) {
  const std::vector<app_list::InternalApp> internal_apps(
      app_list::GetInternalAppList(profile()));
  for (const auto& internal_app : internal_apps) {
    if (!internal_app.show_in_launcher)
      continue;

    const std::string app_id = internal_app.app_id;
    const ash::ShelfID shelf_id(app_id);
    // Pin internal app.
    controller()->PinAppWithID(app_id);
    const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
    ASSERT_TRUE(item);
    EXPECT_EQ(l10n_util::GetStringUTF16(internal_app.name_string_resource_id),
              item->title);
    ash::ShelfItemDelegate* item_delegate =
        model()->GetShelfItemDelegate(shelf_id);
    ASSERT_TRUE(item_delegate);

    const int64_t display_id = GetPrimaryDisplay().id();
    std::unique_ptr<ui::MenuModel> menu =
        GetContextMenu(item_delegate, display_id);
    ASSERT_TRUE(menu);

    // Internal app is pinned but not running.
    EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_OPEN_NEW));
    EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::MENU_PIN));
    EXPECT_FALSE(IsItemPresentInMenu(menu.get(), ash::MENU_CLOSE));
  }
}

// Tests that the number of context menu options of internal app is correct.
TEST_F(ShelfContextMenuTest, InternalAppShelfContextMenuOptionsNumber) {
  const std::vector<app_list::InternalApp> internal_apps(
      app_list::GetInternalAppList(profile()));
  for (const auto& internal_app : internal_apps) {
    const std::string app_id = internal_app.app_id;
    const ash::ShelfID shelf_id(app_id);
    // Pin internal app.
    controller()->PinAppWithID(app_id);
    const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
    ASSERT_TRUE(item);

    ash::ShelfItemDelegate* item_delegate =
        model()->GetShelfItemDelegate(shelf_id);
    ASSERT_TRUE(item_delegate);
    int64_t primary_id = GetPrimaryDisplay().id();
    std::unique_ptr<ui::MenuModel> menu =
        GetContextMenu(item_delegate, primary_id);

    const int expected_options_num = internal_app.show_in_launcher ? 2 : 1;
    EXPECT_EQ(expected_options_num, menu->GetItemCount());
  }
}

// Checks some properties for crostini's terminal app's context menu,
// specifically that every menu item has an icon.
TEST_F(ShelfContextMenuTest, CrostiniTerminalApp) {
  const std::string app_id = crostini::kCrostiniTerminalSystemAppId;
  crostini::CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
      crostini::kCrostiniDefaultVmName);

  controller()->PinAppWithID(app_id);
  const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
  ASSERT_TRUE(item);

  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, primary_id);

  // Check that every menu item has an icon
  for (int i = 0; i < menu->GetItemCount(); ++i)
    EXPECT_FALSE(menu->GetIconAt(i).IsEmpty());

  // When crostini is running, the terminal should have an option to kill the
  // vm.
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::SHUTDOWN_GUEST_OS));
}

// Checks the context menu for a "normal" crostini app (i.e. a registered one).
// Particularly, we ensure that the density changing option exists.
// TODO(crbug.com/1177126) Re-enable test
TEST_F(ShelfContextMenuTest, CrostiniNormalApp) {
  const std::string app_name = "foo";
  crostini_helper()->AddApp(crostini::CrostiniTestHelper::BasicApp(app_name));
  app_service_test().FlushMojoCalls();
  const std::string app_id =
      crostini::CrostiniTestHelper::GenerateAppId(app_name);
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
      ->AppLaunched(app_id);

  controller()->PinAppWithID(app_id);
  const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
  ASSERT_TRUE(item);

  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);
  int64_t primary_id = GetPrimaryDisplay().id();

  // We force a scale factor of 2.0, to check that the normal app has a menu
  // option to change the dpi settings.
  UpdateDisplay("1920x1080*2.0");

  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, primary_id);

  // Check that every menu item has an icon
  for (int i = 0; i < menu->GetItemCount(); ++i)
    EXPECT_FALSE(menu->GetIconAt(i).IsEmpty());

  // Precisely which density option is shown is not important to us, we only
  // care that one is shown.
  EXPECT_TRUE(IsItemEnabledInMenu(menu.get(), ash::CROSTINI_USE_LOW_DENSITY) ||
              IsItemEnabledInMenu(menu.get(), ash::CROSTINI_USE_HIGH_DENSITY));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
}

// Confirms the menu items for unregistered crostini apps (i.e. apps that do not
// have an associated .desktop file, and therefore can only be closed).
TEST_F(ShelfContextMenuTest, CrostiniUnregisteredApps) {
  const std::string fake_window_app_id = "foo";
  const std::string fake_window_startup_id = "bar";
  const std::string app_id = crostini::GetCrostiniShelfAppId(
      profile(), &fake_window_app_id, &fake_window_startup_id);
  controller()->PinAppWithID(app_id);
  const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
  ASSERT_TRUE(item);

  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, primary_id);

  EXPECT_EQ(menu->GetItemCount(), 1);
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::MENU_NEW_WINDOW));
}

TEST_F(ShelfContextMenuTest, WebApp) {
  constexpr char kWebAppUrl[] = "https://webappone.com/";
  constexpr char kWebAppName[] = "WebApp1";

  app_service_test().FlushMojoCalls();
  const web_app::AppId app_id = web_app::test::InstallDummyWebApp(
      profile(), kWebAppName, GURL(kWebAppUrl));

  controller()->PinAppWithID(app_id);
  const ash::ShelfItem* item = controller()->GetItem(ash::ShelfID(app_id));
  ASSERT_TRUE(item);

  ash::ShelfItemDelegate* item_delegate =
      model()->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);
  int64_t primary_id = GetPrimaryDisplay().id();
  std::unique_ptr<ui::MenuModel> menu =
      GetContextMenu(item_delegate, primary_id);

  // Check that every menu item has an icon
  for (int i = 0; i < menu->GetItemCount(); ++i)
    EXPECT_FALSE(menu->GetIconAt(i).IsEmpty());

  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::SHOW_APP_INFO));
  EXPECT_FALSE(IsItemEnabledInMenu(menu.get(), ash::UNINSTALL));
}

}  // namespace
