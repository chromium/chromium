// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_pending_app_manager_impl.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace web_app {

namespace {
const char kSettingsAppInternalName[] = "OSSettings";
const char kCameraAppInternalName[] = "Camera";

GURL AppUrl1() {
  return GURL(content::GetWebUIURL("system-app1"));
}
GURL AppIconUrl1() {
  return GURL(content::GetWebUIURL("system-app1/app.ico"));
}
GURL AppUrl2() {
  return GURL(content::GetWebUIURL("system-app2"));
}
GURL AppIconUrl2() {
  return GURL(content::GetWebUIURL("system-app2/app.ico"));
}
GURL AppUrl3() {
  return GURL(content::GetWebUIURL("system-app3"));
}
GURL AppIconUrl3() {
  return GURL(content::GetWebUIURL("system-app3/app.ico"));
}

std::unique_ptr<WebApplicationInfo> GetApp1WebApplicationInfo() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = AppUrl1();
  info->scope = AppUrl1().GetWithoutFilename();
  info->title = u"Foo Web App";
  return info;
}

WebApplicationInfoFactory GetApp1WebAppInfoFactory() {
  // "static" so that ExternalInstallOptions comparisons in tests work.
  static auto factory = base::BindRepeating(&GetApp1WebApplicationInfo);
  return factory;
}

std::unique_ptr<WebApplicationInfo> GetApp2WebApplicationInfo() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = AppUrl2();
  info->scope = AppUrl2().GetWithoutFilename();
  info->title = u"Bar Web App";
  return info;
}

WebApplicationInfoFactory GetApp2WebAppInfoFactory() {
  // "static" so that ExternalInstallOptions comparisons in tests work.
  static auto factory = base::BindRepeating(&GetApp2WebApplicationInfo);
  return factory;
}

std::unique_ptr<WebApplicationInfo> GetApp3WebApplicationInfo() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = AppUrl3();
  info->scope = AppUrl3().GetWithoutFilename();
  info->title = u"Bar Web App";
  return info;
}

WebApplicationInfoFactory GetApp3WebAppInfoFactory() {
  // "static" so that ExternalInstallOptions comparisons in tests work.
  static auto factory = base::BindRepeating(&GetApp3WebApplicationInfo);
  return factory;
}

ExternalInstallOptions GetWindowedInstallOptions() {
  ExternalInstallOptions options(AppUrl1(), DisplayMode::kStandalone,
                                 ExternalInstallSource::kSystemInstalled);
  options.add_to_applications_menu = true;
  options.add_to_desktop = false;
  options.add_to_quick_launch_bar = false;
  options.add_to_search = true;
  options.add_to_management = false;
  options.is_disabled = false;
  options.bypass_service_worker_check = true;
  options.force_reinstall = true;
  options.only_use_app_info_factory = true;
  options.system_app_type = SystemAppType::SETTINGS;
  options.app_info_factory = GetApp1WebAppInfoFactory();
  return options;
}

struct SystemAppData {
  GURL url;
  GURL icon_url;
  ExternalInstallSource source;
};

class SystemWebAppWaiter {
 public:
  explicit SystemWebAppWaiter(SystemWebAppManager* system_web_app_manager) {
    system_web_app_manager->ResetOnAppsSynchronizedForTesting();
    system_web_app_manager->on_apps_synchronized().Post(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          // Wait one execution loop for on_apps_synchronized() to be
          // called on all listeners.
          base::SequencedTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, run_loop_.QuitClosure());
        }));
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class SystemWebAppManagerTest : public WebAppTest {
 public:
  SystemWebAppManagerTest() = default;
  template <typename... TaskEnvironmentTraits>
  explicit SystemWebAppManagerTest(TaskEnvironmentTraits&&... traits)
      : WebAppTest(std::forward<TaskEnvironmentTraits>(traits)...) {}
  SystemWebAppManagerTest(const SystemWebAppManagerTest&) = delete;
  SystemWebAppManagerTest& operator=(const SystemWebAppManagerTest&) = delete;

  ~SystemWebAppManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();

    controller().SetUp(profile());

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
    icon_manager_ = std::make_unique<WebAppIconManager>(
        profile(), controller().registrar(), std::make_unique<TestFileUtils>());
    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), &icon_manager(), /*legacy_finalizer=*/nullptr);
    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    test_pending_app_manager_impl_ =
        std::make_unique<TestPendingAppManagerImpl>(profile());
    web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());
    test_system_web_app_manager_ =
        std::make_unique<TestSystemWebAppManager>(profile());
    test_ui_manager_ = std::make_unique<TestWebAppUiManager>();

    install_finalizer().SetSubsystems(&controller().registrar(), &ui_manager(),
                                      &controller().sync_bridge(),
                                      &controller().os_integration_manager());

    install_manager().SetSubsystems(&controller().registrar(),
                                    &controller().os_integration_manager(),
                                    &install_finalizer());

    pending_app_manager().SetSubsystems(
        &controller().registrar(), &controller().os_integration_manager(),
        &ui_manager(), &install_finalizer(), &install_manager());

    web_app_policy_manager().SetSubsystems(
        &pending_app_manager(), &controller().registrar(),
        &controller().sync_bridge(), &system_web_app_manager(),
        &controller().os_integration_manager());

    system_web_app_manager().SetSubsystems(
        &pending_app_manager(), &controller().registrar(),
        &controller().sync_bridge(), &ui_manager(),
        &controller().os_integration_manager(), &web_app_policy_manager());

    install_manager().Start();
    install_finalizer().Start();
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  void DestroyManagers() {
    // The reverse order of creation:
    test_ui_manager_.reset();
    test_system_web_app_manager_.reset();
    web_app_policy_manager_.reset();
    test_pending_app_manager_impl_.reset();
    install_manager_.reset();
    install_finalizer_.reset();
    icon_manager_.reset();
    externally_installed_app_prefs_.reset();
    test_registry_controller_.reset();
  }

 protected:
  TestWebAppRegistryController& controller() {
    return *test_registry_controller_;
  }

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  WebAppIconManager& icon_manager() { return *icon_manager_; }

  WebAppInstallFinalizer& install_finalizer() { return *install_finalizer_; }

  WebAppInstallManager& install_manager() { return *install_manager_; }

  TestPendingAppManagerImpl& pending_app_manager() {
    return *test_pending_app_manager_impl_;
  }

  TestSystemWebAppManager& system_web_app_manager() {
    return *test_system_web_app_manager_;
  }

  TestWebAppUiManager& ui_manager() { return *test_ui_manager_; }

  WebAppPolicyManager& web_app_policy_manager() {
    return *web_app_policy_manager_;
  }

  bool IsInstalled(const GURL& install_url) {
    return controller().registrar().IsInstalled(
        GenerateAppIdFromURL(install_url));
  }

  std::unique_ptr<WebApp> CreateWebApp(
      const GURL& start_url,
      Source::Type source_type = Source::kDefault) {
    const AppId app_id = GenerateAppIdFromURL(start_url);

    auto web_app = std::make_unique<WebApp>(app_id);
    web_app->SetStartUrl(start_url);
    web_app->SetName("App Name");
    web_app->AddSource(source_type);
    web_app->SetDisplayMode(DisplayMode::kStandalone);
    web_app->SetUserDisplayMode(DisplayMode::kStandalone);
    return web_app;
  }

  std::unique_ptr<WebApp> CreateSystemWebApp(const GURL& start_url) {
    return CreateWebApp(start_url, Source::Type::kSystem);
  }

  void InitRegistrarWithRegistry(const Registry& registry) {
    controller().database_factory().WriteRegistry(registry);
    controller().Init();
  }

  void InitRegistrarWithSystemApps(
      std::vector<SystemAppData> system_app_data_list) {
    DCHECK(controller().registrar().is_empty());
    DCHECK(!system_app_data_list.empty());

    Registry registry;
    for (const SystemAppData& data : system_app_data_list) {
      std::unique_ptr<WebApp> web_app = CreateSystemWebApp(data.url);
      const AppId app_id = web_app->app_id();
      registry.emplace(app_id, std::move(web_app));

      externally_installed_app_prefs().Insert(
          data.url, GenerateAppIdFromURL(data.url), data.source);
    }
    InitRegistrarWithRegistry(registry);
  }

  void InitEmptyRegistrar() {
    Registry registry;
    InitRegistrarWithRegistry(registry);
  }

  void StartAndWaitForAppsToSynchronize() {
    SystemWebAppWaiter waiter(&system_web_app_manager());
    system_web_app_manager().Start();
    waiter.Wait();
  }

 private:
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<TestPendingAppManagerImpl> test_pending_app_manager_impl_;
  std::unique_ptr<TestSystemWebAppManager> test_system_web_app_manager_;
  std::unique_ptr<TestWebAppUiManager> test_ui_manager_;
  std::unique_ptr<WebAppPolicyManager> web_app_policy_manager_;
};

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, Enabled) {
  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_apps.emplace(SystemAppType::CAMERA,
                      SystemAppInfo(kCameraAppInternalName, AppUrl2(),
                                    GetApp2WebAppInfoFactory()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(2u, pending_app_manager().install_requests().size());
}

// Test that changing the set of System Apps uninstalls apps.
TEST_F(SystemWebAppManagerTest, UninstallAppInstalledInPreviousSession) {
  // Simulate System Apps and a regular app that were installed in the
  // previous session.
  InitRegistrarWithSystemApps(
      {{AppUrl1(), AppIconUrl1(), ExternalInstallSource::kSystemInstalled},
       {AppUrl2(), AppIconUrl2(), ExternalInstallSource::kSystemInstalled},
       {AppUrl3(), AppIconUrl3(), ExternalInstallSource::kInternalDefault}});

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  StartAndWaitForAppsToSynchronize();

  // We should only try to install the app in the System App list.
  std::vector<ExternalInstallOptions> expected_install_options_list;
  expected_install_options_list.push_back(GetWindowedInstallOptions());
  EXPECT_EQ(pending_app_manager().install_requests(),
            expected_install_options_list);

  // We should try to uninstall the app that is no longer in the System App
  // list.
  EXPECT_EQ(std::vector<GURL>({AppUrl2()}),
            pending_app_manager().uninstall_requests());
}

TEST_F(SystemWebAppManagerTest, AlwaysUpdate) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, pending_app_manager().install_requests().size());

  // Create another app. The version hasn't changed but the app should still
  // install.
  system_apps.emplace(SystemAppType::CAMERA,
                      SystemAppInfo(kCameraAppInternalName, AppUrl2(),
                                    GetApp2WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  // This one returns because on_apps_synchronized runs immediately.
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(3u, pending_app_manager().install_requests().size());
}

TEST_F(SystemWebAppManagerTest, UpdateOnVersionChange) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager().install_requests();

  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Create another app. The version hasn't changed, but we should immediately
  // install anyway, as if a user flipped a chrome://flag. The first app won't
  // force reinstall.
  system_apps.emplace(SystemAppType::CAMERA,
                      SystemAppInfo(kCameraAppInternalName, AppUrl2(),
                                    GetApp2WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(3u, install_requests.size());
  EXPECT_FALSE(install_requests[1].force_reinstall);
  EXPECT_FALSE(install_requests[2].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(5u, install_requests.size());
  EXPECT_TRUE(install_requests[3].force_reinstall);
  EXPECT_TRUE(install_requests[4].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Changing the install URL of a system app propagates even without a
  // version change.
  system_apps.find(SystemAppType::SETTINGS)->second.install_url = AppUrl3();
  system_apps.find(SystemAppType::SETTINGS)->second.app_info_factory =
      GetApp3WebAppInfoFactory();
  system_web_app_manager().SetSystemAppsForTesting(system_apps);
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(7u, install_requests.size());
  EXPECT_FALSE(install_requests[5].force_reinstall);
  EXPECT_FALSE(install_requests[6].force_reinstall);
  EXPECT_FALSE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));
  EXPECT_TRUE(IsInstalled(AppUrl3()));
}

TEST_F(SystemWebAppManagerTest, UpdateOnLocaleChange) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager().install_requests();

  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  // First execution.
  system_web_app_manager().set_current_locale("en-US");
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Change locale setting, should trigger reinstall.
  system_web_app_manager().set_current_locale("ja");
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(2u, install_requests.size());
  EXPECT_TRUE(install_requests[1].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Do not reinstall because locale is not changed.
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(3u, install_requests.size());
  EXPECT_FALSE(install_requests[2].force_reinstall);
}

TEST_F(SystemWebAppManagerTest, InstallResultHistogram) {
  base::HistogramTester histograms;
  const std::string settings_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kSettingsAppInternalName;
  const std::string camera_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kCameraAppInternalName;
  // Profile category for Chrome OS testing environment is "Other".
  const std::string profile_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) +
      ".Profiles.Other";

  InitEmptyRegistrar();
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                      GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(system_apps);

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 0);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 0);
    histograms.ExpectTotalCount(profile_install_result_histogram, 0);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 0);

    StartAndWaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 1);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kSuccessOfflineOnlyInstall, 1);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 1);
    histograms.ExpectBucketCount(settings_app_install_result_histogram,
                                 InstallResultCode::kSuccessOfflineOnlyInstall,
                                 1);
    histograms.ExpectTotalCount(profile_install_result_histogram, 1);
    histograms.ExpectBucketCount(profile_install_result_histogram,
                                 InstallResultCode::kSuccessOfflineOnlyInstall,
                                 1);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 1);
  }

  pending_app_manager().SetHandleInstallRequestCallback(
      base::BindLambdaForTesting([](const ExternalInstallOptions&)
                                     -> PendingAppManager::InstallResult {
        return {.code = InstallResultCode::kWebAppDisabled};
      }));

  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                      GetApp1WebAppInfoFactory()));
    system_apps.emplace(SystemAppType::CAMERA,
                        SystemAppInfo(kCameraAppInternalName, AppUrl2(),
                                      GetApp2WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(system_apps);

    StartAndWaitForAppsToSynchronize();

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallResultHistogramName, 3);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kWebAppDisabled, 2);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 2);
    histograms.ExpectBucketCount(settings_app_install_result_histogram,
                                 InstallResultCode::kWebAppDisabled, 1);
    histograms.ExpectBucketCount(camera_app_install_result_histogram,
                                 InstallResultCode::kWebAppDisabled, 1);
  }

  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                      GetApp1WebAppInfoFactory()));
    system_web_app_manager().SetSystemAppsForTesting(system_apps);

    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 2);
    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 0);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 0);

    {
      SystemWebAppWaiter waiter(&system_web_app_manager());
      system_web_app_manager().Start();
      system_web_app_manager().Shutdown();
      waiter.Wait();
    }

    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    histograms.ExpectBucketCount(
        SystemWebAppManager::kInstallResultHistogramName,
        InstallResultCode::kWebAppDisabled, 2);

    histograms.ExpectBucketCount(
        settings_app_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    histograms.ExpectBucketCount(
        profile_install_result_histogram,
        InstallResultCode::kCancelledOnWebAppProviderShuttingDown, 1);
    // If install was interrupted by shutdown, do not report duration.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 2);
  }
}

TEST_F(SystemWebAppManagerTest,
       InstallResultHistogram_ExcludeAlreadyInstalled) {
  base::HistogramTester histograms;
  const std::string settings_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kSettingsAppInternalName;
  const std::string camera_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kCameraAppInternalName;
  // Profile category for Chrome OS testing environment is "Other".
  const std::string profile_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) +
      ".Profiles.Other";

  InitEmptyRegistrar();
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_apps.emplace(SystemAppType::CAMERA,
                      SystemAppInfo(kCameraAppInternalName, AppUrl2(),
                                    GetApp2WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  pending_app_manager().SetHandleInstallRequestCallback(
      base::BindLambdaForTesting([](const ExternalInstallOptions& opts)
                                     -> PendingAppManager::InstallResult {
        if (opts.install_url == AppUrl1())
          return {.code = InstallResultCode::kSuccessAlreadyInstalled};
        return {.code = InstallResultCode::kSuccessNewInstall};
      }));

  StartAndWaitForAppsToSynchronize();

  // Record results that aren't kSuccessAlreadyInstalled.
  histograms.ExpectTotalCount(SystemWebAppManager::kInstallResultHistogramName,
                              1);
  histograms.ExpectTotalCount(settings_app_install_result_histogram, 0);
  histograms.ExpectTotalCount(camera_app_install_result_histogram, 1);
  histograms.ExpectTotalCount(profile_install_result_histogram, 1);
}

TEST_F(SystemWebAppManagerTest,
       InstallDurationHistogram_ExcludeNonForceInstall) {
  base::HistogramTester histograms;

  InitEmptyRegistrar();
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_apps.emplace(SystemAppType::CAMERA,
                      SystemAppInfo(kCameraAppInternalName, AppUrl2(),
                                    GetApp2WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    pending_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting([](const ExternalInstallOptions& opts)
                                       -> PendingAppManager::InstallResult {
          if (opts.install_url == AppUrl1())
            return {.code = InstallResultCode::kWriteDataFailed};
          return {.code = InstallResultCode::kSuccessNewInstall};
        }));

    StartAndWaitForAppsToSynchronize();

    // The install duration histogram should be recorded, because the first
    // install happens on a clean profile.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 1);
  }

  {
    pending_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting([](const ExternalInstallOptions& opts)
                                       -> PendingAppManager::InstallResult {
          if (opts.install_url == AppUrl1())
            return {.code = InstallResultCode::kSuccessNewInstall};
          return {.code = InstallResultCode::kSuccessAlreadyInstalled};
        }));
    StartAndWaitForAppsToSynchronize();

    // Don't record install duration histogram, because this time we don't ask
    // to force install all apps.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 1);
  }
}

TEST_F(SystemWebAppManagerTest, AbandonFailedInstalls) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager().install_requests();

  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  pending_app_manager().SetDropRequestsForTesting(true);
  // Can't use the normal method because RunLoop::Run goes until
  // on_app_synchronized is called, and this fails, never calling that.
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  // 1 successful, 1 abandoned, and 3 more abanonded retries is 5.
  EXPECT_EQ(5u, install_requests.size());
  EXPECT_TRUE(install_requests[1].force_reinstall);
  EXPECT_TRUE(install_requests[2].force_reinstall);
  EXPECT_TRUE(install_requests[3].force_reinstall);
  EXPECT_TRUE(install_requests[4].force_reinstall);

  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // If we don't abandon at the same version, it doesn't even attempt another
  // request
  pending_app_manager().SetDropRequestsForTesting(false);
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_EQ(5u, install_requests.size());

  // Bump the version, and it works.
  system_web_app_manager().set_current_version(base::Version("3.0.0.0"));
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  EXPECT_EQ(6u, install_requests.size());
}

// Same test, but for locale change.
TEST_F(SystemWebAppManagerTest, AbandonFailedInstallsLocaleChange) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager().install_requests();

  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager().set_current_locale("en/us");
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  system_web_app_manager().set_current_locale("en/au");
  pending_app_manager().SetDropRequestsForTesting(true);
  // Can't use the normal method because RunLoop::Run goes until
  // on_app_synchronized is called, and this fails, never calling that.
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  // 1 successful, 1 abandoned, and 3 more abanonded retries is 5.
  EXPECT_EQ(5u, install_requests.size());
  EXPECT_TRUE(install_requests[1].force_reinstall);
  EXPECT_TRUE(install_requests[2].force_reinstall);
  EXPECT_TRUE(install_requests[3].force_reinstall);
  EXPECT_TRUE(install_requests[4].force_reinstall);

  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // If we don't abandon at the same version, it doesn't even attempt another
  // request
  pending_app_manager().SetDropRequestsForTesting(false);
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_EQ(5u, install_requests.size());

  // Bump the version, and it works.
  system_web_app_manager().set_current_locale("fr/fr");
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
}

TEST_F(SystemWebAppManagerTest, SucceedsAfterOneRetry) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager().install_requests();

  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  InitEmptyRegistrar();

  // Set up and install a baseline
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  // Bump the version number, and an update will trigger, and force
  // reinstallation. But, this fails!
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  pending_app_manager().SetDropRequestsForTesting(true);

  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  EXPECT_EQ(2u, install_requests.size());
  EXPECT_TRUE(install_requests[1].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  system_web_app_manager().Start();
  base::RunLoop().RunUntilIdle();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  // Retry a few times, but not until abandonment.
  EXPECT_EQ(3u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(install_requests[1].force_reinstall);
  EXPECT_TRUE(install_requests[2].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Now we succeed at the same version
  pending_app_manager().SetDropRequestsForTesting(false);
  StartAndWaitForAppsToSynchronize();
  pending_app_manager().ClearSynchronizeRequestsForTesting();

  StartAndWaitForAppsToSynchronize();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_EQ(5u, install_requests.size());
  EXPECT_TRUE(install_requests[3].force_reinstall);
  EXPECT_FALSE(install_requests[4].force_reinstall);

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps. This succeeds, everything works.
  system_web_app_manager().set_current_version(base::Version("3.0.0.0"));

  StartAndWaitForAppsToSynchronize();
  pending_app_manager().ClearSynchronizeRequestsForTesting();
  EXPECT_EQ(6u, install_requests.size());
  EXPECT_TRUE(install_requests[5].force_reinstall);

  StartAndWaitForAppsToSynchronize();
  EXPECT_EQ(7u, install_requests.size());
  EXPECT_FALSE(install_requests[6].force_reinstall);
}

TEST_F(SystemWebAppManagerTest, ForceReinstallFeature) {
  const std::vector<ExternalInstallOptions>& install_requests =
      pending_app_manager().install_requests();

  InitEmptyRegistrar();
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  // Register a test system app.
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                                    GetApp1WebAppInfoFactory()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  // Install the App normally.
  {
    StartAndWaitForAppsToSynchronize();

    EXPECT_EQ(1u, install_requests.size());
    EXPECT_TRUE(IsInstalled(AppUrl1()));
  }

  // Enable AlwaysReinstallSystemWebApps feature, verify force_reinstall is set.
  {
    base::test::ScopedFeatureList feature_reinstall;
    feature_reinstall.InitAndEnableFeature(
        features::kAlwaysReinstallSystemWebApps);

    StartAndWaitForAppsToSynchronize();

    EXPECT_EQ(2u, install_requests.size());
    EXPECT_TRUE(install_requests[1].force_reinstall);
    EXPECT_TRUE(IsInstalled(AppUrl1()));
  }
}

TEST_F(SystemWebAppManagerTest, IsSWABeforeSync) {
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  InitEmptyRegistrar();

  // Set up and install a baseline
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                    base::BindRepeating(&GetApp1WebApplicationInfo)));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();
  EXPECT_TRUE(
      system_web_app_manager().IsSystemWebApp(GenerateAppIdFromURL(AppUrl1())));

  auto unsynced_system_web_app_manager =
      std::make_unique<TestSystemWebAppManager>(profile());

  unsynced_system_web_app_manager->SetSubsystems(
      &pending_app_manager(), &controller().registrar(),
      &controller().sync_bridge(), &ui_manager(),
      &controller().os_integration_manager(), &web_app_policy_manager());

  unsynced_system_web_app_manager->SetSystemAppsForTesting(system_apps);

  EXPECT_TRUE(unsynced_system_web_app_manager->IsSystemWebApp(
      GenerateAppIdFromURL(AppUrl1())));
}

class SystemWebAppManagerTimerTest : public SystemWebAppManagerTest {
 public:
  SystemWebAppManagerTimerTest()
      : SystemWebAppManagerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(SystemWebAppManagerTimerTest, TestTimer) {
  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                    base::BindRepeating(&GetApp1WebApplicationInfo)));

  system_apps.at(SystemAppType::SETTINGS).timer_info =
      SystemAppBackgroundTaskInfo();
  system_apps.at(SystemAppType::SETTINGS).timer_info->period =
      base::TimeDelta::FromSeconds(60);
  system_apps.at(SystemAppType::SETTINGS).timer_info->url = AppUrl1();

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  StartAndWaitForAppsToSynchronize();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(false, timers[0]->open_immediately_for_testing());

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  TestWebAppUrlLoader* loader = url_loader.get();
  timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
  loader->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  loader->SetNextLoadUrlResult(AppUrl1(), WebAppUrlLoader::Result::kUrlLoaded);

  EXPECT_EQ(base::TimeDelta::FromSeconds(60), timers[0]->period_for_testing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(0u, timers[0]->opened_count_for_testing());
  // Fast forward until the timer fires.
  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(61));
  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());

  loader->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  loader->SetNextLoadUrlResult(AppUrl1(), WebAppUrlLoader::Result::kUrlLoaded);

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(61));

  EXPECT_EQ(2u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(2u, timers[0]->opened_count_for_testing());

  loader->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  loader->SetNextLoadUrlResult(AppUrl1(),
                               WebAppUrlLoader::Result::kFailedUnknownReason);

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(61));

  EXPECT_EQ(3u, timers[0]->timer_activated_count_for_testing());
  // The timer fired, but we couldn't open the page.
  EXPECT_EQ(2u, timers[0]->opened_count_for_testing());
}

TEST_F(SystemWebAppManagerTimerTest, TestTimerStartsImmediately) {
  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                    base::BindRepeating(&GetApp1WebApplicationInfo)));

  system_apps.at(SystemAppType::SETTINGS).timer_info =
      SystemAppBackgroundTaskInfo();
  system_apps.at(SystemAppType::SETTINGS).timer_info->period =
      base::TimeDelta::FromSeconds(300);
  system_apps.at(SystemAppType::SETTINGS).timer_info->url = AppUrl1();
  system_apps.at(SystemAppType::SETTINGS).timer_info->open_immediately = true;

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  TestWebAppUrlLoader* loader = nullptr;
  SystemWebAppWaiter waiter(&system_web_app_manager());

  // We need to wait until the web contents and url loader are created to
  // intercept the url loader with a TestWebAppUrlLoader. Do that by having a
  // hook into on_apps_synchronized.
  system_web_app_manager().on_apps_synchronized().Post(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();

        auto url_loader = std::make_unique<TestWebAppUrlLoader>();
        loader = url_loader.get();
        timers[0]->SetUrlLoaderForTesting(std::move(url_loader));
        loader->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
        loader->SetNextLoadUrlResult(AppUrl1(),
                                     WebAppUrlLoader::Result::kUrlLoaded);
      }));
  system_web_app_manager().Start();
  waiter.Wait();

  auto& timers = system_web_app_manager().GetBackgroundTasksForTesting();

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(121));
  EXPECT_EQ(1u, timers.size());
  EXPECT_EQ(true, timers[0]->open_immediately_for_testing());
  EXPECT_EQ(base::TimeDelta::FromSeconds(300), timers[0]->period_for_testing());
  EXPECT_EQ(1u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(1u, timers[0]->opened_count_for_testing());

  loader->AddPrepareForLoadResults({WebAppUrlLoader::Result::kUrlLoaded});
  loader->SetNextLoadUrlResult(AppUrl1(), WebAppUrlLoader::Result::kUrlLoaded);

  task_environment()->FastForwardBy(base::TimeDelta::FromSeconds(300));

  EXPECT_EQ(2u, timers[0]->timer_activated_count_for_testing());
  EXPECT_EQ(2u, timers[0]->opened_count_for_testing());
}

TEST_F(SystemWebAppManagerTest,
       HonorsRegisteredAppsDespiteOfPersistedWebAppInfo) {
  InitEmptyRegistrar();

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                    base::BindRepeating(&GetApp1WebApplicationInfo)));
  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));

  base::RunLoop run_loop;
  system_web_app_manager().on_apps_synchronized().Post(FROM_HERE,
                                                       run_loop.QuitClosure());
  system_web_app_manager().Start();
  run_loop.Run();

  // App should be installed.
  auto opt_app_id =
      system_web_app_manager().GetAppIdForSystemApp(SystemAppType::SETTINGS);
  ASSERT_TRUE(opt_app_id.has_value());
  auto opt_type =
      system_web_app_manager().GetSystemAppTypeForAppId(*opt_app_id);
  ASSERT_TRUE(opt_type.has_value());
  ASSERT_EQ(SystemAppType::SETTINGS, *opt_type);

  // Creates a new SystemWebAppManager without the previously installed App.
  auto unsynced_system_web_app_manager =
      std::make_unique<TestSystemWebAppManager>(profile());

  unsynced_system_web_app_manager->SetSubsystems(
      &pending_app_manager(), &controller().registrar(),
      &controller().sync_bridge(), &ui_manager(),
      &controller().os_integration_manager(), &web_app_policy_manager());

  // Before Apps are synchronized, WebAppRegistry should know about the App.
  const WebApp* web_app = controller().registrar().GetAppById(*opt_app_id);
  ASSERT_TRUE(web_app);
  ASSERT_TRUE(web_app->client_data().system_web_app_data.has_value());
  ASSERT_EQ(SystemAppType::SETTINGS,
            web_app->client_data().system_web_app_data->system_app_type);

  // Checks the new SystemWebAppManager reports the App being non-SWA.
  auto opt_app_id2 = unsynced_system_web_app_manager->GetAppIdForSystemApp(
      SystemAppType::SETTINGS);
  EXPECT_FALSE(opt_app_id2.has_value());
  auto opt_type2 =
      unsynced_system_web_app_manager->GetSystemAppTypeForAppId(*opt_app_id);
  EXPECT_FALSE(opt_type2.has_value());
  EXPECT_FALSE(unsynced_system_web_app_manager->IsSystemWebApp(*opt_app_id));
}

}  // namespace web_app
