// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      ScopedRegistryUpdate update = WebAppProvider::GetForTest(profile_)
                                        ->sync_bridge_unsafe()
                                        .BeginUpdate();
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
      ScopedRegistryUpdate update = WebAppProvider::GetForTest(profile_)
                                        ->sync_bridge_unsafe()
                                        .BeginUpdate();
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

class RunOnOsLoginCommandTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  RunOnOsLoginCommandTest() {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers, {{"stage", "write_config"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{}, {features::kOsIntegrationSubManagers});
    }
  }
  ~RunOnOsLoginCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());
    auto os_integration_manager = std::make_unique<TestOsIntegrationManager>(
        profile(), /*app_shortcut_manager=*/nullptr,
        /*file_handler_manager=*/nullptr,
        /*protocol_handler_manager=*/nullptr,
        /*url_handler_manager=*/nullptr);
    os_integration_manager_ = os_integration_manager.get();
    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  WebAppSyncBridge& sync_bridge() { return provider()->sync_bridge_unsafe(); }

  FakeOsIntegrationManager* os_integration_manager() {
    return os_integration_manager_;
  }

  void RegisterApp(std::unique_ptr<WebApp> web_app) {
    web_app->SetRunOnOsLoginOsIntegrationState(RunOnOsLoginMode::kNotRun);
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  WebAppProvider* provider() { return provider_; }

 private:
  raw_ptr<FakeOsIntegrationManager, DanglingUntriaged> os_integration_manager_ =
      nullptr;
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(RunOnOsLoginCommandTest, SetRunOnOsLoginModes) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));
  base::HistogramTester tester;

  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar().GetAppRunOnOsLoginMode(app_id).value);
  EXPECT_EQ(
      RunOnOsLoginMode::kNotRun,
      registrar().GetExpectedRunOnOsLoginOsIntegrationState(app_id).value());
  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 0);

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop.QuitClosure()));
  loop.Run();
  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 1);

  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id).value);
  EXPECT_EQ(
      RunOnOsLoginMode::kWindowed,
      registrar().GetExpectedRunOnOsLoginOsIntegrationState(app_id).value());
  EXPECT_EQ(1u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(0u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());

  base::RunLoop loop1;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kMinimized, loop1.QuitClosure()));
  loop1.Run();
  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 2);

  EXPECT_EQ(RunOnOsLoginMode::kMinimized,
            registrar().GetAppRunOnOsLoginMode(app_id).value);
  EXPECT_EQ(
      RunOnOsLoginMode::kWindowed,
      registrar().GetExpectedRunOnOsLoginOsIntegrationState(app_id).value());
  EXPECT_EQ(2u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(0u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());
}

TEST_P(RunOnOsLoginCommandTest, SyncRunOnOsLoginModes) {
  auto web_app_default = test::CreateWebApp();
  auto web_app_default2 = test::CreateWebApp(GURL("https:/default2.example/"));
  auto web_app_windowed = test::CreateWebApp(GURL("https://windowed.example/"));
  auto web_app_allowed = test::CreateWebApp(GURL("https://allowed.example/"));
  const AppId app_id_default = web_app_default->app_id();
  const AppId app_id_default2 = web_app_default2->app_id();
  const AppId app_id_windowed = web_app_windowed->app_id();
  const AppId app_id_allowed = web_app_allowed->app_id();

  RegisterApp(std::move(web_app_default));
  RegisterApp(std::move(web_app_default2));
  RegisterApp(std::move(web_app_windowed));
  RegisterApp(std::move(web_app_allowed));

  for (const AppId& app_id : {app_id_default2, app_id_allowed}) {
    base::RunLoop loop;
    provider()->command_manager().ScheduleCommand(
        RunOnOsLoginCommand::CreateForSetLoginMode(
            app_id, RunOnOsLoginMode::kWindowed, loop.QuitClosure()));
    loop.Run();
  }

  // app_id_default : RunOnOsLoginMode not changed (default value of kNotRun).
  // app_id_default2 : RunOnOsLoginMode updated to windowed.
  // app_id_windowed : RunOnOsLoginMode not changed (default value of kNotRun).
  // app_id_allowed : RunOnOsLoginMode updated to windowed.
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar().GetAppRunOnOsLoginMode(app_id_default).value);
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id_default2).value);
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar().GetAppRunOnOsLoginMode(app_id_windowed).value);
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id_allowed).value);

  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_default)
                .value());
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_default2)
                .value());
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_windowed)
                .value());
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_allowed)
                .value());
  // 2 RunOnOsLoginModes are modified.
  EXPECT_EQ(2u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(0u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());

  const char kWebAppSettingWithDefaultConfiguration[] = R"([
    {
      "manifest_id": "https://windowed.example/",
      "run_on_os_login": "run_windowed"
    },
    {
      "manifest_id": "https://allowed.example/",
      "run_on_os_login": "allowed"
    },
    {
      "manifest_id": "*",
      "run_on_os_login": "blocked"
    }
  ])";

  // Once we set the policy and refresh it, the WebAppPolicyManager can
  // invoke sync commands to verify that the proper RunOnOsLogin modes are set.
  test::SetWebAppSettingsListPref(profile(),
                                  kWebAppSettingWithDefaultConfiguration);
  WebAppPolicyManager& policy_manager = provider()->policy_manager();
  policy_manager.RefreshPolicySettingsForTesting();

  // After sync, the following should happen:
  // app_id_default : RunOnOsLoginMode not changed (default value of kNotRun).
  // app_id_default2 : RunOnOsLoginMode changed to kNotRun as per policy.
  // app_id_windowed : RunOnOsLoginMode changed to windowed as per policy.
  // app_id_allowed : RunOnOsLoginMode changed to windowed as per policy.
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar().GetAppRunOnOsLoginMode(app_id_default).value);
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar().GetAppRunOnOsLoginMode(app_id_default2).value);
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id_windowed).value);
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id_allowed).value);

  provider()->command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_default)
                .value());
  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_default2)
                .value());
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_windowed)
                .value());
  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar()
                .GetExpectedRunOnOsLoginOsIntegrationState(app_id_allowed)
                .value());
}

TEST_P(RunOnOsLoginCommandTest, RepeatedCallsDoNotCauseRepeatedOSRegistration) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));

  base::RunLoop loop1;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop1.QuitClosure()));
  loop1.Run();
  EXPECT_EQ(1u, os_integration_manager()->num_register_run_on_os_login_calls());

  base::RunLoop loop2;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop2.QuitClosure()));
  loop2.Run();
  // Count should still be 1 because repeated calls cause command to end early
  // as success.
  EXPECT_EQ(1u, os_integration_manager()->num_register_run_on_os_login_calls());
}

TEST_P(RunOnOsLoginCommandTest, NotRunDoesNotAtemptOSRegistration) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kNotRun, loop.QuitClosure()));
  loop.Run();

  // OS registration should not be attempted if the default state of Run On OS
  // Login mode is kNotRun.
  EXPECT_EQ(0u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(0u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());
}

TEST_P(RunOnOsLoginCommandTest, SyncCommandAndUninstallOSHooks) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));
  {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
        RunOnOsLoginMode::kWindowed);
  }

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSyncLoginMode(app_id, loop.QuitClosure()));
  loop.Run();

  // Syncing on a web_app with a Run on OS Login mode of kNotRun will
  // mock a reset of OS Hooks, triggering the UninstallOsHooks workflow.
  EXPECT_EQ(0u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(1u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());
}

TEST_P(RunOnOsLoginCommandTest, AbortOnAppNotLocallyInstalled) {
  base::HistogramTester tester;

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled, 0);

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSyncLoginMode("abc", loop.QuitClosure()));
  loop.Run();

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled, 1);
}

TEST_P(RunOnOsLoginCommandTest,
       AbortCommandOnAlreadyMatchingRunOnOsLoginState) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));
  base::HistogramTester tester;

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 0);

  // Use the command system to first set a Run on OS Login mode.
  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop.QuitClosure()));
  loop.Run();

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 1);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched, 0);

  // Running set again should stop the command from being run again.
  base::RunLoop loop1;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop1.QuitClosure()));
  loop1.Run();

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched, 1);
}

TEST_P(RunOnOsLoginCommandTest, AbortCommandOnPolicyBlockedApp) {
  auto web_app = test::CreateWebApp(GURL("https:/default.example/"));
  const AppId app_id = web_app->app_id();
  RegisterApp(std::move(web_app));
  base::HistogramTester tester;

  const char kWebAppBlockedPolicySetting[] = R"([
    {
      "manifest_id": "https:/default.example/",
      "run_on_os_login": "blocked"
    }
  ])";

  test::SetWebAppSettingsListPref(profile(), kWebAppBlockedPolicySetting);
  provider()->policy_manager().RefreshPolicySettingsForTesting();

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy, 0);

  // Use the command system to first set a Run on OS Login mode, it will
  // be blocked after the policy manager has refreshed the policy.
  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop.QuitClosure()));
  loop.Run();

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy, 1);
}

TEST_P(RunOnOsLoginCommandTest, VerifySetWorksOnAppWithNoStateDefined) {
  auto web_app = test::CreateWebApp();
  const AppId app_id = web_app->app_id();
  // Run on OS Login state in the web_app DB is not defined.
  {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  base::RunLoop loop1;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kNotRun, loop1.QuitClosure()));
  loop1.Run();

  // kNotRun should not invoke any calls.
  EXPECT_EQ(0u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(0u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());

  base::RunLoop loop2;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSetLoginMode(
          app_id, RunOnOsLoginMode::kWindowed, loop2.QuitClosure()));
  loop2.Run();

  // kWindowed should invoke 1 register call.
  EXPECT_EQ(1u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(0u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());
}

TEST_P(RunOnOsLoginCommandTest, VerifySyncWorksOnAppWithNoStateDefined) {
  auto web_app = test::CreateWebApp(GURL("https:/default.example/"));
  const AppId app_id = web_app->app_id();
  {
    ScopedRegistryUpdate update = sync_bridge().BeginUpdate();
    update->CreateApp(std::move(web_app));
  }

  const char kWebAppSettingWithDefaultConfiguration[] = R"([
    {
      "manifest_id": "https://default.example/",
      "run_on_os_login": "allowed"
    }
  ])";

  test::SetWebAppSettingsListPref(profile(),
                                  kWebAppSettingWithDefaultConfiguration);
  WebAppPolicyManager& policy_manager = provider()->policy_manager();
  policy_manager.RefreshPolicySettingsForTesting();

  base::RunLoop loop;
  provider()->command_manager().ScheduleCommand(
      RunOnOsLoginCommand::CreateForSyncLoginMode(app_id, loop.QuitClosure()));
  loop.Run();

  EXPECT_EQ(0u, os_integration_manager()->num_register_run_on_os_login_calls());
  EXPECT_EQ(1u,
            os_integration_manager()->num_unregister_run_on_os_login_calls());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RunOnOsLoginCommandTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace web_app
