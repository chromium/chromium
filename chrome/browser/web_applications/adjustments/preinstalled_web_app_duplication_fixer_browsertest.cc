// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/preinstalled_web_app_duplication_fixer.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/adjustments/web_app_adjustments.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"

#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
// #include "ash/public/cpp/shelf_model.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

class PreinstalledWebAppDuplicationFixerBrowserTest
    : public extensions::ExtensionBrowserTest,
      public testing::WithParamInterface<test::ExternalPrefMigrationTestCases> {
 public:
  static GURL install_url() {
    return GURL("https://www.example.com/install_url");
  }
  static GURL start_url() { return GURL("https://www.example.com/start_url"); }
  static AppId web_app_id() {
    return GenerateAppId(absl::nullopt, start_url());
  }
  static extensions::ExtensionId chrome_app_id() {
    return "kbmnembihfiondgfjekmnmcbddelicoi";
  }

  PreinstalledWebAppDuplicationFixerBrowserTest() {
    PreinstalledWebAppManager::SkipStartupForTesting();
    PreinstalledWebAppDuplicationFixer::SkipStartupForTesting();
    std::vector<base::test::FeatureRef> enabled_features{
        features::kPreinstalledWebAppDuplicationFixer};
    std::vector<base::test::FeatureRef> disabled_features;
    switch (GetParam()) {
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
    }
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~PreinstalledWebAppDuplicationFixerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    provider_ = WebAppProvider::GetForTest(browser()->profile());
    InProcessBrowserTest::SetUpOnMainThread();
    test::WaitUntilReady(provider_);

    ExternalInstallOptions options(install_url(), UserDisplayMode::kStandalone,
                                   ExternalInstallSource::kExternalDefault);
    options.user_type_allowlist = {"unmanaged"};
    options.uninstall_and_replace = {chrome_app_id()};
    options.only_use_app_info_factory = true;
    options.app_info_factory = base::BindRepeating(
        [](GURL start_url) {
          auto info = std::make_unique<WebAppInstallInfo>();
          info->title = u"Test app";
          info->start_url = start_url;
          return info;
        },
        start_url());
    options.expected_app_id = web_app_id();
    preinstalled_app_data_.apps = {options};
  }

  void SyncPreinstalledWebApps() {
    base::RunLoop run_loop;
    provider_->preinstalled_web_app_manager().LoadAndSynchronizeForTesting(
        base::BindLambdaForTesting(
            [&](std::map<GURL, ExternallyManagedAppManager::InstallResult>
                    install_results,
                std::map<GURL, bool> uninstall_results) { run_loop.Quit(); }));
    run_loop.Run();
  }

  void SyncPreinstalledWebAppsAwaitChromeAppUninstall() {
    extensions::TestExtensionRegistryObserver uninstall_observer(
        extensions::ExtensionRegistry::Get(profile()));
    SyncPreinstalledWebApps();
    ASSERT_EQ(uninstall_observer.WaitForExtensionUninstalled()->id(),
              chrome_app_id());
  }

  bool IsWebAppInstalled() const {
    return provider_->registrar_unsafe().IsInstalled(web_app_id());
  }

  bool IsWebAppExternallyInstalled() {
    return provider_->registrar_unsafe().LookupExternalAppId(install_url()) ==
           web_app_id();
  }

  bool IsPreinstalledWebAppUninstalled() {
    return UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
        .DoesAppIdExist(web_app_id());
  }

  bool IsWebAppInSync() const {
    return provider_->registrar_unsafe()
        .GetAppById(web_app_id())
        ->GetSources()
        .test(WebAppManagement::kSync);
  }

  void UninstallWebApp() { test::UninstallWebApp(profile(), web_app_id()); }

  void RunAppDuplicationFix() {
    WebAppAdjustmentsFactory::GetInstance()
        ->Get(profile())
        ->preinstalled_web_app_duplication_fixer()
        ->ScanForDuplicationForTesting();
  }

  std::vector<base::Bucket> GetFixCountMetrics() {
    return histogram_tester_.GetAllSamples(
        PreinstalledWebAppDuplicationFixer::kHistogramAppDuplicationFixApplied);
  }

  std::array<int64_t, 4> GetDuplicationMetrics() {
    return {
        histogram_tester_.GetTotalSum(
            PreinstalledWebAppDuplicationFixer::
                kHistogramWebAppAbsentChromeAppAbsent),
        histogram_tester_.GetTotalSum(
            PreinstalledWebAppDuplicationFixer::
                kHistogramWebAppAbsentChromeAppPresent),
        histogram_tester_.GetTotalSum(
            PreinstalledWebAppDuplicationFixer::
                kHistogramWebAppPresentChromeAppAbsent),
        histogram_tester_.GetTotalSum(
            PreinstalledWebAppDuplicationFixer::
                kHistogramWebAppPresentChromeAppPresent),
    };
  }

  void InstallChromeApp() {
    const extensions::Extension* extension =
        InstallExtension(test_data_dir_.AppendASCII("hosted_app.crx"), 1);
    ASSERT_EQ(extension->id(), chrome_app_id());
  }

  bool IsChromeAppInstalled() {
    return extensions::ExtensionRegistry::Get(profile())
        ->enabled_extensions()
        .Contains(chrome_app_id());
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void PinApp(const std::string& app_id) {
    auto* service =
        app_list::AppListSyncableServiceFactory::GetForProfile(profile());
    service->SetPinPosition(app_id, service->GetLastPosition());
  }

  bool IsAppPinned(const std::string& app_id) {
    return app_list::AppListSyncableServiceFactory::GetForProfile(profile())
        ->GetPinPosition(app_id)
        .IsValid();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 protected:
  raw_ptr<WebAppProvider, DanglingUntriaged> provider_;
  base::test::ScopedFeatureList feature_list_;
  ScopedTestingPreinstalledAppData preinstalled_app_data_;
  base::HistogramTester histogram_tester_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppDuplicationFixerBrowserTest,
                       FixDuplicateChromeApp) {
  SyncPreinstalledWebApps();
  EXPECT_TRUE(IsWebAppInstalled());
  EXPECT_FALSE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternallyInstalled());

  // Running the fix while the Chrome app is not installed should do nothing.
  {
    RunAppDuplicationFix();
    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_FALSE(IsChromeAppInstalled());
    EXPECT_EQ(GetFixCountMetrics(), (std::vector<base::Bucket>{{0, 1}}));
    EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 0, 1, 0}));
    EXPECT_TRUE(IsWebAppExternallyInstalled());
  }

  InstallChromeApp();
  EXPECT_TRUE(IsChromeAppInstalled());

  // Resyncing preinstalled web app configs again will not re-migrate the web
  // app despite the Chrome app being installed.
  {
    SyncPreinstalledWebApps();
    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_TRUE(IsChromeAppInstalled());
  }

  // Running the fix causes the Chrome app to get migrated on the next
  // preinstalled web app sync.
  {
    RunAppDuplicationFix();
    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_TRUE(IsChromeAppInstalled());
    EXPECT_EQ(GetFixCountMetrics(),
              (std::vector<base::Bucket>{{0, 1}, {1, 1}}));
    EXPECT_FALSE(IsWebAppExternallyInstalled());
    EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 0, 1, 1}));
    SyncPreinstalledWebAppsAwaitChromeAppUninstall();
    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_FALSE(IsChromeAppInstalled());
  }

  // Metrics now counts the web app as present and the Chrome app absent.
  {
    RunAppDuplicationFix();
    EXPECT_EQ(GetFixCountMetrics(),
              (std::vector<base::Bucket>{{0, 2}, {1, 1}}));
    EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 0, 2, 1}));
  }
}

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppDuplicationFixerBrowserTest,
                       RemigrateUninstalledWebApp) {
  SyncPreinstalledWebApps();
  InstallChromeApp();
  EXPECT_TRUE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternallyInstalled());

  UninstallWebApp();
  EXPECT_FALSE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_TRUE(IsPreinstalledWebAppUninstalled());

  RunAppDuplicationFix();
  EXPECT_FALSE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_EQ(GetFixCountMetrics(), (std::vector<base::Bucket>{{1, 1}}));
  EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 1, 0, 0}));
  EXPECT_FALSE(IsWebAppExternallyInstalled());
  EXPECT_FALSE(IsPreinstalledWebAppUninstalled());

  // Running the preinstalled web app sync should remigrate the old Chrome app
  // even if the user had uninstalled the web app.
  {
    SyncPreinstalledWebAppsAwaitChromeAppUninstall();
    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_FALSE(IsChromeAppInstalled());
  }

  // Metrics now counts the web app as present and the Chrome app absent.
  {
    RunAppDuplicationFix();
    EXPECT_EQ(GetFixCountMetrics(),
              (std::vector<base::Bucket>{{0, 1}, {1, 1}}));
    EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 1, 1, 0}));
  }
}

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppDuplicationFixerBrowserTest,
                       RunFixOnSyncInstalledWebApp) {
  SyncPreinstalledWebApps();
  InstallChromeApp();
  EXPECT_TRUE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternallyInstalled());

  // Simulate a user install of the same web app to put it in sync.
  {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = start_url();
    AppId app_id = test::InstallWebApp(profile(), std::move(info));
    ASSERT_EQ(app_id, web_app_id());
    ASSERT_TRUE(IsWebAppInSync());
  }

  // The duplication fix should have no effect on the sync install status of the
  // web app.
  {
    RunAppDuplicationFix();
    ASSERT_TRUE(IsWebAppInSync());
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_P(PreinstalledWebAppDuplicationFixerBrowserTest,
                       WebAppPinnedChromeAppUnpinned) {
  SyncPreinstalledWebApps();
  InstallChromeApp();

  PinApp(web_app_id());
  EXPECT_TRUE(IsAppPinned(web_app_id()));
  EXPECT_FALSE(IsAppPinned(chrome_app_id()));

  RunAppDuplicationFix();
  SyncPreinstalledWebAppsAwaitChromeAppUninstall();

  // If the web app already exists and is pinned it should not take on the
  // Chrome app's unpinned UI position.
  EXPECT_TRUE(IsAppPinned(web_app_id()));
}

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppDuplicationFixerBrowserTest,
                       WebAppUnpinnedChromeAppPinned) {
  SyncPreinstalledWebApps();
  InstallChromeApp();

  PinApp(chrome_app_id());
  EXPECT_FALSE(IsAppPinned(web_app_id()));
  EXPECT_TRUE(IsAppPinned(chrome_app_id()));

  RunAppDuplicationFix();
  SyncPreinstalledWebAppsAwaitChromeAppUninstall();

  // If the web app already exists and is not pinned it should take on the
  // Chrome app's pinned UI position.
  EXPECT_TRUE(IsAppPinned(web_app_id()));
}

IN_PROC_BROWSER_TEST_P(PreinstalledWebAppDuplicationFixerBrowserTest,
                       BothUnpinned) {
  SyncPreinstalledWebApps();
  InstallChromeApp();

  EXPECT_FALSE(IsAppPinned(web_app_id()));
  EXPECT_FALSE(IsAppPinned(chrome_app_id()));

  RunAppDuplicationFix();
  SyncPreinstalledWebAppsAwaitChromeAppUninstall();

  // If neither app was pinned the web app should remain unpinned.
  EXPECT_FALSE(IsAppPinned(web_app_id()));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

INSTANTIATE_TEST_SUITE_P(
    All,
    PreinstalledWebAppDuplicationFixerBrowserTest,
    ::testing::Values(
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB),
    test::GetExternalPrefMigrationTestName);

}  // namespace web_app
