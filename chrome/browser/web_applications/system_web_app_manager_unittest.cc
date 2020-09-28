// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_app_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/pending_app_manager_impl.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/test_pending_app_manager_impl.h"
#include "chrome/browser/web_applications/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/test_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/test_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

namespace web_app {

namespace {
const char kSettingsAppInternalName[] = "OSSettings";
const char kDiscoverAppInternalName[] = "Discover";

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
  return options;
}
struct SystemAppData {
  GURL url;
  GURL icon_url;
  ExternalInstallSource source;
};

std::unique_ptr<WebApplicationInfo> GetApp1WebApplicationInfo() {
  std::unique_ptr<WebApplicationInfo> info =
      std::make_unique<WebApplicationInfo>();
  info->start_url = AppUrl1();
  info->scope = AppUrl1().GetWithoutFilename();
  info->title = base::UTF8ToUTF16("Foo Web App");
  return info;
}

class TestDataRetrieverFactory {
 public:
  TestDataRetrieverFactory() = delete;
  explicit TestDataRetrieverFactory(std::vector<SystemAppData> system_app_data)
      : system_app_data_(std::move(system_app_data)) {}

  std::unique_ptr<web_app::WebAppDataRetriever> CreateNextDataRetriever() {
    size_t task_index = task_index_++;

    auto data_retriever = std::make_unique<TestDataRetriever>();
    data_retriever->SetEmptyRendererWebApplicationInfo();

    // System apps require an icon specified in the manifest.
    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = GetSystemAppDataForTask(task_index).url;
    manifest->scope = GetSystemAppDataForTask(task_index).url;
    manifest->short_name = base::ASCIIToUTF16("Manifest SWA Name");

    blink::Manifest::ImageResource icon;
    icon.src = GetSystemAppDataForTask(task_index).icon_url;
    icon.purpose.push_back(blink::Manifest::ImageResource::Purpose::ANY);
    icon.sizes.emplace_back(gfx::Size(icon_size::k256, icon_size::k256));
    manifest->icons.push_back(std::move(icon));
    data_retriever->SetManifest(std::move(manifest),
                                /*is_installable=*/true);

    // Every InstallTask starts with WebAppDataRetriever::GetIcons step.
    data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
        [&, task_index](content::WebContents* web_contents,
                        const std::vector<GURL>& icon_urls,
                        bool skip_page_favicons) {
          IconsMap icons_map;
          AddIconToIconsMap(GetSystemAppDataForTask(task_index).icon_url,
                            icon_size::k256, SK_ColorBLUE, &icons_map);
          return icons_map;
        }));

    return std::unique_ptr<WebAppDataRetriever>(std::move(data_retriever));
  }

 private:
  const SystemAppData& GetSystemAppDataForTask(size_t task_index) const {
    DCHECK(task_index < system_app_data_.size());
    return system_app_data_[task_index];
  }

  size_t task_index_ = 0;
  std::vector<SystemAppData> system_app_data_;
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
  SystemWebAppManagerTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsWithoutExtensions}, {});
  }

  ~SystemWebAppManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    test_registry_controller_ =
        std::make_unique<TestWebAppRegistryController>();

    controller().SetUp(profile());

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
    test_file_handler_manager_ =
        std::make_unique<TestFileHandlerManager>(profile());
    icon_manager_ = std::make_unique<WebAppIconManager>(
        profile(), controller().registrar(), std::make_unique<TestFileUtils>());
    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
        profile(), &icon_manager(), /*legacy_finalizer=*/nullptr);
    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    test_pending_app_manager_impl_ =
        std::make_unique<TestPendingAppManagerImpl>(profile());
    test_os_integration_manager_ = std::make_unique<TestOsIntegrationManager>(
        profile(), /*app_shortcut_manager=*/nullptr,
        /*file_handler_manager=*/nullptr);
    test_system_web_app_manager_ =
        std::make_unique<TestSystemWebAppManager>(profile());
    test_ui_manager_ = std::make_unique<TestWebAppUiManager>();

    install_finalizer().SetSubsystems(&controller().registrar(), &ui_manager(),
                                      &controller().sync_bridge());

    install_manager().SetUrlLoaderForTesting(
        std::make_unique<TestWebAppUrlLoader>());
    install_manager().SetSubsystems(&controller().registrar(),
                                    &os_integration_manager(),
                                    &install_finalizer());

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    pending_app_manager().SetUrlLoaderForTesting(std::move(url_loader));
    pending_app_manager().SetSubsystems(
        &controller().registrar(), &os_integration_manager(), &ui_manager(),
        &install_finalizer(), &install_manager());

    system_web_app_manager().SetSubsystems(
        &pending_app_manager(), &controller().registrar(),
        &controller().sync_bridge(), &ui_manager(), &os_integration_manager());

    install_manager().Start();
    install_finalizer().Start();

    // TODO(https://crbug.com/1108611) we should use a single
    // TestOsIntegrationManager
    WebAppProviderBase::GetProviderBase(profile())
        ->os_integration_manager()
        .SuppressOsHooksForTesting();
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  void DestroyManagers() {
    // The reverse order of creation:
    test_ui_manager_.reset();
    test_system_web_app_manager_.reset();
    test_os_integration_manager_.reset();
    test_pending_app_manager_impl_.reset();
    install_manager_.reset();
    install_finalizer_.reset();
    icon_manager_.reset();
    test_file_handler_manager_.reset();
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

  TestFileHandlerManager& file_handler_manager() {
    return *test_file_handler_manager_;
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

  TestOsIntegrationManager& os_integration_manager() {
    return *test_os_integration_manager_;
  }

  TestWebAppUrlLoader& url_loader() { return *url_loader_; }

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

  void PrepareSystemAppDataToRetrieve(
      std::vector<SystemAppData> system_app_data) {
    test_data_retriever_factory_ =
        std::make_unique<TestDataRetrieverFactory>(std::move(system_app_data));
    install_manager().SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([this]() {
          DCHECK(test_data_retriever_factory_);
          return test_data_retriever_factory_->CreateNextDataRetriever();
        }));
  }

  void PrepareLoadUrlResults(const std::vector<GURL>& urls) {
    std::vector<WebAppUrlLoader::Result> load_results(
        urls.size(), WebAppUrlLoader::Result::kUrlLoaded);
    url_loader().AddPrepareForLoadResults(load_results);
    for (const auto& url : urls) {
      url_loader().SetNextLoadUrlResult(url,
                                        WebAppUrlLoader::Result::kUrlLoaded);
    }
  }

  void StartAndWaitForAppsToSynchronize() {
    SystemWebAppWaiter waiter(&system_web_app_manager());
    system_web_app_manager().Start();
    waiter.Wait();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestWebAppRegistryController> test_registry_controller_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;
  std::unique_ptr<TestFileHandlerManager> test_file_handler_manager_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<TestPendingAppManagerImpl> test_pending_app_manager_impl_;
  std::unique_ptr<TestSystemWebAppManager> test_system_web_app_manager_;
  std::unique_ptr<TestWebAppUiManager> test_ui_manager_;
  std::unique_ptr<TestOsIntegrationManager> test_os_integration_manager_;
  TestWebAppUrlLoader* url_loader_ = nullptr;
  std::unique_ptr<TestDataRetrieverFactory> test_data_retriever_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemWebAppManagerTest);
};

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, Enabled) {
  InitEmptyRegistrar();

  PrepareSystemAppDataToRetrieve(
      {{AppUrl1(), AppIconUrl1()}, {AppUrl2(), AppIconUrl2()}});
  PrepareLoadUrlResults({AppUrl1(), AppUrl2()});

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  StartAndWaitForAppsToSynchronize();

  EXPECT_FALSE(pending_app_manager().install_requests().empty());
}

// Test that System Apps do install with the feature enabled.
TEST_F(SystemWebAppManagerTest, InstallFromWebAppInfo) {
  InitEmptyRegistrar();

  PrepareSystemAppDataToRetrieve(
      {{AppUrl1(), AppIconUrl1()}, {AppUrl2(), AppIconUrl2()}});
  PrepareLoadUrlResults({AppUrl2()});

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(
      SystemAppType::SETTINGS,
      SystemAppInfo(kSettingsAppInternalName, AppUrl1(),
                    base::BindRepeating(&GetApp1WebApplicationInfo)));
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));

  system_web_app_manager().SetSystemAppsForTesting(std::move(system_apps));
  StartAndWaitForAppsToSynchronize();

  EXPECT_FALSE(pending_app_manager().install_requests().empty());
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));
}

// Test that changing the set of System Apps uninstalls apps.
TEST_F(SystemWebAppManagerTest, UninstallAppInstalledInPreviousSession) {
  // Simulate System Apps and a regular app that were installed in the
  // previous session.
  InitRegistrarWithSystemApps(
      {{AppUrl1(), AppIconUrl1(), ExternalInstallSource::kSystemInstalled},
       {AppUrl2(), AppIconUrl2(), ExternalInstallSource::kSystemInstalled},
       {AppUrl3(), AppIconUrl3(), ExternalInstallSource::kInternalDefault}});

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});

  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));

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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, pending_app_manager().install_requests().size());

  // Create another app. The version hasn't changed but the app should still
  // install.
  PrepareSystemAppDataToRetrieve(
      {{AppUrl1(), AppIconUrl1()}, {AppUrl2(), AppIconUrl2()}});
  PrepareLoadUrlResults({AppUrl1(), AppUrl2()});
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));
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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Create another app. The version hasn't changed, but we should immediately
  // install anyway, as if a user flipped a chrome://flag. The first app won't
  // force reinstall.
  PrepareSystemAppDataToRetrieve({{AppUrl2(), AppIconUrl2()}});
  PrepareLoadUrlResults({AppUrl2()});
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(3u, install_requests.size());
  EXPECT_FALSE(install_requests[1].force_reinstall);
  EXPECT_FALSE(install_requests[2].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  PrepareSystemAppDataToRetrieve(
      {{AppUrl1(), AppIconUrl1()}, {AppUrl2(), AppIconUrl2()}});
  PrepareLoadUrlResults({AppUrl1(), AppUrl2()});
  system_web_app_manager().set_current_version(base::Version("2.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(5u, install_requests.size());
  EXPECT_TRUE(install_requests[3].force_reinstall);
  EXPECT_TRUE(install_requests[4].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));
  EXPECT_TRUE(IsInstalled(AppUrl2()));

  // Changing the install URL of a system app propagates even without a
  // version change.
  PrepareSystemAppDataToRetrieve({{AppUrl3(), AppIconUrl3()}});
  PrepareLoadUrlResults({AppUrl3()});
  system_apps.find(SystemAppType::SETTINGS)->second.install_url = AppUrl3();
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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  // First execution.
  system_web_app_manager().set_current_locale("en-US");
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Change locale setting, should trigger reinstall.
  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
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
  const std::string discover_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kDiscoverAppInternalName;
  // Profile category for Chrome OS testing environment is "Other".
  const std::string profile_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) +
      ".Profiles.Other";

  InitEmptyRegistrar();
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kAlwaysUpdate);

  {
    PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
    PrepareLoadUrlResults({AppUrl1()});

    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
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
        InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectTotalCount(settings_app_install_result_histogram, 1);
    histograms.ExpectBucketCount(settings_app_install_result_histogram,
                                 InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectTotalCount(profile_install_result_histogram, 1);
    histograms.ExpectBucketCount(profile_install_result_histogram,
                                 InstallResultCode::kSuccessNewInstall, 1);
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 1);
  }

  pending_app_manager().SetHandleInstallRequestCallback(
      base::BindLambdaForTesting([](const ExternalInstallOptions&) {
        return InstallResultCode::kWebAppDisabled;
      }));

  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
    system_apps.emplace(SystemAppType::DISCOVER,
                        SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));
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
    histograms.ExpectBucketCount(discover_app_install_result_histogram,
                                 InstallResultCode::kWebAppDisabled, 1);
  }

  {
    base::flat_map<SystemAppType, SystemAppInfo> system_apps;
    system_apps.emplace(SystemAppType::SETTINGS,
                        SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
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
  const std::string discover_app_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) + ".Apps." +
      kDiscoverAppInternalName;
  // Profile category for Chrome OS testing environment is "Other".
  const std::string profile_install_result_histogram =
      std::string(SystemWebAppManager::kInstallResultHistogramName) +
      ".Profiles.Other";

  InitEmptyRegistrar();
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  pending_app_manager().SetHandleInstallRequestCallback(
      base::BindLambdaForTesting([](const ExternalInstallOptions& opts) {
        if (opts.install_url == AppUrl1())
          return InstallResultCode::kSuccessAlreadyInstalled;
        return InstallResultCode::kSuccessNewInstall;
      }));

  StartAndWaitForAppsToSynchronize();

  // Record results that aren't kSuccessAlreadyInstalled.
  histograms.ExpectTotalCount(SystemWebAppManager::kInstallResultHistogramName,
                              1);
  histograms.ExpectTotalCount(settings_app_install_result_histogram, 0);
  histograms.ExpectTotalCount(discover_app_install_result_histogram, 1);
  histograms.ExpectTotalCount(profile_install_result_histogram, 1);
}

TEST_F(SystemWebAppManagerTest,
       InstallDurationHistogram_ExcludeNonForceInstall) {
  base::HistogramTester histograms;

  InitEmptyRegistrar();
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_apps.emplace(SystemAppType::DISCOVER,
                      SystemAppInfo(kDiscoverAppInternalName, AppUrl2()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);
  system_web_app_manager().SetUpdatePolicy(
      SystemWebAppManager::UpdatePolicy::kOnVersionChange);

  {
    pending_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting([](const ExternalInstallOptions& opts) {
          if (opts.install_url == AppUrl1())
            return InstallResultCode::kWriteDataFailed;
          return InstallResultCode::kSuccessNewInstall;
        }));

    StartAndWaitForAppsToSynchronize();

    // The install duration histogram should be recorded, because the first
    // install happens on a clean profile.
    histograms.ExpectTotalCount(
        SystemWebAppManager::kInstallDurationHistogramName, 1);
  }

  {
    pending_app_manager().SetHandleInstallRequestCallback(
        base::BindLambdaForTesting([](const ExternalInstallOptions& opts) {
          if (opts.install_url == AppUrl1())
            return InstallResultCode::kSuccessNewInstall;
          return InstallResultCode::kSuccessAlreadyInstalled;
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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  system_web_app_manager().set_current_version(base::Version("1.0.0.0"));
  system_web_app_manager().set_current_locale("en/us");
  StartAndWaitForAppsToSynchronize();

  EXPECT_EQ(1u, install_requests.size());
  EXPECT_TRUE(install_requests[0].force_reinstall);
  EXPECT_TRUE(IsInstalled(AppUrl1()));

  // Bump the version number, and an update will trigger, and force
  // reinstallation of both apps.
  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
  // Set up and install a baseline
  base::flat_map<SystemAppType, SystemAppInfo> system_apps;
  system_apps.emplace(SystemAppType::SETTINGS,
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
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
  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});

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

  PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
  PrepareLoadUrlResults({AppUrl1()});
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
                      SystemAppInfo(kSettingsAppInternalName, AppUrl1()));
  system_web_app_manager().SetSystemAppsForTesting(system_apps);

  // Install the App normally.
  {
    PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
    PrepareLoadUrlResults({AppUrl1()});
    StartAndWaitForAppsToSynchronize();

    EXPECT_EQ(1u, install_requests.size());
    EXPECT_TRUE(IsInstalled(AppUrl1()));
  }

  // Enable AlwaysReinstallSystemWebApps feature, verify force_reinstall is set.
  {
    base::test::ScopedFeatureList feature_reinstall;
    feature_reinstall.InitAndEnableFeature(
        features::kAlwaysReinstallSystemWebApps);

    PrepareSystemAppDataToRetrieve({{AppUrl1(), AppIconUrl1()}});
    PrepareLoadUrlResults({AppUrl1()});
    StartAndWaitForAppsToSynchronize();

    EXPECT_EQ(2u, install_requests.size());
    EXPECT_TRUE(install_requests[1].force_reinstall);
    EXPECT_TRUE(IsInstalled(AppUrl1()));
  }
}

}  // namespace web_app
