// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/external_testing_loader.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/web_applications/components/external_app_install_features.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/updater/extension_cache_fake.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#endif

namespace {

constexpr char kMigrationFlag[] = "MigrationTest";

constexpr char kExtensionUpdatePath[] = "/update_extension";
constexpr char kExtensionId[] = "kbmnembihfiondgfjekmnmcbddelicoi";
constexpr char kExtensionVersion[] = "1.0.0.0";
constexpr char kExtensionCrxPath[] = "/extensions/hosted_app.crx";

constexpr char kWebAppPath[] = "/web_apps/basic.html";

}  // namespace

namespace web_app {

class ExternalWebAppMigrationBrowserTest : public InProcessBrowserTest {
 public:
  ExternalWebAppMigrationBrowserTest() {
    ExternalWebAppManager::SkipStartupForTesting();
    disable_scope_ =
        extensions::ExtensionService::DisableExternalUpdatesForTesting();
  }
  ~ExternalWebAppMigrationBrowserTest() override = default;

  Profile* profile() { return browser()->profile(); }

  extensions::ExtensionService& extension_service() {
    return *extensions::ExtensionSystem::Get(profile())->extension_service();
  }

  GURL GetWebAppUrl() const {
    return embedded_test_server()->GetURL(kWebAppPath);
  }

  AppId GetWebAppId() const { return GenerateAppIdFromURL(GetWebAppUrl()); }

  // InProcessBrowserTest:
  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ExternalWebAppMigrationBrowserTest::RequestHandlerOverride,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    SetUpExtensionTestExternalProvider();

    InProcessBrowserTest::SetUpOnMainThread();
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    std::string request_path = request.GetURL().path();
    if (request_path == kExtensionUpdatePath) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content(base::ReplaceStringPlaceholders(
          R"(<?xml version='1.0' encoding='UTF-8'?>
            <gupdate xmlns='http://www.google.com/update2/response' protocol='2.0'>
              <app appid='$1'>
                <updatecheck codebase='$2' version='$3' />
              </app>
            </gupdate>
          )",
          {kExtensionId,
           embedded_test_server()->GetURL(kExtensionCrxPath).spec(),
           kExtensionVersion},
          nullptr));
      response->set_content_type("text/xml");
      return std::move(response);
    }

    return nullptr;
  }

  void SetUpExtensionTestExternalProvider() {
    extension_service().ClearProvidersForTesting();

    extension_service().updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());

    std::string external_extension_config = base::ReplaceStringPlaceholders(
        R"({
          "$1": {
            "external_update_url": "$2",
            "web_app_migration_flag": "$3"
          }
        })",
        {kExtensionId,
         embedded_test_server()->GetURL(kExtensionUpdatePath).spec(),
         kMigrationFlag},
        nullptr);

    extension_service().AddProviderForTesting(
        std::make_unique<extensions::ExternalProviderImpl>(
            &extension_service(),
            base::MakeRefCounted<extensions::ExternalTestingLoader>(
                external_extension_config,
                base::FilePath(FILE_PATH_LITERAL("//absolute/path"))),
            profile(), extensions::Manifest::EXTERNAL_PREF,
            extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
            extensions::Extension::NO_FLAGS));

    disable_scope_.reset();
  }

  void SyncExternalExtensions() {
    base::RunLoop run_loop;
    extension_service().set_external_updates_finished_callback_for_test(
        run_loop.QuitWhenIdleClosure());
    extension_service().CheckForExternalUpdates();
    run_loop.Run();
  }

  void SyncExternalWebApps(bool expect_install,
                           bool expect_uninstall,
                           bool pass_config = true) {
    base::RunLoop run_loop;

    auto callback = base::BindLambdaForTesting(
        [&](std::map<GURL, InstallResultCode> install_results,
            std::map<GURL, bool> uninstall_results) {
          if (expect_install) {
            EXPECT_EQ(install_results.at(GetWebAppUrl()),
                      InstallResultCode::kSuccessNewInstall);
          } else {
            EXPECT_EQ(install_results.find(GetWebAppUrl()),
                      install_results.end());
          }
          EXPECT_EQ(uninstall_results[GetWebAppUrl()], expect_uninstall);
          run_loop.Quit();
        });

    std::vector<base::Value> app_configs;
    if (pass_config) {
      std::string app_config_string = base::ReplaceStringPlaceholders(
          R"({
            "app_url": "$1",
            "launch_container": "window",
            "user_type": ["unmanaged"],
            "feature_name": "$2",
            "uninstall_and_replace": ["$3"]
          })",
          {GetWebAppUrl().spec(), kMigrationFlag, kExtensionId}, nullptr);
      app_configs.push_back(*base::JSONReader::Read(app_config_string));
    }
    ExternalWebAppManager::SetConfigsForTesting(&app_configs);

    WebAppProvider::Get(profile())
        ->external_web_app_manager_for_testing()
        .LoadAndSynchronizeForTesting(std::move(callback));

    run_loop.Run();

    ExternalWebAppManager::SetConfigsForTesting(nullptr);
  }

  bool IsWebAppInstalled() {
    return WebAppProvider::Get(profile())->registrar().IsLocallyInstalled(
        GetWebAppId());
  }

  bool IsExtensionAppInstalled() {
    return extensions::ExtensionRegistry::Get(profile())->GetExtensionById(
        kExtensionId, extensions::ExtensionRegistry::EVERYTHING);
  }

 private:
  base::Optional<base::AutoReset<bool>> disable_scope_;
  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
};

IN_PROC_BROWSER_TEST_F(ExternalWebAppMigrationBrowserTest,
                       MigrateRevertMigrate) {
  // Set up pre-migration state.
  {
    ASSERT_FALSE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false);

    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_TRUE(IsExtensionAppInstalled());
  }

  // Migrate extension app to web app.
  {
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    // Extension sticks around to be uninstalled by the replacement web app.
    EXPECT_TRUE(IsExtensionAppInstalled());

    {
      extensions::TestExtensionRegistryObserver uninstall_observer(
          extensions::ExtensionRegistry::Get(profile()));

      SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false);
      EXPECT_TRUE(IsWebAppInstalled());

      scoped_refptr<const extensions::Extension> uninstalled_app =
          uninstall_observer.WaitForExtensionUninstalled();
      EXPECT_EQ(uninstalled_app->id(), kExtensionId);
      EXPECT_FALSE(IsExtensionAppInstalled());
    }
  }

  // Revert migration.
  {
    ASSERT_FALSE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/true);

    EXPECT_TRUE(IsExtensionAppInstalled());
    EXPECT_FALSE(IsWebAppInstalled());
  }

  // Re-run migration.
  {
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    extensions::TestExtensionRegistryObserver uninstall_observer(
        extensions::ExtensionRegistry::Get(profile()));

    SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false);
    EXPECT_TRUE(IsWebAppInstalled());

    scoped_refptr<const extensions::Extension> uninstalled_app =
        uninstall_observer.WaitForExtensionUninstalled();
    EXPECT_EQ(uninstalled_app->id(), kExtensionId);
    EXPECT_FALSE(IsExtensionAppInstalled());
  }
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppMigrationBrowserTest, MigratePreferences) {
#if defined(OS_CHROMEOS)
  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  AppListModelUpdater* app_list_model_updater =
      app_list_syncable_service->GetModelUpdater();
  app_list_model_updater->SetActive(true);
#endif
  extensions::AppSorting* app_sorting =
      extensions::ExtensionSystem::Get(profile())->app_sorting();

  // Set up pre-migration state.
  {
    ASSERT_FALSE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false);

    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_TRUE(IsExtensionAppInstalled());

#if defined(OS_CHROMEOS)
    ChromeAppListItem* app_list_item =
        app_list_model_updater->FindItem(kExtensionId);
    app_list_item->SetPosition(syncer::StringOrdinal("testapplistposition"));
    app_list_model_updater->OnItemUpdated(app_list_item->CloneMetadata());
    app_list_syncable_service->SetPinPosition(
        kExtensionId, syncer::StringOrdinal("testpinposition"));
#endif

    // Set chrome://apps position.
    app_sorting->SetAppLaunchOrdinal(kExtensionId,
                                     syncer::StringOrdinal("testapplaunch"));
    app_sorting->SetPageOrdinal(kExtensionId,
                                syncer::StringOrdinal("testpageordinal"));

    // Set user preference to launch as browser tab.
    extensions::SetLaunchType(profile(), kExtensionId,
                              extensions::LAUNCH_TYPE_REGULAR);
  }

  // Migrate extension app to web app.
  {
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    // Extension sticks around to be uninstalled by the replacement web app.
    EXPECT_TRUE(IsExtensionAppInstalled());

    {
      extensions::TestExtensionRegistryObserver uninstall_observer(
          extensions::ExtensionRegistry::Get(profile()));

      SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false);
      EXPECT_TRUE(IsWebAppInstalled());

      scoped_refptr<const extensions::Extension> uninstalled_app =
          uninstall_observer.WaitForExtensionUninstalled();
      EXPECT_EQ(uninstalled_app->id(), kExtensionId);
      EXPECT_FALSE(IsExtensionAppInstalled());
    }
  }

  // Check UI preferences have migrated across.
  {
    const AppId web_app_id = GetWebAppId();

#if defined(OS_CHROMEOS)
    // Chrome OS shelf/list position should migrate.
    EXPECT_EQ(app_list_model_updater->FindItem(web_app_id)
                  ->position()
                  .ToDebugString(),
              "testapplistposition");
    EXPECT_EQ(
        app_list_syncable_service->GetPinPosition(web_app_id).ToDebugString(),
        "testpinposition");
#endif

    // chrome://apps position should migrate.
    EXPECT_EQ(app_sorting->GetAppLaunchOrdinal(web_app_id).ToDebugString(),
              "testapplaunch");
    EXPECT_EQ(app_sorting->GetPageOrdinal(web_app_id).ToDebugString(),
              "testpageordinal");

    // User launch preference should migrate across and override
    // "launch_container": "window" in the JSON config.
    EXPECT_EQ(WebAppProvider::Get(profile())->registrar().GetAppUserDisplayMode(
                  web_app_id),
              DisplayMode::kBrowser);
  }
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppMigrationBrowserTest,
                       UserUninstalledExtensionApp) {
  // Set up pre-migration state.
  {
    ASSERT_FALSE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false);

    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_TRUE(IsExtensionAppInstalled());
  }

  {
    extensions::TestExtensionRegistryObserver uninstall_observer(
        extensions::ExtensionRegistry::Get(profile()), kExtensionId);
    extensions::ExtensionSystem::Get(profile())
        ->extension_service()
        ->UninstallExtension(kExtensionId,
                             extensions::UNINSTALL_REASON_FOR_TESTING, nullptr);
    uninstall_observer.WaitForExtensionUninstalled();
    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_FALSE(IsExtensionAppInstalled());
  }

  // Migrate extension app to web app.
  {
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    EXPECT_FALSE(IsExtensionAppInstalled());

    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false);
    EXPECT_FALSE(IsWebAppInstalled());
  }
}

// Tests the migration from an extension-app to a preinstalled web app provided
// by the preinstalled apps (rather than an external config).
IN_PROC_BROWSER_TEST_F(ExternalWebAppMigrationBrowserTest,
                       MigrateToPreinstalledWebApp) {
  ScopedTestingPreinstalledAppData preinstalled_apps;
  preinstalled_apps.apps.push_back(
      {GetWebAppUrl(), kMigrationFlag, kExtensionId});
  EXPECT_EQ(1, GetPreinstalledWebApps().disabled_count);

  // Set up pre-migration state.
  {
    base::HistogramTester histograms;

    ASSERT_FALSE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false,
                        /*pass_config=*/false);

    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_TRUE(IsExtensionAppInstalled());

    histograms.ExpectUniqueSample(ExternalWebAppManager::kHistogramEnabledCount,
                                  0, 1);
    histograms.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramDisabledCount, 1, 1);
    histograms.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramConfigErrorCount, 0, 1);
  }

  // Migrate extension app to web app.
  {
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag));

    SyncExternalExtensions();
    // Extension sticks around to be uninstalled by the replacement web app.
    EXPECT_TRUE(IsExtensionAppInstalled());

    {
      base::HistogramTester histograms;

      extensions::TestExtensionRegistryObserver uninstall_observer(
          extensions::ExtensionRegistry::Get(profile()));


      SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false,
                          /*pass_config=*/false);
      EXPECT_TRUE(IsWebAppInstalled());

      scoped_refptr<const extensions::Extension> uninstalled_app =
          uninstall_observer.WaitForExtensionUninstalled();
      EXPECT_EQ(uninstalled_app->id(), kExtensionId);
      EXPECT_FALSE(IsExtensionAppInstalled());

      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramEnabledCount, 1, 1);
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramDisabledCount, 0, 1);
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramConfigErrorCount, 0, 1);
    }
  }
}

}  // namespace web_app
