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
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_application_menu_model.h"
#include "ash/shelf/shelf_controller.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_util.h"
#include "chrome/browser/ui/ash/launcher/extension_app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/browser/ui/ash/session_controller_client.h"
#include "chrome/browser/ui/ash/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_image_loader_client.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_util.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/exo/shell_surface.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/session_manager/core/session_manager.h"
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
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;
using extensions::Extension;
using extensions::Manifest;
using extensions::UnloadedExtensionReason;

namespace {
constexpr char kOfflineGmailUrl[] = "https://mail.google.com/mail/mu/u";
constexpr char kGmailUrl[] = "https://mail.google.com/mail/u";
constexpr char kGmailLaunchURL[] = "https://mail.google.com/mail/ca";

// An extension prefix.
constexpr char kCrxAppPrefix[] = "_crx_";

// Dummy app id is used to put at least one pin record to prevent initializing
// pin model with default apps that can affect some tests.
constexpr char kDummyAppId[] = "dummyappid_dummyappid_dummyappid";

// ShelfModelObserver implementation that tracks what messages are invoked.
class TestShelfModelObserver : public ash::ShelfModelObserver {
 public:
  TestShelfModelObserver() {}
  ~TestShelfModelObserver() override {}

  // Overridden from ash::ShelfModelObserver:
  void ShelfItemAdded(int index) override {
    ++added_;
    last_index_ = index;
  }

  void ShelfItemRemoved(int index, const ash::ShelfItem& old_item) override {
    ++removed_;
    last_index_ = index;
  }

  void ShelfItemChanged(int index, const ash::ShelfItem& old_item) override {
    ++changed_;
    last_index_ = index;
  }

  void ShelfItemMoved(int start_index, int target_index) override {
    last_index_ = target_index;
  }

  void clear_counts() {
    added_ = 0;
    removed_ = 0;
    changed_ = 0;
    last_index_ = 0;
  }

  int added() const { return added_; }
  int removed() const { return removed_; }
  int changed() const { return changed_; }
  int last_index() const { return last_index_; }

 private:
  int added_ = 0;
  int removed_ = 0;
  int changed_ = 0;
  int last_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestShelfModelObserver);
};

// Test implementation of AppIconLoader.
class TestAppIconLoaderImpl : public AppIconLoader {
 public:
  TestAppIconLoaderImpl() = default;
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

  DISALLOW_COPY_AND_ASSIGN(TestAppIconLoaderImpl);
};

// Test implementation of LauncherControllerHelper.
class TestLauncherControllerHelper : public LauncherControllerHelper {
 public:
  TestLauncherControllerHelper() : LauncherControllerHelper(nullptr) {}
  explicit TestLauncherControllerHelper(Profile* profile)
      : LauncherControllerHelper(profile) {}
  ~TestLauncherControllerHelper() override {}

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

  DISALLOW_COPY_AND_ASSIGN(TestLauncherControllerHelper);
};

// Test implementation of a V2 app launcher item controller.
class TestV2AppLauncherItemController : public ash::ShelfItemDelegate {
 public:
  explicit TestV2AppLauncherItemController(const std::string& app_id)
      : ash::ShelfItemDelegate(ash::ShelfID(app_id)) {}

  ~TestV2AppLauncherItemController() override {}

  // Override for ash::ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ash::ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    std::move(callback).Run(ash::SHELF_ACTION_WINDOW_ACTIVATED, base::nullopt);
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestV2AppLauncherItemController);
};

// A test ShelfController implementation that tracks state and function calls.
class TestShelfController : public ash::mojom::ShelfController {
 public:
  TestShelfController() : binding_(this) {}
  ~TestShelfController() override = default;

  size_t added_count() const { return added_count_; }
  size_t removed_count() const { return removed_count_; }
  size_t updated_count() const { return updated_count_; }
  size_t set_delegate_count() const { return set_delegate_count_; }
  const ash::ShelfItem& last_item() const { return last_item_; }

  ash::mojom::ShelfControllerPtr CreateInterfacePtrAndBind() {
    ash::mojom::ShelfControllerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // ash::mojom::ShelfController:
  void AddObserver(
      ash::mojom::ShelfObserverAssociatedPtrInfo observer) override {
    observer_.Bind(std::move(observer));
  }
  void AddShelfItem(int32_t, const ash::ShelfItem& item) override {
    added_count_++;
    last_item_ = item;
  }
  void RemoveShelfItem(const ash::ShelfID&) override { removed_count_++; }
  void MoveShelfItem(const ash::ShelfID&, int32_t) override {}
  void UpdateShelfItem(const ash::ShelfItem& item) override {
    updated_count_++;
    last_item_ = item;
    if (updated_count_ == expected_updated_count_ &&
        !updated_callback_.is_null()) {
      base::ResetAndReturn(&updated_callback_).Run();
    }
  }

  void WaitForUpdates(size_t expected_updates) {
    base::RunLoop run_loop;
    expected_updated_count_ = expected_updates + updated_count_;
    updated_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void SetShelfItemDelegate(const ash::ShelfID&,
                            ash::mojom::ShelfItemDelegatePtr) override {
    set_delegate_count_++;
  }

  // Helper that waits for idle and extracts the non-default bitmap from the
  // last updated item in shelf controller.
  SkBitmap GetLastItemImage() {
    if (default_app_image_.isNull()) {
      default_app_image_ =
          *gfx::ImageSkiaOperations::CreateResizedImage(
               *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
                   IDR_APP_DEFAULT_ICON),
               skia::ImageOperations::RESIZE_BEST,
               gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                         extension_misc::EXTENSION_ICON_SMALL))
               .bitmap();
    }

    // Loading icon is multistep process. At first step default app icon is
    // loaded while real icon is requested and decoded.
    // base::RunLoop().RunUntilIdle() hides these steps and in most cases real
    // icon is returned afterward. However in rare cases default icon is left
    // after base::RunLoop().RunUntilIdle(). So make sure we don't return
    // default icon that may fail test expectations.
    while (true) {
      base::RunLoop().RunUntilIdle();
      const SkBitmap* bitmap = last_item().image.bitmap();
      CHECK(bitmap);
      if (!gfx::test::AreBitmapsEqual(default_app_image_, *bitmap))
        return *bitmap;
    }
  }

 private:
  size_t added_count_ = 0;
  size_t removed_count_ = 0;
  size_t updated_count_ = 0;
  size_t expected_updated_count_ = 0;
  base::OnceClosure updated_callback_;
  size_t set_delegate_count_ = 0;
  ash::ShelfItem last_item_;

  ash::mojom::ShelfObserverAssociatedPtr observer_;
  mojo::Binding<ash::mojom::ShelfController> binding_;

  // Used to cache default app image.
  SkBitmap default_app_image_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfController);
};

// Simulates selection of the shelf item.
void SelectItem(ash::ShelfItemDelegate* delegate) {
  std::unique_ptr<ui::Event> event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), ui::EventTimeForNow(),
      ui::EF_NONE, 0);
  delegate->ItemSelected(std::move(event), display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN, base::DoNothing());
}

// Creates a window with TYPE_APP shelf item type and the given app_id.
views::Widget* CreateShelfAppWindow(const std::string& app_id) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(5, 5, 20, 20);
  views::Widget* widget = new views::Widget();
  widget->Init(params);

  aura::Window* window = widget->GetNativeWindow();
  const ash::ShelfID shelf_id(app_id);
  window->SetProperty(ash::kShelfIDKey, new std::string(shelf_id.Serialize()));
  window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);

  widget->Show();
  widget->Activate();
  return widget;
}

}  // namespace

// A test ChromeLauncherController subclass that uses TestShelfController.
class TestChromeLauncherController : public ChromeLauncherController {
 public:
  TestChromeLauncherController(Profile* profile, ash::ShelfModel* model)
      : ChromeLauncherController(profile, model) {
    // Connect to the shelf controller, the base ctor can't call overrides.
    EXPECT_TRUE(ConnectToShelfController());
    ash::mojom::ShelfObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    shelf_controller_->AddObserver(std::move(ptr_info));
  }

  // ChromeLauncherController:
  using ChromeLauncherController::AttachProfile;
  using ChromeLauncherController::ReleaseProfile;
  bool ConnectToShelfController() override {
    // Set the shelf controller pointer to a test instance.
    if (!shelf_controller_.is_bound())
      shelf_controller_ = test_shelf_controller_.CreateInterfacePtrAndBind();
    return true;
  }

  TestShelfController* test_shelf_controller() {
    return &test_shelf_controller_;
  }

 private:
  TestShelfController test_shelf_controller_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeLauncherController);
};

class ChromeLauncherControllerTest : public BrowserWithTestWindowTest {
 protected:
  ChromeLauncherControllerTest()
      : BrowserWithTestWindowTest(Browser::TYPE_TABBED, false) {}

  ~ChromeLauncherControllerTest() override {}

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);

    app_list::AppListSyncableServiceFactory::SetUseInTesting();

    BrowserWithTestWindowTest::SetUp();

    model_observer_ = std::make_unique<TestShelfModelObserver>();
    model_ = std::make_unique<ash::ShelfModel>();
    model_->AddObserver(model_observer_.get());

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName,
                       "launcher controller test extension");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetInteger(extensions::manifest_keys::kManifestVersion, 2);
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "for testing pinned apps");

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

    if (auto_start_arc_test_)
      arc_test_.SetUp(profile());

    // Wait until |extension_system| is signaled as started.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    app_service_ =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    StartAppSyncService(app_service_->GetAllSyncData(syncer::APP_LIST));

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
    // Fake gmail extension.
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

    extension3_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest_gmail, Extension::NO_FLAGS,
                                    extension_misc::kGmailAppId, &error);

    // Fake google docs extension.
    extension4_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest, Extension::NO_FLAGS,
                                    extension_misc::kGoogleDocAppId, &error);
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
    EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

    // Set an empty pinned pref to begin with.
    syncer::SyncChangeList sync_list;
    InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
    SendPinChanges(sync_list, true);
    EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

    // Assume all applications have been added already.
    extension_service_->AddExtension(extension1_.get());
    extension_service_->AddExtension(extension2_.get());
    extension_service_->AddExtension(extension3_.get());
    extension_service_->AddExtension(extension4_.get());
    extension_service_->AddExtension(extension5_.get());
    extension_service_->AddExtension(extension6_.get());
    extension_service_->AddExtension(extension7_.get());
    extension_service_->AddExtension(extension8_.get());
    extension_service_->AddExtension(extension_platform_app_.get());
    // There should be nothing in the list by now.
    EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

    // Set user a preferences.
    InsertAddPinChange(user_a, 0, extension1_->id());
    InsertAddPinChange(user_a, 1, extension2_->id());
    InsertAddPinChange(user_a, 2, extension3_->id());
    InsertAddPinChange(user_a, 3, extension_platform_app_->id());
    InsertAddPinChange(user_a, 4, extension4_->id());
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
    model_->RemoveObserver(model_observer_.get());
    model_observer_.reset();
    launcher_controller_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  BrowserWindow* CreateBrowserWindow() override {
    return CreateTestBrowserWindowAura();
  }

  std::unique_ptr<Browser> CreateBrowserWithTestWindowForProfile(
      Profile* profile) {
    TestBrowserWindow* browser_window = CreateTestBrowserWindowAura();
    new TestBrowserWindowOwner(browser_window);
    return base::WrapUnique(
        CreateBrowser(profile, Browser::TYPE_TABBED, false, browser_window));
  }

  // Create an uninitialized chrome launcher controller instance.
  TestChromeLauncherController* CreateLauncherController() {
    launcher_controller_ =
        std::make_unique<TestChromeLauncherController>(profile(), model_.get());
    return launcher_controller_.get();
  }

  // Create and initialize the controller, owned by the test shell delegate.
  void InitLauncherController() { CreateLauncherController()->Init(); }

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
  TestChromeLauncherController* RecreateLauncherController() {
    // Destroy any existing controller first; only one may exist at a time.
    ResetLauncherController();
    model_->RemoveObserver(model_observer_.get());
    model_ = std::make_unique<ash::ShelfModel>();
    model_observer_ = std::make_unique<TestShelfModelObserver>();
    model_->AddObserver(model_observer_.get());
    return CreateLauncherController();
  }

  void StartAppSyncService(const syncer::SyncDataList& init_sync_list) {
    app_service_->MergeDataAndStartSyncing(
        syncer::APP_LIST, init_sync_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>(),
        std::make_unique<syncer::SyncErrorFactoryMock>());
    EXPECT_EQ(init_sync_list.size(), app_service_->sync_items().size());
  }

  void StopAppSyncService() { app_service_->StopSyncing(syncer::APP_LIST); }

  sync_preferences::PrefModelAssociator* GetPrefSyncService() {
    sync_preferences::PrefServiceSyncable* pref_sync =
        profile()->GetTestingPrefService();
    sync_preferences::PrefModelAssociator* pref_sync_service =
        reinterpret_cast<sync_preferences::PrefModelAssociator*>(
            pref_sync->GetSyncableService(syncer::PREFERENCES));
    return pref_sync_service;
  }

  void StartPrefSyncService(const syncer::SyncDataList& init_sync_list) {
    syncer::SyncMergeResult r = GetPrefSyncService()->MergeDataAndStartSyncing(
        syncer::PREFERENCES, init_sync_list,
        std::make_unique<syncer::FakeSyncChangeProcessor>(),
        std::make_unique<syncer::SyncErrorFactoryMock>());
    EXPECT_FALSE(r.error().IsSet());
  }

  void StartPrefSyncServiceForPins(const base::ListValue& init_value) {
    syncer::SyncDataList init_sync_list;
    std::string serialized;
    JSONStringValueSerializer json(&serialized);
    json.Serialize(init_value);
    sync_pb::EntitySpecifics one;
    sync_pb::PreferenceSpecifics* pref_one = one.mutable_preference();
    pref_one->set_name(prefs::kPinnedLauncherApps);
    pref_one->set_value(serialized);
    init_sync_list.push_back(
        syncer::SyncData::CreateRemoteData(1, one, base::Time()));
    StartPrefSyncService(init_sync_list);
  }

  void StopPrefSyncService() {
    GetPrefSyncService()->StopSyncing(syncer::PREFERENCES);
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

  void InsertPrefValue(base::ListValue* pref_value,
                       int index,
                       const std::string& extension_id) {
    auto entry = std::make_unique<base::DictionaryValue>();
    entry->SetString(kPinnedAppsPrefAppIDPath, extension_id);
    pref_value->Insert(index, std::move(entry));
  }

  void InsertRemoveAllPinsChange(syncer::SyncChangeList* list) {
    for (const auto& sync_peer : app_service_->sync_items()) {
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
    app_service_->ProcessSyncChanges(FROM_HERE, sync_list);
  }

  void SendPinChanges(const syncer::SyncChangeList& sync_list,
                      bool reset_pin_model) {
    if (!reset_pin_model) {
      app_service_->ProcessSyncChanges(FROM_HERE, sync_list);
    } else {
      syncer::SyncChangeList combined_sync_list;
      InsertRemoveAllPinsChange(&combined_sync_list);
      combined_sync_list.insert(combined_sync_list.end(), sync_list.begin(),
                                sync_list.end());
      app_service_->ProcessSyncChanges(FROM_HERE, combined_sync_list);
    }
  }

  // Set the index at which the chrome icon should be.
  void SetShelfChromeIconIndex(int index) {
    DCHECK(
        app_service_->GetPinPosition(extension_misc::kChromeAppId).IsValid());
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
    app_service_->ProcessSyncChanges(FROM_HERE, sync_list);
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
          } else if (app == extension3_->id()) {
            result += "app3";
          } else if (app == extension4_->id()) {
            result += "app4";
          } else if (app == extension5_->id()) {
            result += "app5";
          } else if (app == extension6_->id()) {
            result += "app6";
          } else if (app == extension7_->id()) {
            result += "app7";
          } else if (app == extension8_->id()) {
            result += "app8";
          } else if (app == extension_platform_app_->id()) {
            result += "platform_app";
          } else {
            result += "unknown";
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
          } else if (app == extension3_->id()) {
            result += "App3";
          } else if (app == extension4_->id()) {
            result += "App4";
          } else if (app == extension5_->id()) {
            result += "App5";
          } else if (app == extension6_->id()) {
            result += "App6";
          } else if (app == extension7_->id()) {
            result += "App7";
          } else if (app == extension8_->id()) {
            result += "App8";
          } else if (app == extension_platform_app_->id()) {
            result += "Platform_App";
          } else if (app == arc_support_host_->id()) {
            result += "Play Store";
          } else if (app == crostini::kCrostiniTerminalId) {
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
            if (!arc_app_found)
              result += "unknown";
          }
          break;
        }
        case ash::TYPE_BROWSER_SHORTCUT:
          result += "Chrome";
          break;
        case ash::TYPE_APP_LIST:
          result += "AppList";
          break;
        case ash::TYPE_BACK_BUTTON:
          result += "Back";
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
    arc_test_.app_instance()->RefreshAppList();
    arc_test_.app_instance()->SendRefreshAppList(arc_test_.fake_apps());
  }

  void SendListOfArcShortcuts() {
    arc_test_.app_instance()->SendInstallShortcuts(arc_test_.fake_shortcuts());
  }

  void UninstallArcApps() {
    arc_test_.app_instance()->RefreshAppList();
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
    params.context = ash_test_helper()->CurrentContext();
    views::Widget* widget = new views::Widget();
    widget->Init(params);
    // Set ARC id before showing the window to be recognized in
    // ArcAppWindowLauncherController.
    exo::ShellSurface::SetApplicationId(widget->GetNativeWindow(),
                                        window_app_id);
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
    return app_id;
  }

  void NotifyOnTaskCreated(const arc::mojom::AppInfo& appinfo,
                           int32_t task_id) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskCreated(task_id, appinfo.package_name, appinfo.activity,
                         appinfo.name, std::string());
  }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension_chrome_;
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  scoped_refptr<Extension> extension5_;
  scoped_refptr<Extension> extension6_;
  scoped_refptr<Extension> extension7_;
  scoped_refptr<Extension> extension8_;
  scoped_refptr<Extension> extension_platform_app_;
  scoped_refptr<Extension> arc_support_host_;

  ArcAppTest arc_test_;
  bool auto_start_arc_test_ = false;
  std::unique_ptr<TestChromeLauncherController> launcher_controller_;
  std::unique_ptr<TestShelfModelObserver> model_observer_;
  std::unique_ptr<ash::ShelfModel> model_;

  // |item_delegate_manager_| owns |test_controller_|.
  ash::ShelfItemDelegate* test_controller_ = nullptr;

  extensions::ExtensionService* extension_service_ = nullptr;

  app_list::AppListSyncableService* app_service_ = nullptr;

 private:
  TestBrowserWindow* CreateTestBrowserWindowAura() {
    std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
    window->set_id(0);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window.get(), GetContext(),
                                          gfx::Rect(200, 200));

    return new TestBrowserWindowAura(std::move(window));
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerTest);
};

class ChromeLauncherControllerWithArcTest
    : public ChromeLauncherControllerTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ChromeLauncherControllerWithArcTest() { auto_start_arc_test_ = true; }
  ~ChromeLauncherControllerWithArcTest() override {}

  void SetUp() override {
    if (GetParam())
      arc::SetArcAlwaysStartForTesting(true);

    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    ChromeLauncherControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerWithArcTest);
};

INSTANTIATE_TEST_CASE_P(,
                        ChromeLauncherControllerWithArcTest,
                        ::testing::Bool());

// Watches WebContents and blocks until it is destroyed. This is needed for
// the destruction of a V2 application.
class WebContentsDestroyedWatcher : public content::WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {
    EXPECT_TRUE(web_contents != NULL);
  }
  ~WebContentsDestroyedWatcher() override {}

  // Waits until the WebContents is destroyed.
  void Wait() { message_loop_runner_->Run(); }

 private:
  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override { message_loop_runner_->Quit(); }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedWatcher);
};

// A V1 windowed application.
class V1App : public TestBrowserWindow {
 public:
  V1App(Profile* profile, const std::string& app_name) {
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        kCrxAppPrefix + app_name, true /* trusted_source */, gfx::Rect(),
        profile, true);
    params.window = this;
    browser_.reset(new Browser(params));
    chrome::AddTabAt(browser_.get(), GURL(), 0, true);
  }

  ~V1App() override {
    // close all tabs. Note that we do not need to destroy the browser itself.
    browser_->tab_strip_model()->CloseAllTabs();
  }

  Browser* browser() { return browser_.get(); }

 private:
  // The associated browser with this app.
  std::unique_ptr<Browser> browser_;

  DISALLOW_COPY_AND_ASSIGN(V1App);
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

  virtual ~V2App() {
    WebContentsDestroyedWatcher destroyed_watcher(window_->web_contents());
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

  DISALLOW_COPY_AND_ASSIGN(V2App);
};

// The testing framework to test multi profile scenarios.
class MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest
    : public ChromeLauncherControllerTest {
 protected:
  MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest() {}

  ~MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest() override {}

  // Overwrite the Setup function to enable multi profile and needed objects.
  void SetUp() override {
    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());

    // Initialize WallpaperControllerClient.
    wallpaper_controller_client_ =
        std::make_unique<WallpaperControllerClient>();
    wallpaper_controller_client_->InitForTesting(
        test_wallpaper_controller_.CreateInterfacePtr());

    // Initialize the rest.
    ChromeLauncherControllerTest::SetUp();

    // AvatarMenu and multiple profiles works after user logged in.
    profile_manager()->SetLoggedIn(true);

    // Ensure there are multiple profiles. User 0 is created during setup.
    CreateMultiUserProfile("user1");
    ASSERT_TRUE(SessionControllerClient::IsMultiProfileAvailable());
  }

  void TearDown() override {
    ChromeLauncherControllerTest::TearDown();
    user_manager_enabler_.reset();
    wallpaper_controller_client_.reset();

    // A Task is leaked if we don't destroy everything, then run the message
    // loop.
    base::RunLoop().RunUntilIdle();
  }

  // Creates a profile for a given |user_name|. Note that this class will keep
  // the ownership of the created object.
  TestingProfile* CreateMultiUserProfile(const std::string& user_name) {
    const std::string email_string = user_name + "@example.com";
    const AccountId account_id(AccountId::FromUserEmail(email_string));
    // Add a user to the fake user manager.
    GetFakeUserManager()->AddUser(account_id);

    GetFakeUserManager()->LoginUser(account_id);

    TestingProfile* profile =
        profile_manager()->CreateTestingProfile(account_id.GetUserEmail());
    EXPECT_TRUE(profile);

    // Remember the profile name so that we can destroy it upon destruction.
    created_profiles_[profile] = account_id.GetUserEmail();
    if (MultiUserWindowManager::GetInstance())
      MultiUserWindowManager::GetInstance()->AddUser(profile);
    if (launcher_controller_)
      launcher_controller_->AdditionalUserAddedToSession(profile);
    return profile;
  }

  // Switch to another user.
  void SwitchActiveUser(const AccountId& account_id) {
    GetFakeUserManager()->SwitchActiveUser(account_id);
    MultiUserWindowManagerChromeOS* manager =
        static_cast<MultiUserWindowManagerChromeOS*>(
            MultiUserWindowManager::GetInstance());
    manager->SetAnimationSpeedForTest(
        MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
    manager->ActiveUserChanged(GetFakeUserManager()->FindUser(account_id));
    launcher_controller_->browser_status_monitor_for_test()->ActiveUserChanged(
        account_id.GetUserEmail());

    for (const auto& controller :
         launcher_controller_->app_window_controllers_for_test()) {
      controller->ActiveUserChanged(account_id.GetUserEmail());
    }
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

  DISALLOW_COPY_AND_ASSIGN(
      MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest);
};

class ChromeLauncherControllerMultiProfileWithArcTest
    : public MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  ChromeLauncherControllerMultiProfileWithArcTest() {
    auto_start_arc_test_ = true;
  }
  ~ChromeLauncherControllerMultiProfileWithArcTest() override {}

  void SetUp() override {
    if (GetParam())
      arc::SetArcAlwaysStartForTesting(true);
    MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerMultiProfileWithArcTest);
};

INSTANTIATE_TEST_CASE_P(,
                        ChromeLauncherControllerMultiProfileWithArcTest,
                        ::testing::Bool());

TEST_F(ChromeLauncherControllerTest, DefaultApps) {
  InitLauncherController();
  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Back, AppList, Chrome, App3", GetPinnedAppStatus());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
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
  extension_service_->AddExtension(extension3_.get());

  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, arc_app_id1);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, arc_app_id2);
  InsertAddPinChange(&sync_list, 4, extension3_->id());
  SendPinChanges(sync_list, true);
  SetShelfChromeIconIndex(1);

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("Back, AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());

  // Persist pin state, we don't have active pin for ARC apps yet, but pin
  // model should have it.
  syncer::SyncDataList copy_sync_list =
      app_service_->GetAllSyncData(syncer::APP_LIST);

  ResetLauncherController();
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_service_->sync_items().size());

  // Move to ARC enabled platform, restart syncing with stored data.
  StartAppSyncService(copy_sync_list);
  RecreateLauncherController()->Init();

  // Pins must be automatically updated.
  SendListOfArcApps();
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));

  EXPECT_EQ("Back, AppList, App1, Chrome, Fake App 0, App2, Fake App 1, App3",
            GetPinnedAppStatus());

  // Now move pins on ARC enabled platform.
  model_->Move(2, 5);
  model_->Move(4, 2);
  model_->Move(4, 6);
  model_->Move(5, 3);
  EXPECT_EQ("Back, AppList, App2, Fake App 1, Chrome, App1, Fake App 0, App3",
            GetPinnedAppStatus());

  copy_sync_list = app_service_->GetAllSyncData(syncer::APP_LIST);

  ResetLauncherController();
  ResetPinModel();

  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_service_->sync_items().size());

  // Move back to ARC disabled platform.
  // TODO(victorhsieh): Implement opt-out.
  if (arc::ShouldArcAlwaysStart())
    return;
  EnablePlayStore(false);
  StartAppSyncService(copy_sync_list);
  RecreateLauncherController()->Init();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("Back, AppList, App2, Chrome, App1, App3", GetPinnedAppStatus());

  // Now move/remove pins on ARC disabled platform.
  model_->Move(5, 3);
  launcher_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("Back, AppList, App3, Chrome, App1", GetPinnedAppStatus());
  EnablePlayStore(true);

  SendListOfArcApps();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id1));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("Back, AppList, Fake App 1, App3, Chrome, App1, Fake App 0",
            GetPinnedAppStatus());
}

// Ensure correct merging of policy pinned apps and user pinned apps.
TEST_F(ChromeLauncherControllerTest, MergePolicyAndUserPrefPinnedApps) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());
  extension_service_->AddExtension(extension5_.get());
  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  base::ListValue policy_value;
  // extension 2 4 are pinned by policy
  InsertPrefValue(&policy_value, 0, extension2_->id());
  InsertPrefValue(&policy_value, 1, extension4_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  // 2 is not pinned as it's not installed
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
  // install extension 2 and check
  extension_service_->AddExtension(extension2_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));

  // Check user can manually pin or unpin these apps
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension1_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension2_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension3_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension4_->id(), profile()));

  // Check the order of shelf pinned apps
  EXPECT_EQ("Back, AppList, App2, App4, App1, Chrome, App3",
            GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsReverseOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  extension_service_->AddExtension(extension3_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("Back, AppList, Chrome, App3", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("Back, AppList, Chrome, App2, App3", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrderChromeMoved) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, extension3_->id());
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("Back, AppList, App1, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Back, AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsResyncOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension2_->id());
  InsertAddPinChange(&sync_list0, 2, extension3_->id());
  SendPinChanges(sync_list0, true);

  // The shelf layout has always one static item at the beginning (App List).
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ("Back, AppList, Chrome, App2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension3_->id());
  InsertAddPinChange(&sync_list1, 1, extension1_->id());
  InsertAddPinChange(&sync_list1, 2, extension2_->id());
  InsertAddPinChange(&sync_list1, 3, extension_misc::kChromeAppId);
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Back, AppList, App3, App1, App2, Chrome", GetPinnedAppStatus());

  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension2_->id());
  InsertAddPinChange(&sync_list2, 1, extension3_->id());
  InsertAddPinChange(&sync_list2, 2, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list2, 3, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("Back, AppList, App2, App3, Chrome, App1", GetPinnedAppStatus());

  // Check that the chrome icon can also be at the first possible location.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, extension3_->id());
  InsertAddPinChange(&sync_list3, 1, extension2_->id());
  InsertAddPinChange(&sync_list3, 2, extension1_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("Back, AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Check that unloading of extensions works as expected.
  extension_service_->UnloadExtension(extension1_->id(),
                                      UnloadedExtensionReason::UNINSTALL);
  EXPECT_EQ("Back, AppList, Chrome, App3, App2", GetPinnedAppStatus());

  extension_service_->UnloadExtension(extension2_->id(),
                                      UnloadedExtensionReason::UNINSTALL);
  EXPECT_EQ("Back, AppList, Chrome, App3", GetPinnedAppStatus());

  // Check that an update of an extension does not crash the system.
  extension_service_->UnloadExtension(extension3_->id(),
                                      UnloadedExtensionReason::UPDATE);
  EXPECT_EQ("Back, AppList, Chrome, App3", GetPinnedAppStatus());
}

// Test the V1 app interaction flow: run it, activate it, close it.
TEST_F(ChromeLauncherControllerTest, V1AppRunActivateClose) {
  InitLauncherController();
  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should create a new shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running again should have no effect.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);

  // Reporting that the app is closed should remove its shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed again should have no effect.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(3, model_->item_count());
}

// Test the V1 app interaction flow: pin it, run it, close it, unpin it.
TEST_F(ChromeLauncherControllerTest, V1AppPinRunCloseUnpin) {
  InitLauncherController();
  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should create a new shelf item.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should remove its shelf item.
  launcher_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Test the V1 app interaction flow: run it, pin it, close it, unpin it.
TEST_F(ChromeLauncherControllerTest, V1AppRunPinCloseUnpin) {
  InitLauncherController();

  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should create a new shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should just update the existing item.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should remove its shelf item.
  launcher_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Test the V1 app interaction flow: pin it, run it, unpin it, close it.
TEST_F(ChromeLauncherControllerTest, V1AppPinRunUnpinClose) {
  InitLauncherController();

  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should create a new shelf item.
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should just update the existing item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should just update the existing item.
  launcher_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[3].status);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should remove its shelf item.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            launcher_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Ensure unpinned V1 app ordering is properly restored after user changes.
TEST_F(ChromeLauncherControllerTest, CheckRunningV1AppOrder) {
  InitLauncherController();

  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());

  // Add a few running applications.
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_RUNNING);
  launcher_controller_->SetV1AppStatus(extension2_->id(), ash::STATUS_RUNNING);
  launcher_controller_->SetV1AppStatus(extension3_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(6, model_->item_count());
  // Note that this not only checks the order of applications but also the
  // running type.
  EXPECT_EQ("Back, AppList, Chrome, app1, app2, app3", GetPinnedAppStatus());

  // Remember the current order of applications for the current user.
  const AccountId& current_account_id =
      multi_user_util::GetAccountIdFromProfile(profile());
  RememberUnpinnedRunningApplicationOrder();

  // Switch some items and check that restoring a user which was not yet
  // remembered changes nothing.
  model_->Move(3, 4);
  EXPECT_EQ("Back, AppList, Chrome, app2, app1, app3", GetPinnedAppStatus());
  const AccountId second_fake_account_id(
      AccountId::FromUserEmail("second-fake-user@fake.com"));
  RestoreUnpinnedRunningApplicationOrder(second_fake_account_id);
  EXPECT_EQ("Back, AppList, Chrome, app2, app1, app3", GetPinnedAppStatus());

  // Restoring the stored user should however do the right thing.
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Back, AppList, Chrome, app1, app2, app3", GetPinnedAppStatus());

  // Switch again some items and even delete one - making sure that the missing
  // item gets properly handled.
  model_->Move(4, 5);
  launcher_controller_->SetV1AppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ("Back, AppList, Chrome, app3, app2", GetPinnedAppStatus());
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Back, AppList, Chrome, app2, app3", GetPinnedAppStatus());

  // Check that removing more items does not crash and changes nothing.
  launcher_controller_->SetV1AppStatus(extension2_->id(), ash::STATUS_CLOSED);
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Back, AppList, Chrome, app3", GetPinnedAppStatus());
  launcher_controller_->SetV1AppStatus(extension3_->id(), ash::STATUS_CLOSED);
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcDeferredLaunch) {
  InitLauncherController();

  const arc::mojom::AppInfo& app1 = arc_test_.fake_apps()[0];
  const arc::mojom::AppInfo& app2 = arc_test_.fake_apps()[1];
  const arc::mojom::AppInfo& app3 = arc_test_.fake_apps()[2];
  const arc::mojom::ShortcutInfo& shortcut = arc_test_.fake_shortcuts()[0];
  const std::string arc_app_id1 = ArcAppTest::GetAppId(app1);
  const std::string arc_app_id2 = ArcAppTest::GetAppId(app2);
  const std::string arc_app_id3 = ArcAppTest::GetAppId(app3);
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

  // Now spinner contollers should go away together with shelf items and ARC
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

  EXPECT_TRUE((request1->IsForApp(app2) && request2->IsForApp(app3)) ||
              (request1->IsForApp(app3) && request2->IsForApp(app2)));
  EXPECT_EQ(arc_test_.app_instance()->launch_intents()[0].c_str(),
            shortcut.intent_uri);
}

// Launch is canceled in case app becomes suspended.
TEST_P(ChromeLauncherControllerWithArcTest, ArcDeferredLaunchForSuspendedApp) {
  InitLauncherController();

  arc::mojom::AppInfo app = arc_test_.fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  // Register app first.
  arc_test_.app_instance()->RefreshAppList();
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
  arc_test_.app_instance()->RefreshAppList();
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
      std::make_unique<ExtensionAppWindowLauncherItemController>(shelf_id));
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

TEST_P(ChromeLauncherControllerMultiProfileWithArcTest, ArcMultiUser) {
  SendListOfArcApps();

  InitLauncherController();

  SetLauncherControllerHelper(new TestLauncherControllerHelper);

  // App1 exists all the time.
  // App2 is created when primary user is active and destroyed when secondary
  // user is active.
  // App3 created when secondary user is active.

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

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());
  launcher_controller_->PinAppWithID(arc_app_id);
  launcher_controller_->PinAppWithID(extension2_->id());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));

  EXPECT_EQ("Back, AppList, Chrome, App1, Fake App 0, App2",
            GetPinnedAppStatus());
  // In opt-out mode, only system apps are available and can't be uninstalled.
  // Skip the rest of the test.
  if (arc::ShouldArcAlwaysStart())
    return;
  UninstallArcApps();
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
  SendListOfArcApps();
  EXPECT_FALSE(launcher_controller_->IsAppPinned(arc_app_id));
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());

  // Opt-Out/Opt-In remove item from the shelf.
  launcher_controller_->PinAppWithID(arc_app_id);
  EXPECT_EQ("Back, AppList, Chrome, App1, App2, Fake App 0",
            GetPinnedAppStatus());
  EnablePlayStore(false);
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
  EnablePlayStore(true);
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
  SendListOfArcApps();
  EXPECT_EQ("Back, AppList, Chrome, App1, App2, Fake App 0",
            GetPinnedAppStatus());
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
  EXPECT_EQ("Back, AppList, Chrome, App1, Fake App 1, App2, Fake App 0",
            GetPinnedAppStatus());

  // TODO(victorhsieh): Implement opt-out.
  if (arc::ShouldArcAlwaysStart())
    return;
  EnablePlayStore(false);

  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
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

  EXPECT_EQ("Back, AppList, Chrome, App1, Fake App 1, App2, Fake App 0",
            GetPinnedAppStatus());
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcCustomAppIcon) {
  InitLauncherController();

  TestShelfController* shelf_controller =
      launcher_controller_->test_shelf_controller();
  ASSERT_TRUE(shelf_controller);

  // Wait until other apps are updated to avoid race condition while accessing
  // last updated item.
  base::RunLoop().RunUntilIdle();

  // Register fake ARC apps.
  SendListOfArcApps();
  // Use first fake ARC app for testing.
  const arc::mojom::AppInfo& app = arc_test_.fake_apps()[0];
  const std::string arc_app_id = ArcAppTest::GetAppId(app);

  // Generate icon for the testing app and use compressed png content as test
  // input. Take shortcut to separate from default app icon.
  std::string png_data;
  EXPECT_TRUE(arc_test_.app_instance()->GenerateIconResponse(
      extension_misc::EXTENSION_ICON_SMALL, false /* app_icon */, &png_data));
  EXPECT_FALSE(png_data.empty());
  // Some input that represents invalid png content.
  std::string invalid_png_data("aaaaaa");

  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  views::Widget* window1 = CreateArcWindow(window_app_id1);
  ASSERT_TRUE(window1 && window1->GetNativeWindow());
  arc_test_.app_instance()->SendTaskCreated(1, app, std::string());

  views::Widget* window2 = CreateArcWindow(window_app_id2);
  ASSERT_TRUE(window2 && window2->GetNativeWindow());
  arc_test_.app_instance()->SendTaskCreated(2, app, std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(arc_app_id)));
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(ash::ShelfID(arc_app_id));
  ASSERT_TRUE(item_delegate);
  base::RunLoop().RunUntilIdle();
  const SkBitmap default_icon = shelf_controller->GetLastItemImage();

  // No custom icon set. Acitivating windows should not change icon.
  window1->Activate();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon,
                                         shelf_controller->GetLastItemImage()));
  window2->Activate();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon,
                                         shelf_controller->GetLastItemImage()));

  // Set custom icon on active item. Icon should change to custom.
  arc_test_.app_instance()->SendTaskDescription(2, std::string(), png_data);
  const SkBitmap custom_icon = shelf_controller->GetLastItemImage();
  EXPECT_FALSE(gfx::test::AreBitmapsEqual(default_icon, custom_icon));

  // Switch back to the item without custom icon. Icon should be changed to
  // default.
  window1->Activate();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon,
                                         shelf_controller->GetLastItemImage()));

  // Test that setting an invalid icon should not change custom icon.
  arc_test_.app_instance()->SendTaskDescription(1, std::string(), png_data);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon,
                                         shelf_controller->GetLastItemImage()));
  arc_test_.app_instance()->SendTaskDescription(1, std::string(),
                                                invalid_png_data);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon,
                                         shelf_controller->GetLastItemImage()));

  // Check window removing with active custom icon. Reseting custom icon of
  // inactive window doesn't reset shelf icon.
  arc_test_.app_instance()->SendTaskDescription(2, std::string(),
                                                std::string());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon,
                                         shelf_controller->GetLastItemImage()));
  // Set custom icon back to validate closing active window later.
  arc_test_.app_instance()->SendTaskDescription(2, std::string(), png_data);
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon,
                                         shelf_controller->GetLastItemImage()));

  // Reseting custom icon of active window resets shelf icon.
  arc_test_.app_instance()->SendTaskDescription(1, std::string(),
                                                std::string());
  // Wait for default icon load.
  shelf_controller->WaitForUpdates(ui::GetSupportedScaleFactors().size());
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(default_icon,
                                         shelf_controller->GetLastItemImage()));
  window1->CloseNow();
  EXPECT_TRUE(gfx::test::AreBitmapsEqual(custom_icon,
                                         shelf_controller->GetLastItemImage()));
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcWindowPackageName) {
  InitLauncherController();
  SendListOfArcApps();

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
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitch) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  EXPECT_EQ(3, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, kGmailUrl));
    EXPECT_EQ(4, model_->item_count());

    // After switching to a second user the item should be gone.
    std::string user2 = "user2";
    TestingProfile* profile2 = CreateMultiUserProfile(user2);
    const AccountId account_id2(
        multi_user_util::GetAccountIdFromProfile(profile2));
    const AccountId account_id(
        multi_user_util::GetAccountIdFromProfile(profile()));
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());

    // After switching back the item should be back.
    SwitchActiveUser(account_id);
    EXPECT_EQ(4, model_->item_count());
    // Note we destroy now the gmail app with the closure end.
  }
  EXPECT_EQ(3, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
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
    EXPECT_EQ(3, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(4, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(account_id);
    EXPECT_EQ(3, model_->item_count());
    // Note: the closure ends and the browser will go away.
  }
  EXPECT_EQ(3, model_->item_count());
  SwitchActiveUser(account_id2);
  EXPECT_EQ(3, model_->item_count());
  SwitchActiveUser(account_id);
  EXPECT_EQ(3, model_->item_count());
}

// Check edge case where a visiting V1 app gets closed (crbug.com/321374).
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1CloseOnVisitingDesktop) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  MultiUserWindowManager* manager = MultiUserWindowManager::GetInstance();

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
    EXPECT_EQ(4, model_->item_count());

    // Transfer the app to the other screen and switch users.
    manager->ShowWindowForUser(v1_app->browser()->window()->GetNativeWindow(),
                               account_id2);
    EXPECT_EQ(4, model_->item_count());
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());
  }
  // After the app was destroyed, switch back. (which caused already a crash).
  SwitchActiveUser(account_id);

  // Create the same app again - which was also causing the crash.
  EXPECT_EQ(3, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, kGmailLaunchURL));
    EXPECT_EQ(4, model_->item_count());
  }
  SwitchActiveUser(account_id2);
  EXPECT_EQ(3, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
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
    EXPECT_EQ(3, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(account_id);
    EXPECT_EQ(4, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());
    v1_app.reset();
  }
  EXPECT_EQ(3, model_->item_count());
  SwitchActiveUser(account_id);
  EXPECT_EQ(3, model_->item_count());
  SwitchActiveUser(account_id2);
  EXPECT_EQ(3, model_->item_count());
}

// Check that activating an item which is on another user's desktop, will bring
// it back.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       TestLauncherActivationPullsBackWindow) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  MultiUserWindowManager* manager = MultiUserWindowManager::GetInstance();

  // Create a second test profile. The first is the one in profile() created in
  // BrowserWithTestWindowTest::SetUp().
  // No need to add the profiles to the MultiUserWindowManager here.
  // CreateMultiUserProfile() already does that.
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId current_user =
      multi_user_util::GetAccountIdFromProfile(profile());

  // Create a browser window with a native window for the current user.
  std::unique_ptr<Browser> browser(
      CreateBrowserWithTestWindowForProfile(profile()));
  BrowserWindow* browser_window = browser->window();
  aura::Window* window = browser_window->GetNativeWindow();
  manager->SetWindowOwner(window, current_user);

  // Check that an activation of the window on its owner's desktop does not
  // change the visibility to another user.
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(window, current_user));

  // Transfer the window to another user's desktop and check that activating it
  // does pull it back to that user.
  manager->ShowWindowForUser(
      window, multi_user_util::GetAccountIdFromProfile(profile2));
  EXPECT_FALSE(manager->IsWindowOnDesktopOfUser(window, current_user));
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(window, current_user));
}

// Check that a running windowed V1 application will be properly pinned and
// unpinned when the order gets changed through a profile / policy change.
TEST_F(ChromeLauncherControllerTest,
       RestoreDefaultAndRunningV1AppsResyncOrder) {
  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension3_->id());
  SendPinChanges(sync_list, true);

  // The shelf layout has always one static item at the beginning (App List).
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1", GetPinnedAppStatus());
  extension_service_->AddExtension(extension2_.get());
  // No new app icon will be generated.
  EXPECT_EQ("Back, AppList, Chrome, App1", GetPinnedAppStatus());

  // Set the app status as running, which will add an unpinned item.
  launcher_controller_->SetV1AppStatus(extension2_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ("Back, AppList, Chrome, App1, app2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1, App3, app2", GetPinnedAppStatus());

  // Now request to pin all items, which will pin the running unpinned items.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension3_->id());
  InsertAddPinChange(&sync_list1, 1, extension2_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Back, AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Removing the requirement for app 2 to be pinned should convert it back to
  // running but not pinned. It should move towards the end of the shelf, after
  // the pinned items, as determined by the |ShelfModel|'s weight system.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension3_->id());
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("Back, AppList, Chrome, App3, App1, app2", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  SendPinChanges(syncer::SyncChangeList(), true);
  EXPECT_EQ("Back, AppList, Chrome, App3, app2", GetPinnedAppStatus());
}

// Check that a running unpinned V2 application will be properly pinned and
// unpinned when the order gets changed through a profile / policy change.
TEST_F(ChromeLauncherControllerTest,
       RestoreDefaultAndRunningV2AppsResyncOrder) {
  InitLauncherController();
  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension3_->id());
  SendPinChanges(sync_list0, true);
  // The shelf layout has always one static item at the beginning (app List).
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1", GetPinnedAppStatus());
  extension_service_->AddExtension(extension_platform_app_.get());
  // No new app icon will be generated.
  EXPECT_EQ("Back, AppList, Chrome, App1", GetPinnedAppStatus());
  // Add an unpinned but running V2 app.
  CreateRunningV2App(extension_platform_app_->id());
  EXPECT_EQ("Back, AppList, Chrome, App1, *platform_app", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("Back, AppList, Chrome, App1, App3, *platform_app",
            GetPinnedAppStatus());

  // Now request to pin all items, which should pin the running unpinned item.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, extension3_->id());
  InsertAddPinChange(&sync_list1, 1, extension_platform_app_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Back, AppList, Chrome, App3, *Platform_App, App1",
            GetPinnedAppStatus());

  // Removing the requirement for app 2 to be pinned should convert it back to
  // running but not pinned. It should move towards the end of the shelf, after
  // the pinned items, as determined by the |ShelfModel|'s weight system.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension3_->id());
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("Back, AppList, Chrome, App3, App1, *platform_app",
            GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, extension3_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("Back, AppList, Chrome, App3, *platform_app", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned. Check that when
// switching between the two users, the state gets properly set.
TEST_F(ChromeLauncherControllerTest, UserSwitchIconRestore) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;

  SetUpMultiUserScenario(&user_a, &user_b);

  // Show user 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ(
      "Back, AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
      GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("Back, AppList, App6, App7, App8, Chrome", GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ(
      "Back, AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
      GetPinnedAppStatus());

  // Switch back to 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("Back, AppList, App6, App7, App8, Chrome", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. Check that when switching between the two users, the
// state gets properly set.
TEST_F(ChromeLauncherControllerTest, UserSwitchIconRestoreWithRunningV2App) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;

  SetUpMultiUserScenario(&user_a, &user_b);

  // Run the platform (V2) app.
  CreateRunningV2App(extension_platform_app_->id());

  // Show user 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ(
      "Back, AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
      GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("Back, AppList, App6, App7, App8, Chrome, *platform_app",
            GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  EXPECT_EQ(
      "Back, AppList, App1, App2, App3, *Platform_App, App4, App5, Chrome",
      GetPinnedAppStatus());

  // Switch back to 2.
  SendPinChanges(user_b, true);
  EXPECT_EQ("Back, AppList, App6, App7, App8, Chrome, *platform_app",
            GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. The chrome icon is not the last item in the list.
// Check that when switching between the two users, the state gets properly set.
// There was once a bug associated with this.
TEST_F(ChromeLauncherControllerTest,
       UserSwitchIconRestoreWithRunningV2AppChromeInMiddle) {
  syncer::SyncChangeList user_a;
  syncer::SyncChangeList user_b;
  SetUpMultiUserScenario(&user_a, &user_b);

  // Run the platform (V2) app.
  CreateRunningV2App(extension_platform_app_->id());

  // Show user 1.
  SendPinChanges(user_a, true);
  SetShelfChromeIconIndex(5);
  EXPECT_EQ(
      "Back, AppList, App1, App2, App3, *Platform_App, App4, Chrome, App5",
      GetPinnedAppStatus());

  // Show user 2.
  SendPinChanges(user_b, true);
  SetShelfChromeIconIndex(4);
  EXPECT_EQ("Back, AppList, App6, App7, App8, Chrome, *platform_app",
            GetPinnedAppStatus());

  // Switch back to 1.
  SendPinChanges(user_a, true);
  SetShelfChromeIconIndex(5);
  EXPECT_EQ(
      "Back, AppList, App1, App2, App3, *Platform_App, App4, Chrome, App5",
      GetPinnedAppStatus());
}

TEST_F(ChromeLauncherControllerTest, Policy) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  SendPinChanges(sync_list, true);

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  // Only |extension1_| should get pinned. |extension2_| is specified but not
  // installed, and |extension3_| is part of the default set, but that shouldn't
  // take effect when the policy override is in place.
  ASSERT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  ASSERT_EQ(5, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should not be reflected in the
  // launcher and pin will exist.
  policy_value.Remove(0, NULL);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());
  EXPECT_EQ(5, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      UnloadedExtensionReason::UNINSTALL);

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerTest, SyncUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

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
  InsertAddPinChange(&sync_list, 3, extension4_->id());
  SendPinChanges(sync_list, false);

  expected_pinned_apps.push_back(extension2_->id());
  expected_pinned_apps.push_back(extension4_->id());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.insert(expected_pinned_apps.begin() + 1,
                              extension3_->id());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertUpdatePinChange(&sync_list, 0, extension4_->id());
  InsertUpdatePinChange(&sync_list, 1, extension3_->id());
  InsertUpdatePinChange(&sync_list, 2, extension2_->id());
  SendPinChanges(sync_list, false);
  std::reverse(expected_pinned_apps.begin(), expected_pinned_apps.end());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Sending legacy sync change without pin info should not affect pin model.
  sync_list.clear();
  InsertLegacyPinChange(&sync_list, extension4_->id());
  SendPinChanges(sync_list, false);
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, extension4_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.erase(expected_pinned_apps.begin());
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, extension3_->id());
  InsertRemovePinChange(&sync_list, extension2_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.clear();
  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);
}

TEST_F(ChromeLauncherControllerTest, ImportLegacyPin) {
  // Note extension3_ is actually Gmail app which is default pinned.
  extension_service_->AddExtension(extension3_.get());
  InitLauncherController();

  // Default pins should contain Gmail. Pref is not syncing now.
  EXPECT_EQ("Back, AppList, Chrome, App3", GetPinnedAppStatus());

  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ("Back, AppList, Chrome, App3", GetPinnedAppStatus());

  // Initially pins are imported from legacy pref based model.
  base::ListValue value;
  InsertPrefValue(&value, 0, extension4_->id());
  InsertPrefValue(&value, 1, extension2_->id());
  InsertPrefValue(&value, 2, extension3_->id());
  StartPrefSyncServiceForPins(value);

  // Imported pins contain App2. App2 should be added to pins now.
  EXPECT_EQ("Back, AppList, Chrome, App2, App3", GetPinnedAppStatus());

  // extension4_ is in the pin list.
  extension_service_->AddExtension(extension4_.get());
  // extension5_ is not in the pin list.
  extension_service_->AddExtension(extension5_.get());
  EXPECT_EQ("Back, AppList, Chrome, App4, App2, App3", GetPinnedAppStatus());

  // Apply app sync, unpin one app and pin new one.
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, -1, extension3_->id());
  InsertAddPinChange(&sync_list, 3, extension5_->id());
  SendPinChanges(sync_list, false);
  EXPECT_EQ("Back, AppList, Chrome, App4, App2, App5", GetPinnedAppStatus());

  // At this point changing old pref based model does not affect pin model.
  InsertPrefValue(&value, 3, extension5_->id());
  StopPrefSyncService();
  StartPrefSyncServiceForPins(value);
  EXPECT_EQ("Back, AppList, Chrome, App4, App2, App5", GetPinnedAppStatus());

  // Next Chrome start should preserve pins.
  RecreateLauncherController()->Init();
  StopPrefSyncService();
  StartPrefSyncService(syncer::SyncDataList());
  EXPECT_EQ("Back, AppList, Chrome, App4, App2, App5", GetPinnedAppStatus());
}

TEST_F(ChromeLauncherControllerTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  InitLauncherController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, extension3_->id());
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_pinned_apps;
  expected_pinned_apps.push_back(extension1_->id());
  expected_pinned_apps.push_back(extension3_->id());
  std::vector<std::string> actual_pinned_apps;

  GetPinnedAppIds(launcher_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Install |extension2| and verify it shows up between the other two.
  extension_service_->AddExtension(extension2_.get());
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
  ash::MenuItemList items = controller->GetAppMenuItemsForTesting(item);
  ASSERT_EQ(expected_item_count, items.size());
  for (size_t i = 0; i < expected_item_count; i++)
    EXPECT_EQ(expected_item_titles[i], items[i]->label);
}

// Check that browsers get reflected correctly in the launcher menu.
TEST_F(ChromeLauncherControllerTest, BrowserMenuGeneration) {
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
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
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
  MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);
  CheckAppMenu(launcher_controller_.get(), item_browser, 1, one_menu_item2);

  chrome::CloseTab(browser2.get());
}

// Check that V1 apps are correctly reflected in the launcher menu using the
// refocus logic.
// Note that the extension matching logic is tested by the extension system
// and does not need a separate test here.
TEST_F(ChromeLauncherControllerTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  InitLauncherControllerWithBrowser();

  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension3_->id());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
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
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  chrome::NewTab(browser());

  // Installing |extension3_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension3_->id());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
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
  MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);

  CheckAppMenu(launcher_controller_.get(), item_browser, 0, nullptr);
  CheckAppMenu(launcher_controller_.get(), item_gmail, 0, nullptr);
}

// Check that V2 applications are creating items properly in the launcher when
// instantiated by the current user.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHandlingTwoUsers) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // Check that there is a browser, back button and a app launcher.
  EXPECT_EQ(3, model_->item_count());

  // Add a v2 app.
  V2App v2_app(profile(), extension1_.get());
  EXPECT_EQ(4, model_->item_count());

  // After switching users the item should go away.
  SwitchActiveUser(account_id2);
  EXPECT_EQ(3, model_->item_count());

  // And it should come back when switching back.
  SwitchActiveUser(account_id);
  EXPECT_EQ(4, model_->item_count());
}

// Check that V2 applications are creating items properly in edge cases:
// a background user creates a V2 app, gets active and inactive again and then
// deletes the app.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHandlingTwoUsersEdgeCases) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // Check that there is a browser, back button and a app launcher.
  EXPECT_EQ(3, model_->item_count());

  // Switch to an inactive user.
  SwitchActiveUser(account_id2);
  EXPECT_EQ(3, model_->item_count());

  // Add the v2 app to the inactive user and check that no item was added to
  // the launcher.
  {
    V2App v2_app(profile(), extension1_.get());
    EXPECT_EQ(3, model_->item_count());

    // Switch to the primary user and check that the item is shown.
    SwitchActiveUser(account_id);
    EXPECT_EQ(4, model_->item_count());

    // Switch to the second user and check that the item goes away - even if the
    // item gets closed.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());
  }

  // After the application was killed there should be still 2 items.
  EXPECT_EQ(3, model_->item_count());

  // Switching then back to the default user should not show the additional item
  // anymore.
  SwitchActiveUser(account_id);
  EXPECT_EQ(3, model_->item_count());
}

// Check that V2 applications will be made visible on the target desktop if
// another window of the same type got previously teleported there.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppFollowsTeleportedWindow) {
  InitLauncherController();
  MultiUserWindowManager* manager = MultiUserWindowManager::GetInstance();

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
  SwitchActiveUser(account_id1);

  // A v2 app for user #1 should be shown first and get hidden when switching to
  // desktop #2.
  V2App v2_app_1(profile1, extension1_.get());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(account_id2);
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());

  // Add a v2 app for user #1 while on desktop #2 should not be shown.
  V2App v2_app_2(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Teleport the app from user #1 to the desktop #2 should show it.
  manager->ShowWindowForUser(v2_app_1.window()->GetNativeWindow(), account_id2);
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
  V2App v2_app_4(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_3.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_4.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #3 and creating an app for user #1 should place it on
  // that user's desktop (#1).
  SwitchActiveUser(account_id3);
  V2App v2_app_5(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_5.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(account_id1);
  EXPECT_TRUE(v2_app_5.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #2, hiding the app window and creating an app should
  // teleport there automatically.
  SwitchActiveUser(account_id2);
  v2_app_1.window()->Hide();
  V2App v2_app_6(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_6.window()->GetNativeWindow()->IsVisible());
}

// Check that V2 applications hide correctly on the shelf when the app window
// is hidden.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHiddenWindows) {
  InitLauncherController();

  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUser(account_id);
  EXPECT_EQ(3, model_->item_count());

  V2App v2_app_1(profile(), extension1_.get());
  EXPECT_EQ(4, model_->item_count());
  {
    // Hide and show the app.
    v2_app_1.window()->Hide();
    EXPECT_EQ(3, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(4, model_->item_count());
  }
  {
    // Switch user, hide and show the app and switch back.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(3, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());

    SwitchActiveUser(account_id);
    EXPECT_EQ(4, model_->item_count());
  }
  {
    // Switch user, hide the app, switch back and then show it again.
    SwitchActiveUser(account_id2);
    EXPECT_EQ(3, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(3, model_->item_count());

    SwitchActiveUser(account_id);
    // The following expectation does not work in current impl. It was working
    // before because MultiUserWindowManagerChromeOS is not attached to user
    // associated with profile() hence not actually handling windows for the
    // user. It is a real bug. See http://crbug.com/693634
    // EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(4, model_->item_count());
  }
  {
    // Create a second app, hide and show it and then hide both apps.
    V2App v2_app_2(profile(), extension1_.get());
    EXPECT_EQ(4, model_->item_count());

    v2_app_2.window()->Hide();
    EXPECT_EQ(4, model_->item_count());

    v2_app_2.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(4, model_->item_count());

    v2_app_1.window()->Hide();
    v2_app_2.window()->Hide();
    EXPECT_EQ(3, model_->item_count());
  }
}

// Checks that the generated menu list properly activates items.
TEST_F(ChromeLauncherControllerTest, V1AppMenuExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  const ash::ShelfID gmail_id(extension3_->id());
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(kGmailUrl));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title2);

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
TEST_F(ChromeLauncherControllerTest, V1AppMenuDeletionExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  const ash::ShelfID gmail_id(extension3_->id());
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(kGmailUrl));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title2);

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
    ash::MenuItemList items =
        launcher_controller_->GetAppMenuItemsForTesting(item_gmail);
    item_delegate->ExecuteCommand(false, items[1]->command_id, ui::EF_NONE,
                                  display::kInvalidDisplayId);
    EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  }

  // Delete one tab through the menu item.
  {
    ash::MenuItemList items =
        launcher_controller_->GetAppMenuItemsForTesting(item_gmail);
    item_delegate->ExecuteCommand(false, items[1]->command_id,
                                  ui::EF_SHIFT_DOWN,
                                  display::kInvalidDisplayId);
    EXPECT_EQ(--tabs, browser()->tab_strip_model()->count());
  }
}

// Tests that the Gmail extension matches more than the app itself claims with
// the manifest file.
TEST_F(ChromeLauncherControllerTest, GmailMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check that the launcher controller does not recognize the running app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Installing |extension3_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension3_->id());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Check that it is now handled.
  EXPECT_TRUE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Check also that the app has detected that properly.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  EXPECT_EQ(1U,
            launcher_controller_->GetAppMenuItemsForTesting(item_gmail).size());
}

// Tests that the Gmail extension does not match the offline verison.
TEST_F(ChromeLauncherControllerTest, GmailOfflineMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kOfflineGmailUrl), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Installing |extension3_| pins it to the launcher.
  const ash::ShelfID gmail_id(extension3_->id());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // The content should not be able to be handled by the app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));
}

// Verify that the launcher item positions are persisted and restored.
TEST_F(ChromeLauncherControllerTest, PersistLauncherItemPositions) {
  InitLauncherController();

  TestLauncherControllerHelper* helper = new TestLauncherControllerHelper;
  SetLauncherControllerHelper(helper);

  EXPECT_EQ(ash::TYPE_BACK_BUTTON, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);

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

  EXPECT_EQ(ash::TYPE_BACK_BUTTON, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[4].type);

  // Move browser shortcut item from index 1 to index 3.
  model_->Move(2, 4);
  EXPECT_EQ(ash::TYPE_BACK_BUTTON, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[4].type);

  RecreateLauncherController();
  helper = new TestLauncherControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");
  SetLauncherControllerHelper(helper);
  launcher_controller_->Init();

  // Check ShelfItems are restored after resetting ChromeLauncherController.
  EXPECT_EQ(ash::TYPE_BACK_BUTTON, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[4].type);
}

// Verifies pinned apps are persisted and restored.
TEST_F(ChromeLauncherControllerTest, PersistPinned) {
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
  ASSERT_EQ(initial_size, model_->items().size());
}

TEST_F(ChromeLauncherControllerTest, MultipleAppIconLoaders) {
  InitLauncherControllerWithBrowser();

  const ash::ShelfID shelf_id1(extension1_->id());
  const ash::ShelfID shelf_id2(extension2_->id());
  const ash::ShelfID shelf_id3(extension3_->id());
  // app_icon_loader1 and app_icon_loader2 are owned by
  // ChromeLauncherController.
  TestAppIconLoaderImpl* app_icon_loader1 = new TestAppIconLoaderImpl();
  TestAppIconLoaderImpl* app_icon_loader2 = new TestAppIconLoaderImpl();
  app_icon_loader1->AddSupportedApp(shelf_id1.app_id);
  app_icon_loader2->AddSupportedApp(shelf_id2.app_id);
  SetAppIconLoaders(std::unique_ptr<AppIconLoader>(app_icon_loader1),
                    std::unique_ptr<AppIconLoader>(app_icon_loader2));

  launcher_controller_->CreateAppLauncherItem(
      std::make_unique<ExtensionAppWindowLauncherItemController>(shelf_id3),
      ash::STATUS_RUNNING);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(0, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CreateAppLauncherItem(
      std::make_unique<ExtensionAppWindowLauncherItemController>(shelf_id2),
      ash::STATUS_RUNNING);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  launcher_controller_->CreateAppLauncherItem(
      std::make_unique<ExtensionAppWindowLauncherItemController>(shelf_id1),
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
  InsertPrefValue(&policy_value, 0, appinfo.package_name);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));
}

TEST_P(ChromeLauncherControllerWithArcTest, ArcManaged) {
  // TODO(victorhsieh): Implement opt-in and opt-out.
  if (arc::ShouldArcAlwaysStart())
    return;

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
                   "Back, AppList, Chrome, Play Store");

  // ARC is managed and enabled, Play Store pin should be available.
  // Note: NEGOTIATING_TERMS_OF_SERVICE here means that opt-in flow starts.
  profile()->GetTestingPrefService()->SetManagedPref(
      arc::prefs::kArcEnabled, std::make_unique<base::Value>(true));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(true, true,
                   arc::ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
                   "Back, AppList, Chrome, Play Store");

  // ARC is managed and disabled, Play Store pin should not be available.
  profile()->GetTestingPrefService()->SetManagedPref(
      arc::prefs::kArcEnabled, std::make_unique<base::Value>(false));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(false, true, arc::ArcSessionManager::State::STOPPED,
                   "Back, AppList, Chrome");

  // ARC is not managed and disabled, Play Store pin should be available.
  profile()->GetTestingPrefService()->RemoveManagedPref(
      arc::prefs::kArcEnabled);
  base::RunLoop().RunUntilIdle();
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Back, AppList, Chrome, Play Store");

  // ARC is not managed and enabled, Play Store pin should be available.
  EnablePlayStore(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
                   "Back, AppList, Chrome, Play Store");

  // User disables ARC. ARC is not managed and disabled, Play Store pin should
  // be automatically removed.
  EnablePlayStore(false);
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Back, AppList, Chrome");

  // Even if re-enable it again, Play Store pin does not appear automatically.
  EnablePlayStore(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
                   "Back, AppList, Chrome");
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
  ash::MenuItemList items = item_delegate->GetAppMenuItems(0);
  ASSERT_EQ(items.size(), 2U);
  EXPECT_EQ(items[0]->command_id, 0);
  EXPECT_EQ(items[1]->command_id, 1);

  // Execute command to activate first window.
  item_delegate->ExecuteCommand(false, items[1]->command_id, ui::EF_NONE,
                                display::kInvalidDisplayId);
  EXPECT_TRUE(window1->IsActive());
  EXPECT_FALSE(window2->IsActive());

  // Selecting the item will show its application menu. It does not change the
  // active window.
  SelectItem(item_delegate);
  EXPECT_TRUE(window1->IsActive());
  EXPECT_FALSE(window2->IsActive());

  // Execute command to activate second window.
  item_delegate->ExecuteCommand(false, items[0]->command_id, ui::EF_NONE,
                                display::kInvalidDisplayId);
  EXPECT_FALSE(window1->IsActive());
  EXPECT_TRUE(window2->IsActive());
}

namespace {
class ChromeLauncherControllerArcDefaultAppsTest
    : public ChromeLauncherControllerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeLauncherControllerArcDefaultAppsTest() {}
  ~ChromeLauncherControllerArcDefaultAppsTest() override {}

 protected:
  void SetUp() override {
    if (GetParam())
      arc::SetArcAlwaysStartForTesting(true);
    ArcAppIcon::DisableSafeDecodingForTesting();
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeLauncherControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerArcDefaultAppsTest);
};

INSTANTIATE_TEST_CASE_P(,
                        ChromeLauncherControllerArcDefaultAppsTest,
                        ::testing::Bool());

class ChromeLauncherControllerPlayStoreAvailabilityTest
    : public ChromeLauncherControllerTest,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeLauncherControllerPlayStoreAvailabilityTest() = default;
  ~ChromeLauncherControllerPlayStoreAvailabilityTest() override = default;

 protected:
  void SetUp() override {
    if (GetParam())
      arc::SetArcAlwaysStartForTesting(false);
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeLauncherControllerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerPlayStoreAvailabilityTest);
};

INSTANTIATE_TEST_CASE_P(,
                        ChromeLauncherControllerPlayStoreAvailabilityTest,
                        ::testing::Bool());

}  // namespace

TEST_P(ChromeLauncherControllerArcDefaultAppsTest, DefaultApps) {
  arc_test_.SetUp(profile());
  InitLauncherController();

  TestShelfController* shelf_controller =
      launcher_controller_->test_shelf_controller();
  ASSERT_TRUE(shelf_controller);

  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EnablePlayStore(false);
  EXPECT_FALSE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  ASSERT_TRUE(prefs->GetAppIds().size());

  const std::string app_id =
      ArcAppTest::GetAppId(arc_test_.fake_default_apps()[0]);
  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(app_id)));
  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                             arc::UserInteractionType::NOT_USER_INITIATED));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(app_id)));

  // Stop ARC again. Shelf item should go away.
  EnablePlayStore(false);

  EXPECT_FALSE(launcher_controller_->GetItem(ash::ShelfID(app_id)));

  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                             arc::UserInteractionType::NOT_USER_INITIATED));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(app_id)));

  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);
  EXPECT_TRUE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
  // Wait for non-default item.
  shelf_controller->GetLastItemImage();
  EXPECT_FALSE(item_delegate->image_set_by_controller());

  const size_t update_count_before_launch = shelf_controller->updated_count();

  std::string window_app_id("org.chromium.arc.1");
  CreateArcWindow(window_app_id);
  arc_test_.app_instance()->SendTaskCreated(1, arc_test_.fake_default_apps()[0],
                                            std::string());
  EXPECT_TRUE(launcher_controller_->GetItem(ash::ShelfID(app_id)));
  // Refresh delegate, it was changed.
  item_delegate = model_->GetShelfItemDelegate(ash::ShelfID(app_id));
  ASSERT_TRUE(item_delegate);
  EXPECT_FALSE(
      launcher_controller_->GetShelfSpinnerController()->HasApp(app_id));
  // Default icon is not set.
  EXPECT_FALSE(item_delegate->image_set_by_controller());
  EXPECT_EQ(update_count_before_launch, shelf_controller->updated_count());

  item_delegate = model_->GetShelfItemDelegate(ash::ShelfID(app_id));
  // Shelf icon should not be overwritten by default app icon.
  EXPECT_FALSE(item_delegate->image_set_by_controller());
  EXPECT_EQ(update_count_before_launch, shelf_controller->updated_count());

  // Wait for real app icon image is decoded and set for shelf item.
  shelf_controller->GetLastItemImage();
  // Should have only one update for newly created window with no-icon set plus
  // update for each scale factor. That guarantees default icon was not set in
  // between.
  EXPECT_EQ(
      update_count_before_launch + 1 + ui::GetSupportedScaleFactors().size(),
      shelf_controller->updated_count());
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
TEST_F(ChromeLauncherControllerTest, CheckPositionConflict) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 1, extension3_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("Back, AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());

  const syncer::StringOrdinal position_chrome =
      app_service_->GetPinPosition(extension_misc::kChromeAppId);
  const syncer::StringOrdinal position_1 =
      app_service_->GetPinPosition(extension1_->id());
  const syncer::StringOrdinal position_2 =
      app_service_->GetPinPosition(extension2_->id());
  const syncer::StringOrdinal position_3 =
      app_service_->GetPinPosition(extension3_->id());
  EXPECT_TRUE(position_chrome.LessThan(position_1));
  EXPECT_TRUE(position_1.Equals(position_2));
  EXPECT_TRUE(position_2.Equals(position_3));

  // Move Chrome between App1 and App2.
  // Note, move target_index is in context when moved element is removed from
  // array first.
  model_->Move(2, 3);
  EXPECT_EQ("Back, AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());

  // Expect sync positions for only Chrome is updated and its resolution is
  // after all duplicated ordinals.
  EXPECT_TRUE(position_3.LessThan(
      app_service_->GetPinPosition(extension_misc::kChromeAppId)));
  EXPECT_TRUE(
      position_1.Equals(app_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(
      position_1.Equals(app_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(
      position_2.Equals(app_service_->GetPinPosition(extension2_->id())));
  EXPECT_TRUE(
      position_3.Equals(app_service_->GetPinPosition(extension3_->id())));
}

// Test the case when sync app is turned off and we need to use local copy to
// support user's pins.
TEST_F(ChromeLauncherControllerTest, SyncOffLocalUpdate) {
  InitLauncherController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension_misc::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());

  syncer::SyncDataList copy_sync_list =
      app_service_->GetAllSyncData(syncer::APP_LIST);

  app_service_->StopSyncing(syncer::APP_LIST);
  RecreateLauncherController()->Init();

  // Pinned state should not change.
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
  launcher_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("Back, AppList, Chrome, App1", GetPinnedAppStatus());

  // Resume syncing and sync information overrides local copy.
  StartAppSyncService(copy_sync_list);
  EXPECT_EQ("Back, AppList, Chrome, App1, App2", GetPinnedAppStatus());
}

// Ensure Ash and Chrome ShelfModel changes are synchronized correctly.
TEST_F(ChromeLauncherControllerTest, ShelfModelSync) {
  // ShelfModel creates an app list item, ShelfController creates its delegate.
  TestChromeLauncherController* launcher_controller =
      RecreateLauncherController();
  TestShelfController* shelf_controller =
      launcher_controller->test_shelf_controller();
  EXPECT_EQ(0u, shelf_controller->added_count());
  EXPECT_EQ(0u, shelf_controller->removed_count());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::kBackButtonId, model_->items()[0].id.app_id);
  EXPECT_EQ(ash::TYPE_BACK_BUTTON, model_->items()[0].type);
  EXPECT_EQ(ash::kAppListId, model_->items()[1].id.app_id);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[1].type);
  EXPECT_FALSE(model_->GetShelfItemDelegate(model_->items()[1].id));

  // Init creates the browser item and its delegate in Chrome's ShelfModel.
  // Ash's ShelfController should be notified about the update and delegate.
  launcher_controller->Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(1u, shelf_controller->added_count());
  EXPECT_EQ(0u, shelf_controller->removed_count());
  EXPECT_LE(0u, shelf_controller->updated_count());
  EXPECT_EQ(1u, shelf_controller->set_delegate_count());
  EXPECT_EQ(extension_misc::kChromeAppId, model_->items()[2].id.app_id);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(model_->GetShelfItemDelegate(model_->items()[2].id));
  EXPECT_FALSE(model_->items()[2].title.empty());

  // Add a shelf item using the ShelfController interface.
  ash::ShelfItem item;
  item.type = ash::TYPE_PINNED_APP;
  item.id = ash::ShelfID(kDummyAppId);
  shelf_controller->AddShelfItem(3, item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, shelf_controller->added_count());
  EXPECT_EQ(0u, shelf_controller->removed_count());

  // Remove a shelf item using the ShelfController interface.
  shelf_controller->RemoveShelfItem(item.id);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, shelf_controller->added_count());
  EXPECT_EQ(1u, shelf_controller->removed_count());

  // Add an item to Chrome's model; ShelfController should be notified.
  model_->Add(item);
  EXPECT_EQ(4, model_->item_count());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, shelf_controller->added_count());
  EXPECT_EQ(1u, shelf_controller->removed_count());

  // Remove an item from Chrome's model; ShelfController should be notified.
  model_->RemoveItemAt(3);
  EXPECT_EQ(3, model_->item_count());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, shelf_controller->added_count());
  EXPECT_EQ(2u, shelf_controller->removed_count());
}

// Ensure Ash and Chrome ShelfModel changes are synchronized correctly.
TEST_F(ChromeLauncherControllerTest, ShelfItemImageSync) {
  InitLauncherController();
  base::RunLoop().RunUntilIdle();
  TestShelfController* shelf_controller =
      launcher_controller_->test_shelf_controller();

  // Create a ShelfItem struct with a valid image icon.
  ash::ShelfItem item;
  item.type = ash::TYPE_PINNED_APP;
  item.id = ash::ShelfID(kDummyAppId);
  item.title = base::ASCIIToUTF16("Title");
  item.status = ash::STATUS_CLOSED;
  item.image = gfx::test::CreateImageSkia(1, 1);

  const size_t added_count = shelf_controller->added_count();
  const size_t updated_count = shelf_controller->updated_count();

  // Adding an item to Chrome's model notifies ShelfController with the image.
  launcher_controller_->shelf_model()->Add(item);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(added_count + 1, shelf_controller->added_count());
  EXPECT_EQ(updated_count, shelf_controller->updated_count());
  EXPECT_EQ(item.id, shelf_controller->last_item().id);
  EXPECT_FALSE(shelf_controller->last_item().image.isNull());

  // Updating the item's status notifies ShelfController with a null image.
  // This avoids some image transport costs for the unrelated item change.
  launcher_controller_->SetItemStatus(item.id, ash::STATUS_RUNNING);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(added_count + 1, shelf_controller->added_count());
  EXPECT_EQ(updated_count + 1, shelf_controller->updated_count());
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_controller->last_item().status);
  EXPECT_TRUE(shelf_controller->last_item().image.isNull());

  // Calling SetLauncherItemImage will pass the new image to ShelfController.
  launcher_controller_->SetLauncherItemImage(item.id,
                                             gfx::test::CreateImageSkia(2, 2));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(added_count + 1, shelf_controller->added_count());
  EXPECT_EQ(updated_count + 2, shelf_controller->updated_count());
  EXPECT_EQ(gfx::Size(2, 2), shelf_controller->last_item().image.size());

  // Calling OnAppImageUpdated will pass the new image to ShelfController.
  launcher_controller_->OnAppImageUpdated(item.id.app_id,
                                          gfx::test::CreateImageSkia(3, 3));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(added_count + 1, shelf_controller->added_count());
  EXPECT_EQ(updated_count + 3, shelf_controller->updated_count());
  EXPECT_EQ(gfx::Size(3, 3), shelf_controller->last_item().image.size());
}

// Test the Settings can be pinned and unpinned.
TEST_F(ChromeLauncherControllerTest, InternalAppPinUnpin) {
  InitLauncherController();
  // The model should only contain the browser shortcut, app list and back
  // button items.
  EXPECT_EQ(3, model_->item_count());

  const std::string app_id = app_list::kInternalAppIdSettings;
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));

  // Pin Settings.
  launcher_controller_->PinAppWithID(app_id);
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));

  // Unpin Settings.
  launcher_controller_->UnpinAppWithID(app_id);
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));
}

// Test that internal app can be added and removed on shelf.
TEST_F(ChromeLauncherControllerTest, InternalAppWindowRecreation) {
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
TEST_F(ChromeLauncherControllerTest, InternalAppWindowPropertyChanged) {
  InitLauncherController();

  // Only test the first internal app. The others should be the same.
  const auto& internal_app = app_list::GetInternalAppList(profile()).front();
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
  ~ChromeLauncherControllerDemoModeTest() override {}

  void SetUp() override {
    arc::SetArcAlwaysStartForTesting(true);

    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    // Fake online Demo Mode.
    session_manager_ = std::make_unique<session_manager::SessionManager>();
    chromeos::DBusThreadManager::GetSetterForTesting()->SetImageLoaderClient(
        std::make_unique<chromeos::FakeImageLoaderClient>());
    chromeos::DemoSession::SetDemoConfigForTesting(
        chromeos::DemoSession::DemoModeConfig::kOnline);
    ASSERT_TRUE(chromeos::DemoSession::StartIfInDemoMode());
    chromeos::DemoSession::Get()->SetOfflineResourcesLoadedForTesting(
        base::FilePath());

    ChromeLauncherControllerTest::SetUp();
  }

  void TearDown() override {
    ChromeLauncherControllerTest::TearDown();

    chromeos::DemoSession::ShutDownIfInitialized();
    chromeos::DemoSession::ResetDemoConfigForTesting();
    chromeos::DBusThreadManager::Shutdown();
  }

 private:
  std::unique_ptr<session_manager::SessionManager> session_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerDemoModeTest);
};

TEST_F(ChromeLauncherControllerDemoModeTest, PinnedAppsOnline) {
  net::test::MockNetworkChangeNotifier notifier;
  notifier.SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  InitLauncherControllerWithBrowser();

  base::ListValue policy_value;

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());

  arc::mojom::AppInfo appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(appinfo);

  arc::mojom::AppInfo online_only_appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.onlineonly");
  const std::string online_only_app_id =
      AddArcAppAndShortcut(online_only_appinfo);

  InsertPrefValue(&policy_value, 2, appinfo.package_name);
  InsertPrefValue(&policy_value, 3, online_only_appinfo.package_name);

  // If the device is offline, extension2 and onlineonly should be unpinned.
  chromeos::DemoSession::Get()->OverrideIgnorePinPolicyAppsForTesting(
      {extension2_->id(), online_only_appinfo.package_name});

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

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

TEST_F(ChromeLauncherControllerDemoModeTest, PinnedAppsOffline) {
  net::test::MockNetworkChangeNotifier notifier;
  notifier.SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  InitLauncherControllerWithBrowser();

  base::ListValue policy_value;

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());

  arc::mojom::AppInfo appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(appinfo);

  arc::mojom::AppInfo online_only_appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.onlineonly");
  const std::string online_only_app_id =
      AddArcAppAndShortcut(online_only_appinfo);

  InsertPrefValue(&policy_value, 2, appinfo.package_name);
  InsertPrefValue(&policy_value, 3, online_only_appinfo.package_name);

  // If the device is offline, extension2 and onlineonly should be unpinned.
  chromeos::DemoSession::Get()->OverrideIgnorePinPolicyAppsForTesting(
      {extension2_->id(), online_only_appinfo.package_name});

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, policy_value.CreateDeepCopy());

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

TEST_F(ChromeLauncherControllerTest, CrostiniTerminalPinUnpin) {
  InitLauncherController();

  const std::string app_id = crostini::kCrostiniTerminalId;
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));

  // Load pinned Terminal from prefs without Crostini UI being allowed
  base::ListValue value;
  InsertPrefValue(&value, 0, app_id);
  StartPrefSyncServiceForPins(value);
  EXPECT_EQ("Back, AppList, Chrome", GetPinnedAppStatus());

  // Reload after allowing Crostini UI
  crostini::CrostiniTestHelper test_helper(profile());
  StopPrefSyncService();
  StartPrefSyncServiceForPins(value);
  EXPECT_EQ("Back, AppList, Chrome, Terminal", GetPinnedAppStatus());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));

  // Unpin the Terminal
  launcher_controller_->UnpinAppWithID(app_id);
  EXPECT_EQ(3, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(app_id));

  // Pin Terminal again.
  launcher_controller_->PinAppWithID(app_id);
  EXPECT_EQ("Back, AppList, Chrome, Terminal", GetPinnedAppStatus());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[3].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[3].status);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(app_id));
}
