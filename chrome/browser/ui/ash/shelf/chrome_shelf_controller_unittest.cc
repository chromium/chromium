// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"

#include <stddef.h>

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/compatibility_mode.mojom.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_app_instance.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/public/cpp/multi_user_window_manager.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shelf/shelf_application_menu_model.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/policy_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_icon_loader.h"
#include "chrome/browser/ash/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ash/app_list/internal_app/internal_app_metadata.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/eche_app/app_id.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/apps/camera_app/camera_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/apps/os_flags_system_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate_map.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/multi_user/multi_profile_support.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/ash/shelf/app_service/app_service_app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/shelf/app_window_shelf_item_controller.h"
#include "chrome/browser/ui/ash/shelf/arc_app_window.h"
#include "chrome/browser/ui/ash/shelf/browser_status_monitor.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_test_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller_util.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/shelf/shelf_spinner_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/exo/shell_surface_util.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/services/app_service/public/cpp/stub_icon_loader.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "components/sync/test/sync_change_processor_wrapper_for_test.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/pref_model_associator.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
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
#include "ui/base/l10n/l10n_util.h"
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
#include "ui/gfx/skia_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

using base::ASCIIToUTF16;
using extensions::Extension;
using extensions::UnloadedExtensionReason;
using extensions::mojom::ManifestLocation;

namespace {

constexpr char kGmailUrl[] = "https://mail.google.com/mail/u";
constexpr char kGmailLaunchURL[] = "https://mail.google.com/mail/ca";
constexpr char kLaunchURL[] = "https://foo.example/";

// An extension prefix.
constexpr char kCrxAppPrefix[] = "_crx_";

// Dummy app id is used to put at least one pin record to prevent initializing
// pin model with preinstalled apps that can affect some tests.
constexpr char kDummyAppId[] = "dummyappid_dummyappid_dummyappid";
constexpr int kSizeInDip = extension_misc::EXTENSION_ICON_MEDIUM;

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::vector<arc::mojom::AppInfoPtr> GetArcSettingsAppInfo() {
  std::vector<arc::mojom::AppInfoPtr> apps;
  arc::mojom::AppInfoPtr app(arc::mojom::AppInfo::New());
  app->name = "settings";
  app->package_name = "com.android.settings";
  app->activity = "com.android.settings.Settings";
  app->sticky = true;
  apps.push_back(std::move(app));
  return apps;
}

int GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

bool ValidateImageIsFullyLoaded(const gfx::ImageSkia& image) {
  if (kSizeInDip != image.width() || kSizeInDip != image.height())
    return false;

  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  for (const auto scale_factor : scale_factors) {
    const float scale = ui::GetScaleForResourceScaleFactor(scale_factor);
    if (!image.HasRepresentation(scale))
      return false;

    const gfx::ImageSkiaRep& representation = image.GetRepresentation(scale);
    if (representation.is_null() ||
        representation.pixel_width() != base::ClampCeil(kSizeInDip * scale) ||
        representation.pixel_height() != base::ClampCeil(kSizeInDip * scale)) {
      return false;
    }
  }

  return true;
}

std::unique_ptr<syncer::SyncChangeProcessor> CreateSyncChangeProcessor(
    syncer::SyncChangeProcessor* processor) {
  if (processor) {
    return std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
        processor);
  }
  return std::make_unique<syncer::FakeSyncChangeProcessor>();
}

enum class PinAssertion {
  kNotPinned,
  kUnknownPinSource,
  kOnlyPolicyPinned,
  kUserPinned
};

using PinAssertionMap = base::flat_map<std::string, PinAssertion>;

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

// Fake AppServiceAppIconLoader to wait for icons loaded.
class FakeAppServiceAppIconLoader : public AppServiceAppIconLoader {
 public:
  FakeAppServiceAppIconLoader(Profile* profile,
                              int resource_size_in_dip,
                              AppIconLoaderDelegate* delegate)
      : AppServiceAppIconLoader(profile, resource_size_in_dip, delegate) {}

  void WaitForIconLoadded(
      const std::vector<std::string>& expected_icon_loaded_app_ids) {
    bool icon_loaded = true;
    for (const auto& app_id : expected_icon_loaded_app_ids) {
      if (!base::Contains(icon_loaded_app_ids_, app_id)) {
        icon_loaded = false;
        break;
      }
    }

    if (icon_loaded) {
      return;
    }

    expected_icon_loaded_app_ids_ = expected_icon_loaded_app_ids;
    base::RunLoop run_loop;
    icon_loaded_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // Callback invoked when the icon is loaded.
  void OnLoadIcon(const std::string& app_id,
                  apps::IconValuePtr icon_value) override {
    AppServiceAppIconLoader::OnLoadIcon(app_id, std::move(icon_value));
    icon_loaded_app_ids_.insert(app_id);

    bool icon_loaded = true;
    for (const auto& id : expected_icon_loaded_app_ids_) {
      if (!base::Contains(icon_loaded_app_ids_, id)) {
        icon_loaded = false;
        break;
      }
    }

    if (icon_loaded && !icon_loaded_callback_.is_null()) {
      std::move(icon_loaded_callback_).Run();
    }
  }

  base::OnceClosure icon_loaded_callback_;
  std::set<std::string> icon_loaded_app_ids_;
  std::vector<std::string> expected_icon_loaded_app_ids_;
};

// Test implementation of ShelfControllerHelper.
class TestShelfControllerHelper : public ShelfControllerHelper {
 public:
  TestShelfControllerHelper() : ShelfControllerHelper(nullptr) {}
  explicit TestShelfControllerHelper(Profile* profile)
      : ShelfControllerHelper(profile) {}
  TestShelfControllerHelper(const TestShelfControllerHelper&) = delete;
  TestShelfControllerHelper& operator=(const TestShelfControllerHelper&) =
      delete;
  ~TestShelfControllerHelper() override = default;

  // Sets the id for the specified tab.
  void SetAppID(content::WebContents* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(content::WebContents* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // ShelfControllerHelper:
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

// Test implementation of a V2 app shelf item controller.
class TestV2AppShelfItemController : public ash::ShelfItemDelegate {
 public:
  explicit TestV2AppShelfItemController(const std::string& app_id)
      : ash::ShelfItemDelegate(ash::ShelfID(app_id)) {}

  TestV2AppShelfItemController(const TestV2AppShelfItemController&) = delete;
  TestV2AppShelfItemController& operator=(const TestV2AppShelfItemController&) =
      delete;
  ~TestV2AppShelfItemController() override = default;

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
      ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
      ui::EventTimeForNow(), ui::EF_NONE, 0);
  delegate->ItemSelected(std::move(event), display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_UNKNOWN, base::DoNothing(),
                         base::NullCallback());
}

bool IsWindowOnDesktopOfUser(aura::Window* window,
                             const AccountId& account_id) {
  return MultiUserWindowManagerHelper::GetInstance()->IsWindowOnDesktopOfUser(
      window, account_id);
}

void UpdateAppRegistryCache(Profile* profile,
                            const std::string& app_id,
                            bool block,
                            bool pause,
                            std::optional<bool> show_in_shelf) {
  std::vector<apps::AppPtr> apps;
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, app_id);
  app->app_id = app_id;

  if (block)
    app->readiness = apps::Readiness::kDisabledByPolicy;
  else
    app->readiness = apps::Readiness::kReady;

  if (pause)
    app->paused = true;
  else
    app->paused = false;

  if (show_in_shelf.has_value())
    app->show_in_shelf = show_in_shelf;

  apps.push_back(std::move(app));

  apps::AppServiceProxyFactory::GetForProfile(profile)->OnApps(
      std::move(apps), apps::AppType::kChromeApp,
      false /* should_notify_initialized */);
}

}  // namespace

class ChromeShelfControllerTestBase : public BrowserWithTestWindowTest,
                                      public apps::AppRegistryCache::Observer {
 protected:
  ChromeShelfControllerTestBase()
      : BrowserWithTestWindowTest(Browser::TYPE_NORMAL),
        skip_preinstalled_web_app_startup_(
            web_app::PreinstalledWebAppManager::SkipStartupForTesting()) {}

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kUseFirstDisplayAsInternal);
    // Prevent preinstalled apps from installing so these tests can control when
    // they are installed.
    command_line->AppendSwitch(switches::kDisableDefaultApps);

    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    app_list::AppListSyncableServiceFactory::SetUseInTesting(true);

    BrowserWithTestWindowTest::SetUp();

    model_ = std::make_unique<ash::ShelfModel>();

    base::Value::Dict manifest;
    manifest.SetByDottedPath(extensions::manifest_keys::kName,
                             "launcher controller test extension");
    manifest.SetByDottedPath(extensions::manifest_keys::kVersion, "1");
    manifest.SetByDottedPath(extensions::manifest_keys::kManifestVersion, 2);
    manifest.SetByDottedPath(extensions::manifest_keys::kDescription,
                             "for testing pinned apps");
    // AppService checks the app's type. So set the
    // manifest_keys::kLaunchWebURL, so that the extension can get the type
    // from manifest value, and then AppService can get the extension's type.
    manifest.SetByDottedPath(extensions::manifest_keys::kLaunchWebURL,
                             kLaunchURL);

    base::Value::Dict manifest_platform_app;
    manifest_platform_app.SetByDottedPath(
        extensions::manifest_keys::kName,
        "launcher controller test platform app");
    manifest_platform_app.SetByDottedPath(extensions::manifest_keys::kVersion,
                                          "1");
    manifest_platform_app.SetByDottedPath(
        extensions::manifest_keys::kDescription,
        "for testing pinned platform apps");
    base::Value::List scripts;
    scripts.Append("main.js");
    manifest_platform_app.SetByDottedPath(
        extensions::manifest_keys::kPlatformAppBackgroundScripts,
        std::move(scripts));

    SyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildTestSyncService));

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    DCHECK(profile());
    extension_registry_ = extensions::ExtensionRegistry::Get(profile());
    app_service_test_.SetUp(profile());
    app_registry_cache_observer_.Observe(
        &(apps::AppServiceProxyFactory::GetForProfile(profile())
              ->AppRegistryCache()));

    if (auto_start_arc_test_)
      arc_test_.SetUp(profile());

    // Wait until |extension_system| is signaled as started.
    base::RunLoop run_loop;
    extension_system->ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();

    app_list_syncable_service_ =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    StartAppSyncService(app_list_syncable_service_->GetAllSyncDataForTesting());

    std::string error;
    extension_chrome_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, app_constants::kChromeAppId, &error);
    extension1_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", &error);
    extension2_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", &error);
    extension5_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, "cccccccccccccccccccccccccccccccc", &error);
    extension6_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, "dddddddddddddddddddddddddddddddd", &error);
    extension7_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", &error);
    extension8_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, "ffffffffffffffffffffffffffffffff", &error);
    extension_platform_app_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest_platform_app,
        Extension::NO_FLAGS, "gggggggggggggggggggggggggggggggg", &error);
    arc_support_host_ = Extension::Create(
        base::FilePath(), ManifestLocation::kUnpacked, manifest,
        Extension::NO_FLAGS, arc::kPlayStoreAppId, &error);
    extension_service_->AddExtension(extension_chrome_.get());

    if (StartWebAppProviderForMainProfile())
      StartWebAppProvider(profile());
  }

  virtual bool StartWebAppProviderForMainProfile() const { return true; }

  void StartWebAppProvider(Profile* profile) {
    auto* provider = web_app::FakeWebAppProvider::Get(profile);

    auto* system_web_app_manager = ash::TestSystemWebAppManager::Get(profile);

    provider->SetStartSystemOnStart(true);
    provider->Start();

    system_web_app_manager->ScheduleStart();

    base::RunLoop run_loop;
    provider->on_external_managers_synchronized().Post(FROM_HERE,
                                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  // Note that this resets previously installed SWAs.
  void InstallSystemWebApp(
      std::unique_ptr<ash::SystemWebAppDelegate> delegate) {
    auto* system_web_app_manager =
        ash::SystemWebAppManager::GetForTest(profile());

    ash::SystemWebAppDelegateMap swa_map;
    swa_map.emplace(delegate->GetType(), std::move(delegate));
    system_web_app_manager->SetSystemAppsForTesting(std::move(swa_map));
    system_web_app_manager->InstallSystemAppsForTesting();
  }

  ui::BaseWindow* GetLastActiveWindowForItemController(
      AppWindowShelfItemController* item_controller) {
    return item_controller->last_active_window_;
  }

  // Creates a running platform V2 app (not pinned) of type |app_id|.
  virtual void CreateRunningV2App(const std::string& app_id) {
    DCHECK(!test_controller_);
    // Change the created shelf item controller into a V2 app controller.
    std::unique_ptr<TestV2AppShelfItemController> controller =
        std::make_unique<TestV2AppShelfItemController>(app_id);
    test_controller_ = controller.get();
    ash::ShelfID id = shelf_controller_->CreateAppItem(
        std::move(controller), ash::STATUS_RUNNING, /*pinned=*/false);
    ASSERT_TRUE(IsPlatformApp(id));
  }

  // Sets the stage for a multi user test.
  virtual void SetUpMultiUserScenario(syncer::SyncChangeList* user_a,
                                      syncer::SyncChangeList* user_b) {
    InitShelfController();
    EXPECT_EQ("Chrome", GetPinnedAppStatus());

    // Set an empty pinned pref to begin with.
    syncer::SyncChangeList sync_list;
    InsertAddPinChange(&sync_list, 0, app_constants::kChromeAppId);
    SendPinChanges(sync_list, true);
    EXPECT_EQ("Chrome", GetPinnedAppStatus());

    // Assume all applications have been added already.
    AddWebApp(web_app::kGoogleDocsAppId);
    AddWebApp(web_app::kGmailAppId);
    extension_service_->AddExtension(extension1_.get());
    extension_service_->AddExtension(extension2_.get());
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
    InsertAddPinChange(user_a, 2, web_app::kGmailAppId);
    InsertAddPinChange(user_a, 3, extension_platform_app_->id());
    InsertAddPinChange(user_a, 4, web_app::kGoogleDocsAppId);
    InsertAddPinChange(user_a, 5, extension5_->id());
    InsertAddPinChange(user_a, 6, app_constants::kChromeAppId);

    // Set user b preferences.
    InsertAddPinChange(user_b, 0, extension6_->id());
    InsertAddPinChange(user_b, 1, extension7_->id());
    InsertAddPinChange(user_b, 2, extension8_->id());
    InsertAddPinChange(user_b, 3, app_constants::kChromeAppId);
  }

  void TearDown() override {
    app_registry_cache_observer_.Reset();
    arc_test_.TearDown();
    shelf_controller_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
    ash::ConciergeClient::Shutdown();
    app_list::AppListSyncableServiceFactory::SetUseInTesting(false);
  }

  std::unique_ptr<BrowserWindow> CreateBrowserWindow() override {
    return CreateTestBrowserWindowAura();
  }

  std::unique_ptr<Browser> CreateBrowserWithTestWindowForProfile(
      Profile* profile) {
    auto browser_window = CreateTestBrowserWindowAura();
    auto browser = CreateBrowser(profile, Browser::TYPE_NORMAL, false,
                                 browser_window.get());
    // Self deleting.
    new TestBrowserWindowOwner(std::move(browser_window));
    return browser;
  }

  // Create an uninitialized controller instance.
  ChromeShelfController* CreateShelfController() {
    shelf_controller_ =
        std::make_unique<ChromeShelfController>(profile(), model_.get());
    shelf_controller_->SetProfileForTest(profile());
    shelf_controller_->SetShelfControllerHelperForTest(
        std::make_unique<ShelfControllerHelper>(profile()));
    return shelf_controller_.get();
  }

  // Create and initialize the controller, owned by the test shell delegate.
  void InitShelfController() { CreateShelfController()->Init(); }

  // Create and initialize the controller; create a tab and show the browser.
  void InitShelfControllerWithBrowser() {
    InitShelfController();
    chrome::NewTab(browser());
    browser()->window()->Show();
  }

  // Destroy the controller instance and clear the local pointer.
  void ResetShelfController() {
    shelf_controller_.reset();
  }

  // Destroy and recreate the controller; clear and reinitialize the ShelfModel.
  // Returns a pointer to the uninitialized controller, owned by shell delegate.
  // TODO(msw): This does not accurately represent ChromeShelfController
  // lifetime or usage in production, and does not accurately simulate restarts.
  ChromeShelfController* RecreateShelfController() {
    // Destroy any existing controller first; only one may exist at a time.
    ResetShelfController();
    model_ = std::make_unique<ash::ShelfModel>();
    return CreateShelfController();
  }

  void StartAppSyncService(const syncer::SyncDataList& init_sync_list,
                           syncer::SyncChangeProcessor* processor = nullptr) {
    app_list_syncable_service_->MergeDataAndStartSyncing(
        syncer::APP_LIST, init_sync_list, CreateSyncChangeProcessor(processor));
    EXPECT_EQ(init_sync_list.size(),
              app_list_syncable_service_->sync_items().size());
  }

  void StopAppSyncService() {
    app_list_syncable_service_->StopSyncing(syncer::APP_LIST);
  }

  sync_preferences::PrefModelAssociator* GetPrefSyncService() {
    sync_preferences::PrefServiceSyncable* pref_sync =
        profile()->GetTestingPrefService();
    sync_preferences::PrefModelAssociator* pref_sync_service =
        static_cast<sync_preferences::PrefModelAssociator*>(
            pref_sync->GetSyncableService(syncer::OS_PREFERENCES));
    return pref_sync_service;
  }

  void StartPrefSyncService(const syncer::SyncDataList& init_sync_list) {
    std::optional<syncer::ModelError> error =
        GetPrefSyncService()->MergeDataAndStartSyncing(
            syncer::OS_PREFERENCES, init_sync_list,
            std::make_unique<syncer::FakeSyncChangeProcessor>());
    EXPECT_FALSE(error.has_value());
  }

  syncer::SyncData CreateStringPrefsSyncData(const std::string& pref_name,
                                             const std::string& pref_value) {
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_os_preference()->mutable_preference()->set_name(
        pref_name);
    specifics.mutable_os_preference()->mutable_preference()->set_value(
        base::StringPrintf("\"%s\"", pref_value.c_str()));
    return syncer::SyncData::CreateRemoteData(
        specifics, syncer::ClientTagHash::FromHashed("unused"));
  }

  void SetAppIconLoader(std::unique_ptr<AppIconLoader> loader) {
    std::vector<std::unique_ptr<AppIconLoader>> loaders;
    loaders.push_back(std::move(loader));
    shelf_controller_->SetAppIconLoadersForTest(loaders);
  }

  void SetAppIconLoaders(std::unique_ptr<AppIconLoader> loader1,
                         std::unique_ptr<AppIconLoader> loader2) {
    std::vector<std::unique_ptr<AppIconLoader>> loaders;
    loaders.push_back(std::move(loader1));
    loaders.push_back(std::move(loader2));
    shelf_controller_->SetAppIconLoadersForTest(loaders);
  }

  void SetShelfControllerHelper(ShelfControllerHelper* helper) {
    shelf_controller_->SetShelfControllerHelperForTest(
        base::WrapUnique<ShelfControllerHelper>(helper));
  }

  void AppendPrefValue(base::Value::List& pref_values,
                       std::string_view policy_id) {
    pref_values.Append(base::Value::Dict().Set(
        ChromeShelfPrefs::kPinnedAppsPrefAppIDKey, policy_id));
  }

  void AppendPrefValue(base::Value::List& pref_values,
                       ash::SystemWebAppType app_type) {
    AppendPrefValue(pref_values,
                    *apps_util::GetPolicyIdForSystemWebAppType(app_type));
  }

  void RemovePrefValue(base::Value::List& pref_values,
                       std::string_view policy_id) {
    pref_values.EraseIf([&policy_id](const auto& entry) {
      return *entry.GetDict().FindString(
                 ChromeShelfPrefs::kPinnedAppsPrefAppIDKey) == policy_id;
    });
  }

  void RemovePrefValue(base::Value::List& pref_values,
                       ash::SystemWebAppType app_type) {
    RemovePrefValue(pref_values,
                    *apps_util::GetPolicyIdForSystemWebAppType(app_type));
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
        app_list_syncable_service_->GetPinPosition(app_constants::kChromeAppId)
            .IsValid());
    syncer::StringOrdinal chrome_position;
    chrome_position = index == 0 ? GeneratePinPosition(0).CreateBefore()
                                 : GeneratePinPosition(index - 1).CreateBetween(
                                       GeneratePinPosition(index));

    syncer::SyncChangeList sync_list;
    sync_pb::EntitySpecifics specifics;
    sync_pb::AppListSpecifics* app_list_specifics =
        specifics.mutable_app_list();
    app_list_specifics->set_item_id(app_constants::kChromeAppId);
    app_list_specifics->set_item_type(sync_pb::AppListSpecifics::TYPE_APP);
    app_list_specifics->set_item_pin_ordinal(chrome_position.ToInternalValue());
    syncer::SyncData sync_data = syncer::SyncData::CreateLocalData(
        app_constants::kChromeAppId, "Test", specifics);
    sync_list.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE, sync_data));
    app_list_syncable_service_->ProcessSyncChanges(FROM_HERE, sync_list);
    content::RunAllTasksUntilIdle();
  }

  // Gets the IDs of the currently pinned app items.
  void GetPinnedAppIds(ChromeShelfController* controller,
                       std::vector<std::string>* app_ids) {
    app_ids->clear();
    for (const auto& item : model_->items()) {
      if (item.type == ash::TYPE_PINNED_APP)
        app_ids->push_back(item.id.app_id);
    }
  }

  // Get the setup of the currently shown shelf items in one string.
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
          if (IsPlatformApp(model_->items()[i].id)) {
            result += "*";
          }
          const std::string& app = model_->items()[i].id.app_id;
          EXPECT_FALSE(shelf_controller_->IsAppPinned(app));
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
          } else if (app == web_app::kGmailAppId) {
            result += "gmail";
          } else if (app == extension_platform_app_->id()) {
            result += "platform_app";
          } else if (app == web_app::kGoogleDocsAppId) {
            result += "doc";
          } else if (app == web_app::kYoutubeAppId) {
            result += "youtube";
          } else {
            result += app_service_test_.GetAppName(app);
          }
          break;
        }
        case ash::TYPE_PINNED_APP: {
          if (IsPlatformApp(model_->items()[i].id)) {
            result += "*";
          }
          const std::string& app = model_->items()[i].id.app_id;
          EXPECT_TRUE(shelf_controller_->IsAppPinned(app));
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
          } else if (app == web_app::kGmailAppId) {
            result += "Gmail";
          } else if (app == web_app::kGoogleCalendarAppId) {
            result += "Calendar";
          } else if (app == web_app::kGoogleDocsAppId) {
            result += "Doc";
          } else if (app == web_app::kMessagesAppId) {
            result += "Messages";
          } else if (app == web_app::kYoutubeAppId) {
            result += "Youtube";
          } else if (app == extension_platform_app_->id()) {
            result += "Platform_App";
          } else if (app == arc_support_host_->id()) {
            result += "Play Store";
          } else if (app == arc::kSettingsAppId) {
            result += "Android Settings";
          } else {
            bool arc_app_found = false;
            for (const auto& arc_app : arc_test_.fake_apps()) {
              if (app == ArcAppTest::GetAppId(*arc_app)) {
                result += arc_app->name;
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

  bool IsAppPolicyPinned(const std::string& app_id) {
    return GetPinnableForAppID(app_id, profile()) ==
           AppListControllerDelegate::PIN_FIXED;
  }

  // Returns the list containing app IDs of items shown in shelf. The order of
  // IDs matches the order of associated shelf items in the shelf model.
  std::vector<std::string> GetAppsShownInShelf() const {
    std::vector<std::string> app_ids;
    for (auto& item : model_->items())
      app_ids.push_back(item.id.app_id);
    return app_ids;
  }

  // Remember the order of unpinned but running applications for the current
  // user.
  void RememberUnpinnedRunningApplicationOrder() {
    shelf_controller_->RememberUnpinnedRunningApplicationOrder();
  }

  // Restore the order of running but unpinned applications for a given user.
  void RestoreUnpinnedRunningApplicationOrder(const AccountId& account_id) {
    shelf_controller_->RestoreUnpinnedRunningApplicationOrder(
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
        std::vector<arc::mojom::AppInfoPtr>());
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
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(5, 5, 20, 20);
    params.context = GetContext();
    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));
    // Set ARC id before showing the window to be recognized in
    // AppServiceAppWindowArcTracker.
    exo::SetShellApplicationId(widget->GetNativeWindow(), window_app_id);
    widget->Show();
    widget->Activate();
    return widget;
  }

  arc::mojom::AppInfoPtr CreateAppInfo(const std::string& name,
                                       const std::string& activity,
                                       const std::string& package_name) {
    auto appinfo = arc::mojom::AppInfo::New();
    appinfo->name = name;
    appinfo->package_name = package_name;
    appinfo->activity = activity;
    return appinfo;
  }

  std::string AddArcAppAndShortcut(const arc::mojom::AppInfo& app_info) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();

    std::optional<uint64_t> app_size_in_bytes;
    std::optional<uint64_t> data_size_in_bytes;

    if (!app_info.app_storage.is_null()) {
      app_size_in_bytes = app_info.app_storage->app_size_in_bytes;
      data_size_in_bytes = app_info.app_storage->data_size_in_bytes;
    }

    // Adding app to the prefs, and check that the app is accessible by id.
    prefs->AddAppAndShortcut(
        app_info.name, app_info.package_name, app_info.activity,
        std::string() /* intent_uri */, std::string() /* icon_resource_id */,
        app_info.version_name, false /* sticky */,
        true /* notifications_enabled */, true /* app_ready */,
        false /* suspended */, false /* shortcut */, true /* launchable */,
        false /* need_fixup */, ArcAppListPrefs::WindowLayout(),
        app_size_in_bytes, data_size_in_bytes, app_info.app_category);
    const std::string app_id =
        ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
    EXPECT_TRUE(prefs->GetApp(app_id));
    return app_id;
  }

  void NotifyOnTaskCreated(const arc::mojom::AppInfo& appinfo,
                           int32_t task_id) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskCreated(task_id, appinfo.package_name, appinfo.activity,
                         appinfo.name, std::string(), /*session_id=*/0);
  }

  // Creates a window with TYPE_APP shelf item type and the given app_id.
  views::Widget* CreateShelfAppWindow(const std::string& app_id) {
    views::Widget::InitParams params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.context = GetContext();
    params.bounds = gfx::Rect(5, 5, 20, 20);
    views::Widget* widget = new views::Widget();
    widget->Init(std::move(params));

    aura::Window* window = widget->GetNativeWindow();
    const ash::ShelfID shelf_id(app_id);
    window->SetProperty(ash::kShelfIDKey, shelf_id.Serialize());
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_APP);
    window->SetProperty(ash::kAppIDKey, app_id);

    widget->Show();
    widget->Activate();
    return widget;
  }

  void NotifyOnTaskDestroyed(int32_t task_id) {
    ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
    prefs->OnTaskDestroyed(task_id);
  }

  // Add extension.
  void AddExtension(const Extension* extension) {
    extension_service_->AddExtension(extension);
  }

  // Uninstall extension.
  void UninstallExtension(const std::string& extension_id,
                          extensions::UninstallReason reason) {
    extension_service_->UninstallExtension(extension_id, reason, nullptr);
  }

  // Unload extension.
  void UnloadExtension(const std::string& extension_id,
                       UnloadedExtensionReason reason) {
    extension_service_->UnloadExtension(extension_id, reason);
  }

  const GURL& GetWebAppUrl(const std::string& web_app_id) const {
    static const base::flat_map<std::string, GURL> web_app_id_to_start_url{
        {web_app::kGmailAppId,
         GURL("https://mail.google.com/mail/?usp=installed_webapp")},
        {web_app::kGoogleCalendarAppId,
         GURL("https://calendar.google.com/calendar/r")},
        {web_app::kGoogleDocsAppId,
         GURL("https://docs.google.com/document/?usp=installed_webapp")},
        {web_app::kMessagesAppId, GURL("https://messages.google.com/web/")},
        {web_app::kYoutubeAppId,
         GURL("https://www.youtube.com/?feature=ytca")}};

    DCHECK(base::Contains(web_app_id_to_start_url, web_app_id));
    return web_app_id_to_start_url.at(web_app_id);
  }

  void AddWebApp(const std::string& web_app_id) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GetWebAppUrl(web_app_id));

    webapps::AppId installed_app_id =
        web_app::test::InstallWebApp(profile(), std::move(web_app_info));

    ASSERT_EQ(installed_app_id, web_app_id);
    apps::AppReadinessWaiter(profile(), web_app_id).Await();
  }

  web_app::IsolatedWebAppUrlInfo AddIsolatedWebApp(const GURL& url) {
    web_app::AddDummyIsolatedAppToRegistry(
        profile(), url, "IWA",
        web_app::IsolationData::Builder(
            web_app::IwaStorageOwnedBundle{"", false}, base::Version("1.0.0"))
            .Build(),
        webapps::WebappInstallSource::IWA_EXTERNAL_POLICY);
    base::expected<web_app::IsolatedWebAppUrlInfo, std::string> url_info =
        web_app::IsolatedWebAppUrlInfo::Create(url);
    CHECK(url_info.has_value());
    apps::AppReadinessWaiter(profile(), url_info->app_id()).Await();
    return *url_info;
  }

  webapps::AppId InstallExternalWebApp(
      const GURL& start_url,
      const std::optional<GURL>& install_url = {}) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(start_url));
    web_app_info->install_url = GURL(install_url ? *install_url : start_url);
    const webapps::AppId expected_web_app_id = web_app::GenerateAppId(
        /*manifest_id=*/std::nullopt, web_app_info->start_url());
    webapps::AppId web_app_id = web_app::test::InstallWebApp(
        profile(), std::move(web_app_info),
        /*overwrite_existing_manifest_fields =*/false,
        webapps::WebappInstallSource::EXTERNAL_POLICY);
    DCHECK_EQ(expected_web_app_id, web_app_id);
    return web_app_id;
  }

  webapps::AppId InstallExternalWebApp(
      const std::string& start_url,
      const std::optional<std::string>& install_url = {}) {
    return InstallExternalWebApp(GURL(start_url), install_url
                                                      ? GURL(*install_url)
                                                      : std::optional<GURL>());
  }

  void RemoveWebApp(const char* web_app_id) {
    web_app::test::UninstallWebApp(profile(), web_app_id);
    apps::AppReadinessWaiter(profile(), web_app_id,
                             apps::Readiness::kUninstalledByUser)
        .Await();
  }

  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override {
    app_registry_cache_observer_.Reset();
  }

  void UpdateSyncPinAssertion(const std::string& app_id,
                              PinAssertion pin_assertion) {
    pin_assertions_[app_id] = pin_assertion;
  }

  void ValidateSyncPinAssertions() {
    base::flat_set<std::string> processed_assertions;
    for (const auto& [item_id, sync_item] :
         app_list_syncable_service_->sync_items()) {
      auto itr = pin_assertions_.find(item_id);
      if (itr == pin_assertions_.end()) {
        continue;
      }
      switch (itr->second) {
        case PinAssertion::kNotPinned:
          ASSERT_FALSE(sync_item->item_pin_ordinal.IsValid());
          ASSERT_FALSE(sync_item->is_user_pinned.has_value());
          break;
        case PinAssertion::kUnknownPinSource:
          ASSERT_TRUE(sync_item->item_pin_ordinal.IsValid());
          ASSERT_FALSE(sync_item->is_user_pinned.has_value());
          break;
        case PinAssertion::kOnlyPolicyPinned:
          ASSERT_TRUE(sync_item->item_pin_ordinal.IsValid());
          ASSERT_TRUE(sync_item->is_user_pinned.has_value());
          ASSERT_FALSE(*sync_item->is_user_pinned);
          break;
        case PinAssertion::kUserPinned:
          ASSERT_TRUE(sync_item->item_pin_ordinal.IsValid());
          ASSERT_TRUE(sync_item->is_user_pinned.has_value());
          ASSERT_TRUE(*sync_item->is_user_pinned);
          break;
      }
      processed_assertions.insert(item_id);
    }
    ASSERT_EQ(processed_assertions.size(), pin_assertions_.size());
  }

  syncer::SyncData GetSyncDataFor(std::string_view app_id) const {
    auto sync_data = app_list_syncable_service_->GetAllSyncDataForTesting();
    auto itr = base::ranges::find(sync_data, app_id, [](const auto& sync_item) {
      return sync_item.GetSpecifics().app_list().item_id();
    });
    EXPECT_FALSE(itr == sync_data.end());
    return *itr;
  }

  sync_pb::AppListSpecifics GetAppListSpecificsFor(
      std::string_view app_id) const {
    return GetSyncDataFor(app_id).GetSpecifics().app_list();
  }

  template <class... Args>
  void SetPinnedLauncherAppsPolicy(Args&&... args) {
    base::Value::List pinned_launcher_apps;
    (AppendPrefValue(pinned_launcher_apps, std::forward<Args>(args)), ...);
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kPolicyPinnedLauncherApps,
        base::Value(std::move(pinned_launcher_apps)));
  }

  apps::AppServiceTest& app_service_test() { return app_service_test_; }

  bool IsPlatformApp(const ash::ShelfID& id) const {
    const extensions::Extension* extension =
        GetExtensionForAppID(id.app_id, profile());
    // An extension can be synced / updated at any time and therefore not be
    // available.
    return extension ? extension->is_platform_app() : false;
  }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension_chrome_;
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension5_;
  scoped_refptr<Extension> extension6_;
  scoped_refptr<Extension> extension7_;
  scoped_refptr<Extension> extension8_;
  scoped_refptr<Extension> extension_platform_app_;
  scoped_refptr<Extension> arc_support_host_;

  ArcAppTest arc_test_{ArcAppTest::UserManagerMode::kDoNothing};
  bool auto_start_arc_test_ = false;
  std::unique_ptr<ChromeShelfController> shelf_controller_;
  std::unique_ptr<ash::ShelfModel> model_;

  // |item_delegate_manager_| owns |test_controller_|.
  raw_ptr<ash::ShelfItemDelegate, DanglingUntriaged> test_controller_ = nullptr;

  raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged>
      extension_registry_ = nullptr;

  raw_ptr<extensions::ExtensionService, DanglingUntriaged> extension_service_ =
      nullptr;

  raw_ptr<app_list::AppListSyncableService, DanglingUntriaged>
      app_list_syncable_service_ = nullptr;

  PinAssertionMap pin_assertions_;

  base::AutoReset<bool> skip_preinstalled_web_app_startup_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

 private:
  std::unique_ptr<TestBrowserWindow> CreateTestBrowserWindowAura() {
    auto window = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_NORMAL);
    window->SetId(0);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window.get(), GetContext(),
                                          gfx::Rect(200, 200),
                                          display::kInvalidDisplayId);

    return std::make_unique<TestBrowserWindowAura>(std::move(window));
  }

  apps::AppServiceTest app_service_test_;
};

class ChromeShelfControllerWithArcTest : public ChromeShelfControllerTestBase {
 protected:
  ChromeShelfControllerWithArcTest() { auto_start_arc_test_ = true; }

  ChromeShelfControllerWithArcTest(const ChromeShelfControllerWithArcTest&) =
      delete;
  ChromeShelfControllerWithArcTest& operator=(
      const ChromeShelfControllerWithArcTest&) = delete;
  ~ChromeShelfControllerWithArcTest() override = default;

  void SetUp() override {
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    ChromeShelfControllerTestBase::SetUp();
  }
};

class ChromeShelfControllerTest : public ChromeShelfControllerTestBase {
 public:
  ChromeShelfControllerTest() {
    // `media_router::kMediaRouter` is disabled because it has unmet
    // dependencies and is unrelated to this unit test.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {ash::features::
             kRemoveStalePolicyPinnedAppsFromShelf}, /*disabled_features=*/
        {media_router::kMediaRouter});
  }
  ~ChromeShelfControllerTest() override = default;

 private:
  // CrostiniTestHelper overrides feature list after GPU thread has started.
  viz::TestGpuServiceHolder::ScopedAllowRacyFeatureListOverrides
      gpu_thread_allow_racy_overrides_;

  base::test::ScopedFeatureList feature_list_;
};

// A V1 windowed application.
class V1App : public TestBrowserWindow {
 public:
  V1App(Profile* profile, const std::string& app_name) {
    Browser::CreateParams params = Browser::CreateParams::CreateForApp(
        kCrxAppPrefix + app_name, true /* trusted_source */, gfx::Rect(),
        profile, true);
    params.window = this;
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
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
    window_ = new extensions::AppWindow(
        profile, std::make_unique<ChromeAppDelegate>(profile, true), extension);
    extensions::AppWindow::CreateParams params;
    params.window_type = window_type;
    // Note: normally, the creator RFH is the background page of the
    // app/extension calling chrome.app.window.create. For unit testing
    // purposes, just passing in a random RenderFrameHost is Good Enough™.
    window_->Init(GURL(std::string()),
                  std::make_unique<extensions::AppWindowContentsImpl>(window_),
                  creator_web_contents_->GetPrimaryMainFrame(), params);
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
  raw_ptr<extensions::AppWindow, DanglingUntriaged> window_;
};

// The testing framework to test multi profile scenarios.
class MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest
    : public ChromeShelfControllerTestBase {
 protected:
  MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest() {
    // `kMediaRouter` is disabled because it has unmet dependencies and is
    // unrelated to this unit test.
    scoped_feature_list_.InitAndDisableFeature(media_router::kMediaRouter);
  }
  MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest(
      const MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest&) =
      delete;
  MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest& operator=(
      const MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest&) =
      delete;
  ~MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest() override =
      default;

  // Overwrite the Setup function to enable multi profile and needed objects.
  void SetUp() override {
    // Initialize the rest.
    ChromeShelfControllerTestBase::SetUp();

    // Ensure there are multiple profiles. User 0 is created during setup.
    CreateMultiUserProfile("user1@example.com");
    ASSERT_TRUE(SessionControllerClientImpl::IsMultiProfileAvailable());
  }

  void TearDown() override {
    ChromeShelfControllerTestBase::TearDown();

    // A Task is leaked if we don't destroy everything, then run the message
    // loop.
    base::RunLoop().RunUntilIdle();
  }

  bool StartWebAppProviderForMainProfile() const override {
    // The provider is started in CreateMultiUserProfile()
    return false;
  }

  // Creates a user and profile for a given `email`. Note that this class will
  // keep the ownership of the created object.
  TestingProfile* CreateMultiUserProfile(const std::string& email) {
    LogIn(email);
    return CreateProfile(email);
  }

  // Switch to another user by AccountId.
  // TODO(b/40286020): Migrate into SwitchActiveUser().
  void SwitchActiveUserByAccountId(const AccountId& account_id) {
    user_manager()->SwitchActiveUser(account_id);
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
  // Note that with the use of the shelf_controller_helper as done below,
  // this is only usable with a single v1 application.
  V1App* CreateRunningV1App(Profile* profile,
                            const std::string& app_name,
                            const std::string& url) {
    V1App* v1_app = new V1App(profile, app_name);
    NavigateAndCommitActiveTabWithTitle(v1_app->browser(), GURL(url),
                                        std::u16string());
    return v1_app;
  }

  // Override BrowserWithTestWindowTest:
  std::string GetDefaultProfileName() override { return "user0@example.com"; }

  void LogIn(const std::string& email) override {
    // TODO(crbug.com/40286020): Merge into BrowserWithTestWindowTest.
    const AccountId account_id = AccountId::FromUserEmail(email);
    // Add a user to the fake user manager.
    auto* user = user_manager()->AddUser(account_id);
    ash_test_helper()->test_session_controller_client()->AddUserSession(
        user->GetDisplayEmail());
    user_manager()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
  }

  TestingProfile* CreateProfile(const std::string& profile_name) override {
    TestingProfile* profile =
        BrowserWithTestWindowTest::CreateProfile(profile_name);
    StartWebAppProvider(profile);

    if (MultiUserWindowManagerHelper::GetInstance()) {
      MultiUserWindowManagerHelper::GetInstance()->AddUser(profile);
    }
    if (shelf_controller_) {
      shelf_controller_->AdditionalUserAddedToSession(profile);
    }
    return profile;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ChromeShelfControllerMultiProfileWithArcTest
    : public MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest {
 protected:
  ChromeShelfControllerMultiProfileWithArcTest() {
    auto_start_arc_test_ = true;
  }
  ChromeShelfControllerMultiProfileWithArcTest(
      const ChromeShelfControllerMultiProfileWithArcTest&) = delete;
  ChromeShelfControllerMultiProfileWithArcTest& operator=(
      const ChromeShelfControllerMultiProfileWithArcTest&) = delete;
  ~ChromeShelfControllerMultiProfileWithArcTest() override = default;
};

TEST_F(ChromeShelfControllerTest, DefaultShelfPrefValues) {
  StartPrefSyncService(syncer::SyncDataList());
  InitShelfController();

  // Verify shelf prefs are initialized to default values if they're not set
  // either locally nor in sync data.
  PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(ash::ShelfAlignment::kBottom,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kNever,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));

  // Verify that shelf pref values don't change locally if they change in sync
  // after local value has been initialized.
  syncer::SyncChangeList os_change_list;
  os_change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateStringPrefsSyncData(ash::prefs::kShelfAlignment,
                                ash::kShelfAlignmentLeft));
  os_change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateStringPrefsSyncData(ash::prefs::kShelfAutoHideBehavior,
                                ash::kShelfAutoHideBehaviorAlways));
  GetPrefSyncService()->ProcessSyncChanges(FROM_HERE, os_change_list);

  EXPECT_EQ(ash::ShelfAlignment::kBottom,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kNever,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));
}

TEST_F(ChromeShelfControllerTest, ShelfPrefsInitializedFromSyncData) {
  // Add shelf prefs to synced pref data, and start syncing.
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateStringPrefsSyncData(ash::prefs::kShelfAlignment,
                                                ash::kShelfAlignmentLeft));
  sync_list.push_back(CreateStringPrefsSyncData(
      ash::prefs::kShelfAutoHideBehavior, ash::kShelfAutoHideBehaviorAlways));
  StartPrefSyncService(std::move(sync_list));

  // Verify local shelf state is initialized to reflect values in sync data if
  // shelf controller gets initialized after initial sync pref values have been
  // received.
  InitShelfController();

  PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(ash::ShelfAlignment::kLeft,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kAlways,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));

  // Verify further synced shelf prefs changes do not affect local shelf state.
  syncer::SyncChangeList os_change_list;
  os_change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateStringPrefsSyncData(ash::prefs::kShelfAlignment,
                                ash::kShelfAlignmentBottom));
  os_change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateStringPrefsSyncData(ash::prefs::kShelfAutoHideBehavior,
                                ash::kShelfAutoHideBehaviorNever));
  GetPrefSyncService()->ProcessSyncChanges(FROM_HERE, os_change_list);

  EXPECT_EQ(ash::ShelfAlignment::kLeft,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kAlways,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));
}

TEST_F(ChromeShelfControllerTest,
       ShelfControllerUpdatesShelfPrefAfterInitialPrefSync) {
  // Initialize shelf controller, and verify shelf state reflects default values
  // before initial synced prefs have been received.
  InitShelfController();

  PrefService* const prefs = browser()->profile()->GetPrefs();
  EXPECT_EQ(ash::ShelfAlignment::kBottom,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kNever,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));

  // If initial sync data contains shelf prefs, local shelf state should be
  // updated to reflect shelf state in the initial sync data.
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateStringPrefsSyncData(ash::prefs::kShelfAlignment,
                                                ash::kShelfAlignmentLeft));
  sync_list.push_back(CreateStringPrefsSyncData(
      ash::prefs::kShelfAutoHideBehavior, ash::kShelfAutoHideBehaviorAlways));
  StartPrefSyncService(std::move(sync_list));

  EXPECT_EQ(ash::ShelfAlignment::kLeft,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kAlways,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));

  // Verify further synced shelf prefs changes do not affect local shelf state.
  syncer::SyncChangeList os_change_list;
  os_change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateStringPrefsSyncData(ash::prefs::kShelfAlignment,
                                ash::kShelfAlignmentBottom));
  os_change_list.emplace_back(
      FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
      CreateStringPrefsSyncData(ash::prefs::kShelfAutoHideBehavior,
                                ash::kShelfAutoHideBehaviorNever));
  GetPrefSyncService()->ProcessSyncChanges(FROM_HERE, os_change_list);

  EXPECT_EQ(ash::ShelfAlignment::kLeft,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kAlways,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));
}

TEST_F(ChromeShelfControllerTest, SyncedShelfPrefsDontOverrideLocalPref) {
  // Initialize shelf prefs before shelf controller gets initialized.
  PrefService* const prefs = browser()->profile()->GetPrefs();
  ash::SetShelfAlignmentPref(prefs, GetPrimaryDisplayId(),
                             ash::ShelfAlignment::kLeft);
  ash::SetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId(),
                                    ash::ShelfAutoHideBehavior::kAlways);

  // Set initial sync data to conflict with the local shelf prefs.
  syncer::SyncDataList sync_list;
  sync_list.push_back(CreateStringPrefsSyncData(ash::prefs::kShelfAlignment,
                                                ash::kShelfAlignmentBottom));
  sync_list.push_back(CreateStringPrefsSyncData(
      ash::prefs::kShelfAutoHideBehavior, ash::kShelfAutoHideBehaviorNever));
  StartPrefSyncService(std::move(sync_list));

  // Initialize shelf controller, and verify shelf state reflects initially set
  // local prefs.
  InitShelfController();

  EXPECT_EQ(ash::ShelfAlignment::kLeft,
            ash::GetShelfAlignmentPref(prefs, GetPrimaryDisplayId()));
  EXPECT_EQ(ash::ShelfAutoHideBehavior::kAlways,
            ash::GetShelfAutoHideBehaviorPref(prefs, GetPrimaryDisplayId()));
}

TEST_F(ChromeShelfControllerTest, PreinstalledApps) {
  InitShelfController();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Pinning the non-preinstalled app. It should appear at the end. No
  // preinstalled app is currently installed.
  extension_service_->AddExtension(extension1_.get());
  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  // Install preinstalled apps in reverse order, compared how they are declared.
  // However pin positions should be in the order as they declared. Note,
  // preinstalled apps appear on shelf between manually pinned App1.

  // Prefs are not yet synced. No default pin appears.
  AddWebApp(web_app::kYoutubeAppId);
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  StartPrefSyncService(syncer::SyncDataList());
  EXPECT_EQ("Chrome, Youtube, App1", GetPinnedAppStatus());

  AddWebApp(web_app::kMessagesAppId);
  EXPECT_EQ("Chrome, Messages, Youtube, App1", GetPinnedAppStatus());

  AddWebApp(web_app::kGoogleCalendarAppId);
  EXPECT_EQ("Chrome, Calendar, Messages, Youtube, App1", GetPinnedAppStatus());

  AddWebApp(web_app::kGmailAppId);
  EXPECT_EQ("Chrome, Gmail, Calendar, Messages, Youtube, App1",
            GetPinnedAppStatus());
}

TEST_F(ChromeShelfControllerWithArcTest, ArcAppsHiddenFromLaunchCanBePinned) {
  InitShelfController();

  // Register Android Settings.
  arc::mojom::AppHost* app_host = arc_test_.arc_app_list_prefs();
  app_host->OnAppListRefreshed(GetArcSettingsAppInfo());

  // Pin Android settings.
  PinAppWithIDToShelf(arc::kSettingsAppId);
  EXPECT_EQ("Chrome, Android Settings", GetPinnedAppStatus());

  // The pin should remain after syncing prefs.
  StartPrefSyncService(syncer::SyncDataList());
  EXPECT_EQ("Chrome, Android Settings", GetPinnedAppStatus());
}

TEST_F(ChromeShelfControllerWithArcTest, ArcAppPinCrossPlatformWorkflow) {
  // Work on ARC disabled platform first.
  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[2]);

  InitShelfController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  AddWebApp(web_app::kGmailAppId);

  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, arc_app_id1);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, arc_app_id2);
  InsertAddPinChange(&sync_list, 4, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);
  SetShelfChromeIconIndex(1);

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("App1, Chrome, App2, Gmail", GetPinnedAppStatus());

  // Persist pin state, we don't have active pin for ARC apps yet, but pin
  // model should have it.
  syncer::SyncDataList copy_sync_list =
      app_list_syncable_service_->GetAllSyncDataForTesting();

  ResetShelfController();
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_list_syncable_service_->sync_items().size());

  // Move to ARC enabled platform, restart syncing with stored data.
  StartAppSyncService(copy_sync_list);
  RecreateShelfController()->Init();

  // Set FakeAppServiceAppIconLoader to wait for icons loaded.
  auto fake_app_service_icon_loader =
      std::make_unique<FakeAppServiceAppIconLoader>(
          profile(), extension_misc::EXTENSION_ICON_MEDIUM,
          shelf_controller_.get());
  FakeAppServiceAppIconLoader* icon_loader = fake_app_service_icon_loader.get();
  SetAppIconLoader(std::move(fake_app_service_icon_loader));

  // Pins must be automatically updated.
  SendListOfArcApps();
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id3));

  EXPECT_EQ("App1, Chrome, Fake App 1, App2, Fake App 2, Gmail",
            GetPinnedAppStatus());

  // Now move pins on ARC enabled platform.
  model_->Move(0, 3);
  model_->Move(2, 0);
  model_->Move(2, 4);
  model_->Move(3, 1);
  EXPECT_EQ("App2, Fake App 2, Chrome, App1, Fake App 1, Gmail",
            GetPinnedAppStatus());

  // ARC apps, Fake App 2 and Fake App 1, are updated due to icon updates. So
  // wait for ARC apps icon updated by AppService before reset sync service.
  icon_loader->WaitForIconLoadded(
      std::vector<std::string>{arc_app_id1, arc_app_id2});
  copy_sync_list = app_list_syncable_service_->GetAllSyncDataForTesting();

  ResetShelfController();
  ResetPinModel();

  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_list_syncable_service_->sync_items().size());

  // Move back to ARC disabled platform.
  EnablePlayStore(false);
  StartAppSyncService(copy_sync_list);
  RecreateShelfController()->Init();

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("App2, Chrome, App1, Gmail", GetPinnedAppStatus());

  // Now move/remove pins on ARC disabled platform.
  model_->Move(3, 1);
  shelf_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("Gmail, Chrome, App1", GetPinnedAppStatus());
  EnablePlayStore(true);

  SendListOfArcApps();

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id2));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id3));
  EXPECT_EQ("Gmail, Fake App 2, Chrome, App1, Fake App 1",
            GetPinnedAppStatus());
}

// Ensure correct merging of policy pinned apps and user pinned apps.
TEST_F(ChromeShelfControllerTest, MergePolicyAndUserPrefPinnedApps) {
  InitShelfController();

  // Install two versions of google docs with different install_urls.
  const GURL& google_docs_start_url = GetWebAppUrl(web_app::kGoogleDocsAppId);

  const GURL google_docs_install_url_v1{
      base::StrCat({google_docs_start_url.spec(), "&v=1"})};
  const GURL google_docs_install_url_v2{
      base::StrCat({google_docs_start_url.spec(), "&v=2"})};

  InstallExternalWebApp(/*start_url=*/google_docs_start_url,
                        /*install_url=*/google_docs_install_url_v1);
  InstallExternalWebApp(/*start_url=*/google_docs_start_url,
                        /*install_url=*/google_docs_install_url_v2);

  // Check that both values are propagated to PolicyIds().
  std::vector<std::string> google_docs_install_urls = {
      google_docs_install_url_v1.spec(),
      google_docs_install_url_v2.spec(),
  };
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(web_app::kGoogleDocsAppId, [&google_docs_install_urls](
                                                const auto& update) {
        ASSERT_THAT(update.PolicyIds(), testing::UnorderedElementsAreArray(
                                            google_docs_install_urls));
      });

  InstallExternalWebApp(GetWebAppUrl(web_app::kGmailAppId));
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension5_.get());
  // extension 1, 3 are pinned by user
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, app_constants::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, web_app::kGmailAppId);
  SendPinChanges(sync_list, /*reset_pin_model=*/true);

  base::Value::List policy_value;
  // extension 2 4 are pinned by policy
  AppendPrefValue(policy_value, extension2_->id());
  AppendPrefValue(policy_value, google_docs_install_url_v2.spec());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, base::Value(policy_value.Clone()));

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  // 2 is not pinned as it's not installed
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGoogleDocsAppId));
  // install extension 2 and check
  AddExtension(extension2_.get());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));

  // Check user can manually pin or unpin these apps
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension1_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension2_->id(), profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(web_app::kGmailAppId, profile()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(web_app::kGoogleDocsAppId, profile()));

  // Check the order of shelf pinned apps
  EXPECT_EQ("App2, Doc, App1, Chrome, Gmail", GetPinnedAppStatus());
}

// Check that the restoration of shelf items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(ChromeShelfControllerTest, RestorePreinstalledAppsReverseOrder) {
  InitShelfController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Installing Gmail should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  AddWebApp(web_app::kGmailAppId);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("Chrome, Gmail", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon, but in first location.
  AddExtension(extension2_.get());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("Chrome, App2, Gmail", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the shelf - behind the
  // chrome icon, but in first location.
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());
}

// Check that the restoration of shelf items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(ChromeShelfControllerTest, RestorePreinstalledAppsRandomOrder) {
  InitShelfController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon.
  AddExtension(extension2_.get());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the shelf - behind the
  // chrome icon, but in first location.
  AddExtension(extension1_.get());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  // Installing Gmail should add it to the shelf - behind the chrome icon,
  // but in first location.
  AddWebApp(web_app::kGmailAppId);
  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());
}

// Check that the restoration of shelf items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_F(ChromeShelfControllerTest,
       RestorePreinstalledAppsRandomOrderChromeMoved) {
  InitShelfController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, app_constants::kChromeAppId);
  InsertAddPinChange(&sync_list, 2, extension2_->id());
  InsertAddPinChange(&sync_list, 3, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);

  // The model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  AddExtension(extension2_.get());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the shelf - behind the
  // chrome icon, but in first location.
  AddExtension(extension1_.get());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_EQ("App1, Chrome, App2", GetPinnedAppStatus());

  // Installing Gmail should add it to the shelf - behind the chrome icon,
  // but in first location.
  AddWebApp(web_app::kGmailAppId);
  EXPECT_EQ("App1, Chrome, App2, Gmail", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing.
TEST_F(ChromeShelfControllerTest, RestorePreinstalledAppsResyncOrder) {
  InitShelfController();

  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, extension2_->id());
  InsertAddPinChange(&sync_list0, 2, web_app::kGmailAppId);
  SendPinChanges(sync_list0, true);

  // The shelf layout has always one static item at the beginning (App List).
  AddExtension(extension2_.get());
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  AddWebApp(web_app::kGmailAppId);
  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, web_app::kGmailAppId);
  InsertAddPinChange(&sync_list1, 1, extension1_->id());
  InsertAddPinChange(&sync_list1, 2, extension2_->id());
  InsertAddPinChange(&sync_list1, 3, app_constants::kChromeAppId);
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Gmail, App1, App2, Chrome", GetPinnedAppStatus());

  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, extension2_->id());
  InsertAddPinChange(&sync_list2, 1, web_app::kGmailAppId);
  InsertAddPinChange(&sync_list2, 2, app_constants::kChromeAppId);
  InsertAddPinChange(&sync_list2, 3, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("App2, Gmail, Chrome, App1", GetPinnedAppStatus());

  // Check that the chrome icon can also be at the first possible location.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, web_app::kGmailAppId);
  InsertAddPinChange(&sync_list3, 1, extension2_->id());
  InsertAddPinChange(&sync_list3, 2, extension1_->id());
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("Chrome, Gmail, App2, App1", GetPinnedAppStatus());

  // Check that uninstalling of extensions works as expected.
  UninstallExtension(extension1_->id(),
                     extensions::UninstallReason::UNINSTALL_REASON_FOR_TESTING);
  EXPECT_EQ("Chrome, Gmail, App2", GetPinnedAppStatus());

  // Check that an update of an extension does not crash the system.
  UnloadExtension(extension2_->id(), UnloadedExtensionReason::UPDATE);
  EXPECT_EQ("Chrome, Gmail, App2", GetPinnedAppStatus());
}

// Test the V1 app interaction flow: run it, activate it, close it.
TEST_F(ChromeShelfControllerTest, V1AppRunActivateClose) {
  InitShelfController();
  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should create a new shelf item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running again should have no effect.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);

  // Reporting that the app is closed should remove its shelf item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed again should have no effect.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
}

// Test the V1 app interaction flow: pin it, run it, close it, unpin it.
TEST_F(ChromeShelfControllerTest, V1AppPinRunCloseUnpin) {
  InitShelfController();
  // The model should only contain the browser shortcut.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should create a new shelf item.
  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should just update the existing item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should just update the existing item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should remove its shelf item.
  shelf_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Test the V1 app interaction flow: run it, pin it, close it, unpin it.
TEST_F(ChromeShelfControllerTest, V1AppRunPinCloseUnpin) {
  // Set the app type of extension1_ to a chrome app, as the default unknown app
  // type is not allowed to be pinned.
  std::vector<apps::AppPtr> apps;
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, extension1_->id());
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile())->OnApps(
      std::move(apps), apps::AppType::kChromeApp,
      /*should_notify_initialized=*/false);

  InitShelfController();

  // The model should only contain the browser shortcut.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should create a new shelf item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should just update the existing item.
  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should just update the existing item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should remove its shelf item.
  shelf_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Test the V1 app interaction flow: pin it, run it, unpin it, close it.
TEST_F(ChromeShelfControllerTest, V1AppPinRunUnpinClose) {
  InitShelfController();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Pinning the app should create a new shelf item.
  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is running should just update the existing item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Unpinning the app should just update the existing item.
  shelf_controller_->UnpinAppWithID(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_NE(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));

  // Reporting that the app is closed should remove its shelf item.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(nullptr,
            shelf_controller_->GetItem(ash::ShelfID(extension1_->id())));
}

// Ensure unpinned V1 app ordering is properly restored after user changes.
TEST_F(ChromeShelfControllerTest, CheckRunningV1AppOrder) {
  InitShelfController();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());

  // Add a few running applications.
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_RUNNING);
  shelf_controller_->SetAppStatus(extension2_->id(), ash::STATUS_RUNNING);
  shelf_controller_->SetAppStatus(web_app::kGmailAppId, ash::STATUS_RUNNING);
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
  shelf_controller_->SetAppStatus(extension1_->id(), ash::STATUS_CLOSED);
  EXPECT_EQ("Chrome, gmail, app2", GetPinnedAppStatus());
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome, app2, gmail", GetPinnedAppStatus());

  // Check that removing more items does not crash and changes nothing.
  shelf_controller_->SetAppStatus(extension2_->id(), ash::STATUS_CLOSED);
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome, gmail", GetPinnedAppStatus());
  shelf_controller_->SetAppStatus(web_app::kGmailAppId, ash::STATUS_CLOSED);
  RestoreUnpinnedRunningApplicationOrder(current_account_id);
  EXPECT_EQ("Chrome", GetPinnedAppStatus());
}

TEST_F(ChromeShelfControllerWithArcTest, ArcDeferredLaunch) {
  InitShelfController();

  const arc::mojom::ShortcutInfo& shortcut = arc_test_.fake_shortcuts()[0];
  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[2]);
  const std::string arc_shortcut_id = ArcAppTest::GetAppId(shortcut);

  SendListOfArcApps();
  SendListOfArcShortcuts();

  arc_test_.StopArcInstance();

  const ash::ShelfID shelf_id_app_1(arc_app_id1);
  const ash::ShelfID shelf_id_app_2(arc_app_id2);
  const ash::ShelfID shelf_id_app_3(arc_app_id3);
  const ash::ShelfID shelf_id_shortcut(arc_shortcut_id);
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_1));
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_2));
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_3));
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_shortcut));

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

  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_app_1));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_app_2));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_app_3));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_shortcut));

  // We activated arc_app_id1 twice but expect one close for item controller
  // stops launching request.
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(shelf_id_app_1);
  ASSERT_NE(nullptr, item_delegate);
  item_delegate->Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_1));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_app_2));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_app_3));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id_shortcut));

  arc_test_.RestartArcInstance();
  SendListOfArcApps();

  base::RunLoop().RunUntilIdle();

  // Now spinner controllers should go away together with shelf items and ARC
  // app instance should receive request for launching apps and shortcuts.
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_1));
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_2));
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_app_3));
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id_shortcut));

  EXPECT_EQ(0U, arc_test_.app_instance()->launch_requests().size());
  ASSERT_EQ(3U, arc_test_.app_instance()->launch_intents().size());

  EXPECT_GE(arc_test_.app_instance()->launch_intents()[0].find(
                "component=fake.app.1/.activity;"),
            0u);
  EXPECT_GE(arc_test_.app_instance()->launch_intents()[0].find(
                "S.org.chromium.arc.request.deferred.start="),
            0u);
  EXPECT_GE(arc_test_.app_instance()->launch_intents()[1].find(
                "component=fake.app.2/.activity;"),
            0u);
  EXPECT_GE(arc_test_.app_instance()->launch_intents()[1].find(
                "S.org.chromium.arc.request.deferred.start="),
            0u);
  EXPECT_EQ(arc_test_.app_instance()->launch_intents()[2].c_str(),
            shortcut.intent_uri);
}

// Launch is canceled in case app becomes suspended.
TEST_F(ChromeShelfControllerWithArcTest, ArcDeferredLaunchForSuspendedApp) {
  InitShelfController();

  // Register app first.
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(arc_test_.fake_apps()[0]->Clone());
  const std::string app_id = ArcAppTest::GetAppId(*apps[0]);
  arc_test_.app_instance()->SendRefreshAppList(apps);
  arc_test_.StopArcInstance();

  // Restart ARC
  arc_test_.RestartArcInstance();

  // Deferred controller should be allocated on start.
  const ash::ShelfID shelf_id(app_id);
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id));

  // Send app with suspended state.
  apps[0]->suspended = true;
  arc_test_.app_instance()->SendRefreshAppList(apps);

  // Controller automatically closed.
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id));

  // And no launch request issued.
  EXPECT_TRUE(arc_test_.app_instance()->launch_requests().empty());
}

// Ensure the spinner controller does not override the active app controller
// (crbug.com/701152).
TEST_F(ChromeShelfControllerWithArcTest, ArcDeferredLaunchForActiveApp) {
  InitShelfController();
  SendListOfArcApps();
  arc_test_.StopArcInstance();

  const std::string app_id = ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);

  PinAppWithIDToShelf(app_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  const ash::ShelfID shelf_id(app_id);
  const ash::ShelfItem* item = shelf_controller_->GetItem(shelf_id);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(ash::STATUS_CLOSED, item->status);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item->type);

  // Play Store app is ARC app that might be represented by native Chrome
  // platform app.
  model_->ReplaceShelfItemDelegate(
      shelf_id,
      std::make_unique<AppServiceAppWindowShelfItemController>(
          shelf_id, shelf_controller_->app_service_app_window_controller()));
  shelf_controller_->SetItemStatus(shelf_id, ash::STATUS_RUNNING);

  // This launch request should be ignored in case of active app.
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Closing the app should leave a pinned but closed shelf item shortcut.
  shelf_controller_->ReplaceWithAppShortcutOrRemove(shelf_id);
  item = shelf_controller_->GetItem(shelf_id);
  ASSERT_NE(nullptr, item);
  EXPECT_EQ(ash::STATUS_CLOSED, item->status);
  EXPECT_EQ(ash::TYPE_PINNED_APP, item->type);

  // Now launch request should not be ignored.
  arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));
}

// TODO(crbug.com/915840): this test is flakey and/or often crashes.
TEST_F(ChromeShelfControllerMultiProfileWithArcTest, DISABLED_ArcMultiUser) {
  SendListOfArcApps();

  InitShelfController();

  SetShelfControllerHelper(new TestShelfControllerHelper);

  // App1 exists all the time.
  // App2 is created when primary user is active and destroyed when secondary
  // user is active.
  // Gmail created when secondary user is active.

  const std::string user2 = "user2@example.com";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);
  const std::string arc_app_id3 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[2]);

  std::string window_app_id1("org.chromium.arc.1");
  views::Widget* arc_window1 = CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));

  std::string window_app_id2("org.chromium.arc.2");
  views::Widget* arc_window2 = CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, *arc_test_.fake_apps()[1],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));

  shelf_controller_->SetProfileForTest(profile2);
  SwitchActiveUserByAccountId(account_id2);

  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));

  std::string window_app_id3("org.chromium.arc.3");
  views::Widget* arc_window3 = CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, *arc_test_.fake_apps()[2],
                                            std::string());
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id3)));

  arc_window2->CloseNow();
  arc_test_.app_instance()->SendTaskDestroyed(2);

  shelf_controller_->SetProfileForTest(profile());
  SwitchActiveUserByAccountId(account_id);

  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id3)));

  // Close active window to let test passes.
  arc_window1->CloseNow();
  arc_window3->CloseNow();
}

TEST_F(ChromeShelfControllerWithArcTest, ArcRunningApp) {
  InitShelfController();

  const std::string arc_app_id =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  SendListOfArcApps();
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));

  // Normal flow, create/destroy tasks.
  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  std::string window_app_id3("org.chromium.arc.3");
  CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));
  CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));
  arc_test_.app_instance()->SendTaskDestroyed(1);
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));
  arc_test_.app_instance()->SendTaskDestroyed(2);
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));

  // Stopping bridge removes apps.
  CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));
  arc_test_.StopArcInstance();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));
}

// Test race creation/deletion of ARC app.
// TODO(khmel): Remove after moving everything to wayland protocol.
TEST_F(ChromeShelfControllerWithArcTest, ArcRaceCreateClose) {
  InitShelfController();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);
  SendListOfArcApps();

  // ARC window created before and closed after mojom notification.
  std::string window_app_id1("org.chromium.arc.1");
  views::Widget* arc_window = CreateArcWindow(window_app_id1);
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  ASSERT_TRUE(arc_window);
  arc_test_.app_instance()->SendTaskCreated(1, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  arc_test_.app_instance()->SendTaskDestroyed(1);
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));
  arc_window->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id1)));

  // ARC window created after and closed before mojom notification.
  std::string window_app_id2("org.chromium.arc.2");
  arc_test_.app_instance()->SendTaskCreated(2, *arc_test_.fake_apps()[1],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  arc_window = CreateArcWindow(window_app_id2);
  ASSERT_TRUE(arc_window);
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  arc_window->Close();
  base::RunLoop().RunUntilIdle();
  // Closing window does not close shelf item. It is closed on task destroy.
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));
  arc_test_.app_instance()->SendTaskDestroyed(2);
  EXPECT_FALSE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id2)));
}

TEST_F(ChromeShelfControllerWithArcTest, ArcWindowRecreation) {
  InitShelfController();

  const std::string arc_app_id =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  SendListOfArcApps();

  std::string window_app_id("org.chromium.arc.1");
  views::Widget* arc_window = CreateArcWindow(window_app_id);
  ASSERT_TRUE(arc_window);
  arc_test_.app_instance()->SendTaskCreated(1, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));

  for (int i = 0; i < 3; ++i) {
    arc_window->Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));

    arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc_app_id)));
  }
}

// Verifies edge cases when Extension app item controller may be overwritten by
// ARC app item controller and vice versa. This should not happen in normal
// cases but in case of ARC boot failure this may lead to such situation. This
// test verifies that dynamic change of app item controllers is safe. See more
// crbug.com/770005.
TEST_F(ChromeShelfControllerWithArcTest, OverrideAppItemController) {
  extension_service_->AddExtension(arc_support_host_.get());

  InitShelfController();

  SendListOfArcApps();
  arc::mojom::AppInfoPtr app_info = CreateAppInfo(
      "Play Store", arc::kPlayStoreActivity, arc::kPlayStorePackage);
  EXPECT_EQ(arc::kPlayStoreAppId, AddArcAppAndShortcut(*app_info));

  std::string window_app_id("org.chromium.arc.1");
  const ash::ShelfID play_store_shelf_id(arc::kPlayStoreAppId);

  shelf_controller_->UnpinAppWithID(arc::kPlayStoreAppId);
  EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));

  // Try 4 different scenarios with different creation and destroying orders.

  // Scenario 1: Create OptIn, Play Store. Destroy OptIn, Play Store.
  {
    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, *app_info, std::string());
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));
  }

  // Scenario 2: Create OptIn, Play Store. Destroy Play Store, OptIn.
  {
    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, *app_info, std::string());
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));
  }

  // Scenario 3: Create Play Store, OptIn. Destroy OptIn, Play Store.
  {
    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, *app_info, std::string());
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));
  }

  // Scenario 4: Create Play Store, OptIn. Destroy Play Store, OptIn.
  {
    views::Widget* arc_window = CreateArcWindow(window_app_id);
    ASSERT_TRUE(arc_window);
    arc_test_.app_instance()->SendTaskCreated(1, *app_info, std::string());
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    std::unique_ptr<V2App> play_store_optin =
        std::make_unique<V2App>(profile(), arc_support_host_.get(),
                                extensions::AppWindow::WINDOW_TYPE_DEFAULT);
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    arc_window->CloseNow();
    arc_test_.app_instance()->SendTaskDestroyed(1);
    EXPECT_TRUE(shelf_controller_->GetItem(play_store_shelf_id));

    play_store_optin.reset();
    EXPECT_FALSE(shelf_controller_->GetItem(play_store_shelf_id));
  }
}

// Validate that ARC app is pinned correctly and pin is removed automatically
// once app is uninstalled.
TEST_F(ChromeShelfControllerWithArcTest, ArcAppPin) {
  InitShelfController();

  const std::string arc_app_id =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);

  SendListOfArcApps();
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));

  PinAppWithIDToShelf(extension1_->id());
  PinAppWithIDToShelf(arc_app_id);
  PinAppWithIDToShelf(extension2_->id());

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));

  EXPECT_EQ("Chrome, App1, Fake App 1, App2", GetPinnedAppStatus());

  UninstallArcApps();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id));
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  SendListOfArcApps();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id));
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  // Opt-Out/Opt-In remove item from the shelf.
  PinAppWithIDToShelf(arc_app_id);
  EXPECT_EQ("Chrome, App1, App2, Fake App 1", GetPinnedAppStatus());
  EnablePlayStore(false);
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  EnablePlayStore(true);
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  SendListOfArcApps();
  // Allow async callbacks to run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Chrome, App1, App2, Fake App 1", GetPinnedAppStatus());
}

// Validates that ARC app pins persist across OptOut/OptIn.
TEST_F(ChromeShelfControllerWithArcTest, ArcAppPinOptOutOptIn) {
  InitShelfController();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);

  SendListOfArcApps();
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  PinAppWithIDToShelf(extension1_->id());
  PinAppWithIDToShelf(arc_app_id2);
  PinAppWithIDToShelf(extension2_->id());
  PinAppWithIDToShelf(arc_app_id1);

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id2));
  EXPECT_EQ("Chrome, App1, Fake App 2, App2, Fake App 1", GetPinnedAppStatus());

  EnablePlayStore(false);

  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id2));

  EnablePlayStore(true);
  SendListOfArcApps();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id2));

  EXPECT_EQ("Chrome, App1, Fake App 2, App2, Fake App 1", GetPinnedAppStatus());
}

TEST_F(ChromeShelfControllerWithArcTest, DISABLED_ArcCustomAppIcon) {
  InitShelfController();

  // Wait until other apps are updated to avoid race condition while accessing
  // last updated item.
  base::RunLoop().RunUntilIdle();

  // Register fake ARC apps.
  SendListOfArcApps();
  // Use first fake ARC app for testing.
  const std::string arc_app_id =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
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

  EXPECT_FALSE(shelf_controller_->GetItem(arc_shelf_id));
  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  views::Widget* window1 = CreateArcWindow(window_app_id1);
  ASSERT_TRUE(window1 && window1->GetNativeWindow());
  arc_test_.app_instance()->SendTaskCreated(1, *arc_test_.fake_apps()[0],
                                            std::string());

  views::Widget* window2 = CreateArcWindow(window_app_id2);
  ASSERT_TRUE(window2 && window2->GetNativeWindow());
  arc_test_.app_instance()->SendTaskCreated(2, *arc_test_.fake_apps()[0],
                                            std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(arc_shelf_id));
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(arc_shelf_id);
  ASSERT_TRUE(item_delegate);
  base::RunLoop().RunUntilIdle();

  auto get_icon = [=, this]() {
    return *shelf_controller_->GetItem(arc_shelf_id)->image.bitmap();
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

TEST_F(ChromeShelfControllerWithArcTest, ArcWindowPackageName) {
  InitShelfController();
  SendListOfArcApps();

  std::string window_app_id1("org.chromium.arc.1");
  std::string window_app_id2("org.chromium.arc.2");
  std::string window_app_id3("org.chromium.arc.3");
  views::Widget* arc_window1 = CreateArcWindow(window_app_id1);
  arc_test_.app_instance()->SendTaskCreated(1, *arc_test_.fake_apps()[0],
                                            std::string());
  const std::string* package_name1 =
      arc_window1->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey);
  ASSERT_TRUE(package_name1);
  EXPECT_EQ(*package_name1, arc_test_.fake_apps()[0]->package_name);

  views::Widget* arc_window2 = CreateArcWindow(window_app_id2);
  arc_test_.app_instance()->SendTaskCreated(2, *arc_test_.fake_apps()[1],
                                            std::string());
  const std::string* package_name2 =
      arc_window2->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey);
  ASSERT_TRUE(package_name2);
  EXPECT_EQ(*package_name2, arc_test_.fake_apps()[1]->package_name);

  // Create another window with the same package name.
  views::Widget* arc_window3 = CreateArcWindow(window_app_id3);
  arc_test_.app_instance()->SendTaskCreated(3, *arc_test_.fake_apps()[1],
                                            std::string());
  const std::string* package_name3 =
      arc_window3->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey);
  ASSERT_TRUE(package_name3);
  EXPECT_EQ(*package_name3, arc_test_.fake_apps()[1]->package_name);

  arc_window1->CloseNow();
  arc_window2->CloseNow();
  arc_window3->CloseNow();
}

// Check that with multi profile V1 apps are properly added / removed from the
// shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V1AppUpdateOnUserSwitch) {
  // Create a browser item in the controller.
  InitShelfController();

  EXPECT_EQ(1, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, kGmailUrl));
    EXPECT_EQ(2, model_->item_count());

    // After switching to a second user the item should be gone.
    std::string user2 = "user2@example.com";
    TestingProfile* profile2 = CreateMultiUserProfile(user2);
    const AccountId account_id2(
        multi_user_util::GetAccountIdFromProfile(profile2));
    const AccountId account_id(
        multi_user_util::GetAccountIdFromProfile(profile()));
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(1, model_->item_count());

    // After switching back the item should be back.
    SwitchActiveUserByAccountId(account_id);
    EXPECT_EQ(2, model_->item_count());
    // Note we destroy now the gmail app with the closure end.
  }
  EXPECT_EQ(1, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V1AppUpdateOnUserSwitchEdgecases) {
  // Create a browser item in the controller.
  InitShelfController();

  // First test: Create an app when the user is not active.
  std::string user2 = "user2@example.com";
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
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(2, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUserByAccountId(account_id);
    EXPECT_EQ(1, model_->item_count());
    // Note: the closure ends and the browser will go away.
  }
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUserByAccountId(account_id);
  EXPECT_EQ(1, model_->item_count());
}

// Check edge case where a visiting V1 app gets closed (crbug.com/321374).
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V1CloseOnVisitingDesktop) {
  // Create a browser item in the controller.
  InitShelfController();

  // First create an app when the user is active.
  std::string user2 = "user2@example.com";
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
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(1, model_->item_count());
  }
  // After the app was destroyed, switch back. (which caused already a crash).
  SwitchActiveUserByAccountId(account_id);

  // Create the same app again - which was also causing the crash.
  EXPECT_EQ(1, model_->item_count());
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, kGmailLaunchURL));
    EXPECT_EQ(2, model_->item_count());
  }
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_EQ(1, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V1AppUpdateOnUserSwitchEdgecases2) {
  // Create a browser item in the controller.
  InitShelfController();

  // First test: Create an app when the user is not active.
  std::string user2 = "user2@example.com";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUserByAccountId(account_id2);
  {
    // Create a "windowed gmail app".
    std::unique_ptr<V1App> v1_app(
        CreateRunningV1App(profile(), extension_misc::kGmailAppId, kGmailUrl));
    EXPECT_EQ(1, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUserByAccountId(account_id);
    EXPECT_EQ(2, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(1, model_->item_count());
    v1_app.reset();
  }
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUserByAccountId(account_id);
  EXPECT_EQ(1, model_->item_count());
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_EQ(1, model_->item_count());
}

// Check that activating an item which is on another user's desktop, will bring
// it back.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       TestShelfActivationPullsBackWindow) {
  // Create a browser item in the controller.
  InitShelfController();
  ash::MultiUserWindowManager* window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();

  // Create a second test profile. The first is the one in profile() created in
  // BrowserWithTestWindowTest::SetUp(). No need to add the profiles to the
  // MultiUserWindowManagerHelper here. CreateMultiUserProfile() already does
  // that.
  TestingProfile* profile2 = CreateMultiUserProfile("user2@example.com");
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
  shelf_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(IsWindowOnDesktopOfUser(window, current_user));

  // Transfer the window to another user's desktop and check that activating it
  // does pull it back to that user.
  window_manager->ShowWindowForUser(
      window, multi_user_util::GetAccountIdFromProfile(profile2));
  EXPECT_FALSE(IsWindowOnDesktopOfUser(window, current_user));
  shelf_controller_->ActivateWindowOrMinimizeIfActive(browser_window, false);
  EXPECT_TRUE(IsWindowOnDesktopOfUser(window, current_user));
}

// Tests that web app icon is removed from shelf after user switch if the app is
// not installed by the new active user, even if the user has the URL associated
// with the app open in a tab.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       WebAppNotShownIfNotInstalledAfterUserSwitch) {
  // Create a browser item in the controller.
  InitShelfController();

  std::string user2 = "user2@example.com";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  constexpr char kWebAppUrl[] = "https://webappone.com/";
  constexpr char kWebAppName[] = "WebApp1";

  // Set up the test so
  // *   the primary user has a test app pinned to shelf, and
  // *   secondary user has a tab with the URL associated with the app open (but
  //      does not have the app installed).
  auto web_app_info = web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL(kWebAppUrl));
  webapps::AppId installed_app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  PinAppWithIDToShelf(installed_app_id);

  std::unique_ptr<Browser> profile2_browser =
      CreateBrowserAndTabWithProfile(profile2, kWebAppName, kWebAppUrl);

  EXPECT_EQ(
      std::vector<std::string>({app_constants::kChromeAppId, installed_app_id}),
      GetAppsShownInShelf());

  // Switch to the secondary user, and verify the app only installed in the
  // primary profile is removed from the model.
  SwitchActiveUserByAccountId(account_id2);

  EXPECT_EQ(std::vector<std::string>({app_constants::kChromeAppId}),
            GetAppsShownInShelf());

  chrome::CloseTab(profile2_browser.get());
}

// Check that a running windowed V1 application will be properly pinned and
// unpinned when the order gets changed through a profile / policy change.
TEST_F(ChromeShelfControllerTest, RestoreDefaultAndRunningV1AppsResyncOrder) {
  InitShelfController();

  StartPrefSyncService(syncer::SyncDataList());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);

  // The shelf layout has always one static item at the beginning (App List).
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());
  AddExtension(extension2_.get());
  // No new app icon will be generated.
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  // Set the app status as running, which will add an unpinned item.
  shelf_controller_->SetAppStatus(extension2_->id(), ash::STATUS_RUNNING);
  EXPECT_EQ("Chrome, App1, app2", GetPinnedAppStatus());
  AddWebApp(web_app::kGmailAppId);
  EXPECT_EQ("Chrome, App1, Gmail, app2", GetPinnedAppStatus());

  // Now request to pin all items, which will pin the running unpinned items.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, web_app::kGmailAppId);
  InsertAddPinChange(&sync_list1, 1, extension2_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Chrome, Gmail, App2, App1", GetPinnedAppStatus());

  // Removing the requirement for app 2 to be pinned should convert it back to
  // running but not pinned. It should move towards the end of the shelf, after
  // the pinned items, as determined by the |ShelfModel|'s weight system.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, web_app::kGmailAppId);
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
TEST_F(ChromeShelfControllerTest, RestoreDefaultAndRunningV2AppsResyncOrder) {
  InitShelfController();
  syncer::SyncChangeList sync_list0;
  InsertAddPinChange(&sync_list0, 0, extension1_->id());
  InsertAddPinChange(&sync_list0, 1, web_app::kGmailAppId);
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
  AddWebApp(web_app::kGmailAppId);
  EXPECT_EQ("Chrome, App1, Gmail, *platform_app", GetPinnedAppStatus());

  // Now request to pin all items, which should pin the running unpinned item.
  syncer::SyncChangeList sync_list1;
  InsertAddPinChange(&sync_list1, 0, web_app::kGmailAppId);
  InsertAddPinChange(&sync_list1, 1, extension_platform_app_->id());
  InsertAddPinChange(&sync_list1, 2, extension1_->id());
  SendPinChanges(sync_list1, true);
  EXPECT_EQ("Chrome, Gmail, *Platform_App, App1", GetPinnedAppStatus());

  // Removing the requirement for app 2 to be pinned should convert it back to
  // running but not pinned. It should move towards the end of the shelf, after
  // the pinned items, as determined by the |ShelfModel|'s weight system.
  syncer::SyncChangeList sync_list2;
  InsertAddPinChange(&sync_list2, 0, web_app::kGmailAppId);
  InsertAddPinChange(&sync_list2, 1, extension1_->id());
  SendPinChanges(sync_list2, true);
  EXPECT_EQ("Chrome, Gmail, App1, *platform_app", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  syncer::SyncChangeList sync_list3;
  InsertAddPinChange(&sync_list3, 0, web_app::kGmailAppId);
  SendPinChanges(sync_list3, true);
  EXPECT_EQ("Chrome, Gmail, *platform_app", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned. Check that when
// switching between the two users, the state gets properly set.
TEST_F(ChromeShelfControllerTest, UserSwitchIconRestore) {
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
TEST_F(ChromeShelfControllerTest, UserSwitchIconRestoreWithRunningV2App) {
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
TEST_F(ChromeShelfControllerTest,
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

TEST_F(ChromeShelfControllerTest, Policy) {
  // Install some apps.
  InstallSystemWebApp(std::make_unique<CameraSystemAppDelegate>(profile()));

  extension_service_->AddExtension(extension2_.get());

  AddWebApp(web_app::kYoutubeAppId);

  // Here and below we pretend that Gmail is both a preinstalled and a
  // force-installed app and instruct that "gmail" policy_id has to be treated
  // accordingly. This means that both "gmail" and
  // "https://mail.google.com/mail/?usp=installed_webapp" are valid policy_ids
  // for web_app::kGmailAppId.
  constexpr std::string_view kGmailPolicyId = "gmail";

  base::flat_map<std::string_view, std::string_view>
      preinstalled_web_apps_mapping;
  preinstalled_web_apps_mapping.emplace(kGmailPolicyId, web_app::kGmailAppId);
  apps_util::SetPreinstalledWebAppsMappingForTesting(
      preinstalled_web_apps_mapping);

  // Force-install Gmail.
  auto gmail_start_url = GetWebAppUrl(web_app::kGmailAppId);
  auto gmail_install_url = gmail_start_url;
  InstallExternalWebApp(gmail_start_url, gmail_install_url);

  // Start the controller.
  InitShelfController();

  // Pin Camera and Gmail to shelf; they should be synced with `is_user_pinned =
  // true`.
  PinAppWithIDToShelf(web_app::kCameraAppId);
  PinAppWithIDToShelf(web_app::kGmailAppId);

  // `extension2_` is not pinned yet.
  UpdateSyncPinAssertion(extension2_->id(), PinAssertion::kNotPinned);
  UpdateSyncPinAssertion(web_app::kCameraAppId, PinAssertion::kUserPinned);
  UpdateSyncPinAssertion(web_app::kGmailAppId, PinAssertion::kUserPinned);
  ValidateSyncPinAssertions();

  // Construct a pinning policy.
  base::Value::List policy_value;
  AppendPrefValue(policy_value, extension1_->id());
  AppendPrefValue(policy_value, extension2_->id());
  AppendPrefValue(policy_value, ash::SystemWebAppType::CAMERA);
  // Pin Gmail by its install_url (see above).
  AppendPrefValue(policy_value, gmail_install_url.spec());

  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, base::Value(policy_value.Clone()));

  // |extension2_|, Camera and Gmail should get pinned. |extension1_| is
  // specified but not installed, and Youtube is part of the default set, but
  // that shouldn't take effect when the policy override is in place.
  EXPECT_EQ("Chrome, App2, Camera, Gmail", GetPinnedAppStatus());
  EXPECT_TRUE(IsAppPolicyPinned(extension2_->id()));
  EXPECT_TRUE(IsAppPolicyPinned(web_app::kCameraAppId));
  EXPECT_TRUE(IsAppPolicyPinned(web_app::kGmailAppId));

  // `extension2_` is only pinned by the policy, whereas the other two apps have
  // also been previously pinned by us directly.
  UpdateSyncPinAssertion(extension2_->id(), PinAssertion::kOnlyPolicyPinned);
  ValidateSyncPinAssertions();

  // Installing |extension1_| should add it to the shelf. Note, App1 goes
  // before App2 that is aligned with the pin order in policy.
  AddExtension(extension1_.get());
  EXPECT_EQ("Chrome, App1, App2, Camera, Gmail", GetPinnedAppStatus());
  EXPECT_TRUE(IsAppPolicyPinned(extension1_->id()));

  // `extension1_` is now installed and is present in the pinning policy.
  UpdateSyncPinAssertion(extension1_->id(), PinAssertion::kOnlyPolicyPinned);
  ValidateSyncPinAssertions();

  // Removing `extension1_` from the policy should be reflected in the shelf and
  // the pin should get removed as it was only policy-pinned.
  RemovePrefValue(policy_value, extension1_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, base::Value(policy_value.Clone()));
  EXPECT_EQ("Chrome, App2, Camera, Gmail", GetPinnedAppStatus());
  EXPECT_FALSE(IsAppPolicyPinned(extension1_->id()));

  // `extension1_` was introduced by the policy but is no longer a part of it,
  // so we unpin it.
  UpdateSyncPinAssertion(extension1_->id(), PinAssertion::kNotPinned);
  ValidateSyncPinAssertions();

  // Remove Gmail from the policy. It should stay pinned as it was originally
  // pinned by the user (as simulated in the test), but is no longer fixed.
  RemovePrefValue(policy_value, gmail_install_url.spec());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, base::Value(policy_value.Clone()));
  EXPECT_EQ("Chrome, App2, Camera, Gmail", GetPinnedAppStatus());
  EXPECT_FALSE(IsAppPolicyPinned(web_app::kGmailAppId));

  // Gmail was backed by both the policy and the user, but now that it's removed
  // from the policy, only the user pin should persist.
  UpdateSyncPinAssertion(web_app::kGmailAppId, PinAssertion::kUserPinned);
  ValidateSyncPinAssertions();

  // Check that Gmail can also be pinned by direct mapping.
  AppendPrefValue(policy_value, std::string(kGmailPolicyId));
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, base::Value(policy_value.Clone()));
  EXPECT_EQ("Chrome, App2, Camera, Gmail", GetPinnedAppStatus());
  EXPECT_TRUE(IsAppPolicyPinned(web_app::kGmailAppId));

  ValidateSyncPinAssertions();

  // Assert that this configuration can be restored from sync data.
  auto sync_data = app_list_syncable_service_->GetAllSyncDataForTesting();
  ResetShelfController();
  SendPinChanges(syncer::SyncChangeList(), /*reset_pin_model=*/true);
  StopAppSyncService();
  EXPECT_EQ(0U, app_list_syncable_service_->sync_items().size());

  // Remove `extension2_` from the policy and add `extension1_` again to
  // validate that policy deltas are handled correctly.
  RemovePrefValue(policy_value, extension2_->id());
  AppendPrefValue(policy_value, extension1_->id());
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kPolicyPinnedLauncherApps, base::Value(policy_value.Clone()));

  // Simulate UI start.
  StartAppSyncService(sync_data);

  RecreateShelfController()->Init();

  EXPECT_EQ("Chrome, App1, Camera, Gmail", GetPinnedAppStatus());
  UpdateSyncPinAssertion(extension2_->id(), PinAssertion::kNotPinned);
  UpdateSyncPinAssertion(extension1_->id(), PinAssertion::kOnlyPolicyPinned);
  ValidateSyncPinAssertions();
}

TEST_F(ChromeShelfControllerTest, ChromeCannotBeUnpinnedByPolicy) {
  SetPinnedLauncherAppsPolicy(app_constants::kChromeAppId);

  // Start the controller.
  InitShelfController();
  EXPECT_EQ("Chrome", GetPinnedAppStatus());
  UpdateSyncPinAssertion(app_constants::kChromeAppId,
                         PinAssertion::kUserPinned);
  ValidateSyncPinAssertions();

  EXPECT_EQ(app_list_syncable_service_->sync_items().size(), 1U);
  auto chrome_item = GetAppListSpecificsFor(app_constants::kChromeAppId);

  // Simulate an update from an older version with unset `is_user_pinned` paired
  // with valid `item_pin_ordinal`. The expected result is for the client to
  // ignore this empty value and retain the current state.
  app_list_syncable_service_->ProcessSyncChanges(
      FROM_HERE,
      {syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
          app_list::CreateAppRemoteData(
              chrome_item.item_id(), chrome_item.item_name(),
              chrome_item.parent_id(), chrome_item.item_ordinal(),
              chrome_item.item_pin_ordinal(), chrome_item.item_type(),
              /*is_user_pinned=*/std::nullopt))});

  EXPECT_EQ("Chrome", GetPinnedAppStatus());
  UpdateSyncPinAssertion(app_constants::kChromeAppId,
                         PinAssertion::kUserPinned);
  ValidateSyncPinAssertions();

  SetPinnedLauncherAppsPolicy(/**/);

  EXPECT_EQ("Chrome", GetPinnedAppStatus());
  UpdateSyncPinAssertion(app_constants::kChromeAppId,
                         PinAssertion::kUserPinned);
  ValidateSyncPinAssertions();
}

// Tests that an app that is pinned to shelf but doesn't have a pin source
// receives `is_user_pinned = false` during initial merge if the app is listed
// in PinnedLauncherApps policy.
TEST_F(ChromeShelfControllerTest, InitialMergeAndUpdateForPolicyPinnedApps) {
  InstallSystemWebApp(std::make_unique<CameraSystemAppDelegate>(profile()));

  EXPECT_EQ(app_list_syncable_service_->sync_items().size(), 2U);
  auto camera_item = GetAppListSpecificsFor(web_app::kCameraAppId);

  StopAppSyncService();

  // Pin camera by policy.
  SetPinnedLauncherAppsPolicy(ash::SystemWebAppType::CAMERA);

  auto sync_processor = std::make_unique<syncer::FakeSyncChangeProcessor>();
  // Create a pinned app.
  StartAppSyncService(
      {GetSyncDataFor(app_constants::kChromeAppId),
       app_list::CreateAppRemoteData(
           camera_item.item_id(), camera_item.item_name(),
           camera_item.parent_id(), camera_item.item_ordinal(), "pinordinalx")},
      sync_processor.get());

  EXPECT_EQ(sync_processor->changes().size(), 1U);
  const auto& camera_change =
      sync_processor->changes()[0].sync_data().GetSpecifics().app_list();
  EXPECT_EQ(camera_change.item_id(), web_app::kCameraAppId);
  EXPECT_TRUE(
      syncer::StringOrdinal(camera_change.item_pin_ordinal()).IsValid());
  EXPECT_TRUE(camera_change.has_is_user_pinned());
  EXPECT_FALSE(camera_change.is_user_pinned());
}

// Tests that an app can be unpinned by the admin by correctly toggling the
// policy even after the initial sync.
TEST_F(ChromeShelfControllerTest, PostFactumPolicyUnpin) {
  InstallSystemWebApp(std::make_unique<CameraSystemAppDelegate>(profile()));

  auto sync_changes = app_list_syncable_service_->GetAllSyncDataForTesting();
  ASSERT_EQ(sync_changes.size(), 2U);
  auto camera_sync = std::move(sync_changes[1]);
  sync_changes.pop_back();

  const auto& camera_sync_specs = camera_sync.GetSpecifics().app_list();
  ASSERT_EQ(camera_sync_specs.item_id(), web_app::kCameraAppId);

  StopAppSyncService();

  // Create a pinned app.
  StartAppSyncService(
      {std::move(sync_changes[0]),
       app_list::CreateAppRemoteData(
           camera_sync_specs.item_id(), camera_sync_specs.item_name(),
           camera_sync_specs.parent_id(), camera_sync_specs.item_ordinal(),
           "pinordinalx")});

  // Start the controller.
  InitShelfController();

  // App should be pinned, but the exact pin source is unclear.
  EXPECT_EQ("Chrome, Camera", GetPinnedAppStatus());
  UpdateSyncPinAssertion(web_app::kCameraAppId,
                         PinAssertion::kUnknownPinSource);
  ValidateSyncPinAssertions();

  // Pin camera by policy.
  SetPinnedLauncherAppsPolicy(ash::SystemWebAppType::CAMERA);

  // App should be pinned with source = policy.
  EXPECT_EQ("Chrome, Camera", GetPinnedAppStatus());
  UpdateSyncPinAssertion(web_app::kCameraAppId,
                         PinAssertion::kOnlyPolicyPinned);
  ValidateSyncPinAssertions();

  // Reset policy value.
  SetPinnedLauncherAppsPolicy(/**/);

  // App should no longer be pinned.
  EXPECT_EQ("Chrome", GetPinnedAppStatus());
  UpdateSyncPinAssertion(web_app::kCameraAppId, PinAssertion::kNotPinned);
  ValidateSyncPinAssertions();
}

TEST_F(ChromeShelfControllerTest, UnpinWithUninstall) {
  AddWebApp(web_app::kGmailAppId);
  AddWebApp(web_app::kYoutubeAppId);

  InitShelfController();
  StartPrefSyncService(syncer::SyncDataList());

  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kYoutubeAppId));

  RemoveWebApp(web_app::kGmailAppId);

  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kYoutubeAppId));
}

TEST_F(ChromeShelfControllerTest, SyncUpdates) {
  extension_service_->AddExtension(extension2_.get());
  AddWebApp(web_app::kGmailAppId);
  AddWebApp(web_app::kGoogleDocsAppId);

  InitShelfController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 10, app_constants::kChromeAppId);
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_pinned_apps;
  std::vector<std::string> actual_pinned_apps;
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Unavailable extensions don't create shelf items.
  sync_list.clear();
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 3, web_app::kGoogleDocsAppId);
  SendPinChanges(sync_list, false);

  expected_pinned_apps.push_back(extension2_->id());
  expected_pinned_apps.push_back(web_app::kGoogleDocsAppId);
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertAddPinChange(&sync_list, 2, web_app::kGmailAppId);
  SendPinChanges(sync_list, false);
  expected_pinned_apps.insert(expected_pinned_apps.begin() + 1,
                              web_app::kGmailAppId);
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertUpdatePinChange(&sync_list, 0, web_app::kGoogleDocsAppId);
  InsertUpdatePinChange(&sync_list, 1, web_app::kGmailAppId);
  InsertUpdatePinChange(&sync_list, 2, extension2_->id());
  SendPinChanges(sync_list, false);
  std::reverse(expected_pinned_apps.begin(), expected_pinned_apps.end());
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Sending legacy sync change without pin info should not affect pin model.
  sync_list.clear();
  InsertLegacyPinChange(&sync_list, web_app::kGoogleDocsAppId);
  SendPinChanges(sync_list, false);
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, web_app::kGoogleDocsAppId);
  SendPinChanges(sync_list, false);
  expected_pinned_apps.erase(expected_pinned_apps.begin());
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  sync_list.clear();
  InsertRemovePinChange(&sync_list, web_app::kGmailAppId);
  InsertRemovePinChange(&sync_list, extension2_->id());
  SendPinChanges(sync_list, false);
  expected_pinned_apps.clear();
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);
}

TEST_F(ChromeShelfControllerTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  AddWebApp(web_app::kGmailAppId);

  InitShelfController();

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 2, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);

  std::vector<std::string> expected_pinned_apps;
  expected_pinned_apps.push_back(extension1_->id());
  expected_pinned_apps.push_back(web_app::kGmailAppId);
  std::vector<std::string> actual_pinned_apps;

  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);

  // Install |extension2| and verify it shows up between the other two.
  AddExtension(extension2_.get());
  expected_pinned_apps.insert(expected_pinned_apps.begin() + 1,
                              extension2_->id());
  GetPinnedAppIds(shelf_controller_.get(), &actual_pinned_apps);
  EXPECT_EQ(expected_pinned_apps, actual_pinned_apps);
}

// Ensure |controller| creates the expected menu items for the given shelf item.
void CheckAppMenu(ChromeShelfController* controller,
                  const ash::ShelfItem& item,
                  size_t expected_item_count,
                  std::u16string expected_item_titles[]) {
  auto items = controller->GetAppMenuItemsForTesting(item);
  ASSERT_EQ(expected_item_count, items.size());
  for (size_t i = 0; i < expected_item_count; i++)
    EXPECT_EQ(expected_item_titles[i], items[i].title);
}

// Check that browsers get reflected correctly in the shelf menu.
TEST_F(ChromeShelfControllerTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  chrome::NewTab(browser());

  InitShelfController();

  // Check that the browser list is empty at this time.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(app_constants::kChromeAppId);
  CheckAppMenu(shelf_controller_.get(), item_browser, 0, nullptr);

  // Now make the created browser() visible by showing its browser window.
  browser()->window()->Show();
  std::u16string title1 = u"Test1";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  std::u16string one_menu_item[] = {title1};

  CheckAppMenu(shelf_controller_.get(), item_browser, 1, one_menu_item);

  // Create one more browser/window and check that one more was added.
  std::unique_ptr<Browser> browser2(
      CreateBrowserWithTestWindowForProfile(profile()));
  chrome::NewTab(browser2.get());
  browser2->window()->Show();
  std::u16string title2 = u"Test2";
  NavigateAndCommitActiveTabWithTitle(browser2.get(), GURL("http://test2"),
                                      title2);

  // Check that the list contains now two entries - make furthermore sure that
  // the active item is the first entry.
  std::u16string two_menu_items[] = {title1, title2};
  CheckAppMenu(shelf_controller_.get(), item_browser, 2, two_menu_items);

  // Apparently we have to close all tabs we have.
  chrome::CloseTab(browser2.get());
}

// Check the multi profile case where only user related browsers should show up.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       BrowserMenuGenerationTwoUsers) {
  // Create a browser item in the controller.
  InitShelfController();

  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(app_constants::kChromeAppId);

  // Check that the menu is empty.
  chrome::NewTab(browser());
  CheckAppMenu(shelf_controller_.get(), item_browser, 0, nullptr);

  // Show the created |browser()| by showing its window.
  browser()->window()->Show();
  std::u16string title1 = u"Test1";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  std::u16string one_menu_item1[] = {title1};
  CheckAppMenu(shelf_controller_.get(), item_browser, 1, one_menu_item1);

  // Create a browser for another user and check that it is not included in the
  // users running browser list.
  std::string user2 = "user2@example.com";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  std::unique_ptr<Browser> browser2(
      CreateBrowserAndTabWithProfile(profile2, "user2", "http://test2"));
  std::u16string one_menu_item2[] = {u"user2"};
  CheckAppMenu(shelf_controller_.get(), item_browser, 1, one_menu_item1);

  // Switch to the other user and make sure that only that browser window gets
  // shown.
  SwitchActiveUserByAccountId(account_id2);
  CheckAppMenu(shelf_controller_.get(), item_browser, 1, one_menu_item2);

  // Transferred browsers of other users should not show up in the list.
  MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);
  CheckAppMenu(shelf_controller_.get(), item_browser, 1, one_menu_item2);

  chrome::CloseTab(browser2.get());
}

// Check that V1 apps are correctly reflected in the shelf menu using the
// refocus logic.
// Note that the extension matching logic is tested by the extension system
// and does not need a separate test here.
TEST_F(ChromeShelfControllerTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  InitShelfControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));

  // Installing Gmail pins it to the shelf.
  const ash::ShelfID gmail_id(web_app::kGmailAppId);
  AddWebApp(web_app::kGmailAppId);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  SetRefocusURL(gmail_id, GURL(kGmailUrl));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(app_constants::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  CheckAppMenu(shelf_controller_.get(), item_gmail, 0, nullptr);

  // Set the gmail URL to a new tab.
  std::u16string title1 = u"Test1";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);

  std::u16string one_menu_item[] = {title1};
  CheckAppMenu(shelf_controller_.get(), item_gmail, 1, one_menu_item);

  // Create one empty tab.
  chrome::NewTab(browser());
  std::u16string title2 = u"Test2";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("https://bla"), title2);

  // and another one with another gmail instance.
  chrome::NewTab(browser());
  std::u16string title3 = u"Test3";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title3);
  std::u16string two_menu_items[] = {title1, title3};
  CheckAppMenu(shelf_controller_.get(), item_gmail, 2, two_menu_items);

  // Even though the item is in the V1 app list, it should also be in the
  // browser list.
  std::u16string browser_menu_item[] = {title3};
  CheckAppMenu(shelf_controller_.get(), item_browser, 1, browser_menu_item);

  // Test that closing of (all) the item(s) does work (and all menus get
  // updated properly).
  shelf_controller_->Close(item_gmail.id);

  CheckAppMenu(shelf_controller_.get(), item_gmail, 0, nullptr);
  std::u16string browser_menu_item2[] = {title2};
  CheckAppMenu(shelf_controller_.get(), item_browser, 1, browser_menu_item2);
}

// Check the multi profile case where only user related apps should show up.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V1AppMenuGenerationTwoUsers) {
  // Create a browser item in the controller.
  InitShelfController();
  StartPrefSyncService(syncer::SyncDataList());
  chrome::NewTab(browser());

  // Installing Gmail pins it to the shelf.
  const ash::ShelfID gmail_id(web_app::kGmailAppId);
  AddWebApp(web_app::kGmailAppId);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app::kGmailAppId));
  SetRefocusURL(gmail_id, GURL(kGmailUrl));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id = ash::ShelfID(app_constants::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  CheckAppMenu(shelf_controller_.get(), item_gmail, 0, nullptr);

  // Set the gmail URL to a new tab.
  std::u16string title1 = u"Test1";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);

  std::u16string one_menu_item[] = {title1};
  CheckAppMenu(shelf_controller_.get(), item_gmail, 1, one_menu_item);

  // Create a second profile and switch to that user.
  std::string user2 = "user2@example.com";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  SwitchActiveUserByAccountId(account_id2);

  // No item should have content yet.
  CheckAppMenu(shelf_controller_.get(), item_browser, 0, nullptr);
  CheckAppMenu(shelf_controller_.get(), item_gmail, 0, nullptr);

  // Transfer the browser of the first user - it should still not show up.
  MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(), account_id2);

  CheckAppMenu(shelf_controller_.get(), item_browser, 0, nullptr);
  CheckAppMenu(shelf_controller_.get(), item_gmail, 0, nullptr);
}

// Check that V2 applications are creating items properly in the shelf when
// instantiated by the current user.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V2AppHandlingTwoUsers) {
  InitShelfController();
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  // Check that there is a browser.
  EXPECT_EQ(1, model_->item_count());

  // Add a v2 app.
  AddExtension(extension1_.get());
  V2App v2_app(profile(), extension1_.get());
  EXPECT_EQ(2, model_->item_count());

  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2@example.com");
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  // After switching users the item should go away.
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_EQ(1, model_->item_count());

  // And it should come back when switching back.
  SwitchActiveUserByAccountId(account_id);
  EXPECT_EQ(2, model_->item_count());
}

// Check that V2 applications are creating items properly in edge cases:
// a background user creates a V2 app, gets active and inactive again and then
// deletes the app.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V2AppHandlingTwoUsersEdgeCases) {
  InitShelfController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2@example.com");
  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // Check that there is a browser, back button and an app.
  EXPECT_EQ(1, model_->item_count());

  // Switch to an inactive user.
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_EQ(1, model_->item_count());

  // Add the v2 app to the inactive user and check that no item was added to
  // the shelf.
  {
    AddExtension(extension1_.get());
    V2App v2_app(profile(), extension1_.get());
    EXPECT_EQ(1, model_->item_count());

    // Switch to the primary user and check that the item is shown.
    SwitchActiveUserByAccountId(account_id);
    EXPECT_EQ(2, model_->item_count());

    // Switch to the second user and check that the item goes away - even if the
    // item gets closed.
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(1, model_->item_count());
  }

  // After the application was killed there should still be 1 item.
  EXPECT_EQ(1, model_->item_count());

  // Switching then back to the default user should not show the additional
  // item anymore.
  SwitchActiveUserByAccountId(account_id);
  EXPECT_EQ(1, model_->item_count());
}

TEST_F(ChromeShelfControllerTest, Active) {
  InitShelfController();

  // Creates a new app window.
  int initial_item_count = model_->item_count();
  AddExtension(extension1_.get());
  V2App app_1(profile(), extension1_.get());
  EXPECT_TRUE(app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_EQ(initial_item_count + 1, model_->item_count());
  ash::ShelfItemDelegate* app_item_delegate_1 =
      model_->GetShelfItemDelegate(model_->items()[initial_item_count].id);
  ASSERT_TRUE(app_item_delegate_1);
  AppWindowShelfItemController* app_item_controller_1 =
      app_item_delegate_1->AsAppWindowShelfItemController();
  ASSERT_TRUE(app_item_controller_1);
  ui::BaseWindow* last_active =
      GetLastActiveWindowForItemController(app_item_controller_1);
  EXPECT_EQ(app_1.window()->GetNativeWindow(), last_active->GetNativeWindow());
  // Change the status so that we can verify it gets reset when the active
  // window changes.
  shelf_controller_->SetItemStatus(app_item_delegate_1->shelf_id(),
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
  AppWindowShelfItemController* app_item_controller_2 =
      app_item_delegate_2->AsAppWindowShelfItemController();
  ASSERT_TRUE(app_item_controller_2);
  last_active = GetLastActiveWindowForItemController(app_item_controller_2);
  EXPECT_EQ(app_2.window()->GetNativeWindow(), last_active->GetNativeWindow());
  const ash::ShelfItem* shelf_item_1 =
      shelf_controller_->GetItem(app_item_delegate_1->shelf_id());
  ASSERT_TRUE(shelf_item_1);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_item_1->status);

  shelf_controller_->SetItemStatus(app_item_delegate_2->shelf_id(),
                                   ash::STATUS_ATTENTION);

  // Activate the first window, which should reset the status of the
  // second apps window.
  app_1.window()->GetBaseWindow()->Activate();
  const ash::ShelfItem* shelf_item_2 =
      shelf_controller_->GetItem(app_item_delegate_2->shelf_id());
  ASSERT_TRUE(shelf_item_2);
  EXPECT_EQ(ash::STATUS_RUNNING, shelf_item_2->status);
}

// Check that V2 applications will be made visible on the target desktop if
// another window of the same type got previously teleported there.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V2AppFollowsTeleportedWindow) {
  InitShelfController();
  ash::MultiUserWindowManager* window_manager =
      MultiUserWindowManagerHelper::GetWindowManager();

  // Create and add three users / profiles, and go to #1's desktop.
  TestingProfile* profile1 = CreateMultiUserProfile("user-1@example.com");
  TestingProfile* profile2 = CreateMultiUserProfile("user-2@example.com");
  TestingProfile* profile3 = CreateMultiUserProfile("user-3@example.com");
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

  SwitchActiveUserByAccountId(account_id1);

  // A v2 app for user #1 should be shown first and get hidden when switching
  // to desktop #2.
  extension_service1->AddExtension(extension1_.get());
  V2App v2_app_1(profile1, extension1_.get());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUserByAccountId(account_id2);
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
  SwitchActiveUserByAccountId(account_id1);
  V2App v2_app_4(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_3.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_4.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #3 and creating an app for user #1 should place it
  // on that user's desktop (#1).
  SwitchActiveUserByAccountId(account_id3);
  V2App v2_app_5(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_5.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUserByAccountId(account_id1);
  EXPECT_TRUE(v2_app_5.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #2, hiding the app window and creating an app should
  // teleport there automatically.
  SwitchActiveUserByAccountId(account_id2);
  v2_app_1.window()->Hide();
  V2App v2_app_6(profile1, extension1_.get());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_6.window()->GetNativeWindow()->IsVisible());
}

// Check that V2 applications hide correctly on the shelf when the app window
// is hidden.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       V2AppHiddenWindows) {
  InitShelfController();

  TestingProfile* profile2 = CreateMultiUserProfile("user-2@example.com");
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));
  // If switch to account_id2 is not run, the following switch to account_id
  // is invalid, because the user account is not changed, so switch to
  // account_id2 first.
  SwitchActiveUserByAccountId(account_id2);

  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  SwitchActiveUserByAccountId(account_id);
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
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Show(extensions::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(1, model_->item_count());

    SwitchActiveUserByAccountId(account_id);
    EXPECT_EQ(2, model_->item_count());
  }
  {
    // Switch user, hide the app, switch back and then show it again.
    SwitchActiveUserByAccountId(account_id2);
    EXPECT_EQ(1, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(1, model_->item_count());

    SwitchActiveUserByAccountId(account_id);
    // The following expectation does not work in current impl. It was working
    // before because MultiProfileSupport is not attached to user associated
    // with profile() hence not actually handling windows for the user. It is
    // a real bug. See http://crbug.com/693634 EXPECT_EQ(2,
    // model_->item_count());

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
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       SpinnersUpdateOnUserSwitch) {
  InitShelfController();

  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const std::string user2 = "user2@example.com";
  const TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string app_id = extension1_->id();
  extension_service_->AddExtension(extension1_.get());

  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Add a spinner to the shelf
  shelf_controller_->GetShelfSpinnerController()->AddSpinnerToShelf(
      app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch to a new profile
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch back
  SwitchActiveUserByAccountId(account_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Close the spinner
  shelf_controller_->GetShelfSpinnerController()->CloseSpinner(app_id);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));
}

// Checks that pinned spinners are hidden and restored on profile switching
// but are not removed when the spinner closes.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeShelfControllerTest,
       PinnedSpinnersUpdateOnUserSwitch) {
  InitShelfController();

  const AccountId account_id(
      multi_user_util::GetAccountIdFromProfile(profile()));
  const std::string user2 = "user2@example.com";
  const TestingProfile* profile2 = CreateMultiUserProfile(user2);
  const AccountId account_id2(
      multi_user_util::GetAccountIdFromProfile(profile2));

  const std::string app_id = extension1_->id();
  AddExtension(extension1_.get());

  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Pin an app to the shelf
  PinAppWithIDToShelf(app_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Activate the spinner
  shelf_controller_->GetShelfSpinnerController()->AddSpinnerToShelf(
      app_id, std::make_unique<ShelfSpinnerItemController>(app_id));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch to a new profile
  SwitchActiveUserByAccountId(account_id2);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Switch back
  SwitchActiveUserByAccountId(account_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));

  // Close the spinner
  shelf_controller_->GetShelfSpinnerController()->CloseSpinner(app_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));
}

// Checks that the generated menu list properly activates items.
TEST_F(ChromeShelfControllerTest, V1AppMenuExecution) {
  InitShelfControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // Add Gmail to the shelf and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  const ash::ShelfID gmail_id(web_app::kGmailAppId);
  AddWebApp(web_app::kGmailAppId);
  SetRefocusURL(gmail_id, GURL(kGmailUrl));
  std::u16string title1 = u"Test1";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);
  chrome::NewTab(browser());
  std::u16string title2 = u"Test2";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title2);

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  std::u16string two_menu_items[] = {title1, title2};
  CheckAppMenu(shelf_controller_.get(), item_gmail, 2, two_menu_items);
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(gmail_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Execute the second item in the menu, after the title,
  // this shouldn't do anything since that item is already the active tab.
  {
    ash::ShelfApplicationMenuModel menu(
        std::u16string(),
        shelf_controller_->GetAppMenuItemsForTesting(item_gmail),
        item_delegate);
    menu.ActivatedAt(2);
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Execute the first item in the menu, after the title,
  // this should activate the other tab.
  {
    ash::ShelfApplicationMenuModel menu(
        std::u16string(),
        shelf_controller_->GetAppMenuItemsForTesting(item_gmail),
        item_delegate);
    menu.ActivatedAt(1);
  }
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Checks that the generated menu list properly deletes items.
TEST_F(ChromeShelfControllerTest, V1AppMenuDeletionExecution) {
  InitShelfControllerWithBrowser();
  StartPrefSyncService(syncer::SyncDataList());

  // Add Gmail to the shelf and add two items.
  const ash::ShelfID gmail_id(web_app::kGmailAppId);
  AddWebApp(web_app::kGmailAppId);
  SetRefocusURL(gmail_id, GURL(kGmailUrl));
  std::u16string title1 = u"Test1";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title1);
  chrome::NewTab(browser());
  std::u16string title2 = u"Test2";
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(kGmailUrl), title2);

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_PINNED_APP;
  item_gmail.id = gmail_id;
  std::u16string two_menu_items[] = {title1, title2};
  CheckAppMenu(shelf_controller_.get(), item_gmail, 2, two_menu_items);

  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(gmail_id);
  ASSERT_TRUE(item_delegate);
  int tabs = browser()->tab_strip_model()->count();
  // Activate the proper tab through the menu item.
  {
    auto items = shelf_controller_->GetAppMenuItemsForTesting(item_gmail);
    item_delegate->ExecuteCommand(false, 1, ui::EF_NONE,
                                  display::kInvalidDisplayId);
    EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  }

  // Delete one tab through the menu item.
  {
    auto items = shelf_controller_->GetAppMenuItemsForTesting(item_gmail);
    item_delegate->ExecuteCommand(false, 1, ui::EF_SHIFT_DOWN,
                                  display::kInvalidDisplayId);
    EXPECT_EQ(--tabs, browser()->tab_strip_model()->count());
  }
}

// Verify that the shelf item positions are persisted and restored.
TEST_F(ChromeShelfControllerTest, PersistShelfItemPositions) {
  InitShelfController();

  TestShelfControllerHelper* helper = new TestShelfControllerHelper;
  SetShelfControllerHelper(helper);

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[0].type);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip_model->count());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");

  EXPECT_FALSE(shelf_controller_->IsAppPinned("1"));
  PinAppWithIDToShelf("1");
  EXPECT_TRUE(shelf_controller_->IsAppPinned("1"));
  PinAppWithIDToShelf("2");

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[2].type);

  // Move browser shortcut item from index 0 to index 2.
  model_->Move(0, 2);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);

  RecreateShelfController();
  helper = new TestShelfControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");
  SetShelfControllerHelper(helper);
  shelf_controller_->Init();

  // Check ShelfItems are restored after resetting ChromeShelfController.
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[2].type);
}

// Verifies pinned apps are persisted and restored.
TEST_F(ChromeShelfControllerTest, PersistPinned) {
  InitShelfControllerWithBrowser();
  size_t initial_size = model_->items().size();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());

  TestShelfControllerHelper* helper = new TestShelfControllerHelper;
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetShelfControllerHelper(helper);

  // app_icon_loader is owned by ChromeShelfController.
  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl;
  app_icon_loader->AddSupportedApp("1");
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));
  EXPECT_EQ(0, app_icon_loader->fetch_count());

  PinAppWithIDToShelf("1");
  int app_index = model_->ItemIndexByID(ash::ShelfID("1"));
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[app_index].type);
  EXPECT_TRUE(shelf_controller_->IsAppPinned("1"));
  EXPECT_FALSE(shelf_controller_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, model_->items().size());

  RecreateShelfController();
  helper = new TestShelfControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetShelfControllerHelper(helper);
  // app_icon_loader is owned by ChromeShelfController.
  app_icon_loader = new TestAppIconLoaderImpl;
  app_icon_loader->AddSupportedApp("1");
  SetAppIconLoader(std::unique_ptr<AppIconLoader>(app_icon_loader));
  shelf_controller_->Init();

  app_index = model_->ItemIndexByID(ash::ShelfID("1"));
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  ASSERT_EQ(initial_size + 1, model_->items().size());
  EXPECT_TRUE(shelf_controller_->IsAppPinned("1"));
  EXPECT_FALSE(shelf_controller_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[app_index].type);

  shelf_controller_->UnpinAppWithID("1");
  EXPECT_EQ(initial_size + 1, model_->items().size());

  tab_strip_model->CloseWebContentsAt(0, 0);
  EXPECT_EQ(initial_size, model_->items().size());
}

// Verifies that ShelfID property is updated for browsers that are present when
// ChromeShelfController is created.
TEST_F(ChromeShelfControllerTest, ExistingBrowserWindowShelfIDSet) {
  InitShelfControllerWithBrowser();
  PinAppWithIDToShelf("1");

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(1, tab_strip_model->count());

  TestShelfControllerHelper* helper = new TestShelfControllerHelper;
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "0");
  SetShelfControllerHelper(helper);

  RecreateShelfController();
  helper = new TestShelfControllerHelper(profile());
  helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetShelfControllerHelper(helper);
  shelf_controller_->Init();

  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID("1")));
  EXPECT_EQ(ash::ShelfID("1"),
            ash::ShelfID::Deserialize(
                browser()->window()->GetNativeWindow()->GetProperty(
                    ash::kShelfIDKey)));
}

TEST_F(ChromeShelfControllerTest, MultipleAppIconLoaders) {
  InitShelfControllerWithBrowser();

  const ash::ShelfID shelf_id1(extension1_->id());
  const ash::ShelfID shelf_id2(extension2_->id());
  const ash::ShelfID shelf_id3(web_app::kGmailAppId);
  // app_icon_loader1 and app_icon_loader2 are owned by
  // ChromeShelfController.
  TestAppIconLoaderImpl* app_icon_loader1 = new TestAppIconLoaderImpl();
  TestAppIconLoaderImpl* app_icon_loader2 = new TestAppIconLoaderImpl();
  app_icon_loader1->AddSupportedApp(shelf_id1.app_id);
  app_icon_loader2->AddSupportedApp(shelf_id2.app_id);
  SetAppIconLoaders(std::unique_ptr<AppIconLoader>(app_icon_loader1),
                    std::unique_ptr<AppIconLoader>(app_icon_loader2));

  shelf_controller_->CreateAppItem(
      std::make_unique<AppServiceAppWindowShelfItemController>(
          shelf_id3, shelf_controller_->app_service_app_window_controller()),
      ash::STATUS_RUNNING, /*pinned=*/false);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(0, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  shelf_controller_->CreateAppItem(
      std::make_unique<AppServiceAppWindowShelfItemController>(
          shelf_id2, shelf_controller_->app_service_app_window_controller()),
      ash::STATUS_RUNNING, /*pinned=*/false);
  EXPECT_EQ(0, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  shelf_controller_->CreateAppItem(
      std::make_unique<AppServiceAppWindowShelfItemController>(
          shelf_id1, shelf_controller_->app_service_app_window_controller()),
      ash::STATUS_RUNNING, /*pinned=*/false);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(0, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  shelf_controller_->ReplaceWithAppShortcutOrRemove(shelf_id1);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(0, app_icon_loader2->clear_count());

  shelf_controller_->ReplaceWithAppShortcutOrRemove(shelf_id2);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(1, app_icon_loader2->clear_count());

  shelf_controller_->ReplaceWithAppShortcutOrRemove(shelf_id3);
  EXPECT_EQ(1, app_icon_loader1->fetch_count());
  EXPECT_EQ(1, app_icon_loader1->clear_count());
  EXPECT_EQ(1, app_icon_loader2->fetch_count());
  EXPECT_EQ(1, app_icon_loader2->clear_count());
}

TEST_F(ChromeShelfControllerWithArcTest, ArcAppPinPolicy) {
  InitShelfControllerWithBrowser();

  constexpr char kExampleArcPackageName[] = "com.example.app";

  arc::mojom::AppInfoPtr appinfo =
      CreateAppInfo("Some App", "SomeActivity", kExampleArcPackageName);
  const std::string example_app_id = AddArcAppAndShortcut(*appinfo);

  // Sets up policy that pins this ARC app. Unlike native extensions, ARC apps
  // are pinned by |package_name| rather than the actual |app_id|.
  SetPinnedLauncherAppsPolicy(kExampleArcPackageName);

  EXPECT_TRUE(shelf_controller_->IsAppPinned(example_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(example_app_id, profile()));
}

TEST_F(ChromeShelfControllerWithArcTest, IwaPinPolicy) {
  InitShelfControllerWithBrowser();

  constexpr char kExampleIwaBundleId[] =
      "w2gqjem6b4m7vhiqpjr3btcpp7dxfyjt6h4uuyuxklcsmygtgncaaaac";

  const auto url_info = AddIsolatedWebApp(
      GURL{base::StrCat({"isolated-app://", kExampleIwaBundleId})});
  SetPinnedLauncherAppsPolicy(kExampleIwaBundleId);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(url_info.app_id()));
  EXPECT_TRUE(AppListControllerDelegate::PIN_FIXED ==
              GetPinnableForAppID(url_info.app_id(), profile()));
}

TEST_F(ChromeShelfControllerWithArcTest, ApkWebAppPinPolicy) {
  InitShelfControllerWithBrowser();

  constexpr char kMapsWebPackageName[] = "com.google.maps";

  auto* service = ash::ApkWebAppService::Get(browser()->profile());
  ASSERT_TRUE(service);

  base::test::TestFuture<const std::string&, const webapps::AppId&> future;
  service->SetWebAppInstalledCallbackForTesting(future.GetCallback());

  auto package = arc::mojom::ArcPackageInfo::New(kMapsWebPackageName, 1, 1, 1,
                                                 /*sync=*/true);
  package->web_app_info =
      arc::mojom::WebAppInfo::New("Google Maps", "https://www.google.com/maps/",
                                  "https://www.google.com/", 1000000);
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(std::move(package));
  arc_test_.app_instance()->SendRefreshPackageList(std::move(packages));

  auto [maps_package_name, maps_app_id] = future.Take();
  ASSERT_EQ(maps_package_name, kMapsWebPackageName);

  // Sets up policy that pins this apk-based Web App. Unlike regular Web Apps,
  // Web Apps originating from apks are pinned by |package_name| rather than
  // their |install_url|.
  SetPinnedLauncherAppsPolicy(kMapsWebPackageName);

  EXPECT_TRUE(shelf_controller_->IsAppPinned(maps_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(maps_app_id, profile()));
}

TEST_F(ChromeShelfControllerWithArcTest, ArcManaged) {
  extension_service_->AddExtension(arc_support_host_.get());
  // Test enables ARC, so turn it off for initial values.
  EnablePlayStore(false);

  InitShelfController();

  // To prevent import legacy pins each time.
  // Initially pins are imported from legacy pref based model.
  StartPrefSyncService(syncer::SyncDataList());

  // Initial run, ARC is not managed and disabled, Play Store pin should be
  // available.
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Chrome, Play Store");

  // ARC is managed and enabled, Play Store pin should be available.
  // Note: CHECKING_REQUIREMENTS here means that opt-in flow is skipped.
  profile()->GetTestingPrefService()->SetManagedPref(
      arc::prefs::kArcEnabled, std::make_unique<base::Value>(true));
  base::RunLoop().RunUntilIdle();
  ValidateArcState(true, true,
                   arc::ArcSessionManager::State::CHECKING_REQUIREMENTS,
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
  // Note: CHECKING_REQUIREMENTS here means that opt-in flow starts.
  EnablePlayStore(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::CHECKING_REQUIREMENTS,
                   "Chrome, Play Store");

  // User disables ARC. ARC is not managed and disabled, Play Store pin should
  // be automatically removed.
  EnablePlayStore(false);
  ValidateArcState(false, false, arc::ArcSessionManager::State::STOPPED,
                   "Chrome");

  // Even if re-enable it again, Play Store pin does not appear automatically.
  EnablePlayStore(true);
  ValidateArcState(true, false,
                   arc::ArcSessionManager::State::CHECKING_REQUIREMENTS,
                   "Chrome");
}

// Test the application menu of a shelf item with multiple ARC windows.
TEST_F(ChromeShelfControllerWithArcTest, ShelfItemWithMultipleWindows) {
  InitShelfControllerWithBrowser();

  arc::mojom::AppInfoPtr appinfo =
      CreateAppInfo("Test1", "test", "com.example.app");
  AddArcAppAndShortcut(*appinfo);

  // Widgets will be deleted by the system.
  NotifyOnTaskCreated(*appinfo, 1 /* task_id */);
  views::Widget* window1 = CreateArcWindow("org.chromium.arc.1");
  ASSERT_TRUE(window1);
  EXPECT_TRUE(window1->IsActive());

  NotifyOnTaskCreated(*appinfo, 2 /* task_id */);
  views::Widget* window2 = CreateArcWindow("org.chromium.arc.2");
  ASSERT_TRUE(window2);

  EXPECT_FALSE(window1->IsActive());
  EXPECT_TRUE(window2->IsActive());

  const std::string app_id = ArcAppTest::GetAppId(*appinfo);
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

class ChromeShelfControllerArcDefaultAppsTest
    : public ChromeShelfControllerTestBase {
 public:
  ChromeShelfControllerArcDefaultAppsTest() = default;
  ChromeShelfControllerArcDefaultAppsTest(
      const ChromeShelfControllerArcDefaultAppsTest&) = delete;
  ChromeShelfControllerArcDefaultAppsTest& operator=(
      const ChromeShelfControllerArcDefaultAppsTest&) = delete;
  ~ChromeShelfControllerArcDefaultAppsTest() override = default;

 protected:
  void SetUp() override {
    ArcAppIcon::DisableSafeDecodingForTesting();
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeShelfControllerTestBase::SetUp();
  }
};

class ChromeShelfControllerPlayStoreAvailabilityTest
    : public ChromeShelfControllerTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeShelfControllerPlayStoreAvailabilityTest() = default;
  ChromeShelfControllerPlayStoreAvailabilityTest(
      const ChromeShelfControllerPlayStoreAvailabilityTest&) = delete;
  ChromeShelfControllerPlayStoreAvailabilityTest& operator=(
      const ChromeShelfControllerPlayStoreAvailabilityTest&) = delete;
  ~ChromeShelfControllerPlayStoreAvailabilityTest() override = default;

 protected:
  void SetUp() override {
    if (GetParam())
      arc::SetArcAlwaysStartWithoutPlayStoreForTesting();
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();
    ArcDefaultAppList::UseTestAppsDirectory();
    ChromeShelfControllerTestBase::SetUp();
  }
};

}  // namespace

// TODO(crbug.com/40890072) Test is flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_DefaultApps DISABLED_DefaultApps
#else
#define MAYBE_DefaultApps DefaultApps
#endif
TEST_F(ChromeShelfControllerArcDefaultAppsTest, MAYBE_DefaultApps) {
  arc_test_.SetUp(profile());
  InitShelfController();

  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EnablePlayStore(false);
  EXPECT_FALSE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  ASSERT_TRUE(prefs->GetAppIds().size());

  const std::string app_id =
      ArcAppTest::GetAppId(*arc_test_.fake_default_apps()[0]);
  const ash::ShelfID shelf_id(app_id);
  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id));
  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                             arc::UserInteractionType::NOT_USER_INITIATED));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id));

  // Stop ARC again. Shelf item should go away.
  EnablePlayStore(false);

  EXPECT_FALSE(shelf_controller_->GetItem(shelf_id));

  EXPECT_TRUE(arc::LaunchApp(profile(), app_id, ui::EF_LEFT_MOUSE_BUTTON,
                             arc::UserInteractionType::NOT_USER_INITIATED));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id));

  auto* item_delegate = model_->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));
  // Initially, a default icon is set for the shelf item.
  EXPECT_FALSE(item_delegate->image_set_by_controller());

  std::string window_app_id("org.chromium.arc.1");
  CreateArcWindow(window_app_id);
  arc_test_.app_instance()->SendTaskCreated(
      1, *arc_test_.fake_default_apps()[0], std::string());
  EXPECT_TRUE(shelf_controller_->GetItem(shelf_id));
  // Refresh delegate, it was changed.
  item_delegate = model_->GetShelfItemDelegate(shelf_id);
  ASSERT_TRUE(item_delegate);
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(app_id));
  EXPECT_FALSE(item_delegate->image_set_by_controller());

  // Wait for the real app icon image to be decoded and set for the shelf item.
  base::RunLoop().RunUntilIdle();
  const std::vector<ui::ResourceScaleFactor>& scale_factors =
      ui::GetSupportedResourceScaleFactors();
  for (const auto scale_factor : scale_factors) {
    // Force the icon to be loaded.
    shelf_controller_->GetItem(shelf_id)->image.GetRepresentation(
        ui::GetScaleForResourceScaleFactor(scale_factor));
  }
  EXPECT_TRUE(
      ValidateImageIsFullyLoaded(shelf_controller_->GetItem(shelf_id)->image));
}

TEST_F(ChromeShelfControllerArcDefaultAppsTest, PlayStoreDeferredLaunch) {
  // Add ARC host app to enable Play Store default app.
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());
  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EXPECT_TRUE(prefs->IsRegistered(arc::kPlayStoreAppId));

  InitShelfController();

  EnablePlayStore(true);

  // Pin Play Store. It should be pinned but not scheduled for deferred launch.
  PinAppWithIDToShelf(arc::kPlayStoreAppId);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc::kPlayStoreAppId));
  EXPECT_FALSE(shelf_controller_->GetShelfSpinnerController()->HasApp(
      arc::kPlayStoreAppId));

  // Simulate click. This should schedule Play Store for deferred launch.
  ash::ShelfItemDelegate* item_delegate =
      model_->GetShelfItemDelegate(ash::ShelfID(arc::kPlayStoreAppId));
  EXPECT_TRUE(item_delegate);
  SelectItem(item_delegate);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc::kPlayStoreAppId));
  EXPECT_TRUE(shelf_controller_->GetShelfSpinnerController()->HasApp(
      arc::kPlayStoreAppId));
}

TEST_F(ChromeShelfControllerArcDefaultAppsTest, PlayStoreLaunchMetric) {
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());
  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();

  InitShelfController();
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
  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(arc::mojom::AppInfo::New("", arc::kPlayStorePackage,
                                             arc::kPlayStoreActivity));
  arc_test_.app_instance()->SendRefreshAppList(apps);
  ASSERT_EQ(1U, arc_test_.app_instance()->launch_intents().size());
  std::string play_store_window_id("org.chromium.arc.1");
  views::Widget* play_store_window = CreateArcWindow(play_store_window_id);
  arc_test_.app_instance()->SendTaskCreated(
      1, *apps[0], arc_test_.app_instance()->launch_intents()[0]);
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc::kPlayStoreAppId)));
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
      2, *apps[0], arc_test_.app_instance()->launch_intents()[1]);
  EXPECT_TRUE(shelf_controller_->GetItem(ash::ShelfID(arc::kPlayStoreAppId)));
  // UMA is reported for app-ready launch. Note, previous call of SnapshotDelta
  // resets samples, so we expect here only one recorded.
  EXPECT_EQ(1, histogram->SnapshotDelta()->TotalCount());
  play_store_window->Close();
}

TEST_F(ChromeShelfControllerArcDefaultAppsTest, DeferredLaunchMetric) {
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());

  InitShelfController();
  EnablePlayStore(true);

  constexpr char kHistogramName[] =
      "Arc.FirstAppLaunchDelay.TimeDeltaUntilAppLaunch";

  // Launch Play Store in deferred mode.
  arc::LaunchApp(profile(), arc::kPlayStoreAppId, ui::EF_LEFT_MOUSE_BUTTON,
                 arc::UserInteractionType::NOT_USER_INITIATED);

  EXPECT_FALSE(base::StatisticsRecorder::FindHistogram(kHistogramName));

  std::vector<arc::mojom::AppInfoPtr> apps;
  apps.emplace_back(arc::mojom::AppInfo::New("", arc::kPlayStorePackage,
                                             arc::kPlayStoreActivity));
  arc_test_.app_instance()->SendRefreshAppList(apps);

  // No window attached at this time.
  EXPECT_FALSE(base::StatisticsRecorder::FindHistogram(kHistogramName));

  std::string play_store_window_id("org.chromium.arc.1");
  views::Widget* const play_store_window =
      CreateArcWindow(play_store_window_id);
  ASSERT_EQ(1U, arc_test_.app_instance()->launch_intents().size());
  arc_test_.app_instance()->SendTaskCreated(
      1, *apps[0], arc_test_.app_instance()->launch_intents()[0]);

  // UMA is reported since app becomes ready.
  base::HistogramBase* const histogram =
      base::StatisticsRecorder::FindHistogram(kHistogramName);
  ASSERT_TRUE(histogram);
  std::unique_ptr<base::HistogramSamples> samples = histogram->SnapshotDelta();
  ASSERT_EQ(1, samples->TotalCount());
  play_store_window->Close();
}

// Tests that the Play Store is not visible in AOSP image and visible in default
// images.
TEST_P(ChromeShelfControllerPlayStoreAvailabilityTest, Visible) {
  extension_service_->AddExtension(arc_support_host_.get());
  arc_test_.SetUp(profile());

  InitShelfController();
  StartPrefSyncService(syncer::SyncDataList());

  ArcAppListPrefs* const prefs = arc_test_.arc_app_list_prefs();
  EXPECT_EQ(arc::IsPlayStoreAvailable(),
            prefs->IsRegistered(arc::kPlayStoreAppId));
  // If the Play Store available, it is pinned by default.
  EXPECT_EQ(arc::IsPlayStoreAvailable(),
            shelf_controller_->IsAppPinned(arc::kPlayStoreAppId));
  arc_test_.TearDown();
}

// Checks the case when several app items have the same ordinal position (which
// is valid case).
TEST_F(ChromeShelfControllerTest, CheckPositionConflict) {
  InitShelfController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());
  AddWebApp(web_app::kGmailAppId);

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, app_constants::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  InsertAddPinChange(&sync_list, 1, web_app::kGmailAppId);
  SendPinChanges(sync_list, true);

  EXPECT_EQ("Chrome, App1, App2, Gmail", GetPinnedAppStatus());

  const syncer::StringOrdinal position_chrome =
      app_list_syncable_service_->GetPinPosition(app_constants::kChromeAppId);
  const syncer::StringOrdinal position_1 =
      app_list_syncable_service_->GetPinPosition(extension1_->id());
  const syncer::StringOrdinal position_2 =
      app_list_syncable_service_->GetPinPosition(extension2_->id());
  const syncer::StringOrdinal position_3 =
      app_list_syncable_service_->GetPinPosition(web_app::kGmailAppId);
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
  EXPECT_TRUE(position_3.LessThan(
      app_list_syncable_service_->GetPinPosition(app_constants::kChromeAppId)));
  EXPECT_TRUE(position_1.Equals(
      app_list_syncable_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(position_1.Equals(
      app_list_syncable_service_->GetPinPosition(extension1_->id())));
  EXPECT_TRUE(position_2.Equals(
      app_list_syncable_service_->GetPinPosition(extension2_->id())));
  EXPECT_TRUE(position_3.Equals(
      app_list_syncable_service_->GetPinPosition(web_app::kGmailAppId)));
}

// Test the case when sync app is turned off and we need to use local copy to
// support user's pins.
TEST_F(ChromeShelfControllerTest, SyncOffLocalUpdate) {
  InitShelfController();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, app_constants::kChromeAppId);
  InsertAddPinChange(&sync_list, 1, extension1_->id());
  InsertAddPinChange(&sync_list, 1, extension2_->id());
  SendPinChanges(sync_list, true);

  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());

  syncer::SyncDataList copy_sync_list =
      app_list_syncable_service_->GetAllSyncDataForTesting();

  app_list_syncable_service_->StopSyncing(syncer::APP_LIST);
  RecreateShelfController()->Init();

  // Pinned state should not change.
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
  shelf_controller_->UnpinAppWithID(extension2_->id());
  EXPECT_EQ("Chrome, App1", GetPinnedAppStatus());

  // Resume syncing and sync information overrides local copy.
  StartAppSyncService(copy_sync_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("Chrome, App1, App2", GetPinnedAppStatus());
}

// Test the Settings can be pinned and unpinned.
TEST_F(ChromeShelfControllerTest, InternalAppPinUnpin) {
  InitShelfController();
  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());

  const std::string app_id = ash::kInternalAppIdSettings;
  EXPECT_FALSE(shelf_controller_->IsAppPinned(app_id));

  // Pin Settings.
  PinAppWithIDToShelf(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));

  // Unpin Settings.
  shelf_controller_->UnpinAppWithID(app_id);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(app_id));
}

// TODO(b/194627475): Move these tests to chrome_shelf_controller_browsertest.cc
class ChromeShelfControllerDemoModeTest : public ChromeShelfControllerTestBase {
 protected:
  ChromeShelfControllerDemoModeTest() { auto_start_arc_test_ = true; }
  ChromeShelfControllerDemoModeTest(const ChromeShelfControllerDemoModeTest&) =
      delete;
  ChromeShelfControllerDemoModeTest& operator=(
      const ChromeShelfControllerDemoModeTest&) = delete;
  ~ChromeShelfControllerDemoModeTest() override = default;

  void SetUp() override {
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    ChromeShelfControllerTestBase::SetUp();

    // Fake Demo Mode.
    demo_mode_test_helper_ = std::make_unique<ash::DemoModeTestHelper>();
    GetInstallAttributes()->SetDemoMode();
    demo_mode_test_helper_->InitializeSession();
  }

  void TearDown() override {
    demo_mode_test_helper_.reset();

    ChromeShelfControllerTestBase::TearDown();
  }

 private:
  std::unique_ptr<ash::DemoModeTestHelper> demo_mode_test_helper_;
};

TEST_F(ChromeShelfControllerDemoModeTest, PinnedAppsOnline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);

  InitShelfControllerWithBrowser();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  arc::mojom::AppInfoPtr appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(*appinfo);

  arc::mojom::AppInfoPtr online_only_appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.onlineonly");
  const std::string online_only_app_id =
      AddArcAppAndShortcut(*online_only_appinfo);

  constexpr char kWebAppUrl[] = "https://test-pwa.com/";
  webapps::AppId web_app_id = InstallExternalWebApp(kWebAppUrl);

  // If the device is offline, extension2, onlineonly, and TestPWA should be
  // unpinned. Since the device is online here, these apps should still be
  // pinned, even though we're ignoring them here.
  ash::DemoSession::Get()->OverrideIgnorePinPolicyAppsForTesting(
      {extension2_->id(), online_only_appinfo->package_name});

  SetPinnedLauncherAppsPolicy(extension1_->id(), extension2_->id(),
                              appinfo->package_name,
                              online_only_appinfo->package_name, kWebAppUrl);

  // Since the device is online, all policy pinned apps are pinned.
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension1_->id(), profile()));

  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension2_->id(), profile()));

  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));

  EXPECT_TRUE(shelf_controller_->IsAppPinned(online_only_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(online_only_app_id, profile()));

  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(web_app_id, profile()));
}

TEST_F(ChromeShelfControllerDemoModeTest, PinnedAppsOffline) {
  network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_NONE);

  InitShelfControllerWithBrowser();

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  arc::mojom::AppInfoPtr appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.app");
  const std::string app_id = AddArcAppAndShortcut(*appinfo);

  arc::mojom::AppInfoPtr online_only_appinfo =
      CreateAppInfo("Some App", "SomeActivity", "com.example.onlineonly");
  const std::string online_only_app_id =
      AddArcAppAndShortcut(*online_only_appinfo);

  constexpr char kWebAppUrl[] = "https://test-pwa.com/";
  webapps::AppId web_app_id = InstallExternalWebApp(kWebAppUrl);

  // If the device is offline, extension2 and onlineonly, and TestPWA should be
  // unpinned.
  ash::DemoSession::Get()->OverrideIgnorePinPolicyAppsForTesting(
      {extension2_->id(), online_only_appinfo->package_name, kWebAppUrl});

  SetPinnedLauncherAppsPolicy(extension1_->id(), extension2_->id(),
                              appinfo->package_name,
                              online_only_appinfo->package_name, kWebAppUrl);

  // Since the device is offline, the policy pinned apps that shouldn't be
  // pinned in Demo Mode are unpinned.
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(extension1_->id(), profile()));

  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension2_->id(), profile()));

  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_FIXED,
            GetPinnableForAppID(app_id, profile()));

  EXPECT_FALSE(shelf_controller_->IsAppPinned(online_only_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(online_only_app_id, profile()));

  EXPECT_FALSE(shelf_controller_->IsAppPinned(web_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(web_app_id, profile()));

  // Pin a Chrome app that would have been pinned by policy but was suppressed
  // for Demo Mode.
  PinAppWithIDToShelf(extension2_->id());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(extension2_->id(), profile()));

  // Pin an ARC app that would have been pinned by policy but was suppressed for
  // Demo Mode.
  PinAppWithIDToShelf(online_only_app_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(online_only_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(online_only_app_id, profile()));

  // Pin a web app that would have been pinned by policy but was suppressed for
  // Demo Mode.
  PinAppWithIDToShelf(web_app_id);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(web_app_id));
  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(web_app_id, profile()));
}

// Tests behavior for ensuring some component apps can be marked unpinnable.
TEST_F(ChromeShelfControllerTest, UnpinnableComponentApps) {
  InitShelfController();

  const char* kPinnableApp = file_manager::kFileManagerAppId;
  const char* kNoPinApps[] = {ash::eche_app::kEcheAppId};

  EXPECT_EQ(AppListControllerDelegate::PIN_EDITABLE,
            GetPinnableForAppID(kPinnableApp, profile()));
  for (const char* id : kNoPinApps) {
    EXPECT_EQ(AppListControllerDelegate::NO_PIN,
              GetPinnableForAppID(id, profile()));
  }
}

TEST_F(ChromeShelfControllerTest, DoNotShowInShelf) {
  syncer::SyncChangeList sync_list;
  InsertAddPinChange(&sync_list, 0, extension1_->id());
  InsertAddPinChange(&sync_list, 0, extension2_->id());
  SendPinChanges(sync_list, true);

  AddExtension(extension1_.get());
  AddExtension(extension2_.get());

  // Set App1.show_in_shelf to false.
  std::vector<apps::AppPtr> apps;
  apps::AppPtr app =
      std::make_unique<apps::App>(apps::AppType::kChromeApp, extension1_->id());
  app->show_in_shelf = false;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile())->OnApps(
      std::move(apps), apps::AppType::kChromeApp,
      false /* should_notify_initialized */);

  InitShelfController();
  EXPECT_EQ("Chrome, App2", GetPinnedAppStatus());
  EXPECT_FALSE(IsAppPinEditable(apps::AppType::kChromeApp, extension1_->id(),
                                profile()));
}

TEST_F(ChromeShelfControllerTest, OsFlagsNotShowInShelfNotPinnable) {
  auto delegate = std::make_unique<OsFlagsSystemWebAppDelegate>(profile());
  ash::SystemWebAppType app_type = delegate->GetType();
  InstallSystemWebApp(std::move(delegate));
  InitShelfController();

  std::optional<webapps::AppId> app_id =
      ash::SystemWebAppManager::GetForTest(profile())->GetAppIdForSystemApp(
          app_type);
  ASSERT_TRUE(app_id);
  EXPECT_EQ("Chrome", GetPinnedAppStatus());
  EXPECT_FALSE(IsAppPinEditable(apps::AppType::kSystemWeb, *app_id, profile()));
}

TEST_F(ChromeShelfControllerWithArcTest, ReplacePinnedItem) {
  InitShelfController();
  SendListOfArcApps();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  PinAppWithIDToShelf(extension1_->id());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id1));

  // Replace pin extension to ARC app
  shelf_controller_->ReplacePinnedItem(extension1_->id(), arc_app_id1);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id1));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));

  // Replace pin ARC app to ARC app
  shelf_controller_->ReplacePinnedItem(arc_app_id1, arc_app_id2);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(arc_app_id2));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id1));

  // Replace pin ARC app to extension app
  shelf_controller_->ReplacePinnedItem(arc_app_id2, extension1_->id());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id2));

  // Replace pin extension app to extension app
  shelf_controller_->ReplacePinnedItem(extension1_->id(), extension2_->id());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));

  // Try to replace item that is not pinned.
  shelf_controller_->ReplacePinnedItem(arc_app_id2, extension1_->id());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(shelf_controller_->IsAppPinned(arc_app_id2));

  // Try to replace item with item that is already pinned.
  PinAppWithIDToShelf(extension1_->id());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  shelf_controller_->ReplacePinnedItem(extension2_->id(), extension1_->id());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
}

TEST_F(ChromeShelfControllerWithArcTest, PinAtIndex) {
  InitShelfController();
  SendListOfArcApps();

  const std::string arc_app_id1 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[0]);
  const std::string arc_app_id2 =
      ArcAppTest::GetAppId(*arc_test_.fake_apps()[1]);

  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension2_.get());

  int index = 0;
  shelf_controller_->PinAppAtIndex(extension1_->id(), index);
  EXPECT_EQ(index,
            shelf_controller_->PinnedItemIndexByAppID(extension1_->id()));

  shelf_controller_->PinAppAtIndex(extension2_->id(), index);
  EXPECT_EQ(index,
            shelf_controller_->PinnedItemIndexByAppID(extension2_->id()));
  EXPECT_NE(index,
            shelf_controller_->PinnedItemIndexByAppID(extension1_->id()));

  index = 3;
  shelf_controller_->PinAppAtIndex(arc_app_id1, index);
  EXPECT_EQ(index, shelf_controller_->PinnedItemIndexByAppID(arc_app_id1));

  // Test pinning at invalid index.
  index = -100;
  shelf_controller_->PinAppAtIndex(arc_app_id2, index);
  EXPECT_NE(index, shelf_controller_->PinnedItemIndexByAppID(arc_app_id2));
  EXPECT_EQ(-1, shelf_controller_->PinnedItemIndexByAppID(arc_app_id2));

  // Test pinning already pinned app.
  index = 0;
  shelf_controller_->PinAppAtIndex(arc_app_id1, index);
  EXPECT_NE(index, shelf_controller_->PinnedItemIndexByAppID(arc_app_id1));
  EXPECT_EQ(3, shelf_controller_->PinnedItemIndexByAppID(arc_app_id1));
}

class ChromeShelfControllerWebAppTest : public ChromeShelfControllerTestBase {
 protected:
  ChromeShelfControllerWebAppTest() {}

  ~ChromeShelfControllerWebAppTest() override = default;
};

// Test the web app interaction flow: pin it, run it, unpin it, close it.
TEST_F(ChromeShelfControllerWebAppTest, WebAppPinRunUnpinClose) {
  constexpr char kWebAppUrl[] = "https://webappone.com/";
  constexpr char kWebAppName[] = "WebApp1";

  InitShelfController();

  const webapps::AppId app_id = web_app::test::InstallDummyWebApp(
      profile(), kWebAppName, GURL(kWebAppUrl));
  base::RunLoop().RunUntilIdle();

  // The model should only contain the browser shortcut item.
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(nullptr, shelf_controller_->GetItem(ash::ShelfID(app_id)));

  // Pinning the app should create a new shelf item.
  PinAppWithIDToShelf(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_CLOSED, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_NE(nullptr, shelf_controller_->GetItem(ash::ShelfID(app_id)));

  // Reporting that the app is running should just update the existing item.
  shelf_controller_->SetAppStatus(app_id, ash::STATUS_RUNNING);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_PINNED_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_NE(nullptr, shelf_controller_->GetItem(ash::ShelfID(app_id)));

  // Unpinning the app should just update the existing item.
  shelf_controller_->UnpinAppWithID(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP, model_->items()[1].type);
  EXPECT_EQ(ash::STATUS_RUNNING, model_->items()[1].status);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_NE(nullptr, shelf_controller_->GetItem(ash::ShelfID(app_id)));

  // Reporting that the app is closed should remove its shelf item.
  shelf_controller_->SetAppStatus(app_id, ash::STATUS_CLOSED);
  EXPECT_EQ(1, model_->item_count());
  EXPECT_FALSE(shelf_controller_->IsAppPinned(app_id));
  EXPECT_EQ(nullptr, shelf_controller_->GetItem(ash::ShelfID(app_id)));
}

// Test the app status when the paused app is blocked, un-blocked, and un-paused
TEST_F(ChromeShelfControllerTest, VerifyAppStatusForPausedApp) {
  AddExtension(extension1_.get());

  // Set the app as paused
  UpdateAppRegistryCache(profile(), extension1_->id(), false /* block */,
                         true /* pause */, std::nullopt /* show_in_shelf */);

  InitShelfController();

  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::AppStatus::kPaused, model_->items()[1].app_status);

  // Set the app as blocked
  UpdateAppRegistryCache(profile(), extension1_->id(), true /* block */,
                         true /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kBlocked, model_->items()[1].app_status);

  // Set the app as ready, but still paused;
  UpdateAppRegistryCache(profile(), extension1_->id(), false /* block */,
                         true /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kPaused, model_->items()[1].app_status);

  // Set the app as ready, and not paused;
  UpdateAppRegistryCache(profile(), extension1_->id(), false /* block */,
                         false /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kReady, model_->items()[1].app_status);
}

// Test the app status when the blocked app is paused, un-paused, hidden,
// visible and un-blocked
TEST_F(ChromeShelfControllerTest, VerifyAppStatusForBlockedApp) {
  AddExtension(extension1_.get());

  // Set the app as blocked
  UpdateAppRegistryCache(profile(), extension1_->id(), true /* block */,
                         false /* pause */, std::nullopt /* show_in_shelf */);

  InitShelfController();

  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_EQ(ash::AppStatus::kBlocked, model_->items()[1].app_status);

  // Set the app as paused
  UpdateAppRegistryCache(profile(), extension1_->id(), true /* block */,
                         true /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kBlocked, model_->items()[1].app_status);

  // Set the app as blocked, but un-paused
  UpdateAppRegistryCache(profile(), extension1_->id(), true /* block */,
                         false /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kBlocked, model_->items()[1].app_status);

  // Set the app as ready, and not paused
  UpdateAppRegistryCache(profile(), extension1_->id(), false /* block */,
                         false /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kReady, model_->items()[1].app_status);

  // Set the app as blocked and hidden
  UpdateAppRegistryCache(profile(), extension1_->id(), true /* block */,
                         false /* pause */, false /* show_in_shelf */);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));

  // Set the app as blocked and visible
  UpdateAppRegistryCache(profile(), extension1_->id(), true /* block */,
                         false /* pause */, true /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kBlocked, model_->items()[1].app_status);
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));

  // Set the app as ready
  UpdateAppRegistryCache(profile(), extension1_->id(), false /* block */,
                         false /* pause */, std::nullopt /* show_in_shelf */);
  EXPECT_EQ(ash::AppStatus::kReady, model_->items()[1].app_status);
}

TEST_F(ChromeShelfControllerTest, PinnedAppsRespectShownInShelfState) {
  InitShelfController();
  // Pin a test app.
  AddExtension(extension1_.get());

  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension1_->id()));

  // Update the app so it's considered not shown in shelf, and verify it's no
  // longer pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));

  // Update the app so it's allowed in shelf again, verify it gets pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/true);
  EXPECT_TRUE(model_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(model_->AllowedToSetAppPinState(extension1_->id(), false));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension1_->id()));
}

TEST_F(ChromeShelfControllerTest, AppIndexAfterUnhidingFirstPinnedApp) {
  InitShelfController();
  // Pin a test app.
  AddExtension(extension1_.get());

  PinAppWithIDToShelf(extension1_->id());
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension1_->id()));
  model_->Move(1, 0);
  EXPECT_EQ(0, model_->ItemIndexByAppID(extension1_->id()));

  // Update the app so it's considered not shown in shelf, and verify it's no
  // longer pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension1_->id()));

  // Update the app so it's allowed in shelf again, verify it gets pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/true);
  EXPECT_TRUE(model_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(model_->AllowedToSetAppPinState(extension1_->id(), false));
  EXPECT_EQ(0, model_->ItemIndexByAppID(extension1_->id()));
}

TEST_F(ChromeShelfControllerTest,
       AppIndexAfterUnhidingtPinnedAppWithOtherHiddenApps) {
  InitShelfController();
  // Pin test apps.
  AddExtension(extension1_.get());
  AddExtension(extension2_.get());
  AddExtension(extension5_.get());
  AddExtension(extension6_.get());

  PinAppWithIDToShelf(extension1_->id());
  PinAppWithIDToShelf(extension2_->id());
  PinAppWithIDToShelf(extension5_->id());
  PinAppWithIDToShelf(extension6_->id());
  EXPECT_EQ(5, model_->item_count());
  EXPECT_TRUE(shelf_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ(2, model_->ItemIndexByAppID(extension2_->id()));

  // Update all test apps so they're considered not shown in shelf, and verify
  // it's no longer pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  UpdateAppRegistryCache(profile(), extension2_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  UpdateAppRegistryCache(profile(), extension5_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  EXPECT_FALSE(shelf_controller_->IsAppPinned(extension2_->id()));

  // Update app 2 so it's allowed in shelf again, verify it gets pinned.
  UpdateAppRegistryCache(profile(), extension2_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/true);
  EXPECT_TRUE(model_->IsAppPinned(extension2_->id()));
  EXPECT_TRUE(model_->AllowedToSetAppPinState(extension2_->id(), false));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension2_->id()));
  EXPECT_EQ(2, model_->ItemIndexByAppID(extension6_->id()));
}

TEST_F(ChromeShelfControllerTest, AppsHiddenFromShelfDontGetPinnedByPolicy) {
  AddExtension(extension1_.get());

  // Pin a test app by policy.
  SetPinnedLauncherAppsPolicy(extension1_->id());

  InitShelfController();
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(model_->IsAppPinned(extension1_->id()));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension1_->id()));

  // Update the app so it's considered not shown in shelf, and verify it's no
  // longer pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  EXPECT_FALSE(model_->IsAppPinned(extension1_->id()));

  // Update the app so it's allowed in shelf again, verify it gets pinned.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/true);
  EXPECT_TRUE(model_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(model_->AllowedToSetAppPinState(extension1_->id(), false));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension1_->id()));
}

TEST_F(ChromeShelfControllerTest, AppHiddenFromShelfNotPinnedOnInstall) {
  AddExtension(extension1_.get());
  InitShelfController();
  const std::string app_id = extension1_->id();
  const apps::AppType app_type =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache()
          .GetAppType(app_id);
  PinAppWithIDToShelf(app_id);
  EXPECT_EQ(2, model_->item_count());
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_EQ(1, model_->ItemIndexByAppID(app_id));
  EXPECT_TRUE(IsAppPinEditable(app_type, app_id, profile()));

  // Block the extension so it gets removed from shelf.
  UpdateAppRegistryCache(profile(), app_id, /*block=*/true,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);
  EXPECT_FALSE(model_->IsAppPinned(app_id));

  // Unblock the extension, but mark it as not shown in shelf - verify it
  // doesn't get pinned/added to shelf.
  UpdateAppRegistryCache(profile(), app_id, /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/false);

  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_FALSE(IsAppPinEditable(app_type, app_id, profile()));

  // Allow the app to be shown in shelf, and verify it gets pinned again.
  UpdateAppRegistryCache(profile(), extension1_->id(), /*block=*/false,
                         /*pause=*/false,
                         /*show_in_shelf=*/true);
  EXPECT_TRUE(model_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(model_->AllowedToSetAppPinState(extension1_->id(), false));
  EXPECT_EQ(1, model_->ItemIndexByAppID(extension1_->id()));
  EXPECT_TRUE(IsAppPinEditable(app_type, app_id, profile()));
}

class ChromeShelfControllerPromiseAppsTest : public ChromeShelfControllerTest,
                                             public ash::ShelfModelObserver {
 public:
  ChromeShelfControllerPromiseAppsTest() {
    auto_start_arc_test_ = true;
    feature_list_.InitAndEnableFeature(ash::features::kPromiseIcons);
  }
  ~ChromeShelfControllerPromiseAppsTest() override = default;

  void SetUp() override {
    // To prevent crash on test exit and pending decode request.
    ArcAppIcon::DisableSafeDecodingForTesting();

    ChromeShelfControllerTestBase::SetUp();
  }

  SkBitmap ApplyEffectsToBitmap(SkBitmap bitmap, apps::IconEffects effects) {
    auto iv = std::make_unique<apps::IconValue>();
    iv->uncompressed = gfx::ImageSkia::CreateFromBitmap(bitmap, 1.0f);
    iv->icon_type = apps::IconType::kUncompressed;

    base::test::TestFuture<apps::IconValuePtr> image_with_effects;
    apps::ApplyIconEffects(/*profile=*/nullptr, /*app_id=*/std::nullopt,
                           effects, bitmap.width(), std::move(iv),
                           image_with_effects.GetCallback());

    return *image_with_effects.Get()->uncompressed.bitmap();
  }

  apps::PromiseAppRegistryCache* cache() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->PromiseAppRegistryCache();
  }

  apps::PromiseAppService* service() {
    return apps::AppServiceProxyFactory::GetForProfile(profile())
        ->PromiseAppService();
  }

  void WaitForItemUpdate() {
    if (!obs_.IsObserving()) {
      obs_.Observe(model_.get());
    }
    wait_run_loop_ = std::make_unique<base::RunLoop>();
    wait_run_loop_->Run();
  }

  // ShelfModelObserver overrides:
  void ShelfItemChanged(int, const ash::ShelfItem&) override {
    if (wait_run_loop_ && wait_run_loop_->running()) {
      wait_run_loop_->Quit();
    }
  }

  void ShelfItemAdded(int) override {
    if (wait_run_loop_ && wait_run_loop_->running()) {
      wait_run_loop_->Quit();
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::ScopedObservation<ash::ShelfModel, ash::ShelfModelObserver> obs_{this};
  std::unique_ptr<base::RunLoop> wait_run_loop_;
};

TEST_F(ChromeShelfControllerPromiseAppsTest, PromiseAppUpdatesShelfItem) {
  // Register a promise app.
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "com.example.test");
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->status = apps::PromiseStatus::kPending;
  promise_app->name = "App Name";
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Create a shelf item for the promise app.
  InitShelfController();
  PinAppWithIDToShelf(package_id.ToString());

  // Verify the details of the shelf item.
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  ash::ShelfID id(package_id.ToString());
  const ash::ShelfItem* item = shelf_controller_->GetItem(id);
  EXPECT_EQ(item->title, ShelfControllerHelper::GetLabelForPromiseStatus(
                             apps::PromiseStatus::kPending));
  EXPECT_EQ(item->accessible_name,
            ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
                "App Name", apps::PromiseStatus::kPending));
  EXPECT_EQ(item->progress, 0);
  EXPECT_EQ(item->app_status, ash::AppStatus::kPending);

  // Push an progress and status update to the promise app.
  apps::PromiseAppPtr update = std::make_unique<apps::PromiseApp>(package_id);
  update->progress = 0.3;
  update->status = apps::PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Verify that the shelf item has updated details.
  EXPECT_EQ(item->title, ShelfControllerHelper::GetLabelForPromiseStatus(
                             apps::PromiseStatus::kInstalling));
  EXPECT_EQ(item->accessible_name,
            ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
                "App Name", apps::PromiseStatus::kInstalling));
  EXPECT_EQ(item->progress, 0.3f);
  EXPECT_EQ(item->app_status, ash::AppStatus::kInstalling);
}

TEST_F(ChromeShelfControllerPromiseAppsTest,
       PromiseAppUpdatesCorrectShelfItem) {
  // Register the main promise app that we will check the updates for.
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "main.package.for.test");
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->status = apps::PromiseStatus::kPending;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Register another promise app that will have a shelf item but which we do
  // not expect updates for.
  const apps::PackageId other_package_id =
      apps::PackageId(apps::PackageType::kArc, "other.package");
  apps::PromiseAppPtr other_promise_app =
      std::make_unique<apps::PromiseApp>(other_package_id);
  other_promise_app->status = apps::PromiseStatus::kPending;
  other_promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(other_promise_app));

  // Create shelf items for the promise apps.
  InitShelfController();
  PinAppWithIDToShelf(package_id.ToString());
  PinAppWithIDToShelf(other_package_id.ToString());

  // Verify the status of the main shelf item.
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  ash::ShelfID id(package_id.ToString());
  const ash::ShelfItem* item = shelf_controller_->GetItem(id);
  EXPECT_EQ(item->app_status, ash::AppStatus::kPending);

  // Verify the status of the other shelf item.
  EXPECT_TRUE(model_->IsAppPinned(other_package_id.ToString()));
  ash::ShelfID other_id(other_package_id.ToString());
  const ash::ShelfItem* other_item = shelf_controller_->GetItem(other_id);
  EXPECT_EQ(other_item->app_status, ash::AppStatus::kPending);

  // Push an update to the main promise app.
  apps::PromiseAppPtr update = std::make_unique<apps::PromiseApp>(package_id);
  update->status = apps::PromiseStatus::kInstalling;
  cache()->OnPromiseApp(std::move(update));

  // Verify that the main shelf item has an updated status.
  EXPECT_EQ(item->app_status, ash::AppStatus::kInstalling);

  // Verify that the other shelf item remains the same.
  EXPECT_EQ(other_item->app_status, ash::AppStatus::kPending);
}

TEST_F(ChromeShelfControllerPromiseAppsTest,
       ShelfItemFetchesAndAppliesEffectsToIcon) {
  // Register the main promise app that we will check the updates for.
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "com.example.test");
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->status = apps::PromiseStatus::kPending;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Add a test icon for the promise app.
  SkBitmap base_bitmap =
      gfx::test::CreateBitmap(extension_misc::EXTENSION_ICON_MEDIUM,
                              extension_misc::EXTENSION_ICON_MEDIUM);

  apps::PromiseAppIconPtr icon = std::make_unique<apps::PromiseAppIcon>();
  icon->icon = base_bitmap;
  icon->width_in_pixels = extension_misc::EXTENSION_ICON_MEDIUM;
  service()->PromiseAppIconCache()->SaveIcon(package_id, std::move(icon));

  // Create shelf items for the promise apps. This should trigger the
  // AppServicePromiseAppIconLoader to fetch the image.
  InitShelfController();
  PinAppWithIDToShelf(package_id.ToString());
  WaitForItemUpdate();

  // Verify that the icon has the correct effects applied to it.
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  ash::ShelfID id(package_id.ToString());
  const ash::ShelfItem* item = shelf_controller_->GetItem(id);
  SkBitmap result_bitmap = *item->image.bitmap();
  SkBitmap expected_bitmap =
      ApplyEffectsToBitmap(base_bitmap, apps::IconEffects::kCrOsStandardMask);
  EXPECT_TRUE(gfx::BitmapsAreEqual(result_bitmap, expected_bitmap));
}

TEST_F(ChromeShelfControllerPromiseAppsTest, RemoveShelfItem) {
  // Register a promise app.
  apps::AppType app_type = apps::AppType::kArc;
  std::string identifier = "test.com.example";
  apps::PackageId package_id(apps::PackageType::kArc, identifier);
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->progress = 0.9;
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Create a shelf item for the promise app.
  InitShelfController();
  PinAppWithIDToShelf(package_id.ToString());

  // Verify the details of the shelf item.
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  ash::ShelfID id(package_id.ToString());
  const ash::ShelfItem* item = shelf_controller_->GetItem(id);
  EXPECT_TRUE(item);

  // Register (i.e. "install") an app in AppRegistryCache with a matching
  // package ID. This should trigger removal of the promise app and hence the
  // unpinning of the shelf item.
  std::string app_id = "qwertyuiopasdfghjkl";
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->publisher_id = identifier;
  app->readiness = apps::Readiness::kReady;
  std::vector<apps::AppPtr> apps;
  apps.push_back(std::move(app));
  apps::AppServiceProxyFactory::GetForProfile(profile())->OnApps(
      std::move(apps), app_type,
      /*should_notify_initialized=*/false);

  // Item should no longer be in the shelf.
  EXPECT_FALSE(model_->IsAppPinned(package_id.ToString()));
  item = shelf_controller_->GetItem(id);
  EXPECT_FALSE(item);
}

TEST_F(ChromeShelfControllerPromiseAppsTest, PinnedPromiseAppShelfItemType) {
  // Register a promise app.
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "com.example.test");
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Create a shelf item for the promise app.
  InitShelfController();
  PinAppWithIDToShelf(package_id.ToString());

  // Verify the app is identifiable as a promise app.
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  ash::ShelfID id(package_id.ToString());
  const ash::ShelfItem* item = shelf_controller_->GetItem(id);
  EXPECT_TRUE(item->is_promise_app);
}

TEST_F(ChromeShelfControllerPromiseAppsTest,
       PromiseAppGetsPinnedByMatchingSyncData) {
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "com.example.test");

  // Add entry in sync data that has a matching PackageId with the promise app.
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      "asdfghjkl", "App Name", /*parent_id=*/std::string(), "ordinal",
      /*item_pin_ordinal=*/
      syncer::StringOrdinal::CreateInitialOrdinal().ToDebugString(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/true,
      /*promise_package_id=*/package_id.ToString())));
  StartAppSyncService(sync_list);
  base::RunLoop().RunUntilIdle();

  InitShelfController();

  // Register a new promise app.
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  // Verify that the promise app is in the shelf without having to explicitly
  // pin it.
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  ash::ShelfID id(package_id.ToString());
  const ash::ShelfItem* item = shelf_controller_->GetItem(id);
  ASSERT_TRUE(item);
  EXPECT_TRUE(item->is_promise_app);
}

TEST_F(ChromeShelfControllerPromiseAppsTest,
       PromiseAppNotPinnedByMatchingSyncDataIfNotReadyToBeShown) {
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "com.example.test");

  // Add entry in sync data that has a matching PackageId with our promise app.
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      "asdfghjkl", "App Name", /*parent_id=*/std::string(), "ordinal",
      /*item_pin_ordinal=*/
      syncer::StringOrdinal::CreateInitialOrdinal().ToDebugString(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/true,
      /*promise_package_id=*/package_id.ToString())));
  StartAppSyncService(sync_list);
  base::RunLoop().RunUntilIdle();

  InitShelfController();

  // Verify no promise app shelf item is created if promise app isn't present in
  // the PromiseAppRegistryCache.
  EXPECT_FALSE(model_->IsAppPinned(package_id.ToString()));

  // Register a new promise app with the default should_show=false.
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  cache()->OnPromiseApp(std::move(promise_app));

  // Trigger the shelf to update the pinned apps.
  SendPinChanges(syncer::SyncChangeList(), /*reset_pin_model=*/false);

  // Verify that the promise app is still not on the shelf.
  EXPECT_FALSE(model_->IsAppPinned(package_id.ToString()));
}

// This test verifies that the sync data will trigger the creation of a
// ShelfItem with specific promise app fields for a promise app registration, or
// a regular ShelfItem for an installed app. We should not create a regular
// ShelfItem for a promise app or a ShelfItem with promise app fields for an
// installed app.
TEST_F(ChromeShelfControllerPromiseAppsTest, SyncDataCreatesCorrectShelfItem) {
  std::string package_name = "com.example.test";
  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, package_name);
  std::string app_id = "bijolhehmgkcaahdbconomepmenmeomc";

  // Add entry in sync data that has a matching PackageId with our promise app.
  SendPinChanges(syncer::SyncChangeList(), true);
  StopAppSyncService();
  syncer::SyncDataList sync_list;
  sync_list.push_back((app_list::CreateAppRemoteData(
      app_id, "App Name", /*parent_id=*/std::string(), "ordinal",
      /*item_pin_ordinal=*/GeneratePinPosition(1).ToDebugString(),
      /*item_type=*/sync_pb::AppListSpecifics_AppListItemType_TYPE_APP,
      /*is_user_pinned=*/true,
      /*promise_package_id=*/package_id.ToString())));
  StartAppSyncService(sync_list);
  base::RunLoop().RunUntilIdle();

  InitShelfController();

  // Start the promise app installation but don't allow it to be visible yet.
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->should_show = false;
  cache()->OnPromiseApp(std::move(promise_app));

  // Force an update to the shelf items pinned by sync.
  SendPinChanges(syncer::SyncChangeList(), /*reset_pin_model=*/false);

  // No shelf items should be created for the sync data.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_FALSE(model_->IsAppPinned(package_id.ToString()));

  // Update the visibility of the promise app.
  apps::PromiseAppPtr promise_app_update =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app_update->should_show = true;
  promise_app_update->progress = 0.3;
  cache()->OnPromiseApp(std::move(promise_app_update));

  // Only the promise app ShelfItem should exist.
  EXPECT_FALSE(model_->IsAppPinned(app_id));
  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));

  // Confirm the promise app ShelfItem fields.
  ash::ShelfID promise_shelf_id(package_id.ToString());
  const ash::ShelfItem* promise_item =
      shelf_controller_->GetItem(promise_shelf_id);
  ASSERT_TRUE(promise_item);
  EXPECT_TRUE(promise_item->is_promise_app);
  EXPECT_EQ(promise_item->progress, 0.3f);
  EXPECT_EQ(promise_item->package_id, package_id.ToString());

  // Add an app in ArcAppListPrefs, which will register an app in App Service.
  // We need to add an app into ArcAppListPrefs (and not just the
  // AppRegistryCache) because ChromeShelfPrefs checks ArcAppListPrefs to
  // confirm whether an app ID is valid when retrieving all the pinned apps from
  // sync.
  arc::mojom::AppInfoPtr app_info =
      CreateAppInfo("Test Name", "Test Activity", "com.example.test");
  AddArcAppAndShortcut(*app_info);

  // Only the installed app shelf item should exist.
  EXPECT_TRUE(model_->IsAppPinned(app_id));
  EXPECT_FALSE(model_->IsAppPinned(package_id.ToString()));

  // Confirm that the ShelfItem is not for the promise app.
  ash::ShelfID shelf_id(app_id);
  const ash::ShelfItem* app_item = shelf_controller_->GetItem(shelf_id);
  ASSERT_TRUE(app_item);
  EXPECT_FALSE(app_item->is_promise_app);
  EXPECT_EQ(app_item->progress, -1);
  EXPECT_EQ(app_item->package_id, package_id.ToString());
}

TEST_F(ChromeShelfControllerPromiseAppsTest, ShelfItemCreationUpdatesMetrics) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      apps::kPromiseAppLifecycleEventHistogram,
      apps::PromiseAppLifecycleEvent::kCreatedInShelf, 0);

  const apps::PackageId package_id =
      apps::PackageId(apps::PackageType::kArc, "com.example.test");
  apps::PromiseAppPtr promise_app =
      std::make_unique<apps::PromiseApp>(package_id);
  promise_app->should_show = true;
  cache()->OnPromiseApp(std::move(promise_app));

  InitShelfController();
  PinAppWithIDToShelf(package_id.ToString());

  EXPECT_TRUE(model_->IsAppPinned(package_id.ToString()));
  histogram_tester.ExpectBucketCount(
      apps::kPromiseAppLifecycleEventHistogram,
      apps::PromiseAppLifecycleEvent::kCreatedInShelf, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeShelfControllerPlayStoreAvailabilityTest,
                         ::testing::Values(false, true));
