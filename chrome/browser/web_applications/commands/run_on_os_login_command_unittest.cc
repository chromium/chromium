// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/run_on_os_login_command.h"

#include <memory>
#include <optional>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

namespace {

class RunOnOsLoginCommandTest : public WebAppTest {
 public:
  RunOnOsLoginCommandTest() = default;
  ~RunOnOsLoginCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ = OsIntegrationTestOverrideImpl::OverrideForTesting();
    }
    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    test::UninstallAllWebApps(profile());
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

 protected:
  WebAppRegistrar& registrar() { return provider()->registrar_unsafe(); }

  FakeOsIntegrationManager& os_integration_manager() {
    return static_cast<FakeOsIntegrationManager&>(
        provider()->os_integration_manager());
  }

  WebAppProvider* provider() { return provider_; }

  webapps::AppId InstallNonLocallyInstalledApp(const GURL url) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    info->title = u"Test App";
    info->user_display_mode = mojom::UserDisplayMode::kStandalone;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;

    // InstallFromInfoWithParams does not trigger OS integration.
    provider()->scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    const webapps::AppId app_id = result.Get<webapps::AppId>();
    return app_id;
  }

  void InitSetRunOnOsLoginCommandAndAwaitCompletion(
      const webapps::AppId& app_id,
      RunOnOsLoginMode mode) {
    base::test::TestFuture<void> future;
    provider()->command_manager().ScheduleCommand(
        RunOnOsLoginCommand::CreateForSetLoginMode(app_id, mode,
                                                   future.GetCallback()));
    EXPECT_TRUE(future.Wait());
    provider()->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  void InitSyncRunOnOsLoginCommandAndAwaitCompletion(
      const webapps::AppId& app_id) {
    base::test::TestFuture<void> future;
    provider()->command_manager().ScheduleCommand(
        RunOnOsLoginCommand::CreateForSyncLoginMode(app_id,
                                                    future.GetCallback()));
    EXPECT_TRUE(future.Wait());
    provider()->command_manager().AwaitAllCommandsCompleteForTesting();
  }

  // Error modes or the ROOL mode not set is returned as UNSPECIFIED.
  proto::RunOnOsLoginMode GetRunOnOsLoginMode(const webapps::AppId& app_id) {
    auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
    if (!state.has_value()) {
      return proto::RunOnOsLoginMode::RUN_ON_OS_LOGIN_MODE_UNSPECIFIED;
    }

    const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    if (!os_integration_state.has_run_on_os_login()) {
      return proto::RunOnOsLoginMode::RUN_ON_OS_LOGIN_MODE_UNSPECIFIED;
    }

    return os_integration_state.run_on_os_login().run_on_os_login_mode();
  }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

TEST_F(RunOnOsLoginCommandTest, SetRunOnOsLoginModes) {
  const webapps::AppId app_id =
      InstallNonLocallyInstalledApp(GURL("https://example.com/"));
  base::HistogramTester tester;

  EXPECT_EQ(RunOnOsLoginMode::kNotRun,
            registrar().GetAppRunOnOsLoginMode(app_id).value);

  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN, GetRunOnOsLoginMode(app_id));
  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 0);

  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kWindowed);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 1);

  EXPECT_EQ(RunOnOsLoginMode::kWindowed,
            registrar().GetAppRunOnOsLoginMode(app_id).value);
  EXPECT_EQ(proto::RunOnOsLoginMode::WINDOWED, GetRunOnOsLoginMode(app_id));

  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kMinimized);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 2);

  EXPECT_EQ(RunOnOsLoginMode::kMinimized,
            registrar().GetAppRunOnOsLoginMode(app_id).value);
  EXPECT_EQ(proto::RunOnOsLoginMode::MINIMIZED, GetRunOnOsLoginMode(app_id));
}

TEST_F(RunOnOsLoginCommandTest, SyncRunOnOsLoginModes) {
  const webapps::AppId app_id_default =
      InstallNonLocallyInstalledApp(GURL("https://example.com/"));
  const webapps::AppId app_id_default2 =
      InstallNonLocallyInstalledApp(GURL("https:/default2.example/"));
  const webapps::AppId app_id_windowed =
      InstallNonLocallyInstalledApp(GURL("https://windowed.example/"));
  const webapps::AppId app_id_allowed =
      InstallNonLocallyInstalledApp(GURL("https://allowed.example/"));

  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id_default2,
                                               RunOnOsLoginMode::kWindowed);
  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id_allowed,
                                               RunOnOsLoginMode::kWindowed);

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

  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN,
            GetRunOnOsLoginMode(app_id_default));
  EXPECT_EQ(proto::RunOnOsLoginMode::WINDOWED,
            GetRunOnOsLoginMode(app_id_default2));
  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN,
            GetRunOnOsLoginMode(app_id_windowed));
  EXPECT_EQ(proto::RunOnOsLoginMode::WINDOWED,
            GetRunOnOsLoginMode(app_id_allowed));

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
  provider()->command_manager().AwaitAllCommandsCompleteForTesting();

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

  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN,
            GetRunOnOsLoginMode(app_id_default));
  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN,
            GetRunOnOsLoginMode(app_id_default2));
  EXPECT_EQ(proto::RunOnOsLoginMode::WINDOWED,
            GetRunOnOsLoginMode(app_id_windowed));
  EXPECT_EQ(proto::RunOnOsLoginMode::WINDOWED,
            GetRunOnOsLoginMode(app_id_allowed));
}

// Syncing on a web_app with a Run on OS Login mode of kNotRun will
// mock a reset of OS Hooks, triggering the UninstallOsHooks workflow.
TEST_F(RunOnOsLoginCommandTest, SyncCommandAndUninstallOSHooks) {
  const webapps::AppId app_id =
      InstallNonLocallyInstalledApp(GURL("https://example.com/"));

  InitSyncRunOnOsLoginCommandAndAwaitCompletion(app_id);
  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN, GetRunOnOsLoginMode(app_id));
}

TEST_F(RunOnOsLoginCommandTest, AbortOnAppNotLocallyInstalled) {
  base::HistogramTester tester;

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled, 0);

  InitSyncRunOnOsLoginCommandAndAwaitCompletion("abc");

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kAppNotLocallyInstalled, 1);
}

TEST_F(RunOnOsLoginCommandTest,
       AbortCommandOnAlreadyMatchingRunOnOsLoginState) {
  const webapps::AppId app_id =
      InstallNonLocallyInstalledApp(GURL("https://example.com/"));
  base::HistogramTester tester;

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 0);

  // Use the command system to first set a Run on OS Login mode.
  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kWindowed);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kSuccessfulCompletion, 1);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched, 0);

  // Running set again should stop the command from being run again.
  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kWindowed);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kRunOnOsLoginModeAlreadyMatched, 1);
}

TEST_F(RunOnOsLoginCommandTest, AbortCommandOnPolicyBlockedApp) {
  const webapps::AppId app_id =
      InstallNonLocallyInstalledApp(GURL("https:/default.example/"));
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
  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kWindowed);

  tester.ExpectBucketCount(
      "WebApp.RunOnOsLogin.CommandCompletionState",
      RunOnOsLoginCommandCompletionState::kNotAllowedByPolicy, 1);
}

TEST_F(RunOnOsLoginCommandTest, VerifySetWorksOnAppWithNoStateDefined) {
  const webapps::AppId app_id =
      InstallNonLocallyInstalledApp(GURL("https://example.com/"));

  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kNotRun);
  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN, GetRunOnOsLoginMode(app_id));

  InitSetRunOnOsLoginCommandAndAwaitCompletion(app_id,
                                               RunOnOsLoginMode::kWindowed);
  EXPECT_EQ(proto::RunOnOsLoginMode::WINDOWED, GetRunOnOsLoginMode(app_id));
}

TEST_F(RunOnOsLoginCommandTest, VerifySyncWorksOnAppWithNoStateDefined) {
  const webapps::AppId app_id =
      InstallNonLocallyInstalledApp(GURL("https:/default.example/"));

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

  InitSyncRunOnOsLoginCommandAndAwaitCompletion(app_id);
  EXPECT_EQ(proto::RunOnOsLoginMode::NOT_RUN, GetRunOnOsLoginMode(app_id));
}

}  // namespace

}  // namespace web_app
