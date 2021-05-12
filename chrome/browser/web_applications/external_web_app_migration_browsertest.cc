// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
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
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/external_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    ExternalWebAppManager::BypassOfflineManifestRequirementForTesting();
    disable_external_extensions_scope_ =
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
    os_hooks_suppress_ =
        OsIntegrationManager::ScopedSuppressOsHooksForTesting();
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
            profile(), extensions::mojom::ManifestLocation::kExternalPref,
            extensions::mojom::ManifestLocation::kExternalPrefDownload,
            // Matches |bundled_extension_creation_flags| in
            // ExternalProviderImpl::CreateExternalProviders().
            extensions::Extension::WAS_INSTALLED_BY_DEFAULT |
                extensions::Extension::FROM_WEBSTORE));

    disable_external_extensions_scope_.reset();
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

    base::Optional<InstallResultCode> code;

    auto callback = base::BindLambdaForTesting(
        [&](std::map<GURL, PendingAppManager::InstallResult> install_results,
            std::map<GURL, bool> uninstall_results) {
          if (expect_install) {
            code = install_results.at(GetWebAppUrl()).code;
            EXPECT_TRUE(*code == InstallResultCode::kSuccessNewInstall ||
                        *code == InstallResultCode::kSuccessOfflineOnlyInstall);
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
        ->external_web_app_manager()
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

  void FlushAppService() {
    apps::AppServiceProxyFactory::GetForProfile(profile())
        ->FlushMojoCallsForTesting();
  }

 private:
  base::test::ScopedFeatureList features_;
  base::Optional<base::AutoReset<bool>> disable_external_extensions_scope_;
  std::unique_ptr<extensions::ExtensionCacheFake> test_extension_cache_;
  ScopedOsHooksSuppress os_hooks_suppress_;
};

IN_PROC_BROWSER_TEST_F(ExternalWebAppMigrationBrowserTest,
                       MigrateRevertMigrate) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Grab handles to the app list to update shelf/list state for apps later on.
  app_list::AppListSyncableService* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile());
  AppListModelUpdater* app_list_model_updater =
      app_list_syncable_service->GetModelUpdater();
  app_list_model_updater->SetActive(true);
#endif

  // Set up pre-migration state.
  {
    ASSERT_FALSE(
        IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false);
    FlushAppService();

    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_TRUE(IsExtensionAppInstalled());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ChromeAppListItem* app_list_item =
        app_list_model_updater->FindItem(kExtensionId);
    app_list_item->SetPosition(syncer::StringOrdinal("testapplistposition"));
    app_list_model_updater->OnItemUpdated(app_list_item->CloneMetadata());
    app_list_syncable_service->SetPinPosition(
        kExtensionId, syncer::StringOrdinal("testpinposition"));
    EXPECT_EQ(app_list_syncable_service->GetSyncItem(kExtensionId)->ToString(),
              "kbmnembi { Nothing } [testapplistposition] [testpinposition]");
#endif
  }

  // Migrate extension app to web app.
  {
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

    SyncExternalExtensions();
    // Extension sticks around to be uninstalled by the replacement web app.
    EXPECT_TRUE(IsExtensionAppInstalled());

    {
      base::HistogramTester histograms;
      extensions::TestExtensionRegistryObserver uninstall_observer(
          extensions::ExtensionRegistry::Get(profile()));

      SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false);
      scoped_refptr<const extensions::Extension> uninstalled_app =
          uninstall_observer.WaitForExtensionUninstalled();
      EXPECT_EQ(uninstalled_app->id(), kExtensionId);
      FlushAppService();

      EXPECT_TRUE(IsWebAppInstalled());
      EXPECT_FALSE(IsExtensionAppInstalled());

      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramInstallResult,
          InstallResultCode::kSuccessNewInstall, 1);
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramUninstallAndReplaceCount, 1, 1);

#if BUILDFLAG(IS_CHROMEOS_ASH)
      // Chrome OS shelf/list position should migrate.
      EXPECT_EQ(
          app_list_syncable_service->GetSyncItem(GetWebAppId())->ToString(),
          base::StringPrintf(
              "%s { Basic web app } [testapplistposition] [testpinposition]",
              GetWebAppId().substr(0, 8).c_str()));
      // Old Chrome app is unpinned.
      EXPECT_EQ(
          app_list_syncable_service->GetSyncItem(kExtensionId)->ToString(),
          "kbmnembi { Nothing } [testapplistposition] [INVALID[]]");
#endif
    }
  }

  // Revert migration.
  {
    ASSERT_FALSE(
        IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/true);
    FlushAppService();

    EXPECT_TRUE(IsExtensionAppInstalled());
    EXPECT_FALSE(IsWebAppInstalled());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Re-pin the old Chrome app.
    app_list_syncable_service->SetPinPosition(
        kExtensionId, syncer::StringOrdinal("testpinposition"));
    EXPECT_EQ(app_list_syncable_service->GetSyncItem(kExtensionId)->ToString(),
              "kbmnembi { Nothing } [testapplistposition] [testpinposition]");
#endif
  }

  // Re-run migration.
  {
    base::HistogramTester histograms;
    base::AutoReset<bool> testing_scope =
        SetExternalAppInstallFeatureAlwaysEnabledForTesting();
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

    extensions::TestExtensionRegistryObserver uninstall_observer(
        extensions::ExtensionRegistry::Get(profile()));

    SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false);
    scoped_refptr<const extensions::Extension> uninstalled_app =
        uninstall_observer.WaitForExtensionUninstalled();
    EXPECT_EQ(uninstalled_app->id(), kExtensionId);
    FlushAppService();

    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_FALSE(IsExtensionAppInstalled());

    histograms.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramInstallResult,
        InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectUniqueSample(
        ExternalWebAppManager::kHistogramUninstallAndReplaceCount, 1, 1);

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Chrome OS shelf/list position should re-migrate.
    EXPECT_EQ(
        app_list_syncable_service->GetSyncItem(GetWebAppId())->ToString(),
        base::StringPrintf(
            "%s { Basic web app } [testapplistposition] [testpinposition]",
            GetWebAppId().substr(0, 8).c_str()));
    // Old Chrome app should get unpinned again.
    EXPECT_EQ(app_list_syncable_service->GetSyncItem(kExtensionId)->ToString(),
              "kbmnembi { Nothing } [testapplistposition] [INVALID[]]");
#endif
  }
}

IN_PROC_BROWSER_TEST_F(ExternalWebAppMigrationBrowserTest, MigratePreferences) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
    ASSERT_FALSE(
        IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

    SyncExternalExtensions();
    SyncExternalWebApps(/*expect_install=*/false, /*expect_uninstall=*/false);

    EXPECT_FALSE(IsWebAppInstalled());
    EXPECT_TRUE(IsExtensionAppInstalled());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    ChromeAppListItem* app_list_item =
        app_list_model_updater->FindItem(kExtensionId);
    app_list_item->SetPosition(syncer::StringOrdinal("testapplistposition"));
    app_list_model_updater->OnItemUpdated(app_list_item->CloneMetadata());
    app_list_syncable_service->SetPinPosition(
        kExtensionId, syncer::StringOrdinal("testpinposition"));
    EXPECT_EQ(app_list_syncable_service->GetSyncItem(kExtensionId)->ToString(),
              "kbmnembi { Nothing } [testapplistposition] [testpinposition]");
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
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

    SyncExternalExtensions();
    // Extension sticks around to be uninstalled by the replacement web app.
    EXPECT_TRUE(IsExtensionAppInstalled());

    {
      base::HistogramTester histograms;
      extensions::TestExtensionRegistryObserver uninstall_observer(
          extensions::ExtensionRegistry::Get(profile()));

      SyncExternalWebApps(/*expect_install=*/true, /*expect_uninstall=*/false);
      EXPECT_TRUE(IsWebAppInstalled());

      scoped_refptr<const extensions::Extension> uninstalled_app =
          uninstall_observer.WaitForExtensionUninstalled();
      EXPECT_EQ(uninstalled_app->id(), kExtensionId);
      EXPECT_FALSE(IsExtensionAppInstalled());
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramInstallResult,
          InstallResultCode::kSuccessNewInstall, 1);
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramUninstallAndReplaceCount, 1, 1);
    }
  }

  // Check UI preferences have migrated across.
  {
    const AppId web_app_id = GetWebAppId();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Chrome OS shelf/list position should migrate.
    EXPECT_EQ(
        app_list_syncable_service->GetSyncItem(GetWebAppId())->ToString(),
        base::StringPrintf(
            "%s { Basic web app } [testapplistposition] [testpinposition]",
            GetWebAppId().substr(0, 8).c_str()));
    // Chrome app shelf/list position should be retained.
    EXPECT_EQ(app_list_syncable_service->GetSyncItem(kExtensionId)->ToString(),
              "kbmnembi { Nothing } [testapplistposition] [testpinposition]");
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
    ASSERT_FALSE(
        IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

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
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

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
  ExternalInstallOptions options(GetWebAppUrl(), DisplayMode::kBrowser,
                                 ExternalInstallSource::kExternalDefault);
  options.gate_on_feature = kMigrationFlag;
  options.user_type_allowlist = {"unmanaged"};
  options.uninstall_and_replace.push_back(kExtensionId);
  options.only_use_app_info_factory = true;
  options.app_info_factory = base::BindLambdaForTesting([&]() {
    auto info = std::make_unique<WebApplicationInfo>();
    info->start_url = GetWebAppUrl();
    info->title = u"Test app";
    return info;
  });
  preinstalled_apps.apps.push_back(std::move(options));
  EXPECT_EQ(1u, GetPreinstalledWebApps().size());
  // Set up pre-migration state.
  {
    base::HistogramTester histograms;

    ASSERT_FALSE(
        IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

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
    ASSERT_TRUE(IsExternalAppInstallFeatureEnabled(kMigrationFlag, *profile()));

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
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramInstallResult,
          InstallResultCode::kSuccessOfflineOnlyInstall, 1);
      histograms.ExpectUniqueSample(
          ExternalWebAppManager::kHistogramUninstallAndReplaceCount, 1, 1);
    }
  }
}

}  // namespace web_app
