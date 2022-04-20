// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/preinstalled_web_app_duplication_fixer.h"

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
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

class PreinstalledWebAppDuplicationFixerBrowserTest
    : public extensions::ExtensionBrowserTest {
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
  }
  ~PreinstalledWebAppDuplicationFixerBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();

    provider_ = WebAppProvider::GetForTest(browser()->profile());
    InProcessBrowserTest::SetUpOnMainThread();
    web_app::test::WaitUntilReady(provider_);

    ExternalInstallOptions options(install_url(), DisplayMode::kStandalone,
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
    return provider_->registrar().IsInstalled(web_app_id());
  }

  bool IsWebAppExternalInstallPrefSet() {
    return ExternallyInstalledWebAppPrefs(profile()->GetPrefs())
               .LookupAppId(install_url()) == web_app_id();
  }

  bool IsWebAppInSync() const {
    return provider_->registrar()
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
  WebAppProvider* provider_;
  base::test::ScopedFeatureList feature_list_{
      features::kPreinstalledWebAppDuplicationFixer};
  ScopedTestingPreinstalledAppData preinstalled_app_data_;
  base::HistogramTester histogram_tester_;
  OsIntegrationManager::ScopedSuppressForTesting os_hooks_supress_;
};

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppDuplicationFixerBrowserTest,
                       FixDuplicateChromeApp) {
  SyncPreinstalledWebApps();
  EXPECT_TRUE(IsWebAppInstalled());
  EXPECT_FALSE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternalInstallPrefSet());

  // Running the fix while the Chrome app is not installed should do nothing.
  {
    RunAppDuplicationFix();
    EXPECT_TRUE(IsWebAppInstalled());
    EXPECT_FALSE(IsChromeAppInstalled());
    EXPECT_EQ(GetFixCountMetrics(), (std::vector<base::Bucket>{{0, 1}}));
    EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 0, 1, 0}));
    EXPECT_TRUE(IsWebAppExternalInstallPrefSet());
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
    EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 0, 1, 1}));
    EXPECT_FALSE(IsWebAppExternalInstallPrefSet());

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

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppDuplicationFixerBrowserTest,
                       RemigrateUninstalledWebApp) {
  SyncPreinstalledWebApps();
  InstallChromeApp();
  EXPECT_TRUE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternalInstallPrefSet());

  UninstallWebApp();
  EXPECT_FALSE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternalInstallPrefSet());

  RunAppDuplicationFix();
  EXPECT_FALSE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_EQ(GetFixCountMetrics(), (std::vector<base::Bucket>{{1, 1}}));
  EXPECT_EQ(GetDuplicationMetrics(), (std::array<int64_t, 4>{0, 1, 0, 0}));
  EXPECT_FALSE(IsWebAppExternalInstallPrefSet());

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

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppDuplicationFixerBrowserTest,
                       RunFixOnSyncInstalledWebApp) {
  SyncPreinstalledWebApps();
  InstallChromeApp();
  EXPECT_TRUE(IsWebAppInstalled());
  EXPECT_TRUE(IsChromeAppInstalled());
  EXPECT_TRUE(IsWebAppExternalInstallPrefSet());

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
IN_PROC_BROWSER_TEST_F(PreinstalledWebAppDuplicationFixerBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppDuplicationFixerBrowserTest,
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

IN_PROC_BROWSER_TEST_F(PreinstalledWebAppDuplicationFixerBrowserTest,
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

}  // namespace web_app
