// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_manager.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_uninstall_job.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/common/chrome_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

namespace {

std::unique_ptr<WebAppInstallTask> CreateDummyTask() {
  return std::make_unique<WebAppInstallTask>(
      /*profile=*/nullptr,
      /*install_finalizer=*/nullptr,
      /*data_retriever=*/nullptr,
      /*registrar=*/nullptr, webapps::WebappInstallSource::EXTERNAL_DEFAULT);
}

// TODO(crbug.com/1194709): Retire SyncParam after Lacros ships.
enum class SyncParam { kWithoutSync = 0, kWithSync = 1, kMaxValue = kWithSync };

}  // namespace

class WebAppInstallManagerTest
    : public WebAppTest,
      public ::testing::WithParamInterface<SyncParam> {
 public:
  WebAppInstallManagerTest() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (GetParam() == SyncParam::kWithSync) {
      // Disable WebAppsCrosapi, so that Web Apps get synced in the Ash browser.
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kWebAppsCrosapi, chromeos::features::kLacrosPrimary});
    } else {
      // Enable WebAppsCrosapi, so that Web Apps don't get synced in the Ash
      // browser.
      scoped_feature_list_.InitAndEnableFeature(features::kWebAppsCrosapi);
    }
#else
    DCHECK(GetParam() == SyncParam::kWithSync);
#endif
  }

  void SetUp() override {
    WebAppTest::SetUp();

    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());

    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();
    fake_registry_controller_->SetUp(profile());

    file_utils_ = base::MakeRefCounted<TestFileUtils>();
    icon_manager_ = std::make_unique<WebAppIconManager>(profile(), file_utils_);

    policy_manager_ = std::make_unique<WebAppPolicyManager>(profile());

    install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(profile());

    install_manager_ = std::make_unique<WebAppInstallManager>(profile());
    install_manager_->SetSubsystems(
        &registrar(), &controller().os_integration_manager(),
        &fake_registry_controller_->command_manager(),
        install_finalizer_.get());

    auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();

    test_url_loader_ = test_url_loader.get();
    install_manager_->SetUrlLoaderForTesting(std::move(test_url_loader));

    ui_manager_ = std::make_unique<FakeWebAppUiManager>();

    icon_manager_->SetSubsystems(&registrar(), &install_manager());

    install_finalizer_->SetSubsystems(
        &install_manager(), &registrar(), ui_manager_.get(),
        &fake_registry_controller_->sync_bridge(),
        &fake_registry_controller_->os_integration_manager(),
        icon_manager_.get(), policy_manager_.get(),
        &fake_registry_controller_->translation_manager());
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  WebAppRegistrar& registrar() { return controller().registrar(); }
  WebAppCommandManager& command_manager() {
    return fake_registry_controller_->command_manager();
  }
  WebAppInstallManager& install_manager() { return *install_manager_; }
  WebAppInstallFinalizer& finalizer() { return *install_finalizer_; }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  TestWebAppUrlLoader& url_loader() { return *test_url_loader_; }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }
  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }
  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  std::unique_ptr<WebApp> CreateWebAppFromSyncAndPendingInstallation(
      const GURL& start_url,
      const std::string& app_name,
      absl::optional<UserDisplayMode> user_display_mode,
      SkColor theme_color,
      bool is_locally_installed,
      const GURL& scope,
      const std::vector<apps::IconInfo>& icon_infos) {
    auto web_app = test::CreateWebApp(start_url, WebAppManagement::kSync);
    web_app->SetIsFromSyncAndPendingInstallation(true);
    web_app->SetIsLocallyInstalled(is_locally_installed);
    DCHECK(user_display_mode.has_value());
    web_app->SetUserDisplayMode(*user_display_mode);

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = app_name;
    sync_fallback_data.theme_color = theme_color;
    sync_fallback_data.scope = scope;
    sync_fallback_data.icon_infos = icon_infos;
    web_app->SetSyncFallbackData(std::move(sync_fallback_data));
    return web_app;
  }

  void InitEmptyRegistrar() {
    controller().Init();
    install_finalizer_->Start();
    install_manager_->Start();
  }

  std::set<AppId> InitRegistrarWithRegistry(const Registry& registry) {
    std::set<AppId> app_ids;
    for (auto& kv : registry)
      app_ids.insert(kv.second->app_id());

    controller().database_factory().WriteRegistry(registry);

    controller().Init();
    install_finalizer_->Start();
    install_manager_->Start();

    return app_ids;
  }

  AppId InitRegistrarWithApp(std::unique_ptr<WebApp> app) {
    DCHECK(registrar().is_empty());

    AppId app_id = app->app_id();

    Registry registry;
    registry.emplace(app_id, std::move(app));

    InitRegistrarWithRegistry(registry);
    return app_id;
  }

  struct InstallResult {
    AppId app_id;
    webapps::InstallResultCode code;
  };

  InstallResult InstallSubApp(const AppId& parent_app_id,
                              const GURL& install_url) {
    UseDefaultDataRetriever(install_url);
    url_loader().SetNextLoadUrlResult(install_url,
                                      WebAppUrlLoader::Result::kUrlLoaded);
    InstallResult result;
    base::RunLoop run_loop;
    install_manager().InstallSubApp(
        parent_app_id, install_url,
        GenerateAppId(/*manifest_id=*/{}, install_url),
        /*dialog_callback=*/
        base::BindLambdaForTesting(
            [](content::WebContents* initiator_web_contents,
               std::unique_ptr<WebAppInstallInfo> web_app_info,
               web_app::WebAppInstallationAcceptanceCallback
                   acceptance_callback) {
              std::move(acceptance_callback)
                  .Run(/*user_accepted=*/true, std::move(web_app_info));
            }),
        /*install_callback=*/
        base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                       webapps::InstallResultCode code) {
          result.app_id = installed_app_id;
          result.code = code;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  InstallResult InstallWebAppFromInfo(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_source) {
    InstallResult result;
    base::RunLoop run_loop;
    command_manager().ScheduleCommand(
        std::make_unique<web_app::InstallFromInfoCommand>(
            std::move(install_info), &finalizer(),
            overwrite_existing_manifest_fields, install_source,
            base::BindLambdaForTesting([&](const AppId& installed_app_id,
                                           webapps::InstallResultCode code) {
              result.app_id = installed_app_id;
              result.code = code;
              run_loop.Quit();
            })));

    run_loop.Run();
    return result;
  }

  std::map<SquareSizePx, SkBitmap> ReadIcons(const AppId& app_id,
                                             IconPurpose purpose,
                                             const SortedSizesPx& sizes_px) {
    std::map<SquareSizePx, SkBitmap> result;
    base::RunLoop run_loop;
    icon_manager().ReadIcons(
        app_id, purpose, sizes_px,
        base::BindLambdaForTesting(
            [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
              result = std::move(icon_bitmaps);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  int GetNumFullyInstalledApps() const {
    int num_apps = 0;

    for ([[maybe_unused]] const WebApp& app :
         fake_registry_controller_->registrar().GetApps()) {
      ++num_apps;
    }

    return num_apps;
  }

  webapps::UninstallResultCode UninstallPolicyWebAppByUrl(const GURL& app_url) {
    webapps::UninstallResultCode result;
    base::RunLoop run_loop;
    finalizer().UninstallExternalWebAppByUrl(
        app_url, WebAppManagement::kPolicy,
        webapps::WebappUninstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          result = code;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  webapps::UninstallResultCode UninstallWebApp(const AppId& app_id) {
    webapps::UninstallResultCode result;
    base::RunLoop run_loop;
    finalizer().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kAppMenu,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          result = code;
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }

  void UseDefaultDataRetriever(const GURL& start_url) {
    install_manager().SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([start_url]() {
          auto data_retriever = std::make_unique<FakeDataRetriever>();
          data_retriever->BuildDefaultDataToRetrieve(start_url, start_url);
          return std::unique_ptr<WebAppDataRetriever>(
              std::move(data_retriever));
        }));
  }

  bool WasPreinstalledWebAppUninstalled(const AppId app_id) {
    return UserUninstalledPreinstalledWebAppPrefs(profile()->GetPrefs())
        .DoesAppIdExist(app_id);
  }

  void DestroyManagers() {
    if (ui_manager_)
      ui_manager_->Shutdown();
    if (install_manager_)
      install_manager_->Shutdown();
    if (icon_manager_)
      icon_manager_->Shutdown();
    if (install_finalizer_)
      install_finalizer_->Shutdown();
    if (fake_registry_controller_)
      fake_registry_controller_->DestroySubsystems();

    ui_manager_.reset();
    policy_manager_.reset();
    icon_manager_.reset();
    fake_registry_controller_.reset();
    externally_installed_app_prefs_.reset();
    install_finalizer_.reset();
    install_manager_.reset();

    test_url_loader_ = nullptr;
    file_utils_ = nullptr;
  }

  static std::string ParamInfoToString(
      testing::TestParamInfo<WebAppInstallManagerTest::ParamType> info) {
    switch (info.param) {
      case SyncParam::kWithSync:
        return "WithSync";
      case SyncParam::kWithoutSync:
        return "WithoutSync";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
  std::unique_ptr<WebAppIconManager> icon_manager_;
  std::unique_ptr<WebAppPolicyManager> policy_manager_;
  std::unique_ptr<WebAppInstallManager> install_manager_;
  std::unique_ptr<WebAppInstallFinalizer> install_finalizer_;
  std::unique_ptr<FakeWebAppUiManager> ui_manager_;
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;

  // A weak ptr. The original is owned by install_manager_.
  raw_ptr<TestWebAppUrlLoader> test_url_loader_ = nullptr;
  scoped_refptr<TestFileUtils> file_utils_;
};

using WebAppInstallManagerTest_SyncOnly = WebAppInstallManagerTest;

TEST_P(WebAppInstallManagerTest_SyncOnly,
       UninstallFromSyncAfterRegistryUpdate) {
  std::unique_ptr<WebApp> app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  app->SetUserDisplayMode(UserDisplayMode::kStandalone);

  const AppId app_id = app->app_id();
  InitRegistrarWithApp(std::move(app));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  enum Event {
    kUninstallFromSync,
    kObserver_OnWebAppWillBeUninstalled,
    kObserver_OnWebAppUninstalled,
    kUninstallFromSync_Callback
  };
  std::vector<Event> event_order;

  WebAppInstallManagerObserverAdapter observer(&install_manager());
  observer.SetWebAppWillBeUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppWillBeUninstalled);
      }));
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppUninstalled);
      }));

  base::RunLoop run_loop;
  controller().SetUninstallFromSyncDelegate(base::BindLambdaForTesting(
      [&](const std::vector<AppId>& apps_to_uninstall,
          SyncInstallDelegate::RepeatingUninstallCallback callback) {
        ASSERT_FALSE(apps_to_uninstall.empty());
        EXPECT_EQ(apps_to_uninstall[0], app_id);
        event_order.push_back(Event::kUninstallFromSync);
        install_manager().UninstallFromSync(
            std::move(apps_to_uninstall),
            base::BindLambdaForTesting(
                [&, callback](const AppId& uninstalled_app_id,
                              bool uninstalled) {
                  EXPECT_EQ(uninstalled_app_id, app_id);
                  EXPECT_TRUE(uninstalled);
                  event_order.push_back(Event::kUninstallFromSync_Callback);
                  run_loop.Quit();
                  callback.Run(uninstalled_app_id, uninstalled);
                }));
      }));

  // The sync server sends a change to delete the app.
  sync_bridge_test_utils::DeleteApps(controller().sync_bridge(), {app_id});
  run_loop.Run();

  const std::vector<Event> expected_event_order{
      Event::kUninstallFromSync, Event::kObserver_OnWebAppWillBeUninstalled,
      Event::kObserver_OnWebAppUninstalled, Event::kUninstallFromSync_Callback};
  EXPECT_EQ(expected_event_order, event_order);
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       UninstallFromSyncAfterRegistryUpdateInstallManagerObserver) {
  std::unique_ptr<WebApp> app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  app->SetUserDisplayMode(UserDisplayMode::kStandalone);

  const AppId app_id = app->app_id();
  InitRegistrarWithApp(std::move(app));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  enum Event {
    kUninstallFromSync,
    kObserver_OnWebAppWillBeUninstalled,
    kObserver_OnWebAppUninstalled,
    kUninstallFromSync_Callback
  };
  std::vector<Event> event_order;

  WebAppInstallManagerObserverAdapter observer(&install_manager());
  observer.SetWebAppWillBeUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppWillBeUninstalled);
      }));
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        event_order.push_back(Event::kObserver_OnWebAppUninstalled);
      }));

  base::RunLoop run_loop;
  controller().SetUninstallFromSyncDelegate(base::BindLambdaForTesting(
      [&](const std::vector<AppId>& apps_to_uninstall,
          SyncInstallDelegate::RepeatingUninstallCallback callback) {
        ASSERT_FALSE(apps_to_uninstall.empty());
        EXPECT_EQ(apps_to_uninstall[0], app_id);
        event_order.push_back(Event::kUninstallFromSync);
        install_manager().UninstallFromSync(
            std::move(apps_to_uninstall),
            base::BindLambdaForTesting(
                [&, callback](const AppId& uninstalled_app_id,
                              bool uninstalled) {
                  EXPECT_EQ(uninstalled_app_id, app_id);
                  EXPECT_TRUE(uninstalled);
                  event_order.push_back(Event::kUninstallFromSync_Callback);
                  run_loop.Quit();
                  callback.Run(uninstalled_app_id, uninstalled);
                }));
      }));

  // The sync server sends a change to delete the app.
  sync_bridge_test_utils::DeleteApps(controller().sync_bridge(), {app_id});
  run_loop.Run();

  const std::vector<Event> expected_event_order{
      Event::kUninstallFromSync, Event::kObserver_OnWebAppWillBeUninstalled,
      Event::kObserver_OnWebAppUninstalled, Event::kUninstallFromSync_Callback};
  EXPECT_EQ(expected_event_order, event_order);
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       PolicyAndUser_UninstallExternalWebApp) {
  std::unique_ptr<WebApp> policy_and_user_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  policy_and_user_app->AddSource(WebAppManagement::kPolicy);
  policy_and_user_app->SetUserDisplayMode(UserDisplayMode::kStandalone);

  const AppId app_id = policy_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/policy");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalPolicy);
  InitRegistrarWithApp(std::move(policy_and_user_app));

  EXPECT_FALSE(WasPreinstalledWebAppUninstalled(app_id));

  bool observer_uninstall_called = false;
  WebAppInstallManagerObserverAdapter observer(&install_manager());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        observer_uninstall_called = true;
      }));

  // Unknown url fails.
  EXPECT_EQ(webapps::UninstallResultCode::kNoAppToUninstall,
            UninstallPolicyWebAppByUrl(GURL("https://example.org/")));

  // Uninstall policy app first.
  EXPECT_EQ(webapps::UninstallResultCode::kSuccess,
            UninstallPolicyWebAppByUrl(external_app_url));

  EXPECT_TRUE(registrar().GetAppById(app_id));
  EXPECT_FALSE(observer_uninstall_called);
  EXPECT_FALSE(WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       PolicyAndUser_UninstallExternalWebAppInstallManagerObserver) {
  std::unique_ptr<WebApp> policy_and_user_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  policy_and_user_app->AddSource(WebAppManagement::kPolicy);
  policy_and_user_app->SetUserDisplayMode(UserDisplayMode::kStandalone);

  const AppId app_id = policy_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/policy");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalPolicy);
  InitRegistrarWithApp(std::move(policy_and_user_app));

  EXPECT_FALSE(WasPreinstalledWebAppUninstalled(app_id));

  bool observer_uninstall_called = false;
  WebAppInstallManagerObserverAdapter observer(&install_manager());
  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        observer_uninstall_called = true;
      }));

  // Unknown url fails.
  EXPECT_EQ(webapps::UninstallResultCode::kNoAppToUninstall,
            UninstallPolicyWebAppByUrl(GURL("https://example.org/")));

  // Uninstall policy app first.
  EXPECT_EQ(webapps::UninstallResultCode::kSuccess,
            UninstallPolicyWebAppByUrl(external_app_url));

  EXPECT_TRUE(registrar().GetAppById(app_id));
  EXPECT_FALSE(observer_uninstall_called);
  EXPECT_FALSE(WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
}

TEST_P(WebAppInstallManagerTest_SyncOnly, DefaultAndUser_UninstallWebApp) {
  std::unique_ptr<WebApp> default_and_user_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  default_and_user_app->AddSource(WebAppManagement::kDefault);
  default_and_user_app->SetUserDisplayMode(UserDisplayMode::kStandalone);
  default_and_user_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kDefault, GURL("https://example.com/path"));

  const AppId app_id = default_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/default");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalDefault);
  InitRegistrarWithApp(std::move(default_and_user_app));

  EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
  EXPECT_FALSE(WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_TRUE(registrar().IsActivelyInstalled(app_id));

  WebAppInstallManagerObserverAdapter observer(&install_manager());

  bool observer_uninstalled_called = false;

  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(app_id, uninstalled_app_id);
        observer_uninstalled_called = true;
      }));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  EXPECT_EQ(webapps::UninstallResultCode::kSuccess, UninstallWebApp(app_id));

  EXPECT_FALSE(registrar().GetAppById(app_id));
  EXPECT_TRUE(observer_uninstalled_called);
  EXPECT_FALSE(finalizer().CanUserUninstallWebApp(app_id));
  EXPECT_TRUE(WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_FALSE(registrar().IsActivelyInstalled(app_id));
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       DefaultAndUser_UninstallWebAppInstallManagerObserver) {
  std::unique_ptr<WebApp> default_and_user_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  default_and_user_app->AddSource(WebAppManagement::kDefault);
  default_and_user_app->SetUserDisplayMode(UserDisplayMode::kStandalone);
  default_and_user_app->AddInstallURLToManagementExternalConfigMap(
      WebAppManagement::kDefault, GURL("https://example.com/path"));

  const AppId app_id = default_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/default");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalDefault);
  InitRegistrarWithApp(std::move(default_and_user_app));

  EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
  EXPECT_FALSE(WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_TRUE(registrar().IsActivelyInstalled(app_id));

  WebAppInstallManagerObserverAdapter observer(&install_manager());

  bool observer_uninstalled_called = false;

  observer.SetWebAppUninstalledDelegate(
      base::BindLambdaForTesting([&](const AppId& uninstalled_app_id) {
        EXPECT_EQ(app_id, uninstalled_app_id);
        observer_uninstalled_called = true;
      }));

  file_utils().SetNextDeleteFileRecursivelyResult(true);

  EXPECT_EQ(webapps::UninstallResultCode::kSuccess, UninstallWebApp(app_id));

  EXPECT_FALSE(registrar().GetAppById(app_id));
  EXPECT_TRUE(observer_uninstalled_called);
  EXPECT_FALSE(finalizer().CanUserUninstallWebApp(app_id));
  EXPECT_TRUE(WasPreinstalledWebAppUninstalled(app_id));
  EXPECT_FALSE(registrar().IsActivelyInstalled(app_id));
}

TEST_P(WebAppInstallManagerTest, TaskQueueWebContentsReadyRace) {
  InitEmptyRegistrar();

  std::unique_ptr<WebAppInstallTask> task_a = CreateDummyTask();
  WebAppInstallTask* task_a_ptr = task_a.get();
  std::unique_ptr<WebAppInstallTask> task_b = CreateDummyTask();
  std::unique_ptr<WebAppInstallTask> task_c = CreateDummyTask();

  // Enqueue task A and await it to be started.
  base::RunLoop run_loop_a_start;
  url_loader().SetPrepareForLoadResultLoaded();
  install_manager().EnsureWebContentsCreated();
  install_manager().EnqueueTask(std::move(task_a),
                                run_loop_a_start.QuitClosure());
  run_loop_a_start.Run();

  // Enqueue task B before A has finished.
  bool task_b_started = false;
  install_manager().EnqueueTask(
      std::move(task_b),
      base::BindLambdaForTesting([&]() { task_b_started = true; }));

  // Finish task A.
  url_loader().SetPrepareForLoadResultLoaded();
  install_manager().OnQueuedTaskCompleted(
      task_a_ptr, base::DoNothing(), AppId(),
      webapps::InstallResultCode::kSuccessNewInstall);

  // Task B needs to wait for WebContents to return ready.
  EXPECT_FALSE(task_b_started);

  // Enqueue task C before B has started.
  bool task_c_started = false;
  install_manager().EnqueueTask(
      std::move(task_c),
      base::BindLambdaForTesting([&]() { task_c_started = true; }));

  // Task C should not start before B has started.
  EXPECT_FALSE(task_b_started);
  EXPECT_FALSE(task_c_started);
}

TEST_P(WebAppInstallManagerTest, DefaultNotActivelyInstalled) {
  std::unique_ptr<WebApp> default_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kDefault);
  default_app->SetDisplayMode(DisplayMode::kStandalone);
  default_app->SetUserDisplayMode(UserDisplayMode::kBrowser);

  const AppId app_id = default_app->app_id();
  const GURL external_app_url("https://example.com/path/default");

  externally_installed_app_prefs().Insert(
      external_app_url, app_id, ExternalInstallSource::kExternalDefault);
  InitRegistrarWithApp(std::move(default_app));

  EXPECT_FALSE(registrar().IsActivelyInstalled(app_id));
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       InstallWebAppFromWebAppStoreThenInstallFromSync) {
  InitEmptyRegistrar();

  const GURL start_url("https://example.com/path");
  const AppId app_id = GenerateAppId(/*manifest_id=*/absl::nullopt, start_url);

  // Reproduces `ApkWebAppInstaller` install parameters.
  auto apk_install_info = std::make_unique<WebAppInstallInfo>();
  apk_install_info->start_url = start_url;
  apk_install_info->scope = GURL("https://example.com/apk_scope");
  apk_install_info->title = u"Name from APK";
  apk_install_info->theme_color = SK_ColorWHITE;
  apk_install_info->display_mode = DisplayMode::kStandalone;
  apk_install_info->user_display_mode = UserDisplayMode::kStandalone;
  AddGeneratedIcon(&apk_install_info->icon_bitmaps.any, icon_size::k128,
                   SK_ColorYELLOW);

  InstallResult result =
      InstallWebAppFromInfo(std::move(apk_install_info),
                            /*overwrite_existing_manifest_fields=*/false,
                            webapps::WebappInstallSource::ARC);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  const WebApp* web_app = registrar().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());
  EXPECT_FALSE(web_app->IsSynced());
  EXPECT_FALSE(web_app->is_from_sync_and_pending_installation());

  ASSERT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(SK_ColorWHITE, web_app->theme_color().value());
  EXPECT_EQ("Name from APK", web_app->untranslated_name());
  EXPECT_EQ("https://example.com/apk_scope", web_app->scope().spec());

  ASSERT_TRUE(web_app->sync_fallback_data().theme_color.has_value());
  EXPECT_EQ(SK_ColorWHITE, web_app->sync_fallback_data().theme_color.value());
  EXPECT_EQ("Name from APK", web_app->sync_fallback_data().name);
  EXPECT_EQ("https://example.com/apk_scope",
            web_app->sync_fallback_data().scope.spec());

  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());
  EXPECT_EQ(UserDisplayMode::kStandalone, web_app->user_display_mode());

  EXPECT_TRUE(web_app->manifest_icons().empty());
  EXPECT_TRUE(web_app->sync_fallback_data().icon_infos.empty());

  EXPECT_EQ(SK_ColorYELLOW, IconManagerReadAppIconPixel(icon_manager(), app_id,
                                                        icon_size::k128));

  // Simulates the same web app arriving from sync.
  {
    auto synced_specifics_data = std::make_unique<WebApp>(app_id);
    synced_specifics_data->SetStartUrl(start_url);

    synced_specifics_data->AddSource(WebAppManagement::kSync);
    synced_specifics_data->SetUserDisplayMode(UserDisplayMode::kBrowser);
    synced_specifics_data->SetName("Name From Sync");

    WebApp::SyncFallbackData sync_fallback_data;
    sync_fallback_data.name = "Name From Sync";
    sync_fallback_data.theme_color = SK_ColorMAGENTA;
    sync_fallback_data.scope = GURL("https://example.com/sync_scope");

    apps::IconInfo apps_icon_info = CreateIconInfo(
        /*icon_base_url=*/start_url, IconPurpose::MONOCHROME, icon_size::k64);
    sync_fallback_data.icon_infos.push_back(std::move(apps_icon_info));

    synced_specifics_data->SetSyncFallbackData(std::move(sync_fallback_data));

    std::vector<std::unique_ptr<WebApp>> add_synced_apps_data;
    add_synced_apps_data.push_back(std::move(synced_specifics_data));
    sync_bridge_test_utils::AddApps(controller().sync_bridge(),
                                    add_synced_apps_data);
    // No apps installs should be triggered.
    EXPECT_THAT(registrar().GetAppsFromSyncAndPendingInstallation(),
                testing::IsEmpty());
  }

  EXPECT_EQ(web_app, registrar().GetAppById(app_id));

  EXPECT_TRUE(web_app->IsWebAppStoreInstalledApp());
  EXPECT_TRUE(web_app->IsSynced());
  EXPECT_FALSE(web_app->is_from_sync_and_pending_installation());

  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());
  EXPECT_EQ(UserDisplayMode::kBrowser, web_app->user_display_mode());
  EXPECT_TRUE(registrar().IsActivelyInstalled(app_id));

  ASSERT_TRUE(web_app->theme_color().has_value());
  EXPECT_EQ(SK_ColorWHITE, web_app->theme_color().value());
  EXPECT_EQ("Name from APK", web_app->untranslated_name());
  EXPECT_EQ("https://example.com/apk_scope", web_app->scope().spec());

  ASSERT_TRUE(web_app->sync_fallback_data().theme_color.has_value());
  EXPECT_EQ(SK_ColorMAGENTA, web_app->sync_fallback_data().theme_color.value());
  EXPECT_EQ("Name From Sync", web_app->sync_fallback_data().name);
  EXPECT_EQ("https://example.com/sync_scope",
            web_app->sync_fallback_data().scope.spec());

  EXPECT_TRUE(web_app->manifest_icons().empty());
  ASSERT_EQ(1u, web_app->sync_fallback_data().icon_infos.size());

  const apps::IconInfo& app_icon_info =
      web_app->sync_fallback_data().icon_infos[0];
  EXPECT_EQ(apps::IconInfo::Purpose::kMonochrome, app_icon_info.purpose);
  EXPECT_EQ(icon_size::k64, app_icon_info.square_size_px);
  EXPECT_EQ("https://example.com/icon-64.png", app_icon_info.url.spec());

  EXPECT_EQ(SK_ColorYELLOW, IconManagerReadAppIconPixel(icon_manager(), app_id,
                                                        icon_size::k128));
}

TEST_P(WebAppInstallManagerTest_SyncOnly, InstallSubApp) {
  const GURL parent_url{"https://example.com/parent"};
  const AppId parent_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, parent_url);
  const GURL install_url{"https://example.com/sub/app"};
  const AppId app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, install_url);
  const GURL second_install_url{"https://example.com/sub/second_app"};
  const AppId second_app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, second_install_url);

  InitEmptyRegistrar();

  // Install a sub-app and verify a bunch of things.
  InstallResult result = InstallSubApp(parent_app_id, install_url);

  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(app_id, result.app_id);

  EXPECT_TRUE(registrar().IsInstalled(app_id));
  EXPECT_TRUE(registrar().IsLocallyInstalled(app_id));
  EXPECT_EQ(DisplayMode::kStandalone,
            registrar().GetAppEffectiveDisplayMode(app_id));
  EXPECT_TRUE(registrar().IsActivelyInstalled(app_id));

  const WebApp* app = registrar().GetAppById(app_id);
  EXPECT_EQ(parent_app_id, app->parent_app_id());
  EXPECT_TRUE(app->IsSubAppInstalledApp());
  EXPECT_TRUE(app->CanUserUninstallWebApp());

  // One sub-app.
  EXPECT_EQ(1ul, registrar().GetAllSubAppIds(parent_app_id).size());

  // Check that we get |kSuccessAlreadyInstalled| if we try installing the same
  // app again.
  EXPECT_EQ(webapps::InstallResultCode::kSuccessAlreadyInstalled,
            InstallSubApp(parent_app_id, install_url).code);

  // Still one sub-app.
  EXPECT_EQ(1ul, registrar().GetAllSubAppIds(parent_app_id).size());

  // Install a different sub-app and verify count equals 2.
  result = InstallSubApp(parent_app_id, second_install_url);
  EXPECT_EQ(webapps::InstallResultCode::kSuccessNewInstall, result.code);
  EXPECT_EQ(second_app_id, result.app_id);

  // Two sub-apps.
  EXPECT_EQ(2ul, registrar().GetAllSubAppIds(parent_app_id).size());
}

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppInstallManagerTest,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                         ::testing::Values(SyncParam::kWithoutSync,
                                           SyncParam::kWithSync),
#else
                         ::testing::Values(SyncParam::kWithSync),
#endif
                         WebAppInstallManagerTest::ParamInfoToString);

INSTANTIATE_TEST_SUITE_P(All,
                         WebAppInstallManagerTest_SyncOnly,
                         ::testing::Values(SyncParam::kWithSync),
                         WebAppInstallManagerTest::ParamInfoToString);

}  // namespace web_app
