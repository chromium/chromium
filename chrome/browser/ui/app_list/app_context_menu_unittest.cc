// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/arc/icon_decode_request.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ui/app_list/app_service/app_service_context_menu.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "extensions/common/manifest_constants.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/test/test_screen.h"

using web_app::ProviderType;

namespace {

class FakeAppContextMenuDelegate : public app_list::AppContextMenuDelegate {
 public:
  FakeAppContextMenuDelegate() = default;
  FakeAppContextMenuDelegate(const FakeAppContextMenuDelegate&) = delete;
  FakeAppContextMenuDelegate& operator=(const FakeAppContextMenuDelegate&) =
      delete;
  ~FakeAppContextMenuDelegate() override = default;

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override {}
};

class FakeAppListControllerDelegate
    : public test::TestAppListControllerDelegate {
 public:
  FakeAppListControllerDelegate() = default;
  FakeAppListControllerDelegate(const FakeAppListControllerDelegate&) = delete;
  FakeAppListControllerDelegate& operator=(
      const FakeAppListControllerDelegate&) = delete;
  ~FakeAppListControllerDelegate() override = default;

  void SetAppPinnable(const std::string& app_id, Pinnable type) {
    pinnable_apps_[app_id] = type;
  }

  void SetAppOpen(const std::string& app_id, bool open) {
    if (open)
      open_apps_.insert(app_id);
    else
      open_apps_.erase(app_id);
  }

  bool IsAppOpen(const std::string& app_id) const override {
    return open_apps_.count(app_id) != 0;
  }

  // test::TestAppListControllerDelegate overrides:
  Pinnable GetPinnable(const std::string& app_id) override {
    std::map<std::string, Pinnable>::const_iterator it;
    it = pinnable_apps_.find(app_id);
    if (it == pinnable_apps_.end())
      return NO_PIN;
    return it->second;
  }

 private:
  std::map<std::string, Pinnable> pinnable_apps_;
  std::unordered_set<std::string> open_apps_;
};

std::unique_ptr<KeyedService> MenuManagerFactory(
    content::BrowserContext* context) {
  return extensions::MenuManagerFactory::BuildServiceInstanceForTesting(
      context);
}

std::unique_ptr<AppServiceAppItem> GetAppListItem(Profile* profile,
                                                  const std::string& app_id) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  std::unique_ptr<AppServiceAppItem> item;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [profile, &item](const apps::AppUpdate& update) {
        item = std::make_unique<AppServiceAppItem>(profile, nullptr, nullptr,
                                                   update);
      });
  return item;
}

std::unique_ptr<ui::SimpleMenuModel> GetContextMenuModel(
    ChromeAppListItem* item) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu;
  item->GetContextMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
        menu = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  return menu;
}

std::unique_ptr<ui::SimpleMenuModel> GetMenuModel(
    app_list::AppContextMenu* context_menu) {
  base::RunLoop run_loop;
  std::unique_ptr<ui::SimpleMenuModel> menu;
  context_menu->GetMenuModel(base::BindLambdaForTesting(
      [&](std::unique_ptr<ui::SimpleMenuModel> created_menu) {
        menu = std::move(created_menu);
        run_loop.Quit();
      }));
  run_loop.Run();
  return menu;
}

}  // namespace

class AppContextMenuTest : public AppListTestBase,
                           public ::testing::WithParamInterface<ProviderType> {
 public:
  AppContextMenuTest() {
    if (GetParam() == web_app::ProviderType::kWebApps) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else if (GetParam() == web_app::ProviderType::kBookmarkApps) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }
  AppContextMenuTest(const AppContextMenuTest&) = delete;
  AppContextMenuTest& operator=(const AppContextMenuTest&) = delete;
  ~AppContextMenuTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();
    display::Screen::SetScreenInstance(&test_screen_);
    extensions::MenuManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&MenuManagerFactory));
    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    menu_delegate_ = std::make_unique<FakeAppContextMenuDelegate>();
    model_updater_ = std::make_unique<FakeAppListModelUpdater>();
    ChromeAppListItem::OverrideAppListControllerDelegateForTesting(
        controller());
  }

  void TearDown() override {
    // Let any in-flight tasks finish, otherwise the test might flake
    // (crbug.com/1115763).
    app_service_test().WaitForAppService();
    menu_delegate_.reset();
    controller_.reset();
    menu_manager_.reset();
  }

 protected:
  struct MenuState {
    // Defines separator.
    MenuState() : command_id(-1), is_enabled(true), is_checked(false) {}

    // Defines enabled unchecked command.
    explicit MenuState(int command_id)
        : command_id(command_id), is_enabled(true), is_checked(false) {}

    MenuState(int command_id, bool enabled, bool checked)
        : command_id(command_id), is_enabled(enabled), is_checked(checked) {}

    int command_id;
    bool is_enabled;
    bool is_checked;
  };

  void ValidateItemState(const ui::MenuModel* menu_model,
                         int index,
                         const MenuState& state) {
    EXPECT_EQ(state.command_id, menu_model->GetCommandIdAt(index));
    if (state.command_id == -1)
      return;  // Don't check separator.
    EXPECT_EQ(state.is_enabled, menu_model->IsEnabledAt(index));
    EXPECT_EQ(state.is_checked, menu_model->IsItemCheckedAt(index));
  }

  void ValidateMenuState(const ui::MenuModel* menu_model,
                         const std::vector<MenuState>& states) {
    ASSERT_NE(nullptr, menu_model);
    size_t state_index = 0;
    for (int i = 0; i < menu_model->GetItemCount(); ++i) {
      ASSERT_LT(state_index, states.size());
      ValidateItemState(menu_model, i, states[state_index++]);
    }
    EXPECT_EQ(state_index, states.size());
  }

  FakeAppListControllerDelegate* controller() { return controller_.get(); }

  FakeAppContextMenuDelegate* menu_delegate() { return menu_delegate_.get(); }

  Profile* profile() { return profile_.get(); }

  void AddToStates(const AppServiceContextMenu& menu,
                   MenuState state,
                   std::vector<MenuState>* states) {
    // If the command is not enabled do not add it to states.
    if (!menu.IsCommandIdEnabled(state.command_id))
      return;

    states->push_back(state);
  }

  scoped_refptr<extensions::Extension> MakeApp(const std::string& app_id,
                                               bool platform_app) {
    base::FilePath path;
    base::PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("manifest_tests");
    base::FilePath manifest_path =
        (platform_app) ? path.AppendASCII("init_valid_platform_app.json")
                       : path.AppendASCII("hosted_app_absolute_options.json");

    JSONFileValueDeserializer deserializer(manifest_path);
    std::unique_ptr<base::Value> manifest =
        deserializer.Deserialize(nullptr, nullptr);

    base::Value value = base::Value(std::move(*manifest));
    DCHECK(value.is_dict());
    const base::DictionaryValue* dictionary_manifest = nullptr;
    value.GetAsDictionary(&dictionary_manifest);
    std::string error;
    return extensions::Extension::Create(
        path.DirName(), extensions::Manifest::INTERNAL, *dictionary_manifest,
        extensions::Extension::NO_FLAGS, app_id, &error);
  }

  void TestExtensionApp(const std::string& app_id,
                        bool platform_app,
                        AppListControllerDelegate::Pinnable pinnable,
                        extensions::LaunchType launch_type) {
    scoped_refptr<extensions::Extension> store = MakeApp(app_id, platform_app);
    service_->AddExtension(store.get());
    extensions::ExtensionRegistry::Get(profile())->AddEnabled(store);
    app_service_test_.SetUp(profile());
    app_service_test_.FlushMojoCalls();

    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    controller_->SetAppPinnable(app_id, pinnable);
    controller_->SetExtensionLaunchType(profile(), app_id, launch_type);

    AppServiceContextMenu menu(menu_delegate(), profile(), app_id,
                               controller());
    std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    if (!platform_app)
      AddToStates(menu, MenuState(ash::LAUNCH_NEW), &states);

    if (pinnable != AppListControllerDelegate::NO_PIN) {
      AddToStates(
          menu,
          MenuState(ash::TOGGLE_PIN,
                    pinnable != AppListControllerDelegate::PIN_FIXED, false),
          &states);
    }
    if (!platform_app)
      AddToStates(menu, MenuState(ash::OPTIONS), &states);
    AddToStates(menu, MenuState(ash::UNINSTALL), &states);
    AddToStates(menu, MenuState(ash::SHOW_APP_INFO), &states);

    ValidateMenuState(menu_model.get(), states);
  }

  scoped_refptr<extensions::Extension> MakeChromeApp() {
    std::string err;
    base::DictionaryValue value;
    value.SetString("name", "Chrome App");
    value.SetString("version", "0.0");
    value.SetString("app.launch.web_url", "http://google.com");
    scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
        base::FilePath(), extensions::Manifest::INTERNAL, value,
        extensions::Extension::WAS_INSTALLED_BY_DEFAULT,
        extension_misc::kChromeAppId, &err);
    EXPECT_EQ(err, "");
    return app;
  }

  void TestChromeApp() {
    scoped_refptr<extensions::Extension> store = MakeChromeApp();
    service_->AddExtension(store.get());
    app_service_test_.SetUp(profile());
    app_service_test_.FlushMojoCalls();

    controller_ = std::make_unique<FakeAppListControllerDelegate>();
    AppServiceContextMenu menu(menu_delegate(), profile(),
                               extension_misc::kChromeAppId, controller());
    std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
    ASSERT_NE(nullptr, menu_model);

    std::vector<MenuState> states;
    AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_WINDOW), &states);
    if (!profile()->IsOffTheRecord())
      AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW),
                  &states);
    AddToStates(menu, MenuState(ash::SHOW_APP_INFO), &states);
    ValidateMenuState(menu_model.get(), states);
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  display::test::TestScreen test_screen_;
  std::unique_ptr<KeyedService> menu_manager_;
  std::unique_ptr<FakeAppListControllerDelegate> controller_;
  std::unique_ptr<FakeAppContextMenuDelegate> menu_delegate_;
  std::unique_ptr<FakeAppListModelUpdater> model_updater_;
  apps::AppServiceTest app_service_test_;
};

TEST_P(AppContextMenuTest, ExtensionApp) {
  for (extensions::LaunchType launch_type = extensions::LAUNCH_TYPE_FIRST;
       launch_type < extensions::NUM_LAUNCH_TYPES;
       launch_type = static_cast<extensions::LaunchType>(launch_type + 1)) {
    AppListControllerDelegate::Pinnable pinnable;
    for (pinnable = AppListControllerDelegate::NO_PIN;
         pinnable <= AppListControllerDelegate::PIN_FIXED;
         pinnable =
             static_cast<AppListControllerDelegate::Pinnable>(pinnable + 1)) {
      for (size_t combinations = 0; combinations < (1 << 2); ++combinations) {
        TestExtensionApp(AppListTestBase::kHostedAppId,
                         (combinations & (1 << 0)) != 0, pinnable, launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp1Id,
                         (combinations & (1 << 0)) != 0, pinnable, launch_type);
        TestExtensionApp(AppListTestBase::kPackagedApp2Id,
                         (combinations & (1 << 0)) != 0, pinnable, launch_type);
      }
    }
  }
}

TEST_P(AppContextMenuTest, ChromeApp) {
  TestChromeApp();
}

TEST_P(AppContextMenuTest, NonExistingExtensionApp) {
  AppServiceContextMenu menu(menu_delegate(), profile(),
                             "some_non_existing_extension_app", controller());
  std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
  EXPECT_EQ(nullptr, menu_model);
}

TEST_P(AppContextMenuTest, ArcMenu) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::AppInfo& app_info = arc_test.fake_apps()[1];
  const std::string app_id = ArcAppTest::GetAppId(app_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());
  app_service_test().FlushMojoCalls();

  std::unique_ptr<AppServiceAppItem> item = GetAppListItem(profile(), app_id);

  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
  ASSERT_NE(nullptr, menu);

  // Separators are not added to touchable app context menus. For touchable app
  // context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  const int expected_items = 10;

  ASSERT_EQ(expected_items, menu->GetItemCount());
  int index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

  // Test activate request.
  EXPECT_EQ(0u, arc_test.app_instance()->launch_requests().size());

  menu->ActivatedAt(0);
  app_service_test().FlushMojoCalls();

  const std::vector<std::unique_ptr<arc::FakeAppInstance::Request>>&
      launch_requests = arc_test.app_instance()->launch_requests();
  ASSERT_EQ(1u, launch_requests.size());
  EXPECT_TRUE(launch_requests[0]->IsForApp(app_info));

  controller()->SetAppOpen(app_id, true);
  arc_test.app_instance()->SendTaskCreated(1, app_info, std::string());

  // It is not expected that menu model is unchanged on GetContextMenuModel.
  // ARC app menu requires model to be recalculated.
  menu = GetContextMenuModel(item.get());

  // Separators are not added to touchable app context menus except for arc app
  // shortcuts, which have double separator, three more app shortcuts provided
  // by arc::FakeAppInstance and two separators between shortcuts.
  const int expected_items_app_open = 9;
  ASSERT_EQ(expected_items_app_open, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }

  // Test launching app shortcut item.
  EXPECT_EQ(0, arc_test.app_instance()->launch_app_shortcut_item_count());
  menu->ActivatedAt(menu->GetItemCount() - 1);
  app_service_test().FlushMojoCalls();
  EXPECT_EQ(1, arc_test.app_instance()->launch_app_shortcut_item_count());

  // This makes all apps non-ready.
  controller()->SetAppOpen(app_id, false);
  arc_test.app_instance()->SendTaskDestroyed(1);
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = GetContextMenuModel(item.get());

  // Separators and disabled options are not added to touchable app context
  // menus. For touchable app context menus, arc app has double separator,
  // three more app shortcuts provided by arc::FakeAppInstance and two
  // separators between shortcuts.
  const int expected_items_reopen = 8;
  ASSERT_EQ(expected_items_reopen, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }

  // Uninstall all apps.
  arc_test.app_instance()->SendRefreshAppList(
      std::vector<arc::mojom::AppInfo>());
  app_service_test().FlushMojoCalls();
  controller()->SetAppOpen(app_id, false);

  // No app available case.
  menu = GetContextMenuModel(item.get());
  EXPECT_EQ(0, menu->GetItemCount());
}

TEST_P(AppContextMenuTest, ArcMenuShortcut) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  const arc::mojom::ShortcutInfo& shortcut_info = arc_test.fake_shortcuts()[0];
  const std::string app_id = ArcAppTest::GetAppId(shortcut_info);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);

  arc_test.app_instance()->SendInstallShortcuts(arc_test.fake_shortcuts());
  app_service_test().FlushMojoCalls();

  std::unique_ptr<AppServiceAppItem> item = GetAppListItem(profile(), app_id);

  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
  ASSERT_NE(nullptr, menu);
  // Separators are not added to touchable app context menus. For touchable app
  // context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  const int expected_items = 10;
  int index = 0;
  ASSERT_EQ(expected_items, menu->GetItemCount());
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));
  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }

  // This makes all apps non-ready. Shortcut is still uninstall-able.
  arc::ConnectionObserver<arc::mojom::AppInstance>* connection_observer =
      arc_test.arc_app_list_prefs();
  connection_observer->OnConnectionClosed();

  menu = GetContextMenuModel(item.get());
  // Separators and disabled options are not added to touchable app context
  // menus. For touchable app context menus, arc app has double separator,
  // three more app shortcuts provided by arc::FakeAppInstance and two
  // separators between shortcuts.
  const int expected_items_non_ready = 9;
  ASSERT_EQ(expected_items_non_ready, menu->GetItemCount());
  index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::UNINSTALL));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
  EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
  for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
    EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
              base::UTF16ToUTF8(menu->GetLabelAt(index++)));
    if (index < menu->GetItemCount())
      EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }
}

TEST_P(AppContextMenuTest, ArcMenuStickyItem) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  arc_test.app_instance()->SendRefreshAppList(arc_test.fake_apps());
  app_service_test().FlushMojoCalls();

  {
    // Verify menu of store
    const arc::mojom::AppInfo& store_info = arc_test.fake_apps()[0];
    const std::string store_id = ArcAppTest::GetAppId(store_info);
    controller()->SetAppPinnable(store_id,
                                 AppListControllerDelegate::PIN_EDITABLE);
    std::unique_ptr<AppServiceAppItem> item =
        GetAppListItem(profile(), store_id);
    std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
    ASSERT_NE(nullptr, menu);

    // Separators are not added to touchable app context menus. For touchable
    // app context menus, arc app has double separator, three more app shortcuts
    // provided by arc::FakeAppInstance and two separators between shortcuts.
    int expected_items = 9;
    ASSERT_EQ(expected_items, menu->GetItemCount());
    int index = 0;
    ValidateItemState(menu.get(), index++, MenuState(ash::LAUNCH_NEW));
    ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
    ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

    // Test that arc app shortcuts provided by arc::FakeAppInstance have a
    // separator between each app shortcut.
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
    }
  }
}

// In suspended state app does not have launch item.
TEST_P(AppContextMenuTest, ArcMenuSuspendedItem) {
  app_service_test().SetUp(profile());
  ArcAppTest arc_test;
  arc_test.SetUp(profile());

  arc::mojom::AppInfo app = arc_test.fake_apps()[0];
  app.suspended = true;

  arc_test.app_instance()->SendRefreshAppList({app});
  app_service_test().FlushMojoCalls();

  const std::string app_id = ArcAppTest::GetAppId(app);
  controller()->SetAppPinnable(app_id, AppListControllerDelegate::PIN_EDITABLE);
  std::unique_ptr<AppServiceAppItem> item = GetAppListItem(profile(), app_id);
  std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
  ASSERT_NE(nullptr, menu);

  // Separators are not added to touchable app context menus. For touchable
  // app context menus, arc app has double separator, three more app shortcuts
  // provided by arc::FakeAppInstance and two separators between shortcuts.
  int expected_items = 8;
  ASSERT_EQ(expected_items, menu->GetItemCount());
  int index = 0;
  ValidateItemState(menu.get(), index++, MenuState(ash::TOGGLE_PIN));
  ValidateItemState(menu.get(), index++, MenuState(ash::SHOW_APP_INFO));

  // Test that arc app shortcuts provided by arc::FakeAppInstance have a
  // separator between each app shortcut.
    EXPECT_EQ(ui::DOUBLE_SEPARATOR, menu->GetSeparatorTypeAt(index++));
    for (int shortcut_index = 0; index < menu->GetItemCount(); ++index) {
      EXPECT_EQ(base::StringPrintf("ShortLabel %d", shortcut_index++),
                base::UTF16ToUTF8(menu->GetLabelAt(index++)));
      if (index < menu->GetItemCount())
        EXPECT_EQ(ui::PADDED_SEPARATOR, menu->GetSeparatorTypeAt(index));
  }
}

TEST_P(AppContextMenuTest, CommandIdsMatchEnumsForHistograms) {
  // Tests that CommandId enums are not changed as the values are used in
  // histograms.
  EXPECT_EQ(9, ash::NOTIFICATION_CONTAINER);
  EXPECT_EQ(100, ash::LAUNCH_NEW);
  EXPECT_EQ(101, ash::TOGGLE_PIN);
  EXPECT_EQ(102, ash::SHOW_APP_INFO);
  EXPECT_EQ(103, ash::OPTIONS);
  EXPECT_EQ(104, ash::UNINSTALL);
  EXPECT_EQ(105, ash::REMOVE_FROM_FOLDER);
  EXPECT_EQ(106, ash::APP_CONTEXT_MENU_NEW_WINDOW);
  EXPECT_EQ(107, ash::APP_CONTEXT_MENU_NEW_INCOGNITO_WINDOW);
  EXPECT_EQ(108, ash::INSTALL);
  EXPECT_EQ(200, ash::USE_LAUNCH_TYPE_COMMAND_START);
  EXPECT_EQ(200, ash::USE_LAUNCH_TYPE_PINNED);
  EXPECT_EQ(201, ash::USE_LAUNCH_TYPE_REGULAR);
  EXPECT_EQ(202, ash::USE_LAUNCH_TYPE_FULLSCREEN);
  EXPECT_EQ(203, ash::USE_LAUNCH_TYPE_WINDOW);
}

// Tests that internal app's context menu is correct.
TEST_P(AppContextMenuTest, InternalAppMenu) {
  for (const auto& internal_app : app_list::GetInternalAppList(profile())) {
    controller()->SetAppPinnable(internal_app.app_id,
                                 AppListControllerDelegate::PIN_EDITABLE);

    std::unique_ptr<AppServiceAppItem> item =
        GetAppListItem(profile(), internal_app.app_id);
    std::unique_ptr<ui::MenuModel> menu = GetContextMenuModel(item.get());
    ASSERT_NE(nullptr, menu);
    EXPECT_EQ(1, menu->GetItemCount());
    ValidateItemState(menu.get(), 0, MenuState(ash::TOGGLE_PIN));
  }
}

// Lacros has its own test suite because the feature needs to be enabled before
// SetUp().
class AppContextMenuLacrosTest : public AppContextMenuTest {
 public:
  AppContextMenuLacrosTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  }
  AppContextMenuLacrosTest(const AppContextMenuLacrosTest&) = delete;
  AppContextMenuLacrosTest& operator=(const AppContextMenuLacrosTest&) = delete;
  ~AppContextMenuLacrosTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AppContextMenuLacrosTest, LacrosApp) {
  app_service_test().SetUp(profile());
  app_service_test().FlushMojoCalls();

  // Create the context menu.
  AppServiceContextMenu menu(menu_delegate(), profile(),
                             extension_misc::kLacrosAppId, controller());
  std::unique_ptr<ui::MenuModel> menu_model = GetMenuModel(&menu);
  ASSERT_NE(menu_model, nullptr);

  // Verify expected menu items.
  EXPECT_EQ(menu_model->GetItemCount(), 1);
  std::vector<MenuState> states;
  AddToStates(menu, MenuState(ash::APP_CONTEXT_MENU_NEW_WINDOW), &states);
  ValidateMenuState(menu_model.get(), states);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppContextMenuTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);

INSTANTIATE_TEST_SUITE_P(All,
                         AppContextMenuLacrosTest,
                         ::testing::Values(ProviderType::kBookmarkApps,
                                           ProviderType::kWebApps),
                         web_app::ProviderTypeParamToString);
