// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"

namespace web_app {

namespace {

class TestOsIntegrationManager : public FakeOsIntegrationManager {
 public:
  TestOsIntegrationManager(
      Profile* profile,
      std::unique_ptr<WebAppShortcutManager> shortcut_manager,
      std::unique_ptr<WebAppFileHandlerManager> file_handler_manager,
      std::unique_ptr<WebAppProtocolHandlerManager> protocol_handler_manager,
      std::unique_ptr<UrlHandlerManager> url_handler_manager)
      : FakeOsIntegrationManager(profile,
                                 std::move(shortcut_manager),
                                 std::move(file_handler_manager),
                                 std::move(protocol_handler_manager),
                                 std::move(url_handler_manager)),
        profile_(profile) {}
  ~TestOsIntegrationManager() override = default;

  // OsIntegrationManager:
  void InstallOsHooks(const AppId& app_id,
                      InstallOsHooksCallback callback,
                      std::unique_ptr<WebAppInstallInfo> web_app_info,
                      InstallOsHooksOptions options) override {
    if (options.os_hooks[OsHookType::kRunOnOsLogin]) {
      ScopedRegistryUpdate update(
          &WebAppProvider::GetForTest(profile_)->sync_bridge());
      update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
          RunOnOsLoginMode::kWindowed);
    }
    FakeOsIntegrationManager::InstallOsHooks(app_id, std::move(callback),
                                             std::move(web_app_info), options);
  }

  void UninstallOsHooks(const AppId& app_id,
                        const OsHooksOptions& os_hooks,
                        UninstallOsHooksCallback callback) override {
    if (os_hooks[OsHookType::kRunOnOsLogin]) {
      ScopedRegistryUpdate update(
          &WebAppProvider::GetForTest(profile_)->sync_bridge());
      update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
          RunOnOsLoginMode::kNotRun);
    }
    FakeOsIntegrationManager::UninstallOsHooks(app_id, os_hooks,
                                               std::move(callback));
  }

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace

class RunOnOsLoginCommandUnitTest : public WebAppTest {
 public:
  RunOnOsLoginCommandUnitTest() = default;
  RunOnOsLoginCommandUnitTest(const RunOnOsLoginCommandUnitTest&) = delete;
  RunOnOsLoginCommandUnitTest& operator=(const RunOnOsLoginCommandUnitTest&) =
      delete;
  ~RunOnOsLoginCommandUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();

    auto* provider = FakeWebAppProvider::Get(profile());
    auto os_integration_manager = std::make_unique<TestOsIntegrationManager>(
        profile(), /*app_shortcut_manager=*/nullptr,
        /*file_handler_manager=*/nullptr,
        /*protocol_handler_manager=*/nullptr,
        /*url_handler_manager*/ nullptr);
    os_integration_manager_ = os_integration_manager.get();
    provider_ = provider;
    provider->SetOsIntegrationManager(std::move(os_integration_manager));
    provider->SkipAwaitingExtensionSystem();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider* provider() { return provider_; }

  FakeOsIntegrationManager& fake_os_integration_manager() {
    return *os_integration_manager_;
  }

  AppId RegisterApp(const GURL& start_url = GURL("https://example.com/path")) {
    auto web_app = test::CreateWebApp(start_url);
    AppId app_id = web_app->app_id();
    web_app->SetRunOnOsLoginOsIntegrationState(RunOnOsLoginMode::kNotRun);
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }
    return app_id;
  }

 private:
  raw_ptr<FakeOsIntegrationManager> os_integration_manager_ = nullptr;
  raw_ptr<WebAppProvider> provider_ = nullptr;
};

TEST_F(RunOnOsLoginCommandUnitTest, PersistRunOnOsLoginUserChoice) {
  const AppId app_id = RegisterApp();

  // If an app is not installed, validate we don't attempt to register with the
  // OS.
  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), "FakeAppId", RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(0u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      0u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());

  // RunOnOsLoginMode::kNotRun should be the default, validate we don't attempt
  // to register with the OS.
  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kNotRun);
  EXPECT_EQ(0u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      0u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());

  // Validate that toggling to kWindowed invokes the OsIntegrationManager, and
  // that repeated calls do not.
  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(1u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());

  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(1u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());

  // Validate that toggling back to kNotRun invokes the OsIntegrationManager.
  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kNotRun);
  EXPECT_EQ(1u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      1u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());
}

TEST_F(RunOnOsLoginCommandUnitTest,
       PersistRunOnOsLoginUserChoiceAppNotLocallyInstalled) {
  const AppId app_id = RegisterApp();

  // Simulate the app not locally installed.
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->UpdateApp(app_id)->SetIsLocallyInstalled(false);
  }

  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(0u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      0u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());

  // Simulate the app locally installed.
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->UpdateApp(app_id)->SetIsLocallyInstalled(true);
  }

  PersistRunOnOsLoginUserChoice(
      &provider()->registrar(), &provider()->os_integration_manager(),
      &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(1u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      0u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());
}

TEST_F(RunOnOsLoginCommandUnitTest,
       PersistRunOnOsLoginUserChoiceForAlreadyInstalledApp) {
  // |web_app->run_on_os_login_os_integration_state()| returns an optional
  // value. A null value can be returned if a web app was installed prior to the
  // completion of the work associated with https://crbug.com/1280773. This test
  // validates that both states are handled properly.

  // If the user has configured an app to run during os-login, and there
  // is no os integration state recorded, then confirm that the
  // OSIntegrationManager is invoked when expected.
  {
    auto web_app = test::CreateWebApp(GURL("https://windowed.example/"));
    const AppId app_id = web_app->app_id();
    web_app->SetRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    // Validate we don't attempt to register with the OS.
    PersistRunOnOsLoginUserChoice(
        &provider()->registrar(), &provider()->os_integration_manager(),
        &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        0u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());

    // Validate we do attempt to unregister with the OS.
    PersistRunOnOsLoginUserChoice(
        &provider()->registrar(), &provider()->os_integration_manager(),
        &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kNotRun);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // If the user has not configured an app to run during os-login, and there
  // is no os integration state recorded, then confirm that the
  // OSIntegrationManager is invoked when expected.
  {
    auto web_app = test::CreateWebApp(GURL("https://allowed.example/"));
    const AppId app_id = web_app->app_id();
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    // Validate we don't attempt to unregister with the OS.
    PersistRunOnOsLoginUserChoice(
        &provider()->registrar(), &provider()->os_integration_manager(),
        &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kNotRun);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());

    // Validate we do attempt to register with the OS.
    PersistRunOnOsLoginUserChoice(
        &provider()->registrar(), &provider()->os_integration_manager(),
        &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
    EXPECT_EQ(
        1u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }
}

TEST_F(RunOnOsLoginCommandUnitTest, SyncRunOnOsLoginOsIntegrationState) {
  const char kWebAppSettingWithDefaultConfiguration[] = R"({
    "https://windowed.example/": {
      "run_on_os_login": "run_windowed"
    },
    "https://allowed.example/": {
      "run_on_os_login": "allowed"
    },
    "*": {
      "run_on_os_login": "blocked"
    }
  })";

  test::SetWebAppSettingsDictPref(profile(),
                                  kWebAppSettingWithDefaultConfiguration);
  provider()->policy_manager().RefreshPolicySettingsForTesting();

  // This app falls under the default configuration, and the app has not
  // previously been registered to run-on-os-login.
  {
    const AppId app_id = RegisterApp();
    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        0u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // This app falls under the default configuration, and the app has been
  // previously been registered to run-on-os-login.
  {
    const AppId app_id = RegisterApp(GURL("https://test.example/"));
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
          RunOnOsLoginMode::kWindowed);
    }

    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // This app has a specific policy that forces an app to run during os login.
  {
    const AppId app_id = RegisterApp(GURL("https://windowed.example/"));
    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        1u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
    EXPECT_EQ(RunOnOsLoginMode::kWindowed,
              provider()->registrar().GetAppRunOnOsLoginMode(app_id).value);
  }

  // This app has a specific policy that allows the user to change the
  // run-on-os-login state.
  {
    const AppId app_id = RegisterApp(GURL("https://allowed.example/"));
    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        1u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
    EXPECT_EQ(RunOnOsLoginMode::kNotRun,
              provider()->registrar().GetAppRunOnOsLoginMode(app_id).value);

    PersistRunOnOsLoginUserChoice(
        &provider()->registrar(), &provider()->os_integration_manager(),
        &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kWindowed);
    EXPECT_EQ(
        2u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());

    PersistRunOnOsLoginUserChoice(
        &provider()->registrar(), &provider()->os_integration_manager(),
        &provider()->sync_bridge(), app_id, RunOnOsLoginMode::kNotRun);
    EXPECT_EQ(
        2u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        2u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }
}

TEST_F(RunOnOsLoginCommandUnitTest,
       SyncRunOnOsLoginOsIntegrationStateForAlreadyInstalledApp) {
  // |web_app->run_on_os_login_os_integration_state()| returns an optional
  // value. A null value can be returned if a web app was installed prior to the
  // completion of the work associated with https://crbug.com/1280773. This test
  // validates that both states are handled properly.
  const char kWebAppSettingWithDefaultConfiguration[] = R"({
    "https://windowed.example/": {
      "run_on_os_login": "run_windowed"
    },
    "https://allowed.example/": {
      "run_on_os_login": "allowed"
    },
    "https://allowed2.example/": {
      "run_on_os_login": "allowed"
    },
    "*": {
      "run_on_os_login": "blocked"
    }
  })";

  test::SetWebAppSettingsDictPref(profile(),
                                  kWebAppSettingWithDefaultConfiguration);
  provider()->policy_manager().RefreshPolicySettingsForTesting();

  // This app falls under the default configuration, and the app has not
  // previously been registered to run-on-os-login.
  {
    auto web_app = test::CreateWebApp(GURL("https://example.com/path"));
    const AppId app_id = web_app->app_id();
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        1u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // This app falls under the default configuration, and the app has been
  // previously been registered to run-on-os-login.
  {
    auto web_app = test::CreateWebApp(GURL("https://test.example/"));
    const AppId app_id = web_app->app_id();
    web_app->SetRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        0u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        2u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // This app has a specific policy that forces an app to run during os login.
  {
    auto web_app = test::CreateWebApp(GURL("https://windowed.example/"));
    const AppId app_id = web_app->app_id();
    web_app->SetRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        1u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        2u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // This app has a specific policy that allows the user to change the
  // run-on-os-login state. The user has configured this app to run during OS
  // login.
  {
    auto web_app = test::CreateWebApp(GURL("https://allowed.example/"));
    const AppId app_id = web_app->app_id();
    web_app->SetRunOnOsLoginMode(RunOnOsLoginMode::kWindowed);
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        2u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        2u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }

  // This app has a specific policy that allows the user to change the
  // run-on-os-login state. The user has not configured this app to run during
  // OS login.
  {
    auto web_app = test::CreateWebApp(GURL("https://allowed2.example/"));
    const AppId app_id = web_app->app_id();
    web_app->SetRunOnOsLoginMode(RunOnOsLoginMode::kNotRun);
    {
      ScopedRegistryUpdate update(&provider()->sync_bridge());
      update->CreateApp(std::move(web_app));
    }

    SyncRunOnOsLoginOsIntegrationState(&provider()->registrar(),
                                       &provider()->os_integration_manager(),
                                       app_id);
    EXPECT_EQ(
        2u, fake_os_integration_manager().num_register_run_on_os_login_calls());
    EXPECT_EQ(
        3u,
        fake_os_integration_manager().num_unregister_run_on_os_login_calls());
  }
}

TEST_F(RunOnOsLoginCommandUnitTest,
       SyncRunOnOsLoginOsIntegrationStateAppNotLocallyInstalled) {
  const char kWebAppSettingWithDefaultConfiguration[] = R"({
    "https://windowed.example/": {
      "run_on_os_login": "run_windowed"
    },
    "https://allowed.example/": {
      "run_on_os_login": "allowed"
    },
    "*": {
      "run_on_os_login": "blocked"
    }
  })";

  test::SetWebAppSettingsDictPref(profile(),
                                  kWebAppSettingWithDefaultConfiguration);
  provider()->policy_manager().RefreshPolicySettingsForTesting();

  const AppId app_id = RegisterApp(GURL("https://windowed.example/"));

  // Simulate the app not locally installed.
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->UpdateApp(app_id)->SetIsLocallyInstalled(false);
  }

  SyncRunOnOsLoginOsIntegrationState(
      &provider()->registrar(), &provider()->os_integration_manager(), app_id);
  EXPECT_EQ(0u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      0u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());

  // Simulate the app locally installed.
  {
    ScopedRegistryUpdate update(&provider()->sync_bridge());
    update->UpdateApp(app_id)->SetIsLocallyInstalled(true);
  }

  SyncRunOnOsLoginOsIntegrationState(
      &provider()->registrar(), &provider()->os_integration_manager(), app_id);
  EXPECT_EQ(1u,
            fake_os_integration_manager().num_register_run_on_os_login_calls());
  EXPECT_EQ(
      0u, fake_os_integration_manager().num_unregister_run_on_os_login_calls());
}

}  // namespace web_app
