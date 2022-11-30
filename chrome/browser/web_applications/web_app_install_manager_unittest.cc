// Copyright 2019 The Chromium Authors
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
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
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
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
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

    provider_ = web_app::FakeWebAppProvider::Get(profile());
    provider_->SetDefaultFakeSubsystems();

    file_utils_ = base::MakeRefCounted<TestFileUtils>();
    auto icon_manager =
        std::make_unique<WebAppIconManager>(profile(), file_utils_);
    icon_manager_ = icon_manager.get();

    auto install_finalizer =
        std::make_unique<WebAppInstallFinalizer>(profile());
    install_finalizer_ = install_finalizer.get();

    auto install_manager = std::make_unique<WebAppInstallManager>(profile());
    install_manager_ = install_manager.get();

    // These are needed to set up the WebAppSyncBridge for testing.
    auto command_manager = std::make_unique<WebAppCommandManager>(profile());
    auto registrar = std::make_unique<WebAppRegistrarMutable>(profile());
    registrar_ = registrar.get();
    auto sync_bridge = std::make_unique<WebAppSyncBridge>(registrar.get());
    auto database_factory = std::make_unique<FakeWebAppDatabaseFactory>();
    sync_bridge->SetSubsystems(database_factory.get(), install_manager_,
                               command_manager.get());

    auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();
    test_url_loader_ = test_url_loader.get();
    install_manager_->SetUrlLoaderForTesting(std::move(test_url_loader));

    provider_->SetIconManager(std::move(icon_manager));
    provider_->SetInstallFinalizer(std::move(install_finalizer));
    provider_->SetInstallManager(std::move(install_manager));
    provider_->SetCommandManager(std::move(command_manager));
    provider_->SetRegistrar(std::move(registrar));
    provider_->SetDatabaseFactory(std::move(database_factory));
    provider_->SetSyncBridge(std::move(sync_bridge));

    test::AwaitStartWebAppProviderAndSubsystems(profile());

    provider_->sync_bridge().set_disable_checks_for_testing(true);
  }

  void TearDown() override {
    DestroyManagers();
    WebAppTest::TearDown();
  }

  WebAppRegistrar& registrar() const { return *registrar_; }
  WebAppCommandManager& command_manager() {
    return provider_->command_manager();
  }
  WebAppInstallManager& install_manager() { return *install_manager_; }
  WebAppInstallFinalizer& finalizer() { return *install_finalizer_; }
  WebAppIconManager& icon_manager() { return *icon_manager_; }
  TestWebAppUrlLoader& url_loader() { return *test_url_loader_; }
  TestFileUtils& file_utils() {
    DCHECK(file_utils_);
    return *file_utils_;
  }
  FakeWebAppProvider& provider() { return *provider_; }

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

  AppId InitRegistrarWithApp(std::unique_ptr<WebApp> app) {
    DCHECK(registrar().is_empty());
    const AppId& app_id = app->app_id();
    {
      ScopedRegistryUpdate update(&provider().sync_bridge());
      update->CreateApp(std::move(app));
    }
    return app_id;
  }

  struct InstallResult {
    AppId app_id;
    webapps::InstallResultCode code;
  };

  InstallResult InstallWebAppFromInfo(
      std::unique_ptr<WebAppInstallInfo> install_info,
      bool overwrite_existing_manifest_fields,
      webapps::WebappInstallSource install_source) {
    InstallResult result;
    base::RunLoop run_loop;
    command_manager().ScheduleCommand(std::make_unique<InstallFromInfoCommand>(
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

    for ([[maybe_unused]] const WebApp& app : registrar().GetApps()) {
      ++num_apps;
    }

    return num_apps;
  }

  webapps::UninstallResultCode UninstallPolicyWebAppByUrl(const GURL& app_url) {
    absl::optional<AppId> app_id =
        provider().registrar().LookupExternalAppId(app_url);
    if (!app_id.has_value()) {
      return webapps::UninstallResultCode::kNoAppToUninstall;
    }

    webapps::UninstallResultCode result;
    base::RunLoop run_loop;
    auto uninstall_command = std::make_unique<WebAppUninstallCommand>(
        app_id.value(), WebAppManagement::kPolicy,
        webapps::WebappUninstallSource::kExternalPolicy,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          result = code;
          run_loop.Quit();
        }),
        profile(), &provider().os_integration_manager(),
        &provider().sync_bridge(), &icon_manager(), &registrar(),
        &install_manager(), &provider().translation_manager());
    uninstall_command->SetRemoveManagementTypeCallbackForTesting(
        base::BindLambdaForTesting([&](const AppId& app_id) {
          // On removing the policy source, the web app can now be user
          // uninstalled.
          EXPECT_TRUE(finalizer().CanUserUninstallWebApp(app_id));
        }));
    command_manager().ScheduleCommand(std::move(uninstall_command));
    run_loop.Run();
    return result;
  }

  webapps::UninstallResultCode UninstallWebApp(const AppId& app_id) {
    webapps::UninstallResultCode result;
    base::RunLoop run_loop;

    auto uninstall_command = std::make_unique<WebAppUninstallCommand>(
        app_id, /*management_source=*/absl::nullopt,
        webapps::WebappUninstallSource::kAppMenu,
        base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
          result = code;
          run_loop.Quit();
        }),
        profile(), &provider().os_integration_manager(),
        &provider().sync_bridge(), &icon_manager(), &registrar(),
        &install_manager(), &provider().translation_manager());
    command_manager().ScheduleCommand(std::move(uninstall_command));
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
    provider().Shutdown();
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

  raw_ptr<TestWebAppUrlLoader> test_url_loader_ = nullptr;
  raw_ptr<FakeWebAppProvider> provider_;
  raw_ptr<WebAppIconManager> icon_manager_;
  raw_ptr<WebAppInstallManager> install_manager_;
  raw_ptr<WebAppInstallFinalizer> install_finalizer_;
  raw_ptr<WebAppRegistrar> registrar_;

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
  install_manager().SetUninstallCallbackForTesting(base::BindLambdaForTesting(
      [&](const AppId& uninstalled_app_id, webapps::UninstallResultCode code) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        event_order.push_back(Event::kUninstallFromSync_Callback);
        run_loop.Quit();
      }));

  // The sync server sends a change to delete the app.
  sync_bridge_test_utils::DeleteApps(provider().sync_bridge(), {app_id});
  event_order.push_back(Event::kUninstallFromSync);
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
  install_manager().SetUninstallCallbackForTesting(base::BindLambdaForTesting(
      [&](const AppId& uninstalled_app_id, webapps::UninstallResultCode code) {
        EXPECT_EQ(uninstalled_app_id, app_id);
        EXPECT_EQ(code, webapps::UninstallResultCode::kSuccess);
        event_order.push_back(Event::kUninstallFromSync_Callback);
        run_loop.Quit();
      }));

  // The sync server sends a change to delete the app.
  sync_bridge_test_utils::DeleteApps(provider().sync_bridge(), {app_id});
  event_order.push_back(Event::kUninstallFromSync);
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

  InitRegistrarWithApp(std::move(policy_and_user_app));
  test::AddInstallUrlData(profile()->GetPrefs(), &provider().sync_bridge(),
                          app_id, external_app_url,
                          ExternalInstallSource::kExternalPolicy);

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
}

TEST_P(WebAppInstallManagerTest_SyncOnly,
       PolicyAndUser_UninstallExternalWebAppInstallManagerObserver) {
  std::unique_ptr<WebApp> policy_and_user_app = test::CreateWebApp(
      GURL("https://example.com/path"), WebAppManagement::kSync);
  policy_and_user_app->AddSource(WebAppManagement::kPolicy);
  policy_and_user_app->SetUserDisplayMode(UserDisplayMode::kStandalone);

  const AppId app_id = policy_and_user_app->app_id();
  const GURL external_app_url("https://example.com/path/policy");

  InitRegistrarWithApp(std::move(policy_and_user_app));
  test::AddInstallUrlData(profile()->GetPrefs(), &provider().sync_bridge(),
                          app_id, external_app_url,
                          ExternalInstallSource::kExternalPolicy);

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

  InitRegistrarWithApp(std::move(default_and_user_app));
  test::AddInstallUrlData(profile()->GetPrefs(), &provider().sync_bridge(),
                          app_id, external_app_url,
                          ExternalInstallSource::kExternalDefault);

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

  InitRegistrarWithApp(std::move(default_and_user_app));
  test::AddInstallUrlData(profile()->GetPrefs(), &provider().sync_bridge(),
                          app_id, external_app_url,
                          ExternalInstallSource::kExternalDefault);

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

TEST_P(WebAppInstallManagerTest_SyncOnly,
       InstallWebAppFromWebAppStoreThenInstallFromSync) {
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
    sync_bridge_test_utils::AddApps(provider().sync_bridge(),
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
