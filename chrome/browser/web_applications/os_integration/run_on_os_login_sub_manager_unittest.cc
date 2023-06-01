// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class RunOnOsLoginSubManagerTestBase : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  RunOnOsLoginSubManagerTestBase() = default;
  ~RunOnOsLoginSubManagerTestBase() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverrideImpl::OverrideForTesting(base::GetHomeDir());
    }
    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), /*icon_manager=*/nullptr, file_handler_manager.get(),
        protocol_handler_manager.get());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), /*url_handler_manager=*/nullptr);

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    test::UninstallAllWebApps(profile());
    {
      // Blocking required due to file operations in the shortcut override
      // destructor.
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  AppId InstallWebApp() {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

  void SetWebAppSettingsListPref(const base::StringPiece pref) {
    auto result = base::JSONReader::ReadAndReturnValueWithError(
        pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
    ASSERT_TRUE(result.has_value()) << result.error().message;
    ASSERT_TRUE(result->is_list());
    profile()->GetPrefs()->Set(prefs::kWebAppSettings, std::move(*result));
  }

 protected:
  WebAppProvider& provider() { return *provider_; }
  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_;
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

// Configure tests only.
class RunOnOsLoginSubManagerConfigureTest
    : public RunOnOsLoginSubManagerTestBase,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  RunOnOsLoginSubManagerConfigureTest() = default;
  ~RunOnOsLoginSubManagerConfigureTest() override = default;

  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers, {{"stage", "write_config"}}},
           {features::kDesktopPWAsRunOnOsLogin, {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kDesktopPWAsRunOnOsLogin},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    RunOnOsLoginSubManagerTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(RunOnOsLoginSubManagerConfigureTest,
       VerifyRunOnOsLoginSetProperlyOnInstall) {
  const AppId& app_id = InstallWebApp();

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    // on installation, both values are set to NOT_RUN.
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::NOT_RUN));
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerConfigureTest, VerifyRunOnOsLoginSetFromCommand) {
  const AppId& app_id = InstallWebApp();

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::WINDOWED));
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerConfigureTest, VerifyPolicySettingBlocked) {
  const AppId& app_id = InstallWebApp();

  const char kWebAppSettingPolicyBlockedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "blocked"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyBlockedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::NOT_RUN));
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerConfigureTest, VerifyPolicySettingWindowedMode) {
  const AppId& app_id = InstallWebApp();

  const char kWebAppSettingPolicyWindowedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "run_windowed"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyWindowedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::WINDOWED));
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerConfigureTest, VerifyPolicySettingAllowedMode) {
  const AppId& app_id = InstallWebApp();

  const char kWebAppSettingPolicyAllowedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "allowed"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyAllowedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::NOT_RUN));
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerConfigureTest, StatesEmptyOnUninstall) {
  const AppId& app_id = InstallWebApp();
  test::UninstallAllWebApps(profile());
  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RunOnOsLoginSubManagerConfigureTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

// Configure and Execute tests.
class RunOnOsLoginSubManagerExecuteTest
    : public RunOnOsLoginSubManagerTestBase,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  RunOnOsLoginSubManagerExecuteTest() = default;
  ~RunOnOsLoginSubManagerExecuteTest() override = default;

  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers,
            {{"stage", "execute_and_write_config"}}},
           {features::kDesktopPWAsRunOnOsLogin, {}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kDesktopPWAsRunOnOsLogin},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    RunOnOsLoginSubManagerTestBase::SetUp();
  }

  bool IsRunOnOsLoginExecuteEnabled() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    return true;
#else
    return false;
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(RunOnOsLoginSubManagerExecuteTest, InstallRunOnOsLoginNotRun) {
  const AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  // A default install should have a Run on OS Login mode of kNotRun, so no
  // OS integration should be triggered.
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerExecuteTest,
       InstallAndExecuteWindowedRunOnOsLogin) {
  const AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerExecuteTest, BlockedPolicySettingNoOsIntegration) {
  const AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  const char kWebAppSettingPolicyBlockedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "blocked"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyBlockedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerExecuteTest,
       WindowedPolicySettingAllowsOsIntegration) {
  const AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  const char kWebAppSettingPolicyBlockedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "run_windowed"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyBlockedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerExecuteTest, UpdateRunOnOsLoginMode) {
  const AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future_windowed;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future_windowed.GetCallback());
  EXPECT_TRUE(future_windowed.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }

  // Changing to kNotRun should update and unregister all OS integrations.
  base::test::TestFuture<void> future_not_run;
  provider().scheduler().SetRunOnOsLoginMode(app_id, RunOnOsLoginMode::kNotRun,
                                             future_not_run.GetCallback());
  EXPECT_TRUE(future_not_run.Wait());

  auto updated_state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state_updated =
      updated_state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state_updated.has_run_on_os_login());
  }
}

TEST_P(RunOnOsLoginSubManagerExecuteTest, UnregisterRunOnOsLogin) {
  const AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_run_on_os_login());
  }

  test::UninstallAllWebApps(profile());
  if (AreOsIntegrationSubManagersEnabled()) {
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    RunOnOsLoginSubManagerExecuteTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateAndExecute,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
