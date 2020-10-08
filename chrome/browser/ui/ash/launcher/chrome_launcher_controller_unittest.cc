// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ash/display/display_configuration_controller.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_application_menu_model.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_service/app_service_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface_util.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/app_window/app_window_contents.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;
using extensions::Extension;
using extensions::Manifest;
using extensions::UnloadedExtensionReason;
using web_app::ProviderType;

namespace {
constexpr char kOfflineGmailUrl[] = "https://mail.google.com/mail/mu/u";
constexpr char kGmailUrl[] = "https://mail.google.com/mail/u";
constexpr char kGmailLaunchURL[] = "https://mail.google.com/mail/ca";
constexpr char kLaunchURL[] = "https://foo.example/";

// An extension prefix.
constexpr char kCrxAppPrefix[] = "_crx_";

// Dummy app id is used to put at least one pin record to prevent initializing
// pin model with default apps that can affect some tests.
constexpr char kDummyAppId[] = "dummyappid_dummyappid_dummyappid";

// Test implementation of AppIconLoader.
class TestAppIconLoaderImpl : public AppIconLoader {
 public:
  TestAppIconLoaderImpl() = default;
  TestAppIconLoaderImpl(const TestAppIconLoaderImpl&) = delete;
  TestAppIconLoaderImpl& operator=(const TestAppIconLoaderImpl&) = delete;
  ~TestAppIconLoaderImpl() override = default;

  void AddSupportedApp(const std::string& id) { supported_apps_.insert(id); }

  // AppIconLoader implementation:
  bool CanLoadImageForApp(const std::string& id) override {
    return supported_apps_.find(id) != supported_apps_.end();
  }
  void FetchImage(const std::string& id) override { ++fetch_count_; }
  void ClearImage(const std::string& id) override { ++clear_count_; }
  void UpdateImage(const std::string& id) override {}

  int fetch_count() const { return fetch_count_; }
  int clear_count() const { return clear_count_; }

 private:
  int fetch_count_ = 0;
  int clear_count_ = 0;
  std::set<std::string> supported_apps_;
};

// Test implementation of LauncherControllerHelper.
class TestLauncherControllerHelper : public LauncherControllerHelper {
 public:
  TestLauncherControllerHelper() : LauncherControllerHelper(nullptr) {}
  explicit TestLauncherControllerHelper(Profile* profile)
      : LauncherControllerHelper(profile) {}
  TestLauncherControllerHelper(const TestLauncherControllerHelper&) = delete;
  TestLauncherControllerHelper& operator=(const TestLauncherControllerHelper&) =
      delete;
  ~TestLauncherControllerHelper() override = default;

  // Sets the id for the specified tab.
  void SetAppID(content::WebContents* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(content::WebContents* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // LauncherControllerHelper:
  std::string GetAppID(content::WebContents* tab) override {
    return tab_id_map_.find(tab) != tab_id_map_.end() ? tab_id_map_[tab]
                                                      : std::string();
  }

  bool IsValidIDForCurrentUser(const std::string& id) const override {
    for (TabToStringMap::const_iterator i = tab_id_map_.begin();
         i != tab_id_map_.end(); ++i) {
      if (i->second == id)
        return true;
    }
    return false;
  }

  ArcAppListPrefs* GetArcAppListPrefs() const override { return nullptr; }

 private:
  typedef std::map<content::WebContents*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;
};

// Test implementation of a V2 app launcher item controller.
class TestV2AppLauncherItemController : public ash::ShelfItemDelegate {
 public:
  explicit TestV2AppLauncherItemController(const std::string& app_id)
      : ash::ShelfItemDelegate(ash::ShelfID(app_id)) {}

  TestV2AppLauncherItemController(const TestV2AppLauncherItemController&) =
      delete;
  TestV2AppLauncherItemController& operator=(
      const TestV2AppLauncherItemController&) = delete;
  ~TestV2AppLauncherItemController() override = default;

  // Override for ash::ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback,
                    const ItemFilterPredicate& filter_predicate) override {
    std::move(callback).Run(ash::SHELF_ACTION_WINDOW_ACTIVATED, {});
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}
};

// Simulates selection of the shelf item.
void SelectItem(ash::ShelfItemDelegate* delegate) {
  std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_NONE, 0);
  delegate->ItemSelected(std::move(event), display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN, base::DoNothing(),
                         base::NullCallback());
}

bool IsWindowOnDesktopOfUser(aura::Window* window,
                             const AccountId& account_id) {
  return MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
      window, account_id);
}

}  // namespace

// Test parameters include web_app::ProviderType and Play Store flag (only used
// by ChromeLauncherControllerPlayStoreAvailabilityTest).
typedef std::pair<ProviderType, bool> TestParameter;

class ChromeLauncherControllerTest
    : public BrowserWithTestWindowTest,
      public ::testing::WithParamInterface<TestParameter> {
 protected:
  ChromeLauncherControllerTest()
      : BrowserWithTestWindowTest(Browser::TYPE_NORMAL) {
    if (GetParam().first == web_app::ProviderType::kWebApps) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else if (GetParam().first == web_app::ProviderType::kBookmarkApps) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }

  ChromeLauncherControllerTest(const ChromeLauncherControllerTest&) = delete;
  ChromeLauncherControllerTest& operator=(const ChromeLauncherControllerTest&) =
      delete;
  ~ChromeLauncherControllerTest() override = default;

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);

    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);

    BrowserWithTestWindowTest::SetUp();

    model_ = std::make_unique<ash::ShelfModel>();

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName,
                       "launcher controller test extension");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "for testing pinned apps");
    // AppService checks the app's type. So set the
    // manifest_keys::kLaunchWebURL, so that the extension can get the type
    // from manifest value, and then AppService can get the extension's type.
    manifest.SetString(extensions::manifest_keys::kLaunchWebURL, kLaunchURL);

    base::DictionaryValue manifest_platform_app;
    manifest_platform_app.SetString(extensions::manifest_keys::kName,
                                    "launcher controller test platform app");
    manifest_platform_app.SetString(extensions::manifest_keys::kVersion, "1");
    manifest_platform_app.SetString(extensions::manifest_keys::kDescription,
                                    "for testing pinned platform apps");
    manifest_platform_app.SetString(extensions::manifest_keys::kApp, "true");
    manifest_platform_app.Set(extensions::manifest_keys::kPlatformAppBackground,
                              std::make_unique<base::DictionaryValue>());
    auto scripts = std::make_unique<base::ListValue>();
    scripts->AppendString("main.js");
    manifest_platform_app.Set(
        extensions::manifest_keys::kPlatformAppBackgroundScripts,
        std::move(scripts));

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    DCHECK(profile());
    extension_registry_ = extensions::ExtensionRegistry::Get(profile());
    app_service_test_.SetUp(profile());

    if (auto_start_arc_test_)
      arc_test_.SetUp(profile());

    // Wait until |extension_system| is signaled as started.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    // Many pinned app tests assume OS sync is enabled.
    if (chromeos::features::IsSplitSettingsSyncEnabled()) {
      syncer::SyncService* sync_service =
          ProfileSyncServiceFactory::GetForProfile(profile());
      sync_service->GetUserSettings()->SetOsSyncFeatureEnabled(true);
    }

    app_list_syncable_service_ =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    StartAppSyncService(app_list_syncable_service_->GetAllSyncDataForTesting());

    std::string error;
    extension_chrome_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                          manifest, Extension::NO_FLAGS,
                                          extension_misc::kChromeAppId, &error);
    extension1_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", &error);
    extension2_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", &error);
    extension5_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "cccccccccccccccccccccccccccccccc", &error);
    extension6_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "dddddddddddddddddddddddddddddddd", &error);
    extension7_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", &error);
    extension8_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    "ffffffffffffffffffffffffffffffff", &error);
    extension_platform_app_ = Extension::Create(
        base::FilePath(), Manifest::UNPACKED, manifest_platform_app,
        Extension::NO_FLAGS, "gggggggggggggggggggggggggggggggg", &error);
    arc_support_host_ =
        Extension::Create(base::FilePath(), Manifest::UNPACKED, manifest,
                          Extension::NO_FLAGS, arc::kPlayStoreAppId, &error);
    extension_service_->AddExtension(extension_chrome_.get());

    // Fake Gmail app.
    base::DictionaryValue manifest_gmail;
    manifest_gmail.SetString(extensions::manifest_keys::kName,
                             "Gmail launcher controller test extension");
    manifest_gmail.SetString(extensions::manifest_keys::kVersion, "1");
    manifest_gmail.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
    manifest_gmail.SetString(extensions::manifest_keys::kDescription,
                             "for testing pinned Gmail");
    manifest_gmail.SetString(extensions::manifest_keys::kLaunchWebURL,
                             kGmailLaunchURL);
    auto list = std::make_unique<base::ListValue>();
    list->AppendString("*://mail.google.com/mail/ca");
    manifest_gmail.Set(extensions::manifest_keys::kWebURLs, std::move(list));

    extension_gmail_app_ = Extension::Create(
        base::FilePath(), Manifest::UNPACKED, manifest_gmail,
        Extension::NO_FLAGS, extension_misc::kGmailAppId, &error);
    // Fake Google Doc app.
    extension_doc_app_ = Extension::Create(
        base::FilePath(), Manifest::UNPACKED, manifest, Extension::NO_FLAGS,
        extension_misc::kGoogleDocAppId, &error);

    // Fake Youtube app.
    extension_youtube_app_ = Extension::Create(
        base::FilePath(), Manifest::UNPACKED, manifest, Extension::NO_FLAGS,
        extension_misc::kYoutubeAppId, &error);

    MaybeStartWebAppProvider();
  }

  virtual void MaybeStartWebAppProvider() {
    if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
      web_app::TestWebAppProvider::Get(profile())->Start();
  }

  ui::BaseWindow* GetLastActiveWindowForItemController(
      AppWindowLauncherItemController* item_controller) {
    return item_controller->last_active_window_;
  }

  // Creates a running platform V2 app (not pinned) of type |app_id|.
  virtual void CreateRunningV2App(const std::string& app_id) {
    DCHECK(!test_controller_);
    // Change the created launcher controller into a V2 app controller.
    std::unique_ptr<TestV2AppLauncherItemController> controller =
        std::make_unique<TestV2AppLauncherItemController>(app_id);
    test_controller_ = controller.get();
    ash::ShelfID id = launcher_controller_->InsertAppLauncherItem(
        std::move(controller), ash::STATUS_RUNNING, model_->item_count(),
        ash::TYPE_APP);
    DCHECK(launcher_controller_->IsPlatformApp(id));
  }

  // Sets the stage for a multi user test.
  virtual void SetUpMultiUserScenario(syncer::SyncChangeList* user_a,
                                      syncer::SyncChangeList* user_b) {
    InitLauncherController();
    EXPECT_EQ("Chrome", GetPinnedAppStatus());

    // Set an empty pinned pref to begin with.
    syncer::SyncChangeList sync_list;
    InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
    SendPinChanges(sync_list, true);
    EXPECT_EQ("Chrome", GetPinnedAppStatus());

    // Assume all applications have been added already.
    extension_service_->AddExtension(extension1_.get());
    extension_service_->AddExtension(extension2_.get());
    extension_service_->AddExtension(extension_gmail_app_.get());
    extension_service_->AddExtension(extension_doc_app_.get());
    extension_service_->AddExtension(extension5_.get());
    extension_service_->AddExtension(extension6_.get());
    extension_service_->AddExtension(extension7_.get());
    extension_service_->AddExtension(extension8_.get());
    extension_service_->AddExtension(extension_platform_app_.get());
    // There should be nothing in the list by now.
    EXPECT_EQ("Chrome", GetPinnedAppStatus());

    // Set user a preferences.
    InsertAddPinChange(user_a, 0, extension1_->id());
    InsertAddPinChange(user_a, 1, extension2_->id());
    InsertAddPinChange(user_a, 2, extension_gmail_app_->id());
    InsertAddPinChange(user_a, 3, extension_platform_app_->id());
    InsertAddPinChange(user_a, 4, extension_doc_app_->id());
    InsertAddPinChange(user_a, 5, extension5_->id());
    InsertAddPinChange(user_a, 6, extension_misc::kChromeAppId);

    // Set user b preferences.
    InsertAddPinChange(user_b, 0, extension6_->id());
    InsertAddPinChange(user_b, 1, extension7_->id());
    InsertAddPinChange(user_b, 2, extension8_->id());
    InsertAddPinChange(user_b, 3, extension_misc::kChromeAppId);
  }

  void TearDown() override {
    arc_test_.TearDown();
    launcher_controller_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
    app_list::AppListSyncableServiceFactory::SetUseInTesting(false);
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return std::unique_ptr<TestBrowserWindow>(CreateTestBrowserWindowAura());
  }

  std::unique_ptr<Browser> CreateBrowserWithTestWindowForProfile(
      Profile* profile) {
    TestBrowserWindow* browser_window = CreateTestBrowserWindowAura();
    new TestBrowserWindowOwner(browser_window);
    return CreateBrowser(profile, Browser::TYPE_NORMAL, false, browser_window);
  }

  // Create an uninitialized chrome launcher controller instance.
  ChromeLauncherController* CreateLauncherController() {
    launcher_controller_ =
        std::make_unique<ChromeLauncherController>(profile(), model_.get());
    return launcher_controller_.get();
  }

  // Create and initialize the controller, owned by the test shell delegate.
  void InitLauncherController() {
    CreateLauncherController()->Init();
    app_service_test_.FlushMojoCalls();
  }

  // Create and initialize the controller; create a tab and show the browser.
  void InitLauncherControllerWithBrowser() {
    InitLauncherController();
    chrome::NewTab(browser());
    browser()->window()->Show();
  }

  // Destroy the launcher controller instance and clear the local pointer.
  void ResetLauncherController() { launcher_controller_.reset(); }

  // Destroy and recreate the controller; clear and reinitialize the ShelfModel.
  // Returns a pointer to the uninitialized controller, owned by shell delegate.
  // TODO(msw): This does not accurately represent ChromeLauncherController
  // lifetime or usage in production, and does not accurately simulate restarts.
  ChromeLauncherController* RecreateLauncherController() {
    // Destroy any existing controller first; only one may exist at a time.
    ResetLauncherController();
    model_ = std::make_unique<ash::ShelfModel>();
    return CreateLauncherController();
  }

  void StartAppSyncService(const syncer::SyncDataList& init_sync_list) {
    app_list_syncable_service_->MergeDataAndStartSyncing(
        syncer::APP_LIST, init_sync_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>(),
        std::make_unique<syncer::SyncErrorFactoryMock>());
    EXPECT_EQ(init_sync_list.size(),
              app_list_syncable_service_->sync_items().size());
  }

  void StopAppSyncService() {
    app_list_syncable_service_->StopSyncing(syncer::APP_LIST);
  }

  // static
  syncer::ModelType GetPreferencesModelType() {
    // SplitSettingsSync makes shelf prefs into OS prefs.
    return chromeos::features::IsSplitSettingsSyncEnabled()
               ? syncer::OS_PREFERENCES
               : syncer::PREFERENCES;
  }

  sync_preferences::PrefModelAssociator* GetPrefSyncService() {
    sync_preferences::PrefServiceSyncable* pref_sync =
        profile()->GetTestingPrefService();
    sync_preferences::PrefModelAssociator* pref_sync_service =
        static_cast<sync_preferences::PrefModelAssociator*>(
            pref_sync->GetSyncableService(GetPreferencesModelType()));
    return pref_sync_service;
  }

  void StartPrefSyncService(const syncer::SyncDataList& init_sync_list) {
    base::Optional<syncer::ModelError> error =
        GetPrefSyncService()->MergeDataAndStartSyncing(
            GetPreferencesModelType(), init_sync_list,
            std::make_unique<syncer::FakeSyncChangeProcessor>(),
            std::make_unique<syncer::SyncErrorFactoryMock>());
    EXPECT_FALSE(error.has_value());
  }

  void SetAppIconLoader(std::unique_ptr<AppIconLoader> loader) {
    std::vector<std::unique_ptr<AppIconLoader>> loaders;
    loaders.push_back(std::move(loader));
    launcher_controller_->SetAppIconLoadersForTest(loaders);
  }

  void SetAppIconLoaders(std::unique_ptr<AppIconLoader> loader1,
                         std::unique_ptr<AppIconLoader> loader2) {
    std::vector<std::unique_ptr<AppIconLoader>> loaders;
    loaders.push_back(std::move(loader1));
    loaders.push_back(std::move(loader2));
    launcher_controller_->SetAppIconLoadersForTest(loaders);
  }

  void SetLauncherControllerHelper(LauncherControllerHelper* helper) {
    launcher_controller_->SetLauncherControllerHelperForTest(
        base::WrapUnique<LauncherControllerHelper>(helper));
  }

  void AppendPrefValue(base::ListValue* pref_value,
                       const std::string& extension_id) {
    base::DictionaryValue entry;
    entry.SetKey(kPinnedAppsPrefAppIDKey, base::Value(extension_id));
    pref_value->Append(std::move(entry));
  }

  void InsertRemoveAllPinsChange(syncer::SyncChangeList* list) {
    for (const auto& sync_peer : app_list_syncable_service_->sync_items()) {
      sync_pb::EntitySpecifics specifics;
      sync_pb::AppListSpecifics* app_list_specifics =
          specifics.mutable_app_list();
      app_list_specifics->set_item_id(sync_peer.first);
      app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
      syncer::SyncData sync_data =
          syncer::SyncData::CreateLocalData(sync_peer.first, "Test", specifics);
      list->push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_DELETE, sync_data));
    }
  }

  syncer::StringOrdinal GeneratePinPosition(int position) {
    syncer::StringOrdinal ordinal_position =
        syncer::StringOrdinal::CreateInitialOrdinal();
    for (int i = 0; i < position; ++i)
      ordinal_position = ordinal_position.CreateAfter();
    return ordinal_position;
  }

  void InsertPinChange(syncer::SyncChangeList* list,
                       int position,
                       bool add_pin_change,
                       const std::string& app_id,
                       syncer::SyncChange::SyncChangeType type) {
    sync_pb::EntitySpecifics specifics;
    sync_pb::AppListSpecifics* app_list_specifics =
        specifics.mutable_app_list();
    app_list_specifics->set_item_id(app_id);
    app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
    if (add_pin_change) {
      if (position >= 0) {
        app_list_specifics->set_item_pin_ordinal(
            GeneratePinPosition(position).ToInternalValue());
      } else {
        app_list_specifics->set_item_pin_ordinal(std::string());
      }
    }
    syncer::SyncData sync_data =
        syncer::SyncData::CreateLocalData(app_id, "Test", specifics);
    list->push_back(syncer::SyncChange(FROM_HERE, type, sync_data));
  }

  void InsertAddPinChange(syncer::SyncChangeList* list,
                          int position,
                          const std::string& app_id) {
    InsertPinChange(list, position, true, app_id,
                    syncer::SyncChange::ACTION_ADD);
  }

  void InsertUpdatePinChange(syncer::SyncChangeList* list,
                             int position,
                             const std::string& app_id) {
    InsertPinChange(list, position, true, app_id,
                    syncer::SyncChange::ACTION_UPDATE);
  }

  void InsertRemovePinChange(syncer::SyncChangeList* list,
                             const std::string& app_id) {
    InsertPinChange(list, -1, true, app_id, syncer::SyncChange::ACTION_UPDATE);
  }

  void InsertLegacyPinChange(syncer::SyncChangeList* list,
                             const std::string& app_id) {
    InsertPinChange(list, -1, false, app_id, syncer::SyncChange::ACTION_UPDATE);
  }

  void ResetPinModel() {
    syncer::SyncChangeList sync_list;
    InsertRemoveAllPinsChange(&sync_list);
    InsertAddPinChange(&sync_list, 0, kDummyAppId);
    app_list_syncable_service_->ProcessSyncChanges(FROM_HERE, sync_list);
  }

  void SendPinChanges(const syncer::SyncChangeList& sync_list,
                      bool reset_pin_model) {
    if (!reset_pin_model) {
      app_list_syncable_service_->ProcessSyncChanges(FROM_HERE, sync_list);
    } else {
      syncer::SyncChangeList combined_sync_list;
      InsertRemoveAllPinsChange(&combined_sync_list);
      combined_sync_list.insert(combined_sync_list.end(), sync_list.begin(),
                                sync_list.end());
      app_list_syncable_service_->ProcessSyncChanges(FROM_HERE,
                                                     combined_sync_list);
    }
    content::RunAllTasksUntilIdle();
  }

  // Set the index at which the chrome icon should be.
  void SetShelfChromeIconIndex(int index) {
    DCHECK(
        app_list_syncable_service_->GetPinPosition(extension_misc::kChromeAppId)
            .IsValid());
    syncer::StringOrdinal chrome_position;
    chrome_position = index == 0 ? GeneratePinPosition(0).CreateBefore()
                                 : GeneratePinPosition(index - 1).CreateBetween(
                                       GeneratePinPosition(index));

    syncer::SyncChangeList sync_list;
    sync_pb::EntitySpecifics specifics;
    sync_pb::AppListSpecifics* app_list_specifics =
        specifics.mutable_app_list();
    app_list_specifics->set_item_id(extension_misc::kChromeAppId);
    app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
    app_list_specifics->set_item_pin_ordinal(chrome_position.ToInternalValue());
    syncer::SyncData sync_data = syncer::SyncData::CreateLocalData(
        extension_misc::kChromeAppId, "Test", specifics);
    sync_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
    app_list_syncable_service_->ProcessSyncChanges(FROM_HERE, sync_list);
  }

  // Gets the IDs of the currently pinned app items.
  void GetPinnedAppIds(ChromeLauncherController* controller,
                       std::vector<std::string>* app_ids) {
    app_ids->clear();
    for (const auto& item : model_->items()) {
      if (item.type == ash::TYPE_PINNED_APP)
        app_ids->push_back(item.id.app_id);
    }
  }

  // Get the setup of the currently shown launcher items in one string.
  // Each pinned element will start with a big letter, each running but not
  // pinned V1 app will start with a small letter and each running but not
  // pinned V2 app will start with a '*' + small letter.
  std::string GetPinnedAppStatus() {
    std::string result;
    for (int i = 0; i < model_->item_count(); i++) {
      if (!result.empty())
        result.append(", ");
      switch (model_->items()[i].type) {
        case ash::TYPE_APP: {
          if (launcher_controller_->IsPlatformApp(model_->items()[i].id))
            result += "*";
          const std::string& app = model_->items()[i].id.app_id;
          EXPECT_FALSE(launcher_controller_->IsAppPinned(app));
          if (app == extension1_->id()) {
            result += "app1";
          } else if (app == extension2_->id()) {
            result += "app2";
          } else if (app == extension5_->id()) {
            result += "app5";
          } else if (app == extension6_->id()) {
            result += "app6";
          } else if (app == extension7_->id()) {
            result += "app7";
          } else if (app == extension8_->id()) {
            result += "app8";
          } else if (app == extension_gmail_app_->id()) {
            result += "gmail";
          } else if (app == extension_platform_app_->id()) {
            result += "platform_app";
          } else if (app == extension_doc_app_->id()) {
            result += "doc";
          } else if (app == extension_youtube_app_->id()) {
            result += "youtube";
          } else {
            result += app_service_test_.GetAppName(app);
          }
          break;
        }
        case ash::TYPE_PINNED_APP: {
          if (launcher_controller_->IsPlatformApp(model_->items()[i].id))
            result += "*";
          const std::string& app = model_->items()[i].id.app_id;
          EXPECT_TRUE(launcher_controller_->IsAppPinned(app));
          if (app == extension1_->id()) {
            result += "App1";
          } else if (app == extension2_->id()) {
            result += "App2";
          } else if (app == extension5_->id()) {
            result += "App5";
          } else if (app == extension6_->id()) {
            result += "App6";
          } else if (app == extension7_->id()) {
            result += "App7";
          } else if (app == extension8_->id()) {
            result += "App8";
          } else if (app == extension_gmail_app_->id()) {
            result += "Gmail";
          } else if (app == extension_doc_app_->id()) {
            result += "Doc";
          } else if (app == extension_youtube_app_->id()) {
            result += "Youtube";
          } else if (app == extension_platform_app_->id()) {
            result += "Platform_App";
          } else if (app == arc_support_host_->id()) {
            result += "Play Store";
          } else if (app == crostini::kCrostiniTerminalSystemAppId) {
            result += "Terminal";
          } else {
            bool arc_app_found = false;
            for (const auto& arc_app : arc_test_.fake_apps()) {
              if (app == ArcAppTest::GetAppId(arc_app)) {
                result += arc_app.name;
                arc_app_found = true;
                break;
              }
            }
            if (!arc_app_found) {
              result += app_service_test_.GetAppName(app);
            }
          }
          break;
        }
        case ash::TYPE_BROWSER_SHORTCUT:
          result += "Chrome";
          break;
        default:
          result += "Unknown";
          break;
      }
    }
    return result;
  }

  // Remember the order of unpinned but running applications for the current
  // user.
  void RememberUnpinnedRunningApplicationOrder() {
    launcher_controller_->RememberUnpinnedRunningApplicationOrder();
  }

  // Restore the order of running but unpinned applications for a given user.
  void RestoreUnpinnedRunningApplicationOrder(const AccountId& account_id) {
    launcher_controller_->RestoreUnpinnedRunningApplicationOrder(
        account_id.GetUserEmail());
  }

  void SendListOfArcApps() {
    arc_test_.app_instance()->SendRefreshAppList(arc_test_.fake_apps());
  }

  void SendListOfArcShortcuts() {
    arc_test_.app_instance()->SendInstallShortcuts(arc_test_.fake_shortcuts());
  }

  void UninstallArcApps() {
    arc_test_.app_instance()->SendRefreshAppList(
        std::vector<arc::mojom::AppInfo>());
  }

  // TODO(victorhsieh): Add test coverage for when ARC is started regardless
  // Play Store opt-in status, and the followed opt-in and opt-out.
  void EnablePlayStore(bool enabled) {
    arc::SetArcPlayStoreEnabledForProfile(profile(), enabled);
    base::RunLoop().RunUntilIdle();
  }

  void ValidateArcState(bool arc_enabled,
                        bool arc_managed,
                        arc::ArcSessionManager::State state,
                        const std::string& pin_status) {
    EXPECT_EQ(arc_enabled, arc::IsArcPlayStoreEnabledForProfile(profile()));
    EXPECT_EQ(arc_managed,
              arc::IsArcPlayStoreEnabledPreferenceManagedForProfile(profile()));
    EXPECT_EQ(state, arc_test_.arc_session_manager()->state());
    EXPECT_EQ(pin_status, GetPinnedAppStatus());
  }

  // Creates app window and set optional ARC application id.
  views::Widget* CreateArcWindow(const std::string& window_app_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(5, 5, 20, 20);
    params.context = GetContext();
    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));
    // Set ARC id before showing the window to be recognized in
    // ArcAppWindowLauncherController.
    exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
    widget->Show();
    widget->Activate();
    return widget;
  }

  arc::mojom::AppInfo CreateAppInfo(const std::string& name,
                                    const std::string& activity,
                                    const std::string& package_name) {
    arc::mojom::AppInfo appinfo;
    appinfo.name = name;
    appinfo.package_name = package_name;
    appinfo.activity = activity;
    return appinfo;
  }

  std::string AddArcAppAndShortcut(const arc::mojom::AppInfo& app_info) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    // Adding app to the prefs, and check that the app is accessible by id.
    prefs->AddAppAndShortcut(
        app_info.name, app_info.package_name, app_info.activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        false /* sticky */, true /* notifications_enabled */,
        true /* app_ready */, false /* suspended */, false /* shortcut */,
        true /* launchable */);
    const std::string app_id =
        ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
    EXPECT_TRUE(prefs->GetApp(app_id));
    app_service_test().FlushMojoCalls();
    return app_id;
  }

  void NotifyOnTaskCreated(const arc::mojom::AppInfo& appinfo,
                           int32_t task_id) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskCreated(task_id, appinfo.package_name, appinfo.activity,
                         appinfo.name, std::string());
  }

  // Creates a window with TYPE_APP shelf item type and the given app_id.
  views::Widget* CreateShelfAppWindow(const std::string& app_id) {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.context = GetContext();
    params.bounds = gfx::Rect(5, 5, 20, 20);
    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));

    aura::Window* window = widget->GetNativeWindow();
    const ash::ShelfID shelf_id(app_id);
    window->SetProperty(ash::kShelfIDKey,
                        new std::string(shelf_id.Serialize()));
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
    window->SetProperty(ash::kAppIDKey, new std::string(app_id));

    widget->Show();
    widget->Activate();
    return widget;
  }

  void NotifyOnTaskDestroyed(int32_t task_id) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskDestroyed(task_id);
  }

  // Add extension and allow AppService async callbacks to run.
  void AddExtension(const Extension* extension) {
    extension_service_->AddExtension(extension);
    app_service_test_.WaitForAppService();
  }

  // Remove extension and allow AppService async callbacks to run.
  void UnloadExtension(const std::string& extension_id,
                       UnloadedExtensionReason reason) {
    extension_service_->UnloadExtension(extension_id, reason);
    app_service_test_.WaitForAppService();
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension_chrome_;
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension5_;
  scoped_refptr<Extension> extension6_;
  scoped_refptr<Extension> extension7_;
  scoped_refptr<Extension> extension8_;
  scoped_refptr<Extension> extension_gmail_app_;
  scoped_refptr<Extension> extension_doc_app_;
  scoped_refptr<Extension> extension_youtube_app_;
  scoped_refptr<Extension> extension_platform_app_;
  scoped_refptr<Extension> arc_support_host_;

  ArcAppTest arc_test_;
  bool auto_start_arc_test_ = false;
  std::unique_ptr<ChromeLauncherController> launcher_controller_;
  std::unique_ptr<ash::ShelfModel> model_;

  // |item_delegate_manager_| owns |test_controller_|.
  ash::ShelfItemDelegate* test_controller_ = nullptr;

  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  extensions::ExtensionService* extension_service_ = nullptr;

  app_list::AppListSyncableService* app_list_syncable_service_ = nullptr;

 private:
  TestBrowserWindow* CreateTestBrowserWindowAura() {
    std::unique_ptr<aura::Window> window(
        new aura::Window(nullptr, aura::client::WINDOW_TYPE_NORMAL));
    window->set_id(0);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window.get(), GetContext(),
                                          gfx::Rect(200, 200));

    return new TestBrowserWindowAura(std::move(window));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  apps::AppServiceTest app_service_test_;
};

class ChromeLauncherControllerWithArcTest
    : public ChromeLauncherControllerTest {
 protected:
  ChromeLauncherControllerWithArcTest() {
    auto_start_arc_test_ = true;
  }

  ChromeLauncherControllerWithArcTest(
      const ChromeLauncherControllerWithArcTest&) = delete;
  ChromeLauncherControllerWithArcTest& operator=(
      const ChromeLauncherControllerWithArcTest&) = delete;
  ~ChromeLauncherControllerWithArcTest() override = default;

  void SetUp() override {
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    ChromeLauncherControllerTest::SetUp();
  }
};

// Tests for feature SplitSettingsSync. Exists as a separate class because the
// feature must be initialized before ChromeLauncherControllerTest::SetUp().
class ChromeLauncherControllerSplitSettingsSyncTest
    : public ChromeLauncherControllerTest {
 public:
  ChromeLauncherControllerSplitSettingsSyncTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kSplitSettingsSync);
  }
  ~ChromeLauncherControllerSplitSettingsSyncTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests for Lacros integration. Exists as a separate class because the feature
// must be initialized before ChromeLauncherControllerTest::SetUp().
class ChromeLauncherControllerLacrosTest : public ChromeLauncherControllerTest {
 public:
  ChromeLauncherControllerLacrosTest() {
    feature_list_.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  }
  ChromeLauncherControllerLacrosTest(
      const ChromeLauncherControllerLacrosTest&) = delete;
  ChromeLauncherControllerLacrosTest& operator=(
      const ChromeLauncherControllerLacrosTest&) = delete;
  ~ChromeLauncherControllerLacrosTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class ChromeLauncherControllerExtendedShelfTest
    : public ChromeLauncherControllerWithArcTest {
 protected:
  ChromeLauncherControllerExtendedShelfTest() = default;
  ChromeLauncherControllerExtendedShelfTest(
      const ChromeLauncherControllerExtendedShelfTest&) = delete;
  ChromeLauncherControllerExtendedShelfTest& operator=(
      const ChromeLauncherControllerExtendedShelfTest&) = delete;
  ~ChromeLauncherControllerExtendedShelfTest() override = default;

  void SetUp() override {
    ChromeLauncherControllerWithArcTest::SetUp();

    StartPrefSyncService(syncer::SyncDataList());

    extension_service_->AddExtension(extension_gmail_app_.get());
    extension_service_->AddExtension(extension_doc_app_.get());
    extension_service_->AddExtension(extension_youtube_app_.get());
    extension_service_->AddExtension(arc_support_host_.get());

    std::string error;
    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "for testing pinned apps");
    // AppService checks the app's type. So set the
    // manifest_keys::kLaunchWebURL, so that the extension can get the type
    // from manifest value, and then AppService can get the extension's type.
    manifest.SetString(extensions::manifest_keys::kLaunchWebURL, kLaunchURL);

    const std::vector<std::pair<std::string, std::string>> extra_extensions = {
        {extension_misc::kCalendarAppId, "Calendar"},
        {extension_misc::kGoogleSheetsAppId, "Sheets"},
        {extension_misc::kGoogleSlidesAppId, "Slides"},
        {extension_misc::kFilesManagerAppId, "Files"},
        {extension_misc::kCameraAppId, "Camera"},
        {extension_misc::kGooglePhotosAppId, "Photos"},
    };

    for (const auto& extension_id_name : extra_extensions) {
      manifest.SetString(extensions::manifest_keys::kName,
                         extension_id_name.second);
      scoped_refptr<Extension> extension = Extension::Create(
          base::FilePath(), Manifest::UNPACKED, manifest, Extension::NO_FLAGS,
          extension_id_name.first, &error);
      extension_service_->AddExtension(extension.get());
      extra_extensions_.emplace_back(extension);
    }
  }

 private:
  std::vector<scoped_refptr<Extension>> extra_extensions_;
};

// A V1 windowed application.
class V1App : public TestBrowserWindow {
 public:
  V1App(Profile* profile, const std::string& app_name) {
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        kCrxAppPrefix + app_name, true /* trusted_source */, gfx::Rect(),
        profile, true);
    params.window = this;
    browser_ = std::make_unique<Browser>(params);
    chrome::AddTabAt(browser_.get(), GURL(), 0, true);
  }
  V1App(const V1App&) = delete;
  V1App& operator=(const V1App&) = delete;
  ~V1App() override {
    // close all tabs. Note that we do not need to destroy the browser itself.
    browser_->tab_strip_model()->CloseAllTabs();
  }

  Browser* browser() { return browser_.get(); }

 private:
  // The associated browser with this app.
  std::unique_ptr<Browser> browser_;
};

// A V2 application window created with an |extension| and for a |profile|.
// Upon destruction it will properly close the application.
class V2App {
 public:
  V2App(Profile* profile,
        const extensions::Extension* extension,
        extensions::AppWindow::WindowType window_type =
            extensions::AppWindow::WINDOW_TYPE_DEFAULT)
      : creator_web_contents_(
            content::WebContentsTester::CreateTestWebContents(profile,
                                                              nullptr)) {
    window_ = new extensions::AppWindow(profile, new ChromeAppDelegate(true),
                                        extension);
    extensions::AppWindow::CreateParams params;
    params.window_type = window_type;
    // Note: normally, the creator RFH is the background page of the
    // app/extension
    // calling chrome.app.window.create. For unit testing purposes, just passing
    // in a random RenderFrameHost is Good Enough™.
    window_->Init(GURL(std::string()),
                  new extensions::AppWindowContentsImpl(window_),
                  creator_web_contents_->GetMainFrame(), params);
  }

  V2App(const V2App&) = delete;
  V2App& operator=(const V2App&) = delete;
  virtual ~V2App() {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        window_->web_contents());
    window_->GetBaseWindow()->Close();
    destroyed_watcher.Wait();
  }

  extensions::AppWindow* window() { return window_; }

 private:
  std::unique_ptr<content::WebContents> creator_web_contents_;

  // The app window which represents the application. Note that the window
  // deletes itself asynchronously after window_->GetBaseWindow()->Close() gets
  // called.
  extensions::AppWindow* window_;
};

// The testing framework to test multi profile scenarios.
class MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest
    : public ChromeLauncherControllerTest {
 protected:
  MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest() = default;
  MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest(
      const MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest&) =
      delete;
  MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest& operator=(
      const MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest&) =
      delete;
  ~MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest() override =
      default;

  // Overwrite the Setup function to enable multi profile and needed objects.
  void SetUp() override {
    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());

    // Initialize the rest.
    ChromeLauncherControllerTest::SetUp();

    // Initialize WallpaperControllerClient.
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClient>();
    wallpaper_controller_client_->InitForTesting(&test_wallpaper_controller_);

    // Ensure there are multiple profiles. User 0 is created during setup.
    CreateMultiUserProfile("user1");
    ASSERT_TRUE(SessionControllerClientImpl::IsMultiProfileAvailable());
  }

  void TearDown() override {
    ChromeLauncherControllerTest::TearDown();
    user_manager_enabler_.reset();
    wallpaper_controller_client_.reset();

    // A Task is leaked if we don't destroy everything, then run the message
    // loop.
    base::RunLoop().RunUntilIdle();
  }

  void MaybeStartWebAppProvider() override {
    // Deliberately do nothing; the provider is started in
    // CreateMultiUserProfile()
  }

  // Creates a profile for a given |user_name|. Note that this class will keep
  // the ownership of the created object.
  TestingProfile* CreateMultiUserProfile(const std::string& user_name) {
    const std::string email_string = user_name + "@example.com";
    const AccountId account_id(AccountId::FromUserEmail(email_string));
    // Add a user to the fake user manager.
    auto* user = GetFakeUserManager()->AddUser(account_id);
    ash_test_helper()->test_session_controller_client()->AddUserSession(
        user->GetDisplayEmail());

    GetFakeUserManager()->LoginUser(account_id);

    TestingProfile* profile =
        profile_manager()->CreateTestingProfile(account_id.GetUserEmail());
    EXPECT_TRUE(profile);

    if (base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions))
      web_app::TestWebAppProvider::Get(profile)->Start();

    // Remember the profile name so that we can destroy it upon destruction.
    created_profiles_[profile] = account_id.GetUserEmail();
    if (MultiUserWindowManagerHelper::GetInstance())
      MultiUserWindowManagerHelper::GetInstance()->AddUser(profile);
    if (launcher_controller_)
      launcher_controller_->AdditionalUserAddedToSession(profile);
    return profile;
  }

  // Switch to another user.
  void SwitchActiveUser(const AccountId& account_id) {
    GetFakeUserManager()->SwitchActiveUser(account_id);
    ash::MultiUserWindowManagerImpl::Get()->SetAnimationSpeedForTest(
        ash::MultiUserWindowManagerImpl::ANIMATION_SPEED_DISABLED);
    ash::MultiUserWindowManagerImpl::Get()->OnActiveUserSessionChanged(
        account_id);
  }

  // Creates a browser with a |profile| and load a tab with a |title| and |url|.
  std::unique_ptr<Browser> CreateBrowserAndTabWithProfile(
      Profile* profile,
      const std::string& title,
      const std::string& url) {
    std::unique_ptr<Browser> browser(
        CreateBrowserWithTestWindowForProfile(profile));
    chrome::NewTab(browser.get());

    browser->window()->Show();
    NavigateAndCommitActiveTabWithTitle(browser.get(), GURL(url),
                                        ASCIIToUTF16(title));
    return browser;
  }

  // Creates a running V1 application.
  // Note that with the use of the launcher_controller_helper as done below,
  // this is only usable with a single v1 application.
  V1App* CreateRunningV1App(Profile* profile,
                            const std::string& app_name,
                            const std::string& url) {
    V1App* v1_app = new V1App(profile, app_name);
    NavigateAndCommitActiveTabWithTitle(v1_app->browser(), GURL(url),
                                        base::string16());
    return v1_app;
  }

  // Override BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    return CreateMultiUserProfile("user0");
  }

 private:
  typedef std::map<Profile*, std::string> ProfileToNameMap;

  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  std::unique_ptr<WallpaperControllerClient> wallpaper_controller_client_;

  TestWallpaperController test_wallpaper_controller_;

  ProfileToNameMap created_profiles_;
};

class ChromeLauncherControllerMultiProfileWithArcTest
    : public MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest {
 protected:
  ChromeLauncherControllerMultiProfileWithArcTest() {
    auto_start_arc_test_ = true;
  }
  ChromeLauncherControllerMultiProfileWithArcTest(
      const ChromeLauncherControllerMultiProfileWithArcTest&) = delete;
  ChromeLauncherControllerMultiProfileWithArcTest& operator=(
      const ChromeLauncherControllerMultiProfileWithArcTest&) = delete;
  ~ChromeLauncherControllerMultiProfileWithArcTest() override = default;
};

TEST_P(ChromeLauncherControllerTest, DefaultApps) {
  InitLauncherController();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Pinning the non-default app. It should appear at the end. No default app
  // is currently installed.
  extension_service_->AddExtension(extension1_.get());
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  // Install default apps in reverse order, compared how they are declared.
  // However pin positions should be in the order as they declared. Note,
  // default apps appear on shelf between manually pinned App1.

  // Prefs are not yet synced. No default pin appears.
  AddExtension(extension_youtube_app_.get());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  StartPrefSyncService(syncer::SyncDataList());
  EXPECT_EQ("Chrome, Youtube, App1", GetPinnedAppStatus());

  AddExtension(extension_doc_app_.get());
  EXPECT_EQ("Chrome, Doc, Youtube, App1", GetPinnedAppStatus());
  AddExtension(extension_gmail_app_.get());
  EXPECT_EQ("Chrome, Gmail, Doc, Youtube, App1", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerSplitSettingsSyncTest, DefaultApps) {
  // Simulate a user who opted out of sync.
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile());
  sync_service->GetUserSettings()->SetOsSyncFeatureEnabled(false);

  InitLauncherController();
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Simulate the default app loader installing some apps. Don't start the
  // pref sync service, because this user opted out of sync.
  AddExtension(extension_youtube_app_.get());
  AddExtension(extension_doc_app_.get());
  AddExtension(extension_gmail_app_.get());

  // Default apps are pinned.
  EXPECT_EQ("Chrome, Gmail, Doc, Youtube", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerLacrosTest, LacrosPinnedByDefault) {
  // Checking to see if Lacros is allowed requires a user.
  auto user_manager = std::make_unique<chromeos::FakeChromeUserManager>();
  auto* fake_user_manager = user_manager.get();
  user_manager::ScopedUserManager scoped_user_manager(std::move(user_manager));
  AccountId account_id = AccountId::FromUserEmail("user@example.com");
  user_manager::User* user = fake_user_manager->AddUser(account_id);
  fake_user_manager->LoginUser(account_id);

  TestingProfile::Builder profile_builder;
  profile_builder.SetProfileName(account_id.GetUserEmail());
  std::unique_ptr<TestingProfile> testing_profile = profile_builder.Build();
  chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
      user, testing_profile.get());

  InitLauncherController();
  EXPECT_EQ("Chrome, Lacros", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerExtendedShelfTest, ExtendedShefDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableExtendedShelfLayout,
      {std::pair<std::string, std::string>("app_count", "0")});

  InitLauncherController();
  EXPECT_EQ("Chrome, Gmail, Doc, Youtube, Play Store", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerExtendedShelfTest, ExtendedShef7Apps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableExtendedShelfLayout,
      {std::pair<std::string, std::string>("app_count", "7")});

  InitLauncherController();
  EXPECT_EQ("Chrome, Gmail, Doc, Photos, Files, Youtube, Play Store",
            GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerExtendedShelfTest, ExtendedShef10Apps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableExtendedShelfLayout,
      {std::pair<std::string, std::string>("app_count", "10")});

  InitLauncherController();
  EXPECT_EQ(
      "Chrome, Gmail, Calendar, Doc, Sheets, Slides, Files, Camera, Photos, "
      "Play Store",
      GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerExtendedShelfTest, UpgradeFromDefault) {
  InitLauncherController();
  EXPECT_EQ("Chrome, Gmail, Doc, Youtube, Play Store", GetPinnedAppStatus());

  // Upgrade happens only in case default layout is active.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableExtendedShelfLayout,
      {std::pair<std::string, std::string>("app_count", "10")});

  // Trigger layout update, app_id does not matter.
  AddExtension(extension1_.get());

  EXPECT_EQ(
      "Chrome, Gmail, Calendar, Doc, Sheets, Slides, Files, Camera, Photos, "
      "Play Store",
      GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerExtendedShelfTest, NoDefaultAfterExperemental) {
  const std::string expectations =
      "Chrome, Gmail, Calendar, Doc, Sheets, "
      "Slides, Files, Camera, Photos, Play Store";
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kEnableExtendedShelfLayout,
        {std::pair<std::string, std::string>("app_count", "10")});

    InitLauncherController();
    EXPECT_EQ(expectations, GetPinnedAppStatus());

    // Trigger layout update, app_id does not matter. Experiment is still
    // forced.
    AddExtension(extension1_.get());

    // Youtube is included into default but not to expermenetal. Refreshing
    // should not affect layout.
    EXPECT_EQ(expectations, GetPinnedAppStatus());
  }

  // Re-update but experiment is off now. No change to layout.
  AddExtension(extension2_.get());
  EXPECT_EQ(expectations, GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerExtendedShelfTest, NoUpgradeFromNonDefault) {
  InitLauncherController();
  launcher_controller_->UnpinAppWithID(extension_misc::kYoutubeAppId);
  EXPECT_EQ("Chrome, Gmail, Doc, Play Store", GetPinnedAppStatus());

  // Upgrade does not happen due to default pin layout change.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kEnableExtendedShelfLayout,
      {std::pair<std::string, std::string>("app_count", "10")});

  // Trigger layout update, app_id does not matter.
  AddExtension(extension1_.get());

  EXPECT_EQ("Chrome, Gmail, Doc, Play Store", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcAppPinCrossPlatformWorkflow) {
  // Work on ARC disabled platform first.
  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[2]);

  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension_gmail_app_.get());
  app_service_test().WaitForAppService();

  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, arc_app_id1);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, arc_app_id2);
  InsertAddPinChange(&sync_list, 4, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);
  SetShelfChromeIconIndex(1);

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("App1, Chrome, App2, Gmail", GetPinnedAppStatus());

  // Persist pin state, we don't have active pin for ARC apps yet, but pin
  // model should have it.
  syncer::SyncDataList copy_sync_list =
      app_list_syncable_service_->GetAllSyncDataForTesting();

  ResetLauncherController();
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_list_syncable_service_->sync_items().size());

  // Move to ARC enabled platform, restart syncing with stored data.
  StartAppSyncService(copy_sync_list);
  RecreateLauncherController()->Init();

  // Pins must be automatically updated.
  SendListOfArcApps();
  app_service_test().WaitForAppService();
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));

  EXPECT_EQ("App1, Chrome, Fake App 0, App2, Fake App 1, Gmail",
            GetPinnedAppStatus());

  // Now move pins on ARC enabled platform.
  model_->Move(0, 3);
  model_->Move(2, 0);
  model_->Move(2, 4);
  model_->Move(3, 1);
  EXPECT_EQ("App2, Fake App 1, Chrome, App1, Fake App 0, Gmail",
            GetPinnedAppStatus());

  app_service_test().WaitForAppService();
  copy_sync_list = app_list_syncable_service_->GetAllSyncDataForTesting();

  ResetLauncherController();
  ResetPinModel();

  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_list_syncable_service_->sync_items().size());

  // Move back to ARC disabled platform.
  EnablePlayStore(false);
  StartAppSyncService(copy_sync_list);
  RecreateLauncherController()->Init();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("App2, Chrome, App1, Gmail", GetPinnedAppStatus());

  // Now move/remove pins on ARC disabled platform.
  model_->Move(3, 1);
  launcher_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("Gmail, Chrome, App1", GetPinnedAppStatus());
  EnablePlayStore(true);

  SendListOfArcApps();
  app_service_test().WaitForAppService();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("Fake App 1, Gmail, Chrome, App1, Fake App 0",
            GetPinnedAppStatus());
}

// Ensure correct merging of policy pinned apps and user pinned apps.
TEST_P(ChromeLauncherControllerTest, MergePolicyAndUserPrefPinnedApps) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension_gmail_app_.get());
  extension_service_->AddExtension(extension_doc_app_.get());
  extension_service_->AddExtension(extension5_.get());
  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  base::ListValue policy_value;
  // extension 2 4 are pinned by policy
  AppendPrefValue(&policy_value, extension2_->id());
  AppendPrefValue(&policy_value, extension_doc_app_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  // 2 is not pinned as it's not installed
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_doc_app_->id()));
  // install extension 2 and check
  AddExtension(extension2_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));

  // Check user can manually pin or unpin these apps
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension1_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension2_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension_gmail_app_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension_doc_app_->id(), profile()));

  // Check the order of shelf pinned apps
  EXPECT_EQ("App2, Doc, App1, Chrome, Gmail", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_P(ChromeLauncherControllerTest, RestoreDefaultAppsReverseOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Installing |extension_gmail_app_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  AddExtension(extension_gmail_app_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("Chrome, Gmail", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("Chrome, App2, Gmail", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_P(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  // Installing |extension_gmail_app_| should add it to the launcher - behind
  // the chrome icon, but in first location.
  AddExtension(extension_gmail_app_.get());
  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_P(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrderChromeMoved) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_EQ("App1, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension_gmail_app_| should add it to the launcher - behind
  // the chrome icon, but in first location.
  AddExtension(extension_gmail_app_.get());
  EXPECT_EQ("App1, Chrome, App2, Gmail", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing.
TEST_P(ChromeLauncherControllerTest, RestoreDefaultAppsResyncOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension2_->id());
  InsertAddPinChange(&sync_list0, 2, extension_gmail_app_->id());
  SendPinChanges(sync_list0, true);

  // The shelf layout has always one static item at the beginning (App List).
  AddExtension(extension2_.get());
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  AddExtension(extension_gmail_app_.get());
  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list1, 1, extension1_->id());
  InsertAddPinChange(&sync_list1, 2, extension2_->id());
  InsertAddPinChange(&sync_list1, 3, extension_misc::kChromeAppId);
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Gmail, App1, App2, Chrome", GetPinnedAppStatus());

  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension2_->id());
  InsertAddPinChange(&sync_list2, 1, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list2, 2, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list2, 3, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("App2, Gmail, Chrome, App1", GetPinnedAppStatus());

  // Check that the chrome icon can also be at the first possible location.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list3, 1, extension2_->id());
  InsertAddPinChange(&sync_list3, 2, extension1_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("Chrome, Gmail, App2, App1", GetPinnedAppStatus());

  // Check that unloading of extensions works as expected.
  UnloadExtension(extension1_->id(), UnloadedExtensionReason::UNINSTALL);
  EXPECT_EQ("Chrome, Gmail, App2", GetPinnedAppStatus());

  UnloadExtension(extension2_->id(), UnloadedExtensionReason::UNINSTALL);
  EXPECT_EQ("Chrome, Gmail", GetPinnedAppStatus());

  // Check that an update of an extension does not crash the system.
  UnloadExtension(extension_gmail_app_->id(), UnloadedExtensionReason::UPDATE);
  EXPECT_EQ("Chrome, Gmail", GetPinnedAppStatus());
}

// Test the V1 app interaction flow: run it, activate it, close it.
TEST_P(ChromeLauncherControllerTest, V1AppRunActivateClose) {
  InitLauncherController();
  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should create a new shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running again should have no effect.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);

  // Reporting that the app is closed should remove its shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed again should have no effect.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
}

// Test the V1 app interaction flow: pin it, run it, close it, unpin it.
TEST_P(ChromeLauncherControllerTest, V1AppPinRunCloseUnpin) {
  InitLauncherController();
  // The model should only contain the browser shortcut.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should create a new shelf item.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should remove its shelf item.
  launcher_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Test the V1 app interaction flow: run it, pin it, close it, unpin it.
TEST_P(ChromeLauncherControllerTest, V1AppRunPinCloseUnpin) {
  InitLauncherController();

  // The model should only contain the browser shortcut.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should create a new shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should just update the existing item.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should remove its shelf item.
  launcher_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Test the V1 app interaction flow: pin it, run it, unpin it, close it.
TEST_P(ChromeLauncherControllerTest, V1AppPinRunUnpinClose) {
  InitLauncherController();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should create a new shelf item.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should just update the existing item.
  launcher_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should remove its shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Ensure unpinned V1 app ordering is properly restored after user changes.
TEST_P(ChromeLauncherControllerTest, CheckRunningV1AppOrder) {
  InitLauncherController();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());

  // Add a few running applications.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  launcher_controller_->SetV1AppStatus(extension2_->id(), ash::STATUS_RUNNING);
  launcher_controller_->SetV1AppStatus(extension_gmail_app_->id(),
                                       ash::STATUS_RUNNING);
  EXPECT_EQ(4, model_->item_count());
  // Note that this not only checks the order of applications but also the
  // running type.
  EXPECT_EQ("Chrome, app1, app2, gmail", GetPinnedAppStatus());

  // Remember the current order of applications for the current user.
  const AccountId& current_account_id =
      multi_user_util::GetAccountIdFromProfile(profile());
  RememberUnpinnedRunningApplicationOrder();

  // Switch some items and check that restoring a user which was not yet
  // remembered changes nothing.
  model_->Move(1, 2);
  EXPECT_EQ("Chrome, app2, app1, gmail", GetPinnedAppStatus());
  const AccountId second_fake_account_id(
      AccountId::FromUserEmail("second-fake-user@fake.com"));
  RestoreUnpinnedRunningApplicationOrder(second_fake_account_id);
  EXPECT_EQ("Chrome, app2, app1, gmail", GetPinnedAppStatus());

  // Restoring the stored user should however do the right thing.
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome, app1, app2, gmail", GetPinnedAppStatus());

  // Switch again some items and even delete one - making sure that the missing
  // item gets properly handled.
  model_->Move(2, 3);
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ("Chrome, gmail, app2", GetPinnedAppStatus());
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome, app2, gmail", GetPinnedAppStatus());

  // Check that removing more items does not crash and changes nothing.
  launcher_controller_->SetV1AppStatus(extension2_->id(), ash::STATUS_CLOSED);
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome, gmail", GetPinnedAppStatus());
  launcher_controller_->SetV1AppStatus(extension_gmail_app_->id(),
                                       ash::STATUS_CLOSED);
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcDeferredLaunch) {
  InitLauncherController();

  const arc::mojom::AppInfo& app1 = arc_test_.fake_apps()[0];
  const arc::mojom::AppInfo& app2 = arc_test_.fake_apps()[1];
  const arc::mojom::AppInfo& gmail = arc_test_.fake_apps()[2];
  const arc::mojom::ShortcutInfo& shortcut = arc_test_.fake_shortcuts()[0];
  const std::string arc_app_id1 = ArcAppTest::GetAppId(app1);
  const std::string arc_app_id2 = ArcAppTest::GetAppId(app2);
  const std::string arc_app_id3 = ArcAppTest::GetAppId(gmail);
  const std::string arc_shortcut_id = ArcAppTest::GetAppId(shortcut);

  SendListOfArcApps();
  SendListOfArcShortcuts();

  arc_test_.StopArcInstance();

  const ash::ShelfID shelf_id_app_1(arc_app_id1);
  const ash::ShelfID shelf_id_app_2(arc_app_id2);
  const ash::ShelfID shelf_id_app_3(arc_app_id3);
  const ash::ShelfID shelf_id_shortcut(arc_shortcut_id);
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_1));
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_2));
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_3));
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_shortcut));

  arc::LaunchApp(profile(), arc_app_id1, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  arc::LaunchApp(profile(), arc_app_id1, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  arc::LaunchApp(profile(), arc_app_id2, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  arc::LaunchApp(profile(), arc_app_id3, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  arc::LaunchApp(profile(), arc_shortcut_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);

  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_app_1));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_app_2));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_app_3));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_shortcut));

  // We activated arc_app_id1 twice but expect one close for item controller
  // stops launching request.
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(shelf_id_app_1);
  ASSERT_NE(nullptr, item_delegate);
  item_delegate->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_1));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_app_2));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_app_3));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id_shortcut));

  arc_test_.RestartArcInstance();
  SendListOfArcApps();

  base::RunLoop().RunUntilIdle();

  // Now spinner controllers should go away together with shelf items and ARC
  // app instance should receive request for launching apps and shortcuts.
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_1));
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_2));
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_app_3));
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id_shortcut));

  ASSERT_EQ(2U, arc_test_.app_instance()->launch_requests().size());
  ASSERT_EQ(1U, arc_test_.app_instance()->launch_intents().size());

  const arc::FakeAppInstance::Request* request1 =
      arc_test_.app_instance()->launch_requests()[0].get();
  const arc::FakeAppInstance::Request* request2 =
      arc_test_.app_instance()->launch_requests()[1].get();

  EXPECT_TRUE((request1->IsForApp(app2) && request2->IsForApp(gmail)) ||
              (request1->IsForApp(gmail) && request2->IsForApp(app2)));
  EXPECT_EQ(arc_test_.app_instance()->launch_intents()[0].c_str(),
            shortcut.intent_uri);
}

// Launch is canceled in case app becomes suspended.
TEST_P(ChromeLauncherControllerWithArcTest, ArcDeferredLaunchForSuspendedApp) {
  InitLauncherController();

  arc::mojom::AppInfo app = arc_test_.fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  // Register app first.
  arc_test_.app_instance()->SendRefreshAppList({app});
  arc_test_.StopArcInstance();

  // Restart ARC
  arc_test_.RestartArcInstance();

  // Deferred controller should be allocated on start.
  const ash::ShelfID shelf_id(app_id);
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));

  // Send app with suspended state.
  app.suspended = true;
  arc_test_.app_instance()->SendRefreshAppList({app});

  // Controler automatically closed.
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  // And no launch request issued.
  EXPECT_TRUE(arc_test_.app_instance()->launch_requests().empty());
}

// Ensure the spinner controller does not override the active app controller
// (crbug.com/701152).
TEST_P(ChromeLauncherControllerWithArcTest, ArcDeferredLaunchForActiveApp) {
  InitLauncherController();
  SendListOfArcApps();
  arc_test_.StopArcInstance();

  const arc::mojom::AppInfo& app = arc_test_.fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  launcher_controller_->PinAppWithID(app_id);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  const ash::ShelfID shelf_id(app_id);
  const ash::ShelfItem* item = launcher_controller_->GetItem(shelf_id);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(ash::STATUS_CLOSED, item->status);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item->type);

  // Play Store app is ARC app that might be represented by native Chrome
  // platform app.
  model_->SetShelfItemDelegate(
      shelf_id,
      std::make_unique<AppServiceAppWindowLauncherItemController>(
          shelf_id, launcher_controller_->app_service_app_window_controller()));
  launcher_controller_->SetItemStatus(shelf_id, ash::STATUS_RUNNING);

  // This launch request should be ignored in case of active app.
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Closing the app should leave a pinned but closed shelf item shortcut.
  launcher_controller_->CloseLauncherItem(shelf_id);
  item = launcher_controller_->GetItem(shelf_id);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(ash::STATUS_CLOSED, item->status);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item->type);

  // Now launch request should not be ignored.
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
}

// TODO(crbug.com/915840): this test is flakey and/or often crashes.
TEST_P(ChromeLauncherControllerMultiProfileWithArcTest, DISABLED_ArcMultiUser) {
  SendListOfArcApps();

  InitLauncherController();

  SetLauncherControllerHelper(new TestLauncherControllerHelper);

  // App1 exists all the time.
  // App2 is created when primary user is active and destroyed when secondary
  // user is active.
  // Gmail created when secondary user is active.

  const std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[2]);

  std::string window_app_id1("org.chromium.arc.1");
  views::Widget* arc_window1 = CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));

  std::string window_app_id2("org.chromium.arc.2");
  views::Widget* arc_window2 = CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[1],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));

  launcher_controller_->SetProfileForTest(profile2);
  SwitchActiveUser(account_id2);

  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));

  std::string window_app_id3("org.chromium.arc.3");
  views::Widget* arc_window3 = CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, arc_test_.fake_apps()[2],
                                            std::string());
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id3)));

  arc_window2->CloseNow();
  arc_test_.app_instance()->SendTaskDestroyed(2);

  launcher_controller_->SetProfileForTest(profile());
  SwitchActiveUser(account_id);

  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id3)));

  // Close active window to let test passes.
  arc_window1->CloseNow();
  arc_window3->CloseNow();
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcRunningApp) {
  InitLauncherController();

  const std::string arc_app_id = ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  SendListOfArcApps();
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));

  // Normal flow, create/destroy tasks.
  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  std::string window_app_id3("org.chromium.arc.3");
  CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  arc_test_.app_instance()->SendTaskDestroyed(1);
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  arc_test_.app_instance()->SendTaskDestroyed(2);
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));

  // Stopping bridge removes apps.
  CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  arc_test_.StopArcInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
}

// Test race creation/deletion of ARC app.
// TODO(khmel): Remove after moving everything to wayland protocol.
TEST_P(ChromeLauncherControllerWithArcTest, ArcRaceCreateClose) {
  InitLauncherController();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);
  SendListOfArcApps();

  // ARC window created before and closed after mojom notification.
  std::string window_app_id1("org.chromium.arc.1");
  views::Widget* arc_window = CreateArcWindow(window_app_id1);
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  ASSERT_TRUE(arc_window);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  arc_test_.app_instance()->SendTaskDestroyed(1);
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  arc_window->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id1)));

  // ARC window created after and closed before mojom notification.
  std::string window_app_id2("org.chromium.arc.2");
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[1],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  arc_window = CreateArcWindow(window_app_id2);
  ASSERT_TRUE(arc_window);
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  arc_window->Close();
  base::RunLoop().RunUntilIdle();
  // Closing window does not close shelf item. It is closed on task destroy.
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  arc_test_.app_instance()->SendTaskDestroyed(2);
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id2)));
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcWindowRecreation) {
  InitLauncherController();

  const std::string arc_app_id = ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  SendListOfArcApps();

  std::string window_app_id("org.chromium.arc.1");
  views::Widget* arc_window = CreateArcWindow(window_app_id);
  ASSERT_TRUE(arc_window);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));

  for (int i = 0; i < 3; ++i) {
    arc_window->Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));

    arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  }
}

// Verifies edge cases when Extension app launcher may be overwritten by ARC app
// launcher controller and vice versa. This should not happen in normal cases
// but in case of ARC boot failure this may lead to such situation. This test
// verifies that dynamic change of app launcher controllers is safe.
// See more crbug.com/770005.
TEST_P(ChromeLauncherControllerWithArcTest, OverrideAppItemController) {
  extension_service_->AddExtension(arc_support_host_.get());

  InitLauncherController();

  SendListOfArcApps();
  arc::mojom::AppInfo app_info = CreateAppInfo(
      "Play Store", arc::kPlayStoreActivity, arc::kPlayStorePackage);
  EXPECT_EQ(arc::kPlayStoreAppId, AddArcAppAndShortcut(app_info));
  app_service_test().WaitForAppService();

  std::string window_app_id("org.chromium.arc.1");
  const ash::ShelfID play_store_shelf_id(arc::kPlayStoreAppId);

  launcher_controller_->UnpinAppWithID(arc::kPlayStoreAppId);
  EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));

  // Try 4 different scenarios with different creation and destroying orders.

  // Scenario 1: Create OptIn, Play Store. Destroy OptIn, Play Store.
  {
    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, app_info, std::string());
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));
  }

  // Scenario 2: Create OptIn, Play Store. Destroy Play Store, OptIn.
  {
    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, app_info, std::string());
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));
  }

  // Scenario 3: Create Play Store, OptIn. Destroy OptIn, Play Store.
  {
    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, app_info, std::string());
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));
  }

  // Scenario 4: Create Play Store, OptIn. Destroy Play Store, OptIn.
  {
    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, app_info, std::string());
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_TRUE(launcher_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_FALSE(launcher_controller_->GetItem(play_store_shelf_id));
  }
}

// Validate that ARC app is pinned correctly and pin is removed automatically
// once app is uninstalled.
TEST_P(ChromeLauncherControllerWithArcTest, ArcAppPin) {
  InitLauncherController();

  const std::string arc_app_id = ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);

  SendListOfArcApps();
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());
  launcher_controller_->PinAppWithID(arc_app_id);
  launcher_controller_->PinAppWithID(extension2_->id());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));

  EXPECT_EQ("Chrome, App1, Fake App 0, App2", GetPinnedAppStatus());

  UninstallArcApps();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  SendListOfArcApps();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  // Opt-Out/Opt-In remove item from the shelf.
  launcher_controller_->PinAppWithID(arc_app_id);
  EXPECT_EQ("Chrome, App1, App2, Fake App 0", GetPinnedAppStatus());
  EnablePlayStore(false);
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  EnablePlayStore(true);
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  SendListOfArcApps();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Chrome, App1, App2, Fake App 0", GetPinnedAppStatus());
}

// Validates that ARC app pins persist across OptOut/OptIn.
TEST_P(ChromeLauncherControllerWithArcTest, ArcAppPinOptOutOptIn) {
  InitLauncherController();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);

  SendListOfArcApps();
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  launcher_controller_->PinAppWithID(extension1_->id());
  launcher_controller_->PinAppWithID(arc_app_id2);
  launcher_controller_->PinAppWithID(extension2_->id());
  launcher_controller_->PinAppWithID(arc_app_id1);

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_EQ("Chrome, App1, Fake App 1, App2, Fake App 0", GetPinnedAppStatus());

  EnablePlayStore(false);

  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));

  EnablePlayStore(true);
  SendListOfArcApps();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));

  EXPECT_EQ("Chrome, App1, Fake App 1, App2, Fake App 0", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerWithArcTest, DISABLED_ArcCustomAppIcon) {
  InitLauncherController();

  // Wait until other apps are updated to avoid race condition while accessing
  // last updated item.
  base::RunLoop().RunUntilIdle();

  // Register fake ARC apps.
  SendListOfArcApps();
  // Use first fake ARC app for testing.
  const arc::mojom::AppInfo& app = arc_test_.fake_apps()[0];
  const std::string arc_app_id = ArcAppTest::GetAppId(app);
  const ash::ShelfID arc_shelf_id(arc_app_id);

  // Generate icon for the testing app and use compressed png content as test
  // input. Take shortcut to separate from default app icon.
  auto icon = arc_test_.app_instance()->GenerateIconResponse(
      extension_misc::EXTENSION_ICON_SMALL, false /* app_icon */);
  ASSERT_TRUE(icon);
  ASSERT_TRUE(icon->icon_png_data.has_value());
  EXPECT_FALSE(icon->icon_png_data->empty());
  std::string png_data(icon->icon_png_data->begin(),
                       icon->icon_png_data->end());
  // Some input that represents invalid png content.
  std::string invalid_png_data("aaaaaa");

  EXPECT_FALSE(launcher_controller_->GetItem(arc_shelf_id));
  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  views::Widget* window1 = CreateArcWindow(window_app_id1);
  ASSERT_TRUE(window1 && window1->GetNativeWindow());
  arc_test_.app_instance()->SendTaskCreated(1, app, std::string());

  views::Widget* window2 = CreateArcWindow(window_app_id2);
  ASSERT_TRUE(window2 && window2->GetNativeWindow());
  arc_test_.app_instance()->SendTaskCreated(2, app, std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(arc_shelf_id));
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(arc_shelf_id);
  ASSERT_TRUE(item_delegate);
  base::RunLoop().RunUntilIdle();

  auto get_icon = [=]() {
    return *launcher_controller_->GetItem(arc_shelf_id)->image.bitmap();
  };
  const SkBitmap default_icon = get_icon();

  // No custom icon set. Acitivating windows should not change icon.
  window1->Activate();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon, get_icon()));
  window2->Activate();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon, get_icon()));

  // Set custom icon on active item. Icon should change to custom.
  arc_test_.app_instance()->SendTaskDescription(2, std::string(), png_data);
  const SkBitmap custom_icon = get_icon();
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(default_icon, custom_icon));

  // Switch back to the item without custom icon. Icon should be changed to
  // default.
  window1->Activate();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon, get_icon()));

  // Test that setting an invalid icon should not change custom icon.
  arc_test_.app_instance()->SendTaskDescription(1, std::string(), png_data);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon, get_icon()));
  arc_test_.app_instance()->SendTaskDescription(1, std::string(),
                                                invalid_png_data);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon, get_icon()));

  // Check window removing with active custom icon. Reseting custom icon of
  // inactive window doesn't reset shelf icon.
  arc_test_.app_instance()->SendTaskDescription(2, std::string(),
                                                std::string());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon, get_icon()));
  // Set custom icon back to validate closing active window later.
  arc_test_.app_instance()->SendTaskDescription(2, std::string(), png_data);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon, get_icon()));

  // Reseting custom icon of active window resets shelf icon.
  arc_test_.app_instance()->SendTaskDescription(1, std::string(),
                                                std::string());
  // Wait for default icon load.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon, get_icon()));
  window1->CloseNow();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon, get_icon()));
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcWindowPackageName) {
  InitLauncherController();
  SendListOfArcApps();
  app_service_test().WaitForAppService();

  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  std::string window_app_id3("org.chromium.arc.3");
  views::Widget* arc_window1 = CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_apps()[0],
                                            std::string());
  const std::string* package_name1 =
      arc_window1->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey);
  ASSERT_TRUE(package_name1);
  EXPECT_EQ(*package_name1, arc_test_.fake_apps()[0].package_name);

  views::Widget* arc_window2 = CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, arc_test_.fake_apps()[1],
                                            std::string());
  const std::string* package_name2 =
      arc_window2->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey);
  ASSERT_TRUE(package_name2);
  EXPECT_EQ(*package_name2, arc_test_.fake_apps()[1].package_name);

  // Create another window with the same package name.
  views::Widget* arc_window3 = CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, arc_test_.fake_apps()[1],
                                            std::string());
  const std::string* package_name3 =
      arc_window3->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey);
  ASSERT_TRUE(package_name3);
  EXPECT_EQ(*package_name3, arc_test_.fake_apps()[1].package_name);

  arc_window1->CloseNow();
  arc_window2->CloseNow();
  arc_window3->CloseNow();
}

// Check that with multi profile V1 apps are properly added / removed from the
// shelf.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitch) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  EXPECT_EQ(1, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, kGmailUrl));
    EXPECT_EQ(2, model_->item_count());

    // After switching to a second user the item should be gone.
    std::string user2 = "user2";
    TestingProfile* profile2 = CreateMultiUserProfile(user2);
    const AccountId account_id2(
        multi_user_util::GetAccountIdFromProfile(profile2));
    const AccountId account_id(
        multi_user_util::GetAccountIdFromProfile(profile()));
    SwitchActiveUser(account_id2);
    EXPECT_EQ(1, model_->item_count());

    // After switching back the item should be back.
    SwitchActiveUser(account_id);
    EXPECT_EQ(2, model_->item_count());
    // Note we destroy now the gmail app with the closure end.
  }
  EXPECT_EQ(1, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitchEdgecases) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  // First test: Create an app when the user is not active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile2, extension_misc::kGmailAppId, kGmailUrl));
    EXPECT_EQ(1, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(2, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(account_id);
    EXPECT_EQ(1, model_->item_count());
    // Note: the closure ends and the browser will go away.
  }
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUser(account_id2);
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUser(account_id);
  EXPECT_EQ(1, model_->item_count());
}

// Check edge case where a visiting V1 app gets closed (crbug.com/321374).
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1CloseOnVisitingDesktop) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  // First create an app when the user is active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, kGmailLaunchURL));
    EXPECT_EQ(2, model_->item_count());
    SwitchActiveUser(account_id2);
    EXPECT_EQ(1, model_->item_count());
  }
  // After the app was destroyed, switch back. (which caused already a crash).
  SwitchActiveUser(account_id);

  // Create the same app again - which was also causing the crash.
  EXPECT_EQ(1, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, kGmailLaunchURL));
    EXPECT_EQ(2, model_->item_count());
  }
  SwitchActiveUser(account_id2);
  EXPECT_EQ(1, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitchEdgecases2) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  // First test: Create an app when the user is not active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUser(account_id2);
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, kGmailUrl));
    EXPECT_EQ(1, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(account_id);
    EXPECT_EQ(2, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(1, model_->item_count());
    v1_app.reset();
  }
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUser(account_id);
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUser(account_id2);
  EXPECT_EQ(1, model_->item_count());
}

// Check that activating an item which is on another user's desktop, will bring
// it back.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       TestLauncherActivationPullsBackWindow) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  ash::MultiUserWindowManager* window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();

  // Create a second test profile. The first is the one in profile() created in
  // BrowserWithTestWindowTest::SetUp().
  // No need to add the profiles to the MultiUserWindowManagerHelper here.
  // CreateMultiUserProfile() already does that.
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId current_user =
      multi_user_util::GetAccountIdFromProfile(profile());

  // Create a browser window with a native window for the current user.
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForProfile(profile()));
  BrowserWindow* browser_window = browser->window();
  aura::Window* window = browser_window->GetNativeWindow();
  window_manager->SetWindowOwner(window, current_user);

  // Check that an activation of the window on its owner's desktop does not
  // change the visibility to another user.
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(IsWindowOnDesktopOfUser(window, current_user));

  // Transfer the window to another user's desktop and check that activating it
  // does pull it back to that user.
  window_manager->ShowWindowForUser(
      window, multi_user_util::GetAccountIdFromProfile(profile2));
  EXPECT_FALSE(IsWindowOnDesktopOfUser(window, current_user));
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(IsWindowOnDesktopOfUser(window, current_user));
}

// Check that a running windowed V1 application will be properly pinned and
// unpinned when the order gets changed through a profile / policy change.
TEST_P(ChromeLauncherControllerTest,
       RestoreDefaultAndRunningV1AppsResyncOrder) {
  InitLauncherController();

  StartPrefSyncService(syncer::SyncDataList());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  // The shelf layout has always one static item at the beginning (App List).
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());
  AddExtension(extension2_.get());
  // No new app icon will be generated.
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  // Set the app status as running, which will add an unpinned item.
  launcher_controller_->SetV1AppStatus(extension2_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ("Chrome, App1, app2", GetPinnedAppStatus());
  AddExtension(extension_gmail_app_.get());
  EXPECT_EQ("Chrome, App1, Gmail, app2", GetPinnedAppStatus());

  // Now request to pin all items, which will pin the running unpinned items.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list1, 1, extension2_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Chrome, Gmail, App2, App1", GetPinnedAppStatus());

  // Removing the requirement for app 2 to be pinned should convert it back to
  // running but not pinned. It should move towards the end of the shelf, after
  // the pinned items, as determined by the |ShelfModel|'s weight system.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("Chrome, Gmail, App1, app2", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  syncer::SyncChangeList sync_list3;
  InsertRemovePinChange(&sync_list3, extension1_->id());
  SendPinChanges(sync_list3, false /* reset_pin_model */);
  EXPECT_EQ("Chrome, Gmail, app2", GetPinnedAppStatus());
}

// Check that a running unpinned V2 application will be properly pinned and
// unpinned when the order gets changed through a profile / policy change.
TEST_P(ChromeLauncherControllerTest,
       RestoreDefaultAndRunningV2AppsResyncOrder) {
  InitLauncherController();
  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension_gmail_app_->id());
  SendPinChanges(sync_list0, true);
  // The shelf layout has always one static item at the beginning (app List).
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());
  AddExtension(extension_platform_app_.get());
  // No new app icon will be generated.
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());
  // Add an unpinned but running V2 app.
  CreateRunningV2App(extension_platform_app_->id());
  EXPECT_EQ("Chrome, App1, *platform_app", GetPinnedAppStatus());
  AddExtension(extension_gmail_app_.get());
  EXPECT_EQ("Chrome, App1, Gmail, *platform_app", GetPinnedAppStatus());

  // Now request to pin all items, which should pin the running unpinned item.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list1, 1, extension_platform_app_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Chrome, Gmail, *Platform_App, App1", GetPinnedAppStatus());

  // Removing the requirement for app 2 to be pinned should convert it back to
  // running but not pinned. It should move towards the end of the shelf, after
  // the pinned items, as determined by the |ShelfModel|'s weight system.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension_gmail_app_->id());
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("Chrome, Gmail, App1, *platform_app", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, extension_gmail_app_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("Chrome, Gmail, *platform_app", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned. Check that when
// switching between the two users, the state gets properly set.
TEST_P(ChromeLauncherControllerTest, UserSwitchIconRestore) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;

  SetUpMultiUserScenario(&user_a, &user_b);

  // Show user 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("App1, App2, Gmail, *Platform_App, Doc, App5, Chrome",
            GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("App6, App7, App8, Chrome", GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("App1, App2, Gmail, *Platform_App, Doc, App5, Chrome",
            GetPinnedAppStatus());

  // Switch back to 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("App6, App7, App8, Chrome", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. Check that when switching between the two users, the
// state gets properly set.
TEST_P(ChromeLauncherControllerTest, UserSwitchIconRestoreWithRunningV2App) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;

  SetUpMultiUserScenario(&user_a, &user_b);

  // Run the platform (V2) app.
  CreateRunningV2App(extension_platform_app_->id());

  // Show user 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("App1, App2, Gmail, *Platform_App, Doc, App5, Chrome",
            GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("App6, App7, App8, Chrome, *platform_app", GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ("App1, App2, Gmail, *Platform_App, Doc, App5, Chrome",
            GetPinnedAppStatus());

  // Switch back to 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("App6, App7, App8, Chrome, *platform_app", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. The chrome icon is not the last item in the list.
// Check that when switching between the two users, the state gets properly set.
// There was once a bug associated with this.
TEST_P(ChromeLauncherControllerTest,
       UserSwitchIconRestoreWithRunningV2AppChromeInMiddle) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;
  SetUpMultiUserScenario(&user_a, &user_b);

  // Run the platform (V2) app.
  CreateRunningV2App(extension_platform_app_->id());

  // Show user 1.
  SendPinChanges(user_a, true);
  SetShelfChromeIconIndex(5);
  EXPECT_EQ("App1, App2, Gmail, *Platform_App, Doc, Chrome, App5",
            GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  SetShelfChromeIconIndex(3);
  EXPECT_EQ("App6, App7, App8, Chrome, *platform_app", GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  SetShelfChromeIconIndex(5);
  EXPECT_EQ("App1, App2, Gmail, *Platform_App, Doc, Chrome, App5",
            GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerTest, Policy) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension_gmail_app_.get());

  // Pin policy should be initilized before controller start.
  base::ListValue policy_value;
  AppendPrefValue(&policy_value, extension1_->id());
  AppendPrefValue(&policy_value, extension2_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  InitLauncherController();

  // Only |extension2_| should get pinned. |extension1_| is specified but not
  // installed, and |extension_gmail_app_| is part of the default set, but that
  // shouldn't take effect when the policy override is in place.
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher. Note, App1 goes
  // before App2 that is aligned with the pin order in policy.
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  // Removing |extension1_| from the policy should not be reflected in the
  // launcher and pin will exist.
  policy_value.Remove(0, nullptr);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension_gmail_app_.get());
  extension_service_->AddExtension(extension_doc_app_.get());

  InitLauncherController();
  StartPrefSyncService(syncer::SyncDataList());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_doc_app_->id()));

  UnloadExtension(extension_gmail_app_->id(),
                  UnloadedExtensionReason::UNINSTALL);

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_doc_app_->id()));
}

TEST_P(ChromeLauncherControllerTest, SyncUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension_gmail_app_.get());
  extension_service_->AddExtension(extension_doc_app_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 10, extension_misc::kChromeAppId);
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_pinned_apps;
  std::vector<std::string> actual_pinned_apps;
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Unavailable extensions don't create launcher items.
  sync_list.clear();
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 3, extension_doc_app_->id());
  SendPinChanges(sync_list, false);

  expected_pinned_apps.push_back(extension2_->id());
  expected_pinned_apps.push_back(extension_doc_app_->id());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertAddPinChange(&sync_list, 2, extension_gmail_app_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.insert(expected_pinned_apps.begin() + 1,
                              extension_gmail_app_->id());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertUpdatePinChange(&sync_list, 0, extension_doc_app_->id());
  InsertUpdatePinChange(&sync_list, 1, extension_gmail_app_->id());
  InsertUpdatePinChange(&sync_list, 2, extension2_->id());
  SendPinChanges(sync_list, false);
  std::reverse(expected_pinned_apps.begin(), expected_pinned_apps.end());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Sending legacy sync change without pin info should not affect pin model.
  sync_list.clear();
  InsertLegacyPinChange(&sync_list, extension_doc_app_->id());
  SendPinChanges(sync_list, false);
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, extension_doc_app_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.erase(expected_pinned_apps.begin());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, extension_gmail_app_->id());
  InsertRemovePinChange(&sync_list, extension2_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.clear();
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);
}

TEST_P(ChromeLauncherControllerTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension_gmail_app_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_pinned_apps;
  expected_pinned_apps.push_back(extension1_->id());
  expected_pinned_apps.push_back(extension_gmail_app_->id());
  std::vector<std::string> actual_pinned_apps;

  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Install |extension2| and verify it shows up between the other two.
  AddExtension(extension2_.get());
  expected_pinned_apps.insert(expected_pinned_apps.begin() + 1,
                              extension2_->id());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);
}

// Ensure |controller| creates the expected menu items for the given shelf item.
void CheckAppMenu(ChromeLauncherController* controller,
                  const ash::ShelfItem& item,
                  size_t expected_item_count,
                  base::string16 expected_item_titles[]) {
  auto items = controller->GetAppMenuItemsForTesting(item);
  ASSERT_EQ(expected_item_count, items.size());
  for (size_t i = 0; i < expected_item_count; i++)
    EXPECT_EQ(expected_item_titles[i], items[i].title);
}

// Check that browsers get reflected correctly in the launcher menu.
TEST_P(ChromeLauncherControllerTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  chrome::NewTab(browser());

  InitLauncherController();

  // Check that the browser list is empty at this time.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(extension_misc::kChromeAppId);
  CheckAppMenu(launcher_controller_.get(), item_browser, 0, nullptr);

  // Now make the created browser() visible by showing its browser window.
  browser()->window()->Show();
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  base::string16 one_menu_item[] = {title1};

  CheckAppMenu(launcher_controller_.get(), item_browser, 1, one_menu_item);

  // Create one more browser/window and check that one more was added.
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForProfile(profile()));
  chrome::NewTab(browser2.get());
  browser2->window()->Show();
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser2.get(), GURL("http://test2"),
                                      title2);

  // Check that the list contains now two entries - make furthermore sure that
  // the active item is the first entry.
  base::string16 two_menu_items[] = {title1, title2};
  CheckAppMenu(launcher_controller_.get(), item_browser, 2, two_menu_items);

  // Apparently we have to close all tabs we have.
  chrome::CloseTab(browser2.get());
}

// Check the multi profile case where only user related browsers should show up.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       BrowserMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(extension_misc::kChromeAppId);

  // Check that the menu is empty.
  chrome::NewTab(browser());
  CheckAppMenu(launcher_controller_.get(), item_browser, 0, nullptr);

  // Show the created |browser()| by showing its window.
  browser()->window()->Show();
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  base::string16 one_menu_item1[] = {title1};
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, one_menu_item1);

  // Create a browser for another user and check that it is not included in the
  // users running browser list.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  std::unique_ptr<Browser> browser2(
      CreateBrowserAndTabWithProfile(profile2, user2, "http://test2"));
  base::string16 one_menu_item2[] = {ASCIIToUTF16(user2)};
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, one_menu_item1);

  // Switch to the other user and make sure that only that browser window gets
  // shown.
  SwitchActiveUser(account_id2);
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, one_menu_item2);

  // Transferred browsers of other users should not show up in the list.
  MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, one_menu_item2);

  chrome::CloseTab(browser2.get());
}

// Check that V1 apps are correctly reflected in the launcher menu using the
// refocus logic.
// Note that the extension matching logic is tested by the extension system
// and does not need a separate test here.
TEST_P(ChromeLauncherControllerTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  InitLauncherControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));

  // Installing |extension_gmail_app_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension_gmail_app_->id());
  AddExtension(extension_gmail_app_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(kGmailUrl));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(extension_misc::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  CheckAppMenu(launcher_controller_.get(), item_gmail, 0, nullptr);

  // Set the gmail URL to a new tab.
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);

  base::string16 one_menu_item[] = {title1};
  CheckAppMenu(launcher_controller_.get(), item_gmail, 1, one_menu_item);

  // Create one empty tab.
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("https://bla"), title2);

  // and another one with another gmail instance.
  chrome::NewTab(browser());
  base::string16 title3 = ASCIIToUTF16("Test3");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title3);
  base::string16 two_menu_items[] = {title1, title3};
  CheckAppMenu(launcher_controller_.get(), item_gmail, 2, two_menu_items);

  // Even though the item is in the V1 app list, it should also be in the
  // browser list.
  base::string16 browser_menu_item[] = {title3};
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, browser_menu_item);

  // Test that closing of (all) the item(s) does work (and all menus get
  // updated properly).
  launcher_controller_->Close(item_gmail.id);

  CheckAppMenu(launcher_controller_.get(), item_gmail, 0, nullptr);
  base::string16 browser_menu_item2[] = {title2};
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, browser_menu_item2);
}

// Check the multi profile case where only user related apps should show up.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  StartPrefSyncService(syncer::SyncDataList());
  chrome::NewTab(browser());

  // Installing |extension_gmail_app_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension_gmail_app_->id());
  AddExtension(extension_gmail_app_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(kGmailUrl));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(extension_misc::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  CheckAppMenu(launcher_controller_.get(), item_gmail, 0, nullptr);

  // Set the gmail URL to a new tab.
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);

  base::string16 one_menu_item[] = {title1};
  CheckAppMenu(launcher_controller_.get(), item_gmail, 1, one_menu_item);

  // Create a second profile and switch to that user.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUser(account_id2);

  // No item should have content yet.
  CheckAppMenu(launcher_controller_.get(), item_browser, 0, nullptr);
  CheckAppMenu(launcher_controller_.get(), item_gmail, 0, nullptr);

  // Transfer the browser of the first user - it should still not show up.
  MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);

  CheckAppMenu(launcher_controller_.get(), item_browser, 0, nullptr);
  CheckAppMenu(launcher_controller_.get(), item_gmail, 0, nullptr);
}

// Check that V2 applications are creating items properly in the launcher when
// instantiated by the current user.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHandlingTwoUsers) {
  InitLauncherController();
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  // Check that there is a browser.
  EXPECT_EQ(1, model_->item_count());

  // Add a v2 app.
  AddExtension(extension1_.get());
  V2App v2_app(profile(), extension1_.get());
  EXPECT_EQ(2, model_->item_count());

  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  // After switching users the item should go away.
  SwitchActiveUser(account_id2);
  EXPECT_EQ(1, model_->item_count());

  // And it should come back when switching back.
  SwitchActiveUser(account_id);
  EXPECT_EQ(2, model_->item_count());
}

// Check that V2 applications are creating items properly in edge cases:
// a background user creates a V2 app, gets active and inactive again and then
// deletes the app.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHandlingTwoUsersEdgeCases) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // Check that there is a browser, back button and a app launcher.
  EXPECT_EQ(1, model_->item_count());

  // Switch to an inactive user.
  SwitchActiveUser(account_id2);
  EXPECT_EQ(1, model_->item_count());

  // Add the v2 app to the inactive user and check that no item was added to
  // the launcher.
  {
    AddExtension(extension1_.get());
    V2App v2_app(profile(), extension1_.get());
    EXPECT_EQ(1, model_->item_count());

    // Switch to the primary user and check that the item is shown.
    SwitchActiveUser(account_id);
    EXPECT_EQ(2, model_->item_count());

    // Switch to the second user and check that the item goes away - even if the
    // item gets closed.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(1, model_->item_count());
  }

  app_service_test().WaitForAppService();
  // After the application was killed there should still be 1 item.
  EXPECT_EQ(1, model_->item_count());

  // Switching then back to the default user should not show the additional item
  // anymore.
  SwitchActiveUser(account_id);
  EXPECT_EQ(1, model_->item_count());
}

TEST_P(ChromeLauncherControllerTest, Active) {
  InitLauncherController();

  // Creates a new app window.
  int initial_item_count = model_->item_count();
  AddExtension(extension1_.get());
  V2App app_1(profile(), extension1_.get());
  EXPECT_TRUE(app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_EQ(initial_item_count + 1, model_->item_count());
  ash::ShelfItemDelegate* app_item_delegate_1 =
      model_->GetShelfItemDelegate(model_->items()[initial_item_count].id);
  ASSERT_TRUE(app_item_delegate_1);
  AppWindowLauncherItemController* app_item_controller_1 =
      app_item_delegate_1->AsAppWindowLauncherItemController();
  ASSERT_TRUE(app_item_controller_1);
  ui::BaseWindow* last_active =
      GetLastActiveWindowForItemController(app_item_controller_1);
  EXPECT_EQ(app_1.window()->GetNativeWindow(), last_active->GetNativeWindow());
  // Change the status so that we can verify it gets reset when the active
  // window changes.
  launcher_controller_->SetItemStatus(app_item_delegate_1->shelf_id(),
                                      ash::STATUS_ATTENTION);

  // Creates another app window, which should become active and reset |app_1|'s
  // status (to running).
  AddExtension(extension2_.get());
  V2App app_2(profile(), extension2_.get());
  EXPECT_TRUE(app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_EQ(initial_item_count + 2, model_->item_count());
  ash::ShelfItemDelegate* app_item_delegate_2 =
      model_->GetShelfItemDelegate(model_->items()[initial_item_count + 1].id);
  ASSERT_TRUE(app_item_delegate_2);
  AppWindowLauncherItemController* app_item_controller_2 =
      app_item_delegate_2->AsAppWindowLauncherItemController();
  ASSERT_TRUE(app_item_controller_2);
  last_active = GetLastActiveWindowForItemController(app_item_controller_2);
  EXPECT_EQ(app_2.window()->GetNativeWindow(), last_active->GetNativeWindow());
  const ash::ShelfItem* shelf_item_1 =
      launcher_controller_->GetItem(app_item_delegate_1->shelf_id());
  ASSERT_TRUE(shelf_item_1);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_item_1->status);

  launcher_controller_->SetItemStatus(app_item_delegate_2->shelf_id(),
                                      ash::STATUS_ATTENTION);

  // Activate the first window, which should reset the status of the
  // second apps window.
  app_1.window()->GetBaseWindow()->Activate();
  const ash::ShelfItem* shelf_item_2 =
      launcher_controller_->GetItem(app_item_delegate_2->shelf_id());
  ASSERT_TRUE(shelf_item_2);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_item_2->status);
}

// Check that V2 applications will be made visible on the target desktop if
// another window of the same type got previously teleported there.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppFollowsTeleportedWindow) {
  InitLauncherController();
  ash::MultiUserWindowManager* window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();

  // Create and add three users / profiles, and go to #1's desktop.
  TestingProfile* profile1 = CreateMultiUserProfile("user-1");
  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  TestingProfile* profile3 = CreateMultiUserProfile("user-3");
  const AccountId account_id1(
      multi_user_util::GetAccountIdFromProfile(profile1));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  const AccountId account_id3(
      multi_user_util::GetAccountIdFromProfile(profile3));

  extensions::TestExtensionSystem* extension_system1(
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile1)));
  extensions::ExtensionService* extension_service1 =
      extension_system1->CreateExtensionService(
          base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extension_service1->Init();
  apps::AppServiceProxy* proxy1 =
      apps::AppServiceProxyFactory::GetForProfile(profile1);

  SwitchActiveUser(account_id1);

  // A v2 app for user #1 should be shown first and get hidden when switching
  // to desktop #2.
  extension_service1->AddExtension(extension1_.get());
  proxy1->FlushMojoCallsForTesting();
  V2App v2_app_1(profile1, extension1_.get());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(account_id2);
  app_service_test().FlushMojoCalls();
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());

  // Add a v2 app for user #1 while on desktop #2 should not be shown.
  V2App v2_app_2(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Teleport the app from user #1 to the desktop #2 should show it.
  window_manager->ShowWindowForUser(v2_app_1.window()->GetNativeWindow(),
                                    account_id2);
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Creating a new application for user #1 on desktop #2 should teleport it
  // there automatically.
  V2App v2_app_3(profile1, extension1_.get());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_3.window()->GetNativeWindow()->IsVisible());

  // Switching back to desktop#1 and creating an app for user #1 should move
  // the app on desktop #1.
  SwitchActiveUser(account_id1);
  app_service_test().FlushMojoCalls();
  V2App v2_app_4(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_3.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_4.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #3 and creating an app for user #1 should place it
  // on that user's desktop (#1).
  SwitchActiveUser(account_id3);
  app_service_test().FlushMojoCalls();
  V2App v2_app_5(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_5.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(account_id1);
  EXPECT_TRUE(v2_app_5.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #2, hiding the app window and creating an app should
  // teleport there automatically.
  SwitchActiveUser(account_id2);
  app_service_test().FlushMojoCalls();
  v2_app_1.window()->Hide();
  V2App v2_app_6(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_6.window()->GetNativeWindow()->IsVisible());
}

// Check that V2 applications hide correctly on the shelf when the app window
// is hidden.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHiddenWindows) {
  InitLauncherController();

  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // If switch to account_id2 is not run, the following switch to account_id
  // is invalid, because the user account is not changed, so switch to
  // account_id2 first.
  SwitchActiveUser(account_id2);

  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  SwitchActiveUser(account_id);
  EXPECT_EQ(1, model_->item_count());

  AddExtension(extension1_.get());
  V2App v2_app_1(profile(), extension1_.get());
  EXPECT_EQ(2, model_->item_count());
  {
    // Hide and show the app.
    v2_app_1.window()->Hide();
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(2, model_->item_count());
  }
  {
    // Switch user, hide and show the app and switch back.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(1, model_->item_count());

    SwitchActiveUser(account_id);
    EXPECT_EQ(2, model_->item_count());
  }
  {
    // Switch user, hide the app, switch back and then show it again.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(1, model_->item_count());

    SwitchActiveUser(account_id);
    // The following expectation does not work in current impl. It was working
    // before because MultiProfileSupport is not attached to user
    // associated with profile() hence not actually handling windows for the
    // user. It is a real bug. See http://crbug.com/693634
    // EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(2, model_->item_count());
  }
  {
    // Create a second app, hide and show it and then hide both apps.
    V2App v2_app_2(profile(), extension1_.get());
    EXPECT_EQ(2, model_->item_count());

    v2_app_2.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    v2_app_2.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Hide();
    v2_app_2.window()->Hide();
    EXPECT_EQ(1, model_->item_count());
  }
}

// Checks that spinners are hidden and restored on profile switching
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       SpinnersUpdateOnUserSwitch) {
  InitLauncherController();

  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const std::string user2 = "user2";
  const TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string app_id = extension1_->id();
  extension_service_->AddExtension(extension1_.get());

  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Add a spinner to the shelf
  launcher_controller_->GetShelfSpinnerController()->AddSpinnerToShelf(
      app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch to a new profile
  SwitchActiveUser(account_id2);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch back
  SwitchActiveUser(account_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Close the spinner
  launcher_controller_->GetShelfSpinnerController()->CloseSpinner(app_id);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
}

// Checks that pinned spinners are hidden and restored on profile switching
// but are not removed when the spinner closes.
TEST_P(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       PinnedSpinnersUpdateOnUserSwitch) {
  InitLauncherController();

  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const std::string user2 = "user2";
  const TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string app_id = extension1_->id();
  AddExtension(extension1_.get());

  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Pin an app to the shelf
  launcher_controller_->PinAppWithID(app_id);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Activate the spinner
  launcher_controller_->GetShelfSpinnerController()->AddSpinnerToShelf(
      app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch to a new profile
  SwitchActiveUser(account_id2);
  app_service_test().FlushMojoCalls();
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch back
  SwitchActiveUser(account_id);
  app_service_test().FlushMojoCalls();
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Close the spinner
  launcher_controller_->GetShelfSpinnerController()->CloseSpinner(app_id);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
}

// Checks that the generated menu list properly activates items.
TEST_P(ChromeLauncherControllerTest, V1AppMenuExecution) {
  InitLauncherControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // Add |extension_gmail_app_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  const ash::ShelfID gmail_id(extension_gmail_app_->id());
  AddExtension(extension_gmail_app_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(kGmailUrl));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title2);
  app_service_test().WaitForAppService();

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  base::string16 two_menu_items[] = {title1, title2};
  CheckAppMenu(launcher_controller_.get(), item_gmail, 2, two_menu_items);
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(gmail_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Execute the second item in the menu, after the title,
  // this shouldn't do anything since that item is already the active tab.
  {
    ash::ShelfApplicationMenuModel menu(
        base::string16(),
        launcher_controller_->GetAppMenuItemsForTesting(item_gmail),
        item_delegate);
    menu.ActivatedAt(2);
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Execute the first item in the menu, after the title,
  // this should activate the other tab.
  {
    ash::ShelfApplicationMenuModel menu(
        base::string16(),
        launcher_controller_->GetAppMenuItemsForTesting(item_gmail),
        item_delegate);
    menu.ActivatedAt(1);
  }
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Checks that the generated menu list properly deletes items.
TEST_P(ChromeLauncherControllerTest, V1AppMenuDeletionExecution) {
  InitLauncherControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // Add |extension_gmail_app_| to the launcher and add two items.
  const ash::ShelfID gmail_id(extension_gmail_app_->id());
  AddExtension(extension_gmail_app_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(kGmailUrl));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title2);
  app_service_test().WaitForAppService();

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  base::string16 two_menu_items[] = {title1, title2};
  CheckAppMenu(launcher_controller_.get(), item_gmail, 2, two_menu_items);

  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(gmail_id);
  ASSERT_TRUE(item_delegate);
  int tabs = browser()->tab_strip_model()->count();
  // Activate the proper tab through the menu item.
  {
    auto items = launcher_controller_->GetAppMenuItemsForTesting(item_gmail);
    item_delegate->ExecuteCommand(false, 1, ui::EF_NONE,
                                  display::kInvalidDisplayId);
    EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  }

  // Delete one tab through the menu item.
  {
    auto items = launcher_controller_->GetAppMenuItemsForTesting(item_gmail);
    item_delegate->ExecuteCommand(false, 1, ui::EF_SHIFT_DOWN,
                                  display::kInvalidDisplayId);
    EXPECT_EQ(--tabs, browser()->tab_strip_model()->count());
  }
}

// Tests that the Gmail extension matches more than the app itself claims with
// the manifest file.
TEST_P(ChromeLauncherControllerTest, GmailMatching) {
  InitLauncherControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check that the launcher controller does not recognize the running app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Installing |extension_gmail_app_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension_gmail_app_->id());
  AddExtension(extension_gmail_app_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));

  // Check that it is now handled.
  EXPECT_TRUE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Check also that the app has detected that properly.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  EXPECT_EQ(1U,
            launcher_controller_->GetAppMenuItemsForTesting(item_gmail).size());
}

// Tests that the Gmail extension does not match the offline version.
TEST_P(ChromeLauncherControllerTest, GmailOfflineMatching) {
  InitLauncherControllerWithBrowser();

  StartPrefSyncService(syncer::SyncDataList());

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kOfflineGmailUrl), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Installing |extension_gmail_app_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension_gmail_app_->id());
  AddExtension(extension_gmail_app_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension_gmail_app_->id()));

  // The content should not be able to be handled by the app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));
}

// Verify that the launcher item positions are persisted and restored.
TEST_P(ChromeLauncherControllerTest, PersistLauncherItemPositions) {
  InitLauncherController();

  TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
  SetLauncherControllerHelper(helper);

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[0].type);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip_model->count());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");

  EXPECT_FALSE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("1");
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("2");

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[2].type);

  // Move browser shortcut item from index 0 to index 2.
  model_->Move(0, 2);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);

  RecreateLauncherController();
  helper = new TestLauncherControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");
  SetLauncherControllerHelper(helper);
  launcher_controller_->Init();

  // Check ShelfItems are restored after resetting ChromeLauncherController.
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
}

// Verifies pinned apps are persisted and restored.
TEST_P(ChromeLauncherControllerTest, PersistPinned) {
  InitLauncherControllerWithBrowser();
  size_t initial_size = model_->items().size();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());

  TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetLauncherControllerHelper(helper);

  // app_icon_loader is owned by ChromeLauncherController.
  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl;
  app_icon_loader->AddSupportedApp("1");
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));
  EXPECT_EQ(0, app_icon_loader->fetch_count());

  launcher_controller_->PinAppWithID("1");
  const int app_index = model_->ItemIndexByID(ash::ShelfID("1"));
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[app_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, model_->items().size());

  RecreateLauncherController();
  helper = new TestLauncherControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetLauncherControllerHelper(helper);
  // app_icon_loader is owned by ChromeLauncherController.
  app_icon_loader = new TestAppIconLoaderImpl;
  app_icon_loader->AddSupportedApp("1");
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));
  launcher_controller_->Init();

  EXPECT_EQ(1, app_icon_loader->fetch_count());
  ASSERT_EQ(initial_size + 1, model_->items().size());
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[app_index].type);

  launcher_controller_->UnpinAppWithID("1");
  EXPECT_EQ(initial_size + 1, model_->items().size());

  tab_strip_model->CloseWebContentsAt(0, 0);
  EXPECT_EQ(initial_size, model_->items().size());
}

// Verifies that ShelfID property is updated for browsers that are present when
// ChromeLauncherController is created.
TEST_P(ChromeLauncherControllerTest, ExistingBrowserWindowShelfIDSet) {
  InitLauncherControllerWithBrowser();
  launcher_controller_->PinAppWithID("1");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());

  TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "0");
  SetLauncherControllerHelper(helper);

  RecreateLauncherController();
  helper = new TestLauncherControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetLauncherControllerHelper(helper);
  launcher_controller_->Init();

  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID("1")));
  EXPECT_EQ(ash::ShelfID("1"),
            ash::ShelfID::Deserialize(
                browser()->window()->GetNativeWindow()->GetProperty(
                    ash::kShelfIDKey)));
}

TEST_P(ChromeLauncherControllerTest, MultipleAppIconLoaders) {
  InitLauncherControllerWithBrowser();

  const ash::ShelfID shelf_id1(extension1_->id());
  const ash::ShelfID shelf_id2(extension2_->id());
  const ash::ShelfID shelf_id3(extension_gmail_app_->id());
  // app_icon_loader1 and app_icon_loader2 are owned by
  // ChromeLauncherController.
  TestAppIconLoaderImpl* app_icon_loader1 = new TestAppIconLoaderImpl();
  TestAppIconLoaderImpl* app_icon_loader2 = new TestAppIconLoaderImpl();
  app_icon_loader1->AddSupportedApp(shelf_id1.app_id);
  app_icon_loader2->AddSupportedApp(shelf_id2.app_id);
  SetAppIconLoaders(std::unique_ptr<AppIconLoader>(app_icon_loader1),
                    std::unique_ptr<AppIconLoader>(app_icon_loader2));

  launcher_controller_->CreateAppLauncherItem(
      std::make_unique<AppServiceAppWindowLauncherItemController>(
          shelf_id3, launcher_controller_->app_service_app_window_controller()),
      ash::STATUS_RUNNING);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(0, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CreateAppLauncherItem(
      std::make_unique<AppServiceAppWindowLauncherItemController>(
          shelf_id2, launcher_controller_->app_service_app_window_controller()),
      ash::STATUS_RUNNING);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CreateAppLauncherItem(
      std::make_unique<AppServiceAppWindowLauncherItemController>(
          shelf_id1, launcher_controller_->app_service_app_window_controller()),
      ash::STATUS_RUNNING);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CloseLauncherItem(shelf_id1);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CloseLauncherItem(shelf_id2);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(1, app_icon_loader2->clear_count());

  launcher_controller_->CloseLauncherItem(shelf_id3);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(1, app_icon_loader2->clear_count());
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcAppPinPolicy) {
  InitLauncherControllerWithBrowser();
  arc::mojom::AppInfo appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(appinfo);

  // Set policy, that makes pins ARC app. Unlike native extension, for ARC app
  // package_name (not hash) specified as id. In this test we check that
  // by hash we can determine that appropriate package was set by policy.
  base::ListValue policy_value;
  AppendPrefValue(&policy_value, appinfo.package_name);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcManaged) {
  extension_service_->AddExtension(arc_support_host_.get());
  // Test enables ARC, so turn it off for initial values.
  EnablePlayStore(false);

  InitLauncherController();

  // To prevent import legacy pins each time.
  // Initially pins are imported from legacy pref based model.
  StartPrefSyncService(syncer::SyncDataList());

  // Initial run, ARC is not managed and disabled, Play Store pin should be
  // available.
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Chrome, Play Store");

  // ARC is managed and enabled, Play Store pin should be available.
  // Note: CHECKING_ANDROID_MANAGEMENT here means that opt-in flow is skipped.
  profile()->GetTestingPrefService()->SetManagedPref(
      arc::prefs::kArcEnabled, std::make_unique<base::Value>(true));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(true, true,
                   arc::ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
                   "Chrome, Play Store");

  // ARC is managed and disabled, Play Store pin should not be available.
  profile()->GetTestingPrefService()->SetManagedPref(
      arc::prefs::kArcEnabled, std::make_unique<base::Value>(false));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(false, true, arc::ArcSessionManager::State::STOPPED,
                   "Chrome");

  // ARC is not managed and disabled, Play Store pin should be available.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      arc::prefs::kArcEnabled);
  base::RunLoop().RunUntilIdle();
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Chrome, Play Store");

  // ARC is not managed and enabled, Play Store pin should be available.
  // Note: NEGOTIATING_TERMS_OF_SERVICE here means that opt-in flow starts.
  EnablePlayStore(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
                   "Chrome, Play Store");

  // User disables ARC. ARC is not managed and disabled, Play Store pin should
  // be automatically removed.
  EnablePlayStore(false);
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Chrome");

  // Even if re-enable it again, Play Store pin does not appear automatically.
  EnablePlayStore(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
                   "Chrome");
}

// Test the application menu of a shelf item with multiple ARC windows.
TEST_P(ChromeLauncherControllerWithArcTest, ShelfItemWithMultipleWindows) {
  InitLauncherControllerWithBrowser();

  arc::mojom::AppInfo appinfo =
      CreateAppInfo("Test1", "test", "com.example.app");
  AddArcAppAndShortcut(appinfo);

  // Widgets will be deleted by the system.
  NotifyOnTaskCreated(appinfo, 1 /* task_id */);
  views::Widget* window1 = CreateArcWindow("org.chromium.arc.1");
  ASSERT_TRUE(window1);
  EXPECT_TRUE(window1->IsActive());

  NotifyOnTaskCreated(appinfo, 2 /* task_id */);
  views::Widget* window2 = CreateArcWindow("org.chromium.arc.2");
  ASSERT_TRUE(window2);

  EXPECT_FALSE(window1->IsActive());
  EXPECT_TRUE(window2->IsActive());

  const std::string app_id = ArcAppTest::GetAppId(appinfo);
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);

  // Selecting the item will show its application menu. It does not change the
  // active window.
  SelectItem(item_delegate);
  EXPECT_FALSE(window1->IsActive());
  EXPECT_TRUE(window2->IsActive());

  // Command ids are just app window indices. Note, apps are registered in
  // opposite order. Last created goes in front.
  auto items = item_delegate->GetAppMenuItems(0, base::NullCallback());
  ASSERT_EQ(items.size(), 2U);

  // Execute command 1 to activate the first window.
  item_delegate->ExecuteCommand(false, 1, ui::EF_NONE,
                                display::kInvalidDisplayId);
  EXPECT_TRUE(window1->IsActive());
  EXPECT_FALSE(window2->IsActive());

  // Selecting the item will show its application menu. It does not change the
  // active window.
  SelectItem(item_delegate);
  EXPECT_TRUE(window1->IsActive());
  EXPECT_FALSE(window2->IsActive());

  // Execute command 0 to activate the second window.
  item_delegate->ExecuteCommand(false, 0, ui::EF_NONE,
                                display::kInvalidDisplayId);
  EXPECT_FALSE(window1->IsActive());
  EXPECT_TRUE(window2->IsActive());
}

namespace {

class ChromeLauncherControllerArcDefaultAppsTest
    : public ChromeLauncherControllerTest {
 public:
  ChromeLauncherControllerArcDefaultAppsTest() = default;
  ChromeLauncherControllerArcDefaultAppsTest(
      const ChromeLauncherControllerArcDefaultAppsTest&) = delete;
  ChromeLauncherControllerArcDefaultAppsTest& operator=(
      const ChromeLauncherControllerArcDefaultAppsTest&) = delete;
  ~ChromeLauncherControllerArcDefaultAppsTest() override = default;

 protected:
  void SetUp() override {
    ArcAppIcon::DisableSafeDecodingForTesting();
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeLauncherControllerTest::SetUp();
  }
};

class ChromeLauncherControllerPlayStoreAvailabilityTest
    : public ChromeLauncherControllerTest {
 public:
  ChromeLauncherControllerPlayStoreAvailabilityTest() = default;
  ChromeLauncherControllerPlayStoreAvailabilityTest(
      const ChromeLauncherControllerPlayStoreAvailabilityTest&) = delete;
  ChromeLauncherControllerPlayStoreAvailabilityTest& operator=(
      const ChromeLauncherControllerPlayStoreAvailabilityTest&) = delete;
  ~ChromeLauncherControllerPlayStoreAvailabilityTest() override = default;

 protected:
  void SetUp() override {
    if (GetParam().second)
      arc::SetArcAlwaysStartWithoutPlayStoreForTesting();
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeLauncherControllerTest::SetUp();
  }
};

}  // namespace

TEST_P(ChromeLauncherControllerArcDefaultAppsTest, DefaultApps) {
  arc_test_.SetUp(profile());
  InitLauncherController();

  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EnablePlayStore(false);
  EXPECT_FALSE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  ASSERT_TRUE(prefs->GetAppIds().size());

  const std::string app_id =
      ArcAppTest::GetAppId(arc_test_.fake_default_apps()[0]);
  const ash::ShelfID shelf_id(app_id);
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));
  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                             arc::UserInteractionType::NOT_USER_INITIATED));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));

  // Stop ARC again. Shelf item should go away.
  EnablePlayStore(false);

  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                             arc::UserInteractionType::NOT_USER_INITIATED));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));

  auto* item_delegate = model_->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
  // Initially, a default icon is set for the shelf item.
  EXPECT_FALSE(item_delegate->image_set_by_controller());
  auto get_icon = [=]() {
    return *launcher_controller_->GetItem(shelf_id)->image.bitmap();
  };
  const SkBitmap default_icon = get_icon();

  std::string window_app_id("org.chromium.arc.1");
  CreateArcWindow(window_app_id);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_default_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));
  // Refresh delegate, it was changed.
  item_delegate = model_->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
  EXPECT_FALSE(item_delegate->image_set_by_controller());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon, get_icon()));

  // Wait for the real app icon image to be decoded and set for the shelf item.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(default_icon, get_icon()));
}

TEST_P(ChromeLauncherControllerArcDefaultAppsTest, PlayStoreDeferredLaunch) {
  // Add ARC host app to enable Play Store default app.
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());
  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EXPECT_TRUE(prefs->IsRegistered(arc::kPlayStoreAppId));

  InitLauncherController();

  EnablePlayStore(true);

  // Pin Play Store. It should be pinned but not scheduled for deferred launch.
  launcher_controller_->PinAppWithID(arc::kPlayStoreAppId);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc::kPlayStoreAppId));
  EXPECT_FALSE(launcher_controller_->GetShelfSpinnerController()->HasApp(
      arc::kPlayStoreAppId));

  // Simulate click. This should schedule Play Store for deferred launch.
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(ash::ShelfID(arc::kPlayStoreAppId));
  EXPECT_TRUE(item_delegate);
  SelectItem(item_delegate);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc::kPlayStoreAppId));
  EXPECT_TRUE(launcher_controller_->GetShelfSpinnerController()->HasApp(
      arc::kPlayStoreAppId));
}

TEST_P(ChromeLauncherControllerArcDefaultAppsTest, PlayStoreLaunchMetric) {
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());
  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();

  InitLauncherController();
  EnablePlayStore(true);

  // Play Store available now as a default app but is not ready yet.
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(arc::kPlayStoreAppId);
  ASSERT_TRUE(app_info);
  EXPECT_FALSE(app_info->ready);

  constexpr char kHistogramName[] = "Arc.PlayStoreLaunch.TimeDelta";

  // Launch Play Store in deferred mode.
  arc::LaunchApp(profile(), arc::kPlayStoreAppId, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  // This is deferred launch, no actual intents are delivered to ARC.
  EXPECT_EQ(0U, arc_test_.app_instance()->launch_intents().size());
  arc::mojom::AppInfo app;
  app.activity = arc::kPlayStoreActivity;
  app.package_name = arc::kPlayStorePackage;
  arc_test_.app_instance()->SendRefreshAppList({app});
  ASSERT_EQ(1U, arc_test_.app_instance()->launch_intents().size());
  std::string play_store_window_id("org.chromium.arc.1");
  views::Widget* play_store_window = CreateArcWindow(play_store_window_id);
  arc_test_.app_instance()->SendTaskCreated(
      1, app, arc_test_.app_instance()->launch_intents()[0]);
  EXPECT_TRUE(
      launcher_controller_->GetItem(ash::ShelfID(arc::kPlayStoreAppId)));
  // UMA is reported since app becomes ready.
  base::HistogramBase* const histogram =
      base::StatisticsRecorder::FindHistogram(kHistogramName);
  ASSERT_TRUE(histogram);
  std::unique_ptr<base::HistogramSamples> samples = histogram->SnapshotDelta();
  ASSERT_EQ(1, samples->TotalCount());
  play_store_window->Close();

  // Launch Play Store in app-ready mode.
  arc::LaunchApp(profile(), arc::kPlayStoreAppId, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  ASSERT_EQ(2U, arc_test_.app_instance()->launch_intents().size());
  play_store_window_id = "org.chromium.arc.2";
  play_store_window = CreateArcWindow(play_store_window_id);
  arc_test_.app_instance()->SendTaskCreated(
      2, app, arc_test_.app_instance()->launch_intents()[1]);
  EXPECT_TRUE(
      launcher_controller_->GetItem(ash::ShelfID(arc::kPlayStoreAppId)));
  // UMA is reported for app-ready launch. Note, previous call of SnapshotDelta
  // resets samples, so we expect here only one recorded.
  EXPECT_EQ(1, histogram->SnapshotDelta()->TotalCount());
  play_store_window->Close();
}

// Tests that the Play Store is not visible in AOSP image and visible in default
// images.
TEST_P(ChromeLauncherControllerPlayStoreAvailabilityTest, Visible) {
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());

  InitLauncherController();
  StartPrefSyncService(syncer::SyncDataList());

  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EXPECT_EQ(arc::IsPlayStoreAvailable(),
            prefs->IsRegistered(arc::kPlayStoreAppId));
  // If the Play Store available, it is pinned by default.
  EXPECT_EQ(arc::IsPlayStoreAvailable(),
            launcher_controller_->IsAppPinned(arc::kPlayStoreAppId));
  arc_test_.TearDown();
}

// Checks the case when several app items have the same ordinal position (which
// is valid case).
TEST_P(ChromeLauncherControllerTest, CheckPositionConflict) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension_gmail_app_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 1, extension_gmail_app_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());

  const syncer::StringOrdinal position_chrome =
      app_list_syncable_service_->GetPinPosition(extension_misc::kChromeAppId);
  const syncer::StringOrdinal position_1 =
      app_list_syncable_service_->GetPinPosition(extension1_->id());
  const syncer::StringOrdinal position_2 =
      app_list_syncable_service_->GetPinPosition(extension2_->id());
  const syncer::StringOrdinal position_3 =
      app_list_syncable_service_->GetPinPosition(extension_gmail_app_->id());
  EXPECT_TRUE(position_chrome.LessThan(position_1));
  EXPECT_TRUE(position_1.Equals(position_2));
  EXPECT_TRUE(position_2.Equals(position_3));

  // Move Chrome between App1 and App2.
  // Note, move target_index is in context when moved element is removed from
  // array first.
  model_->Move(0, 1);
  EXPECT_EQ("App1, Chrome, App2, Gmail", GetPinnedAppStatus());

  // Expect sync positions for only Chrome is updated and its resolution is
  // after all duplicated ordinals.
  EXPECT_TRUE(position_3.LessThan(app_list_syncable_service_->GetPinPosition(
      extension_misc::kChromeAppId)));
  EXPECT_TRUE(position_1.Equals(
      app_list_syncable_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(position_1.Equals(
      app_list_syncable_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(position_2.Equals(
      app_list_syncable_service_->GetPinPosition(extension2_->id())));
  EXPECT_TRUE(position_3.Equals(
      app_list_syncable_service_->GetPinPosition(extension_gmail_app_->id())));
}

// Test the case when sync app is turned off and we need to use local copy to
// support user's pins.
TEST_P(ChromeLauncherControllerTest, SyncOffLocalUpdate) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  syncer::SyncDataList copy_sync_list =
      app_list_syncable_service_->GetAllSyncDataForTesting();

  app_list_syncable_service_->StopSyncing(syncer::APP_LIST);
  RecreateLauncherController()->Init();

  // Pinned state should not change.
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  launcher_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  // Resume syncing and sync information overrides local copy.
  StartAppSyncService(copy_sync_list);
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
}

// Test the Settings can be pinned and unpinned.
TEST_P(ChromeLauncherControllerTest, InternalAppPinUnpin) {
  InitLauncherController();
  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());

  const std::string app_id = ash::kInternalAppIdSettings;
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));

  // Pin Settings.
  launcher_controller_->PinAppWithID(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));

  // Unpin Settings.
  launcher_controller_->UnpinAppWithID(app_id);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));
}

// Test that internal app can be added and removed on shelf.
TEST_P(ChromeLauncherControllerTest, InternalAppWindowRecreation) {
  InitLauncherController();

  // Only test the first internal app. The others should be the same.
  const auto& internal_app = app_list::GetInternalAppList(profile()).front();
  const std::string app_id = internal_app.app_id;
  const ash::ShelfID shelf_id(app_id);
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  views::Widget* internal_app_window = CreateShelfAppWindow(app_id);
  ASSERT_TRUE(internal_app_window);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));

  internal_app_window->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  // Create and close again.
  internal_app_window = CreateShelfAppWindow(app_id);
  ASSERT_TRUE(internal_app_window);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));

  internal_app_window->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));
}

// Test that internal app can be added and removed by SetProperty of
// ash::kShelfIDKey.
TEST_P(ChromeLauncherControllerTest, InternalAppWindowPropertyChanged) {
  InitLauncherController();

  // Only test the first internal app. The others should be the same.
  const auto internal_app = app_list::GetInternalAppList(profile()).front();
  std::string app_id;
  ash::ShelfID shelf_id;
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  // Set an empty ash::kShelfIDKey.
  views::Widget* internal_app_window = CreateShelfAppWindow(app_id);
  ASSERT_TRUE(internal_app_window);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  // Set an invalid ash::kShelfIDKey.
  app_id = "An invalid internal app id";
  shelf_id = ash::ShelfID(app_id);
  internal_app_window->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));

  // Set a valid ash::kShelfIDKey.
  app_id = internal_app.app_id;
  shelf_id = ash::ShelfID(app_id);
  internal_app_window->GetNativeWindow()->SetProperty(
      ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(launcher_controller_->GetItem(shelf_id));

  internal_app_window->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(launcher_controller_->GetItem(shelf_id));
}

class ChromeLauncherControllerDemoModeTest
    : public ChromeLauncherControllerTest {
 protected:
  ChromeLauncherControllerDemoModeTest() { auto_start_arc_test_ = true; }
  ChromeLauncherControllerDemoModeTest(
      const ChromeLauncherControllerDemoModeTest&) = delete;
  ChromeLauncherControllerDemoModeTest& operator=(
      const ChromeLauncherControllerDemoModeTest&) = delete;
  ~ChromeLauncherControllerDemoModeTest() override = default;

  void SetUp() override {
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    ChromeLauncherControllerTest::SetUp();

    // Fake Demo Mode.
    demo_mode_test_helper_ = std::make_unique<chromeos::DemoModeTestHelper>();
    demo_mode_test_helper_->InitializeSession();
  }

  void TearDown() override {
    demo_mode_test_helper_.reset();

    ChromeLauncherControllerTest::TearDown();
  }

 private:
  std::unique_ptr<chromeos::DemoModeTestHelper> demo_mode_test_helper_;
};

TEST_P(ChromeLauncherControllerDemoModeTest, PinnedAppsOnline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);

  InitLauncherControllerWithBrowser();

  base::ListValue policy_value;

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  AppendPrefValue(&policy_value, extension1_->id());
  AppendPrefValue(&policy_value, extension2_->id());

  arc::mojom::AppInfo appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(appinfo);

  arc::mojom::AppInfo online_only_appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.onlineonly");
  const std::string online_only_app_id =
      AddArcAppAndShortcut(online_only_appinfo);

  AppendPrefValue(&policy_value, appinfo.package_name);
  AppendPrefValue(&policy_value, online_only_appinfo.package_name);

  // If the device is offline, extension2 and onlineonly should be unpinned.
  chromeos::DemoSession::Get()->OverrideIgnorePinPolicyAppsForTesting(
      {extension2_->id(), online_only_appinfo.package_name});

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  app_service_test().FlushMojoCalls();

  // Since the device is online, all policy pinned apps are pinned.
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension1_->id(), profile()));

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension2_->id(), profile()));

  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));

  EXPECT_TRUE(launcher_controller_->IsAppPinned(online_only_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(online_only_app_id, profile()));
}

TEST_P(ChromeLauncherControllerDemoModeTest, PinnedAppsOffline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  InitLauncherControllerWithBrowser();

  base::ListValue policy_value;

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  AppendPrefValue(&policy_value, extension1_->id());
  AppendPrefValue(&policy_value, extension2_->id());

  arc::mojom::AppInfo appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(appinfo);

  arc::mojom::AppInfo online_only_appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.onlineonly");
  const std::string online_only_app_id =
      AddArcAppAndShortcut(online_only_appinfo);

  AppendPrefValue(&policy_value, appinfo.package_name);
  AppendPrefValue(&policy_value, online_only_appinfo.package_name);

  // If the device is offline, extension2 and onlineonly should be unpinned.
  chromeos::DemoSession::Get()->OverrideIgnorePinPolicyAppsForTesting(
      {extension2_->id(), online_only_appinfo.package_name});

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());
  app_service_test().FlushMojoCalls();

  // Since the device is online, the policy pinned apps that shouldn't be pinned
  // in Demo Mode are unpinned.
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension1_->id(), profile()));

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension2_->id(), profile()));

  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));

  EXPECT_FALSE(launcher_controller_->IsAppPinned(online_only_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(online_only_app_id, profile()));

  // Pin a Chrome app that would have been pinned by policy but was suppressed
  // for Demo Mode.
  launcher_controller_->PinAppWithID(extension2_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension2_->id(), profile()));

  // Pin an ARC app that would have been pinned by policy but was suppressed
  // for Demo Mode.
  launcher_controller_->PinAppWithID(online_only_app_id);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(online_only_app_id, profile()));
}

TEST_P(ChromeLauncherControllerTest, CrostiniTerminalPinUnpin) {
  InitLauncherController();

  // Load pinned Terminal from prefs without Crostini UI being allowed
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 1, crostini::kCrostiniTerminalSystemAppId);
  SendPinChanges(sync_list, true);
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Reload after allowing Crostini UI
  crostini::CrostiniTestHelper test_helper(profile());
  test_helper.ReInitializeAppServiceIntegration();
  // TODO(crubug.com/918739): Fix pins are not refreshed on enabling Crostini.
  // As a workaround add any app that triggers pin update.
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, Terminal", GetPinnedAppStatus());

  // Unpin the Terminal
  launcher_controller_->UnpinAppWithID(crostini::kCrostiniTerminalSystemAppId);
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Pin Terminal again.
  launcher_controller_->PinAppWithID(crostini::kCrostiniTerminalSystemAppId);
  EXPECT_EQ("Chrome, Terminal", GetPinnedAppStatus());
}

// TODO(crbug.com/846546) Recognising app id from the browser app_name is only
// necessary because the crostini terminal is a little hacky. Pending a better
// terminal implementation we should remove this test.
TEST_P(ChromeLauncherControllerTest, CrostiniBrowserWindowsRecogniseShelfItem) {
  InitLauncherController();
  crostini::CrostiniTestHelper helper(profile());

  // We want to match this shelf item.
  ash::ShelfItem item;
  item.id = ash::ShelfID("blah");
  item.type = ash::ShelfItemType::TYPE_APP;
  model_->Add(item);

  // We manually create a browser window with the correct app_name, as this is
  // how the app_id is communicated.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      crostini::AppNameFromCrostiniAppId(item.id.app_id),
      true /* trusted_srouce */, browser()->window()->GetBounds(), profile(),
      true /* user_gesture */);
  params.window = browser()->window();
  params.type = Browser::TYPE_NORMAL;
  Browser* b = new Browser(params);
  set_browser(b);
  chrome::NewTab(browser());
  browser()->window()->Show();

  EXPECT_EQ(launcher_controller_->GetAppIDForWebContents(
                browser()->tab_strip_model()->GetActiveWebContents()),
            item.id.app_id);
}

// Tests behavior for ensuring some component apps can be marked unpinnable.
TEST_P(ChromeLauncherControllerTest, UnpinnableComponentApps) {
  InitLauncherController();

  const char* kPinnableApp = file_manager::kFileManagerAppId;
  const char* kNoPinApps[] = {file_manager::kGalleryAppId,
                              extension_misc::kFeedbackExtensionId};

  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(kPinnableApp, profile()));
  for (const char* id : kNoPinApps) {
    EXPECT_EQ(AppListControllerDelegate::NO_PIN,
              GetPinnableForAppID(id, profile()));
  }
}

TEST_P(ChromeLauncherControllerTest, DoNotShowInShelf) {
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 0, extension2_->id());
  SendPinChanges(sync_list, true);

  AddExtension(extension1_.get());
  AddExtension(extension2_.get());

  // Set App1.show_in_shelf to false.
  std::vector<apps::mojom::AppPtr> apps;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kExtension;
  app->app_id = extension1_->id();
  app->show_in_shelf = apps::mojom::OptionalBool::kFalse;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .OnApps(std::move(apps));

  InitLauncherController();
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerWithArcTest, ReplacePinnedItem) {
  InitLauncherController();
  SendListOfArcApps();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));

  // Replace pin extension to ARC app
  launcher_controller_->ReplacePinnedItem(extension1_->id(), arc_app_id1);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));

  // Replace pin ARC app to ARC app
  launcher_controller_->ReplacePinnedItem(arc_app_id1, arc_app_id2);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));

  // Replace pin ARC app to extension app
  launcher_controller_->ReplacePinnedItem(arc_app_id2, extension1_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));

  // Replace pin extension app to extension app
  launcher_controller_->ReplacePinnedItem(extension1_->id(), extension2_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));

  // Try to replace item that is not pinned.
  launcher_controller_->ReplacePinnedItem(arc_app_id2, extension1_->id());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));

  // Try to replace item with item that is already pinned.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  launcher_controller_->ReplacePinnedItem(extension2_->id(), extension1_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
}

TEST_P(ChromeLauncherControllerWithArcTest, PinAtIndex) {
  InitLauncherController();
  SendListOfArcApps();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(arc_test_.fake_apps()[1]);

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  int index = 0;
  launcher_controller_->PinAppAtIndex(extension1_->id(), index);
  EXPECT_EQ(index,
            launcher_controller_->PinnedItemIndexByAppID(extension1_->id()));

  launcher_controller_->PinAppAtIndex(extension2_->id(), index);
  EXPECT_EQ(index,
            launcher_controller_->PinnedItemIndexByAppID(extension2_->id()));
  EXPECT_NE(index,
            launcher_controller_->PinnedItemIndexByAppID(extension1_->id()));

  index = 3;
  launcher_controller_->PinAppAtIndex(arc_app_id1, index);
  EXPECT_EQ(index, launcher_controller_->PinnedItemIndexByAppID(arc_app_id1));

  // Test pinning at invalid index.
  index = -100;
  launcher_controller_->PinAppAtIndex(arc_app_id2, index);
  EXPECT_NE(index, launcher_controller_->PinnedItemIndexByAppID(arc_app_id2));
  EXPECT_EQ(-1, launcher_controller_->PinnedItemIndexByAppID(arc_app_id2));

  // Test pinning already pinned app.
  index = 0;
  launcher_controller_->PinAppAtIndex(arc_app_id1, index);
  EXPECT_NE(index, launcher_controller_->PinnedItemIndexByAppID(arc_app_id1));
  EXPECT_EQ(3, launcher_controller_->PinnedItemIndexByAppID(arc_app_id1));
}

class ChromeLauncherControllerWebAppTest : public ChromeLauncherControllerTest {
 protected:
  ChromeLauncherControllerWebAppTest() {}

  ~ChromeLauncherControllerWebAppTest() override = default;

  void MaybeStartWebAppProvider() override {
    DCHECK(
        base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions));
    auto system_web_app_manager =
        std::make_unique<web_app::TestSystemWebAppManager>(profile());

    auto* provider = web_app::TestWebAppProvider::Get(profile());
    provider->SetSystemWebAppManager(std::move(system_web_app_manager));
    provider->SetRunSubsystemStartupTasks(true);
    provider->Start();
  }
};

// Test the web app interaction flow: pin it, run it, unpin it, close it.
TEST_P(ChromeLauncherControllerWebAppTest, WebAppPinRunUnpinClose) {
  constexpr char kWebAppUrl[] = "https://webappone.com/";
  constexpr char kWebAppName[] = "WebApp1";

  InitLauncherController();

  const web_app::AppId app_id =
      web_app::InstallDummyWebApp(profile(), kWebAppName, GURL(kWebAppUrl));
  base::RunLoop().RunUntilIdle();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(nullptr, launcher_controller_->GetItem(ash::ShelfID(app_id)));

  // Pinning the app should create a new shelf item.
  launcher_controller_->PinAppWithID(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_NE(nullptr, launcher_controller_->GetItem(ash::ShelfID(app_id)));

  // Reporting that the app is running should just update the existing item.
  launcher_controller_->SetV1AppStatus(app_id, ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_NE(nullptr, launcher_controller_->GetItem(ash::ShelfID(app_id)));

  // Unpinning the app should just update the existing item.
  launcher_controller_->UnpinAppWithID(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_NE(nullptr, launcher_controller_->GetItem(ash::ShelfID(app_id)));

  // Reporting that the app is closed should remove its shelf item.
  launcher_controller_->SetV1AppStatus(app_id, ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(nullptr, launcher_controller_->GetItem(ash::ShelfID(app_id)));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerWithArcTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerSplitSettingsSyncTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerLacrosTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerExtendedShelfTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerMultiProfileWithArcTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerArcDefaultAppsTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerPlayStoreAvailabilityTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false),
                      std::make_pair(ProviderType::kBookmarkApps, true),
                      std::make_pair(ProviderType::kWebApps, true)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerDemoModeTest,
    ::testing::Values(std::make_pair(ProviderType::kBookmarkApps, false),
                      std::make_pair(ProviderType::kWebApps, false)));

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeLauncherControllerWebAppTest,
    ::testing::Values(std::make_pair(ProviderType::kWebApps, false)));
